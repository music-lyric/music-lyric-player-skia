package music.lyric.player.skia

import android.os.Handler
import android.os.Looper

/**
 * Hands a callback to the main looper, which is where an app's own UI work has to happen.
 *
 * This is the Android half of the seam [LyricPlayer] leaves open, kept in a file of its own so the player itself stays free of platform types.
 * Pass it as the player's dispatch to have every listener callback arrive on the main thread.
 */
object MainThreadDispatch : (Runnable) -> Unit {
    private val handler = Handler(Looper.getMainLooper())

    /**
     * Runs `runnable` on the main thread, taking the direct route when the caller is already on it.
     */
    override fun invoke(runnable: Runnable) {
        if (Looper.myLooper() === this.handler.looper) {
            runnable.run()
            return
        }
        this.handler.post(runnable)
    }
}
