package music.lyric.player.skia

/**
 * The shared library everything else here calls into, plus the two entry points that belong to no single player.
 */
object NativeLibrary {
    init {
        System.loadLibrary("music-lyric-player")
    }

    /**
     * The version of the native library, which is the version of the package as a whole.
     */
    @JvmStatic
    val version: String
        get() = nativeVersion()

    /**
     * Adds a font file to the process wide set every renderer resolves families from.
     * Returns false when the bytes are empty or that exact file is already registered, in which case nothing is rebuilt.
     *
     * Registration is process wide rather than per player, and every renderer alive takes the new set at once.
     * Call it on the render thread, as with everything else reaching native.
     */
    @JvmStatic
    fun registerFont(data: ByteArray): Boolean = nativeRegisterFont(data)

    /**
     * Loads the shared library, whichever type happens to be touched first.
     * The work is done by this object's initializer, and the call only exists to make something trigger it.
     */
    @JvmStatic
    internal fun load() {
    }

    @JvmStatic
    private external fun nativeVersion(): String

    @JvmStatic
    private external fun nativeRegisterFont(bytes: ByteArray): Boolean
}
