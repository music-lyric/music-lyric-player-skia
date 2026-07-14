#ifndef MUSIC_LYRIC_PLAYER_ABI_H_
#define MUSIC_LYRIC_PLAYER_ABI_H_

#include <stddef.h>
#include <stdint.h>

#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

// Playback player — a headless timing engine.

/**
 * Opaque handle to the headless timing engine.
 */
typedef struct music_lyric_player_handle music_lyric_player_handle;

/**
 * Receives a playback time in milliseconds; used by the play and pause hooks.
 */
typedef void (*music_lyric_player_time_callback)(double time_ms, void* user);

/**
 * Receives the active-line set: `count` indices in `indices`, the primary active index (or -1) and
 * whether the change came from a seek. The pointer is valid only for the duration of the call.
 */
typedef void (*music_lyric_player_lines_callback)(const int* indices, size_t count, int active, int is_seek, void* user);

/**
 * Creates a headless timing engine, or null on failure.
 */
MUSIC_LYRIC_PLAYER_API music_lyric_player_handle* music_lyric_player_create(void);

/**
 * Destroys a player returned by `music_lyric_player_create`.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_destroy(music_lyric_player_handle* player);

/**
 * Updates the current lyric from serialized bytes in the model's protobuf wire format.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_update_lyric(music_lyric_player_handle* player, const uint8_t* data, size_t size);

/**
 * Starts or resumes playback at the current position.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_play(music_lyric_player_handle* player);

/**
 * Seeks to `seek_ms` and starts playback.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_play_from(music_lyric_player_handle* player, double seek_ms);

/**
 * Pauses playback.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_pause(music_lyric_player_handle* player);

/**
 * Advances active-line tracking one step; call once per frame.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_tick(music_lyric_player_handle* player);

/**
 * Sets the user's temporary offset in milliseconds and resyncs.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_update_temp_offset(music_lyric_player_handle* player, double value);

/**
 * Whether playback is currently running; non-zero means playing.
 */
MUSIC_LYRIC_PLAYER_API int music_lyric_player_current_playing(const music_lyric_player_handle* player);

/**
 * The current playback time in milliseconds.
 */
MUSIC_LYRIC_PLAYER_API double music_lyric_player_current_time(const music_lyric_player_handle* player);

/**
 * The combined effective offset in milliseconds (config global + meta + temp).
 */
MUSIC_LYRIC_PLAYER_API double music_lyric_player_current_offset(const music_lyric_player_handle* player);

/**
 * The primary active line index, or -1 when none.
 */
MUSIC_LYRIC_PLAYER_API int music_lyric_player_current_active(const music_lyric_player_handle* player);

/**
 * Copies up to `capacity` active-line indices into `out` and returns the total count.
 * Pass `out = NULL` and `capacity = 0` to query the count only.
 */
MUSIC_LYRIC_PLAYER_API size_t music_lyric_player_current_index(const music_lyric_player_handle* player, int* out, size_t capacity);

/**
 * Converts a content time to a playback time by removing the active offset.
 */
MUSIC_LYRIC_PLAYER_API double music_lyric_player_convert_content_time(const music_lyric_player_handle* player, double content_time);

/**
 * Registers a listener fired when playback starts or resumes.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_on_play(music_lyric_player_handle* player, music_lyric_player_time_callback callback, void* user);

/**
 * Registers a listener fired when playback pauses.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_on_pause(music_lyric_player_handle* player, music_lyric_player_time_callback callback, void* user);

/**
 * Registers a listener fired when the active-line set changes.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_on_lines_update(music_lyric_player_handle* player, music_lyric_player_lines_callback callback, void* user);

// Renderer — paints a player's lyric into a native window.

/**
 * Opaque handle to a renderer bound to a player and a native window.
 */
typedef struct music_lyric_player_renderer_handle music_lyric_player_renderer_handle;

/**
 * Creates a renderer that paints `player` into the native window `window`, or null on failure.
 * The renderer borrows `player`, which must outlive it.
 */
MUSIC_LYRIC_PLAYER_API music_lyric_player_renderer_handle* music_lyric_player_renderer_create(music_lyric_player_handle* player, void* window);

/**
 * Destroys a renderer returned by `music_lyric_player_renderer_create`.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_renderer_destroy(music_lyric_player_renderer_handle* renderer);

/**
 * Paints one frame into the window.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_renderer_render(music_lyric_player_renderer_handle* renderer);

/**
 * Rebuilds the surface after the host window resized.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_renderer_resize(music_lyric_player_renderer_handle* renderer);

/**
 * Merges a JSON config patch into the renderer's config; only the fields present in `json` take effect.
 * The JSON object mirrors the renderer config tree: camelCase keys, enum values as their names.
 */
MUSIC_LYRIC_PLAYER_API void music_lyric_player_renderer_update_config_json(music_lyric_player_renderer_handle* renderer, const char* json);

#ifdef __cplusplus
}
#endif

#endif // MUSIC_LYRIC_PLAYER_ABI_H_
