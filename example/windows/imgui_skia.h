#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_IMGUI_SKIA_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_IMGUI_SKIA_H_

class SkCanvas;
struct ImDrawData;

namespace example {
	/**
	 * Announces the renderer capabilities to the active ImGui context; call once after creating it.
	 */
	void initImGuiSkia();

	/**
	 * Releases every atlas texture this backend still holds for the active ImGui context.
	 */
	void shutdownImGuiSkia();

	/**
	 * Draws one ImGui frame onto `canvas`, creating or refreshing atlas textures as ImGui requests them.
	 */
	void renderImGuiSkia(SkCanvas* canvas, ImDrawData* drawData);
} // namespace example

#endif // MUSIC_LYRIC_PLAYER_EXAMPLE_IMGUI_SKIA_H_
