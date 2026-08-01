package music.lyric.player.skia

import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.view.Choreographer
import android.view.Surface
import java.util.concurrent.CountDownLatch
import music.lyric.player.skia.config.RenderingConfig

/**
 * The render thread and everything bound to it: the GPU surface, the lyric renderer drawing into it, and the frame loop driving both.
 *
 * One renderer owns one thread and outlives the surfaces attached to it, so a view whose surface is recreated calls [attach] and [detach] again rather than building another renderer.
 *
 * Every native call this package offers has to run on that thread, which [post] is the way onto.
 * An app therefore drives its player with `renderer.post { player.play() }` rather than calling the player straight.
 *
 * A renderer borrows the player it draws from, so release the renderer first and the player after.
 */
class LyricRenderer(private val player: LyricPlayer) : AutoCloseable {
    private val thread = HandlerThread(THREAD_NAME).apply { start() }

    private val handler = Handler(this.thread.looper)

    private val frameCallback = Choreographer.FrameCallback { this.onFrame() }

    @Volatile
    private var handle: Long = 0L

    @Volatile
    private var closed: Boolean = false

    // Touched by the render thread alone, which is what lets them stay plain.
    private var running: Boolean = false
    private var choreographer: Choreographer? = null

    /**
     * Where the time of each frame comes from, or null to let the player keep its own steady clock.
     *
     * Set it to read an app's own source, such as `MediaPlayer.getCurrentPosition`, and the value is fed to the player before every tick.
     * It is called on the render thread, so whatever it reads has to be safe to read from there.
     */
    @Volatile
    var timeSource: (() -> Double)? = null

    /**
     * Where a failure on the render thread is reported, or null to let it take the thread down.
     *
     * It covers both a surface that no GPU backend could open and anything native raised during a frame.
     * It is called on the render thread.
     */
    @Volatile
    var onError: ((Throwable) -> Unit)? = null

    /**
     * Whether a surface is currently attached and being drawn into.
     */
    val isAttached: Boolean
        get() = this.handle != 0L

    /**
     * Runs `block` on the render thread, which is the only thread the player and this renderer may be touched from.
     * A failure inside it reaches [onError] like any other.
     */
    fun post(block: () -> Unit) {
        this.handler.post { this.guarded(block) }
    }

    /**
     * Takes `surface` as what to draw into, replacing whichever one was attached, and starts the frame loop.
     *
     * `devicePixelRatio` is the ratio of physical pixels to layout pixels, which is what scales every size the config is written in.
     * On Android that is `resources.displayMetrics.density`.
     *
     * The work happens on the render thread, so the surface is not yet attached when this returns.
     */
    @JvmOverloads
    fun attach(surface: Surface, devicePixelRatio: Float = 1.0f) {
        this.post {
            this.releaseSurface()

            val handle = nativeCreate(this.player.requireHandle(), surface)
            if (handle == 0L) {
                throw IllegalStateException("no gpu backend could open the surface")
            }

            this.handle = handle
            nativeSetDevicePixelRatio(handle, devicePixelRatio)
            this.startLoop()
        }
    }

    /**
     * Stops the frame loop and releases the attached surface, blocking until the render thread is done with it.
     *
     * Blocking is the point: `SurfaceHolder.Callback.surfaceDestroyed` may only return once nothing holds the surface any more.
     * A frame still in flight would otherwise draw into freed memory.
     */
    fun detach() {
        this.runBlocking {
            this.stopLoop()
            this.releaseSurface()
        }
    }

    /**
     * Reports that the attached surface changed size, which the renderer reads back off the surface itself.
     */
    fun resize() {
        this.post {
            val handle = this.handle
            if (handle != 0L) {
                nativeResize(handle)
            }
        }
    }

    /**
     * Sets the ratio of physical pixels to layout pixels, which is what scales every size the config is written in.
     */
    fun setDevicePixelRatio(devicePixelRatio: Float) {
        this.post {
            val handle = this.handle
            if (handle != 0L) {
                nativeSetDevicePixelRatio(handle, devicePixelRatio)
            }
        }
    }

