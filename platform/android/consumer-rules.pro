# The shared library resolves its entry points from the name of the class declaring them, so a renamed class is a link error at load time.
-keepclasseswithmembernames class music.lyric.player.skia.** {
    native <methods>;
}

# The bridge resolves the playback callbacks through GetMethodID, by name and signature, which R8 has no way of seeing.
-keep class music.lyric.player.skia.LyricPlayer$NativeListener {
    void onPlay(double);
    void onPause(double);
    void onLinesUpdate(int[], int, boolean);
    void onLyricUpdate(java.lang.String, boolean, int);
}
