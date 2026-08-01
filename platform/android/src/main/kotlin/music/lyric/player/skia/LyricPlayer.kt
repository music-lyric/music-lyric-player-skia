package music.lyric.player.skia

/**
 * One timing engine: the lyric it holds, where playback stands in it, and the events it reports along the way.
 *
 * Every method reaching native has to run on the one render thread, which [LyricRenderer.post] is the way onto.
 * The state getters are the exception: they read a snapshot the render thread publishes once a frame, so any thread may read them.
 *
 * A player outlives the renderers drawing from it, so release those first and this one last.
 */
class LyricPlayer(
    /**
     * Where listener callbacks are handed to before reaching the app.
     * Native reports events on the render thread, and this is what carries them somewhere the app can touch its UI from.
     * The default runs them where they arrive, which is the render thread; on Android pass [MainThreadDispatch].
     */
    private val dispatch: (Runnable) -> Unit = Runnable::run,
) : AutoCloseable {
    /**
     * What a player reports as playback moves through a lyric.
     * Every method arrives through the player's dispatch, so with [MainThreadDispatch] they all land on the main thread.
     */
    interface Listener {
        /**
         * Playback started or resumed at `time` ms.
         */
        fun onPlay(time: Double) {}

        /**
         * Playback paused at `time` ms.
         */
        fun onPause(time: Double) {}

        /**
         * The set of active lines changed to `indices`, of which `active` is the leading one.
         * `isSeek` marks the change as the result of a jump rather than of time passing.
         */
        fun onLinesUpdate(indices: IntArray, active: Int, isSeek: Boolean) {}

        /**
         * A lyric was loaded, carrying `version`, whether it parsed as `valid`, and how many `lines` it holds.
         */
        fun onLyricUpdate(version: String, valid: Boolean, lines: Int) {}
    }

    /**
     * What native calls back into, kept apart from [Listener] so the app's own type is free of the shape JNI resolves by name.
     * Its four methods are looked up through GetMethodID, so consumer-rules.pro keeps them from being renamed.
     */
    private inner class NativeListener {
        fun onPlay(time: Double) {
            this@LyricPlayer.report { it.onPlay(time) }
        }

        fun onPause(time: Double) {
            this@LyricPlayer.report { it.onPause(time) }
        }

        fun onLinesUpdate(indices: IntArray, active: Int, isSeek: Boolean) {
            this@LyricPlayer.report { it.onLinesUpdate(indices, active, isSeek) }
        }

        fun onLyricUpdate(version: String, valid: Boolean, lines: Int) {
            this@LyricPlayer.report { it.onLyricUpdate(version, valid, lines) }
        }
    }

    private var handle: Long = nativeCreate()

    private val nativeListener = NativeListener()

    // The buffers a snapshot is read into are held rather than allocated per frame, and the index one grows to whatever the player reports.
    private val stateBuffer = DoubleArray(SNAPSHOT_SLOTS)
    private var indexBuffer = IntArray(INITIAL_INDEX_CAPACITY)

    @Volatile
    private var listener: Listener? = null

    /**
     * Whether playback is running, as of the last frame.
     */
    @Volatile
    var currentPlaying: Boolean = false
        private set

    /**
     * Playback time in ms, as of the last frame.
     */
    @Volatile
    var currentTime: Double = 0.0
        private set

    /**
     * The combined offset in ms, which is the config's global one plus the lyric's own plus the temporary one, as of the last frame.
     */
    @Volatile
    var currentOffset: Double = 0.0
        private set

    /**
     * The leading active line, or -1 while none is, as of the last frame.
     */
    @Volatile
    var currentActive: Int = -1
        private set

    /**
     * Every active line index, as of the last frame.
     * The array is a snapshot the render thread replaced wholesale, so reading it needs no lock; do not write into it.
     */
    @Volatile
    var currentIndex: IntArray = EMPTY_INDEX
        private set

    init {
        if (this.handle == 0L) {
            throw IllegalStateException("the native player could not be created")
        }
        nativeSetListener(this.handle, this.nativeListener)
    }

    /**
     * Takes `listener` as what every playback event is reported to, replacing whichever one it stands in for.
     * Passing null is how an app stops being called back without releasing the player.
     */
    fun setListener(listener: Listener?) {
        this.listener = listener
    }

    /**
     * Loads a lyric from the encoded bytes of a `parsed.Info` protobuf message.
     */
    fun updateLyric(data: ByteArray) {
        nativeUpdateLyric(this.require(), data)
    }

    /**
     * Starts or resumes playback from where it left off.
     */
    fun play() {
        nativePlay(this.require())
    }

    /**
     * Seeks to `seek` ms and starts playing from there.
     */
    fun play(seek: Double) {
        nativePlayFrom(this.require(), seek)
    }

    /**
     * Pauses playback.
     */
    fun pause() {
        nativePause(this.require())
    }

    /**
     * Sets the user's temporary offset in ms and resyncs immediately.
     */
    fun updateTempOffset(value: Double) {
        nativeUpdateTempOffset(this.require(), value)
    }

    /**
     * Feeds the player a time in ms from the app's own source, such as a `MediaPlayer`.
     * The first call takes the clock over from steady time for good, after which a fresh time has to arrive before every [tick].
     */
    fun setExternalTime(now: Double) {
        nativeSetExternalTime(this.require(), now)
    }

    /**
     * Converts a content time to a playback time by removing the active offset.
     */
    fun convertContentTime(contentTime: Double): Double = nativeConvertContentTime(this.require(), contentTime)

    /**
     * Advances active line tracking one step and republishes the state the getters read.
     * The renderer calls this once a frame, so an app driving its own loop is the only one that has to.
     */
    fun tick() {
        val handle = this.require()
        nativeTick(handle)
        this.refreshSnapshot(handle)
    }

    /**
     * Releases the native player.
     * Every renderer drawing from it has to be released first, since each borrows what this owns.
     * Releasing twice does nothing, which is what lets an app close from more than one path.
     */
    override fun close() {
        val handle = this.handle
        if (handle == 0L) {
            return
        }

        this.handle = 0L
        nativeSetListener(handle, null)
        nativeDispose(handle)
    }

    /**
     * The handle a renderer binds itself to; not part of the public surface.
     */
    internal fun requireHandle(): Long = this.require()

    /**
     * Returns the live handle, rejecting a player the app already closed.
     */
    private fun require(): Long {
        val handle = this.handle
        if (handle == 0L) {
            throw IllegalStateException("the player is closed")
        }
        return handle
    }

    /**
     * Hands one event to the app's listener through the dispatch, dropping it when nothing is listening.
     */
    private fun report(event: (Listener) -> Unit) {
        val listener = this.listener ?: return
        this.dispatch(Runnable { event(listener) })
    }

    /**
     * Reads one frame's worth of state out of native, growing the index buffer when the player has more lines than it holds.
     * One call rather than a getter each is what makes the published values a single frame's.
     */
    private fun refreshSnapshot(handle: Long) {
        var total = nativeSnapshot(handle, this.stateBuffer, this.indexBuffer)
        if (total > this.indexBuffer.size) {
            this.indexBuffer = IntArray(total)
            total = nativeSnapshot(handle, this.stateBuffer, this.indexBuffer)
        }

        this.currentPlaying = this.stateBuffer[SNAPSHOT_PLAYING] != 0.0
        this.currentTime = this.stateBuffer[SNAPSHOT_TIME]
        this.currentOffset = this.stateBuffer[SNAPSHOT_OFFSET]
        this.currentActive = this.stateBuffer[SNAPSHOT_ACTIVE].toInt()

        // The active set changes far less often than once a frame, so republishing only on a change keeps the steady state free of allocation.
        val count = minOf(total, this.indexBuffer.size)
        if (!this.currentIndex.matches(this.indexBuffer, count)) {
            this.currentIndex = this.indexBuffer.copyOf(count)
        }
    }

    /**
     * Whether this array is exactly the first `count` entries of `buffer`.
     */
    private fun IntArray.matches(buffer: IntArray, count: Int): Boolean {
        if (this.size != count) {
            return false
        }
        for (index in 0 until count) {
            if (this[index] != buffer[index]) {
                return false
            }
        }
        return true
    }

    // A JNI symbol is named after the class declaring it, so @JvmStatic is what keeps these on the class itself rather than on Companion.
    companion object {
        init {
            NativeLibrary.load()
        }

        // The slots nativeSnapshot fills, in the order the bridge writes them.
        private const val SNAPSHOT_PLAYING = 0
        private const val SNAPSHOT_TIME = 1
        private const val SNAPSHOT_OFFSET = 2
        private const val SNAPSHOT_ACTIVE = 3
        private const val SNAPSHOT_SLOTS = 4

        private const val INITIAL_INDEX_CAPACITY = 8

        private val EMPTY_INDEX = IntArray(0)

        @JvmStatic
        private external fun nativeCreate(): Long

        @JvmStatic
        private external fun nativeUpdateLyric(handle: Long, bytes: ByteArray)

        @JvmStatic
        private external fun nativePlay(handle: Long)

        @JvmStatic
        private external fun nativePlayFrom(handle: Long, seek: Double)

        @JvmStatic
        private external fun nativePause(handle: Long)

        @JvmStatic
        private external fun nativeTick(handle: Long)

        @JvmStatic
        private external fun nativeUpdateTempOffset(handle: Long, value: Double)

        @JvmStatic
        private external fun nativeSetExternalTime(handle: Long, now: Double)

        @JvmStatic
        private external fun nativeSnapshot(handle: Long, state: DoubleArray, indices: IntArray): Int

        @JvmStatic
        private external fun nativeConvertContentTime(handle: Long, contentTime: Double): Double

        @JvmStatic
        private external fun nativeSetListener(handle: Long, listener: Any?)

        @JvmStatic
        private external fun nativeDispose(handle: Long)
    }
}
