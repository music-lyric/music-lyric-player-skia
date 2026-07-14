#ifndef MUSIC_LYRIC_PLAYER_EXPORT_H_
#define MUSIC_LYRIC_PLAYER_EXPORT_H_

// Decorates the public C ABI symbols with the shared library's export/import marker, or nothing for a static build.
#ifdef MUSIC_LYRIC_PLAYER_STATIC_DEFINE
	#define MUSIC_LYRIC_PLAYER_API
#else
	#if defined(_WIN32)
		#ifdef MUSIC_LYRIC_PLAYER_BUILDING
			#define MUSIC_LYRIC_PLAYER_API __declspec(dllexport)
		#else
			#define MUSIC_LYRIC_PLAYER_API __declspec(dllimport)
		#endif
	#else
		#ifdef MUSIC_LYRIC_PLAYER_BUILDING
			#define MUSIC_LYRIC_PLAYER_API __attribute__((visibility("default")))
		#else
			#define MUSIC_LYRIC_PLAYER_API
		#endif
	#endif
#endif

#endif // MUSIC_LYRIC_PLAYER_EXPORT_H_
