#include <jni.h>

/**
 * Returns the version the library was built at, which is also what proves the native library loaded.
 */
extern "C" JNIEXPORT jstring JNICALL Java_io_github_musiclyric_player_skia_NativeLibrary_nativeVersion(JNIEnv* env, jclass) {
	return env->NewStringUTF(MUSIC_LYRIC_PLAYER_VERSION);
}
