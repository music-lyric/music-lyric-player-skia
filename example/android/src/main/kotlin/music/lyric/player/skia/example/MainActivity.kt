package music.lyric.player.skia.example

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.SurfaceHolder
import android.view.View
import android.view.WindowInsets
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.CheckBox
import android.widget.SeekBar
import android.widget.Spinner
import android.widget.TextView

import androidx.activity.ComponentActivity
import androidx.activity.result.contract.ActivityResultContracts

import music.lyric.player.skia.LyricPlayer
import music.lyric.player.skia.LyricSurfaceView
import music.lyric.player.skia.NativeLibrary
import music.lyric.player.skia.config.*

class MainActivity : ComponentActivity() {
    private val audio = AudioSource()

    private val ticker = Handler(Looper.getMainLooper())

    private lateinit var state: AppState
    private lateinit var view: LyricSurfaceView
    private lateinit var toggle: Button
    private lateinit var seek: SeekBar
    private lateinit var time: TextView
    private lateinit var notice: TextView
    private lateinit var status: TextView

    private var lines = 0
    private var seeking = false

    private val version: String by lazy { NativeLibrary.version }

    private val audioPicker = registerForActivityResult(PersistableDocument()) { uri ->
        uri?.let { this.pickAudio(it) }
    }

    private val lyricPicker = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri?.let { this.loadLyric(it) }
    }

    private val fontPicker = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri?.let { this.loadFont(it) }
    }

    private val refreshLoop = object : Runnable {
        override fun run() {
            this@MainActivity.refreshPanel()
            this@MainActivity.ticker.postDelayed(this, REFRESH_INTERVAL)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        this.setContentView(R.layout.activity_main)

        this.state = AppState(this.filesDir)
        this.state.load()

        this.view = this.findViewById(R.id.lyric)
        this.toggle = this.findViewById(R.id.toggle)
        this.seek = this.findViewById(R.id.seek)
        this.time = this.findViewById(R.id.time)
        this.notice = this.findViewById(R.id.notice)
        this.status = this.findViewById(R.id.status)

        this.bindPlayer()
        this.bindPanel()
        this.bindInsets()
        this.restoreSession()
    }

    override fun onStart() {
        super.onStart()
        this.ticker.post(this.refreshLoop)
    }

    override fun onStop() {
        super.onStop()
        this.ticker.removeCallbacks(this.refreshLoop)
    }

    override fun onDestroy() {
        super.onDestroy()

        this.view.close()
        this.audio.close()
    }

    private fun bindPlayer() {
        this.view.player.setListener(object : LyricPlayer.Listener {
            override fun onLyricUpdate(version: String, valid: Boolean, lines: Int) {
                if (!valid) {
                    this@MainActivity.report("the lyric did not parse")
                    return
                }

                this@MainActivity.lines = lines
                this@MainActivity.notice.text = this@MainActivity.getString(R.string.notice_lyric, lines)
            }
        })

        this.view.renderer.onError = { error ->
            this.runOnUiThread { this.report(error.message ?: error.toString()) }
        }

        // The config belongs to the native renderer, which every new surface builds afresh, so the panel puts its settings back each time one arrives.
        // The view registered its own callback first, so by the time this one runs the surface is already on its way to being attached.
        this.view.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                this@MainActivity.applyConfig()
            }

            override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
            }

            override fun surfaceDestroyed(holder: SurfaceHolder) {
            }
        })

        this.audio.onCompleted = {
            this.view.renderer.post { this.view.player.pause() }
        }
    }

    private fun bindPanel() {
        val panel = this.findViewById<View>(R.id.panel)

        // The panel covers a good part of a phone screen, so the lyric itself is what puts it away.
        this.view.setOnClickListener {
            panel.visibility = if (panel.visibility == View.VISIBLE) View.GONE else View.VISIBLE
        }

        this.findViewById<Button>(R.id.pick_audio).setOnClickListener { this.audioPicker.launch(arrayOf("audio/*")) }

        // A lyric is a protobuf message and a font is a binary the picker has no type for, so both are opened as anything.
        this.findViewById<Button>(R.id.pick_lyric).setOnClickListener { this.lyricPicker.launch(arrayOf("*/*")) }
        this.findViewById<Button>(R.id.pick_font).setOnClickListener { this.fontPicker.launch(arrayOf("*/*")) }

        this.toggle.setOnClickListener { this.togglePlayback() }

        this.seek.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(bar: SeekBar, progress: Int, fromUser: Boolean) {
            }

            override fun onStartTrackingTouch(bar: SeekBar) {
                this@MainActivity.seeking = true
            }

            override fun onStopTrackingTouch(bar: SeekBar) {
                this@MainActivity.seeking = false
                this@MainActivity.seekTo(bar.progress)
            }
        })

        this.bindSlider(R.id.config_size, R.id.config_size_value, SIZE_MIN, SIZE_MAX, this.state.settings.fontSize) { size ->
            this.state.settings = this.state.settings.copy(fontSize = size)
        }
        this.bindSlider(R.id.config_gap, R.id.config_gap_value, GAP_MIN, GAP_MAX, this.state.settings.lineGap) { gap ->
            this.state.settings = this.state.settings.copy(lineGap = gap)
        }
        this.bindAlign()
        this.bindSyllable()
    }

    private fun bindSlider(sliderId: Int, valueId: Int, minimum: Int, maximum: Int, current: Int, apply: (Int) -> Unit) {
        val label = this.findViewById<TextView>(valueId)
        val slider = this.findViewById<SeekBar>(sliderId)

        val update = { progress: Int ->
            val amount = minimum + progress
            label.text = this.getString(R.string.value_pixels, amount)
            apply(amount)
        }

        slider.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(bar: SeekBar, progress: Int, fromUser: Boolean) {
                update(progress)
                this@MainActivity.applyConfig()
            }

            override fun onStartTrackingTouch(bar: SeekBar) {
            }

            override fun onStopTrackingTouch(bar: SeekBar) {
                this@MainActivity.state.save()
            }
        })

        // The slider carries no minimum below API 26, so the range is shifted here instead.
        slider.max = maximum - minimum
        slider.progress = (current - minimum).coerceIn(0, slider.max)

        // A progress that lands on what the slider already showed leaves the listener silent, so the label is filled in from here.
        update(slider.progress)
    }

    private fun bindAlign() {
        val spinner = this.findViewById<Spinner>(R.id.config_align)
        val options = LayoutAlign.entries

        spinner.adapter = ArrayAdapter(this, android.R.layout.simple_spinner_item, options.map { it.name }).apply {
            setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        }
        spinner.setSelection(options.indexOf(this.state.settings.align))

        spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>, item: View?, position: Int, id: Long) {
                this@MainActivity.state.settings = this@MainActivity.state.settings.copy(align = options[position])
                this@MainActivity.applyConfig()
                this@MainActivity.state.save()
            }

            override fun onNothingSelected(parent: AdapterView<*>) {
            }
        }
    }

    private fun bindSyllable() {
        val syllable = this.findViewById<CheckBox>(R.id.config_syllable)

        syllable.isChecked = this.state.settings.enableSyllable
        syllable.setOnCheckedChangeListener { _, checked ->
            this.state.settings = this.state.settings.copy(enableSyllable = checked)
            this.applyConfig()
            this.state.save()
        }
    }

    private fun bindInsets() {
        val panel = this.findViewById<View>(R.id.panel)
        val padding = panel.paddingBottom

        panel.setOnApplyWindowInsetsListener { target, insets ->
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                val bars = insets.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
                target.setPadding(padding + bars.left, padding, padding + bars.right, padding + bars.bottom)
            }
            insets
        }
    }

    private fun restoreSession() {
        this.state.readLyric()?.let { bytes ->
            this.view.renderer.post { this.view.player.updateLyric(bytes) }
        }

        this.state.audio?.let { this.openAudio(it) }
    }

    private fun pickAudio(uri: Uri) {
        val previous = this.state.audio
        if (previous != null && previous != uri) {
            this.releaseAccess(previous)
        }
        this.takeAccess(uri)

        this.state.audio = uri
        this.state.save()
        this.openAudio(uri)
    }

    private fun openAudio(uri: Uri) {
        this.audio.load(
            context = this,
            uri = uri,
            onReady = {
                this.seek.max = this.audio.duration
                this.seek.progress = 0
                this.seek.isEnabled = true

                this.view.renderer.timeSource = { this.audio.position().toDouble() }
                this.notice.setText(R.string.notice_audio)
            },
            onFailed = { message ->
                this.report(message)
                this.forgetAudio(uri)
            },
        )
    }

    private fun forgetAudio(uri: Uri) {
        if (this.state.audio != uri) {
            return
        }

        this.releaseAccess(uri)
        this.state.audio = null
        this.state.save()
    }

    private fun takeAccess(uri: Uri) {
        try {
            this.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        } catch (error: SecurityException) {
            // A provider handing out nothing lasting still plays for this launch, which is not worth a notice of its own.
        }
    }

    private fun releaseAccess(uri: Uri) {
        try {
            this.contentResolver.releasePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        } catch (error: SecurityException) {
            // Nothing was held, which is where this leaves it anyway.
        }
    }

    private fun loadLyric(uri: Uri) {
        this.view.renderer.post {
            val bytes = this.readBytes(uri)

            this.view.player.updateLyric(bytes)
            this.state.writeLyric(bytes)
        }
    }

    private fun loadFont(uri: Uri) {
        this.view.renderer.post {
            val added = NativeLibrary.registerFont(this.readBytes(uri))
            this.runOnUiThread { this.notice.setText(if (added) R.string.notice_font else R.string.notice_font_known) }
        }
    }

    private fun readBytes(uri: Uri): ByteArray {
        val stream = this.contentResolver.openInputStream(uri) ?: throw IllegalStateException("nothing could be read from $uri")
        return stream.use { it.readBytes() }
    }

    private fun togglePlayback() {
        val player = this.view.player

        if (!this.audio.isLoaded) {
            val playing = player.currentPlaying
            this.view.renderer.post { if (playing) player.pause() else player.play() }
            return
        }

        if (this.audio.isPlaying) {
            this.audio.pause()
            this.view.renderer.post { player.pause() }
            return
        }

        // Starting again once the audio has run out rewinds it, so the position is read after the start rather than before it.
        this.audio.play()
        val position = this.audio.position().toDouble()
        this.view.renderer.post { player.play(position) }
    }

    private fun seekTo(position: Int) {
        if (!this.audio.isLoaded) {
            return
        }

        this.audio.seekTo(position)

        val player = this.view.player
        val playing = this.audio.isPlaying
        val now = position.toDouble()

        this.view.renderer.post {
            player.setExternalTime(now)
            player.play(now)
            if (!playing) {
                player.pause()
            }
        }
    }

    private fun applyConfig() {
        val settings = this.state.settings

        this.view.renderer.updateConfig(
            RenderingConfig(
                container = ContainerConfig(backgroundColor = "#ffffff", paddingX = "24px"),
                layout = LayoutConfig(align = settings.align, gap = "${settings.lineGap}px"),
                scroll = ScrollConfig(
                    anchor = 0.5,
                    animation = ScrollAnimationConfig(
                        mode = ScrollMode.Stagger,
                        stagger = ScrollStaggerConfig(duration = 500.0, easing = "ease", range = 4.0, step = 40.0),
                    ),
                ),
                line = LineConfig(
                    normal = LineNormalConfig(
                        base = LineNormalBase(
                            font = CommonFontConfig(size = "${settings.fontSize}px"),
                            style = CommonStateStyleConfig(
                                normal = CommonStyleConfig(color = "#000000", opacity = 0.6),
                                active = CommonStyleConfig(color = "#000000", opacity = 1.0),
                                played = CommonStyleConfig(color = "#000000", opacity = 0.4),
                            ),
                        ),
                        main = LineNormalMainConfig(
                            syllable = LineNormalMainSyllableConfig(
                                word = LineNormalMainSyllableWordConfig(
                                    animation = LineNormalMainSyllableWordAnimationConfig(
                                        floating = LineNormalMainSyllableFloatConfig(enabled = true),
                                        mask = LineNormalMainSyllableMaskConfig(enabled = settings.enableSyllable),
                                    ),
                                ),
                            ),
                        ),
                    ),
                    interlude = LineInterludeConfig(
                        style = LineInterludeStateConfig(
                            normal = LineInterludeStyleConfig(color = "#000000", opacity = 0.2),
                            active = LineInterludeStyleConfig(color = "#000000", opacity = 0.8),
                        ),
                    ),
                ),
            ),
        )
    }

    private fun refreshPanel() {
        val player = this.view.player
        val position = this.audio.position()

        // Writing while the slider is under a finger would fight the drag that is moving it.
        if (!this.seeking) {
            this.seek.progress = position
        }

        this.time.text = this.getString(R.string.time, formatTime(position), formatTime(this.audio.duration))
        this.toggle.setText(if (player.currentPlaying) R.string.pause else R.string.play)
        this.status.text = this.getString(R.string.status, this.version, this.lines, player.currentTime.toInt(), player.currentActive)
    }

    private fun report(message: String) {
        this.notice.text = this.getString(R.string.notice_failed, message)
    }

    companion object {
        private const val REFRESH_INTERVAL = 200L

        private const val SIZE_MIN = 12
        private const val SIZE_MAX = 96

        private const val GAP_MIN = 0
        private const val GAP_MAX = 64

        /**
         * Renders `milliseconds` as `m:ss`, which is what the transport shows on either side of the slider.
         */
        private fun formatTime(milliseconds: Int): String {
            val total = (milliseconds / 1000).coerceAtLeast(0)
            return "${total / 60}:${(total % 60).toString().padStart(2, '0')}"
        }
    }
}

// Only the audio is opened again on a later launch, so it is the one pick that asks for a grant outliving the one it was made in.
private class PersistableDocument : ActivityResultContracts.OpenDocument() {
    override fun createIntent(context: Context, input: Array<String>): Intent {
        return super.createIntent(context, input)
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
    }
}
