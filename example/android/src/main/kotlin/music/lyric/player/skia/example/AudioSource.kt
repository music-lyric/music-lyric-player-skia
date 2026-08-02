package music.lyric.player.skia.example

import android.content.Context
import android.media.AudioAttributes
import android.media.MediaPlayer
import android.net.Uri
import android.os.SystemClock
import kotlin.math.abs

class AudioSource : AutoCloseable {
    private val lock = Any()

    private var media: MediaPlayer? = null

    // The play head as the demo reports it: a position that was true at one point on the monotonic clock, and whether it has been moving since.
    private var anchor = 0
    private var anchoredAt = 0L
    private var polledAt = 0L
    private var playing = false

    var onCompleted: (() -> Unit)? = null

    @Volatile
    var duration: Int = 0
        private set

    @Volatile
    var isLoaded: Boolean = false
        private set

    val isPlaying: Boolean
        get() = synchronized(this.lock) { this.playing }

    fun load(context: Context, uri: Uri, onReady: () -> Unit, onFailed: (String) -> Unit) {
        val media = MediaPlayer()
        media.setAudioAttributes(
            AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .build(),
        )

        media.setOnPreparedListener {
            this.swap(media)
            onReady()
        }

        media.setOnCompletionListener {
            synchronized(this.lock) {
                if (this.media === media) {
                    this.playing = false
                    this.anchorTo(this.duration, SystemClock.uptimeMillis())
                }
            }
            this.onCompleted?.invoke()
        }

        // An error retires the player it came from, whether or not it ever became the one being played.
        media.setOnErrorListener { _, what, extra ->
            this.discard(media)
            onFailed("media player error $what/$extra")
            true
        }

        try {
            media.setDataSource(context, uri)
            media.prepareAsync()
        } catch (error: Exception) {
            media.release()
            onFailed(error.message ?: error.toString())
        }
    }

    fun play() {
        synchronized(this.lock) {
            val media = this.media ?: return
            media.start()

            // Starting once the audio has run out rewinds it, so where it now stands is read back rather than assumed.
            val now = SystemClock.uptimeMillis()
            this.playing = true
            this.anchorTo(media.currentPosition, now)

            // Sound takes a moment to actually flow, and asking again inside that moment would only anchor to a head that has not moved yet.
            this.polledAt = now
        }
    }

    fun pause() {
        synchronized(this.lock) {
            val media = this.media ?: return

            // The estimate is what the lyric has been following, so it is where the pause leaves it.
            val now = SystemClock.uptimeMillis()
            this.anchorTo(this.estimate(now), now)
            this.playing = false

            media.pause()
        }
    }

    fun seekTo(position: Int) {
        synchronized(this.lock) {
            val media = this.media ?: return
            media.seekTo(position)

            // A seek lands over the next moment rather than at once, so the estimate holds the target and the player is left alone until it gets there.
            val now = SystemClock.uptimeMillis()
            this.anchorTo(position, now)
            this.polledAt = now
        }
    }

    fun position(): Int {
        synchronized(this.lock) {
            val media = this.media ?: return 0
            val now = SystemClock.uptimeMillis()

            if (now - this.polledAt >= POLL_INTERVAL) {
                this.polledAt = now

                // Only a gap wider than the reporting granularity is worth following: a stall, or an end that arrived before it was due.
                val reported = media.currentPosition
                if (abs(reported - this.estimate(now)) > DRIFT_LIMIT) {
                    this.anchorTo(reported, now)
                }
            }

            return this.estimate(now)
        }
    }

    override fun close() {
        this.swap(null)
    }


    private fun estimate(now: Long): Int {
        if (!this.playing) {
            return this.anchor
        }

        val elapsed = this.anchor + (now - this.anchoredAt)
        if (this.duration <= 0) {
            return elapsed.toInt()
        }
        return elapsed.coerceAtMost(this.duration.toLong()).toInt()
    }

    private fun anchorTo(position: Int, now: Long) {
        this.anchor = position
        this.anchoredAt = now
    }

    private fun swap(media: MediaPlayer?) {
        val retired = synchronized(this.lock) {
            val retired = this.media

            this.media = media
            this.duration = media?.duration ?: 0
            this.isLoaded = media != null

            this.playing = false
            this.anchorTo(0, SystemClock.uptimeMillis())
            this.polledAt = 0L

            retired
        }

        retired?.release()
    }

    private fun discard(media: MediaPlayer) {
        synchronized(this.lock) {
            if (this.media === media) {
                this.media = null
                this.duration = 0
                this.isLoaded = false
                this.playing = false
            }
        }

        media.release()
    }

    companion object {
        // How often the player itself is asked where it stands, with the monotonic clock carrying everything in between.
        private const val POLL_INTERVAL = 500L

        // How far the estimate may stand from the player before it follows, which is wide enough to cover one report of a deep audio buffer.
        private const val DRIFT_LIMIT = 120
    }
}
