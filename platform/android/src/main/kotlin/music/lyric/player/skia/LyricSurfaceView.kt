package music.lyric.player.skia

import android.content.Context
import android.util.AttributeSet
import android.view.SurfaceHolder
import android.view.SurfaceView

/**
 * A ready made view drawing a lyric, which is a [LyricPlayer] and a [LyricRenderer] wired to a surface and nothing more.
 *
 * Both are exposed as they stand, so an app drives playback through `view.renderer.post { view.player.play() }` and reads where playback stands off `view.player` from any thread.
 *
 * The view holds a render thread and a native player, neither of which a garbage collector can reach, so [close] it when the screen holding it goes away.
 */
class LyricSurfaceView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0,
) : SurfaceView(context, attrs, defStyleAttr), SurfaceHolder.Callback, AutoCloseable {
    /**
     * The timing engine this view draws from, reporting its events on the main thread.
     */
    val player: LyricPlayer = LyricPlayer(MainThreadDispatch)

    /**
     * The render thread this view draws on, and the way onto it for anything touching [player].
     */
    val renderer: LyricRenderer = LyricRenderer(this.player)

    init {
        this.holder.addCallback(this)
    }

    /**
     * Hands the fresh surface to the renderer, which is what starts the frame loop.
     */
    override fun surfaceCreated(holder: SurfaceHolder) {
        this.renderer.attach(holder.surface, this.resources.displayMetrics.density)
    }

    /**
     * Reports the new size, along with a density that a move to another display may have changed.
     */
    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        this.renderer.setDevicePixelRatio(this.resources.displayMetrics.density)
        this.renderer.resize()
    }

    /**
     * Releases the surface before returning, which is what this callback's contract demands: the surface dies the moment it does.
     */
    override fun surfaceDestroyed(holder: SurfaceHolder) {
        this.renderer.detach()
    }

    /**
     * Shuts the render thread down and releases the player, in the order the two require.
     */
    override fun close() {
        this.renderer.close()
        this.player.close()
    }
}