    /**
     * Merges a partial config over the current one.
     * A field left null keeps its current value, which is what makes [RenderingConfig] a patch rather than a whole config.
     */
    fun updateConfig(config: RenderingConfig) {
        this.updateConfig(config.toJson().toString())
    }

    /**
     * Merges a partial config, given as JSON, over the current one.
     * Keys the JSON leaves out keep their current value, so this doubles as a sparse patch.
     */
    fun updateConfig(json: String) {
        this.post {
            val handle = this.handle
            if (handle != 0L) {
                nativeUpdateConfig(handle, json)
            }
        }
    }

    /**
     * Releases the surface and shuts the render thread down.
     * The player this draws from stays alive, and closing it is the app's next step.
     * Closing twice does nothing, which is what lets an app close from more than one path.
     */
    override fun close() {
        if (this.closed) {
            return
        }
        this.closed = true

        this.detach()
        this.thread.quitSafely()
    }

    /**
     * Runs `block` on the render thread and waits for it, taking the direct route when the caller is already on that thread.
     * A dead thread is treated as done, since whatever was to be released went with it.
     */
    private fun runBlocking(block: () -> Unit) {
        if (Looper.myLooper() === this.thread.looper) {
            this.guarded(block)
            return
        }

        val done = CountDownLatch(1)
        val posted = this.handler.post {
            try {
                this.guarded(block)
            } finally {
                done.countDown()
            }
        }
        if (!posted) {
            return
        }
        done.await()
    }

    /**
     * Runs `block`, handing whatever it raises to [onError] and letting it through when nothing is listening.
     */
    private fun guarded(block: () -> Unit) {
        try {
            block()
        } catch (error: Throwable) {
            val listener = this.onError ?: throw error
            listener(error)
        }
    }

    /**
     * Starts asking for frames, which the render thread's own looper is what makes possible.
     */
    private fun startLoop() {
        if (this.running) {
            return
        }

        this.running = true
        val choreographer = Choreographer.getInstance()
        this.choreographer = choreographer
        choreographer.postFrameCallback(this.frameCallback)
    }

    /**
     * Stops asking for frames.
     */
    private fun stopLoop() {
        if (!this.running) {
            return
        }

        this.running = false
        this.choreographer?.removeFrameCallback(this.frameCallback)
    }

    /**
     * Draws one frame: the app's time if it supplies one, then a tick, then the paint.
     * The next frame is asked for first, so a failure in this one does not end the loop.
     */
    private fun onFrame() {
        if (!this.running) {
            return
        }
        this.choreographer?.postFrameCallback(this.frameCallback)

        val handle = this.handle
        if (handle == 0L) {
            return
        }

        this.guarded {
            this.timeSource?.let { this.player.setExternalTime(it()) }
            this.player.tick()
            nativeRenderFrame(handle)
        }
    }

    /**
     * Releases the native renderer and the surface behind it, taking an unattached renderer as nothing to do.
     */
    private fun releaseSurface() {
        val handle = this.handle
        if (handle == 0L) {
            return
        }

        this.handle = 0L
        nativeDispose(handle)
    }

    // A JNI symbol is named after the class declaring it, so @JvmStatic is what keeps these on the class itself rather than on Companion.
    companion object {
        init {
            NativeLibrary.load()
        }

        private const val THREAD_NAME = "music-lyric-render"

        @JvmStatic
        private external fun nativeCreate(player: Long, target: Surface): Long

        @JvmStatic
        private external fun nativeSetDevicePixelRatio(handle: Long, devicePixelRatio: Float)

        @JvmStatic
        private external fun nativeResize(handle: Long)

        @JvmStatic
        private external fun nativeRenderFrame(handle: Long)

        @JvmStatic
        private external fun nativeUpdateConfig(handle: Long, json: String)

        @JvmStatic
        private external fun nativeDispose(handle: Long)
    }
}
