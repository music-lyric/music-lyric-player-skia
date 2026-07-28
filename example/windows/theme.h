#ifndef MUSIC_LYRIC_PLAYER_EXAMPLE_THEME_H_
#define MUSIC_LYRIC_PLAYER_EXAMPLE_THEME_H_

#include "imgui.h"

namespace example::theme {
	/**
	 * The playground's palette, one constant per custom property its stylesheet declares.
	 */
	namespace color {
		// Brand: a soft, light red.
		inline constexpr ImU32 primary       = IM_COL32(0xDC, 0x64, 0x64, 0xFF);
		inline constexpr ImU32 primaryStrong = IM_COL32(0xC2, 0x51, 0x51, 0xFF);
		inline constexpr ImU32 primarySoft   = IM_COL32(0xFB, 0xE6, 0xE6, 0xFF);
		inline constexpr ImU32 primaryFaint  = IM_COL32(0xFD, 0xF3, 0xF3, 0xFF);
		inline constexpr ImU32 primaryBorder = IM_COL32(0xF0, 0xC8, 0xC8, 0xFF);

		// Neutrals.
		inline constexpr ImU32 background       = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);
		inline constexpr ImU32 backgroundAlt    = IM_COL32(0xF8, 0xF8, 0xF8, 0xFF);
		inline constexpr ImU32 backgroundSubtle = IM_COL32(0xFA, 0xFA, 0xFA, 0xFF);
		inline constexpr ImU32 surfaceHover     = IM_COL32(0xF3, 0xF3, 0xF3, 0xFF);
		inline constexpr ImU32 border           = IM_COL32(0xEC, 0xEC, 0xEC, 0xFF);
		inline constexpr ImU32 borderSoft       = IM_COL32(0xF2, 0xF2, 0xF2, 0xFF);
		inline constexpr ImU32 borderStrong     = IM_COL32(0xD4, 0xD4, 0xD4, 0xFF);

		// Text.
		inline constexpr ImU32 text          = IM_COL32(0x1F, 0x1F, 0x1F, 0xFF);
		inline constexpr ImU32 textSecondary = IM_COL32(0x5E, 0x5E, 0x5E, 0xFF);
		inline constexpr ImU32 textMuted     = IM_COL32(0x9A, 0x9A, 0x9A, 0xFF);
		inline constexpr ImU32 textInverse   = IM_COL32(0xFF, 0xFF, 0xFF, 0xFF);

		// The tint `--shadow-sm` spreads under a lifted surface, and the darker one a thumb carries.
		inline constexpr ImU32 shadow      = IM_COL32(0x0F, 0x0F, 0x0F, 0x0A);
		inline constexpr ImU32 shadowThumb = IM_COL32(0x00, 0x00, 0x00, 0x26);
	} // namespace color

	/**
	 * How long the stylesheet's two transition speeds take, in seconds.
	 */
	namespace motion {
		inline constexpr float fast = 0.12f;
		inline constexpr float base = 0.20f;
	} // namespace motion

	/**
	 * Every length the panel lays out with, already multiplied by the display scale.
	 * The defaults are the logical pixels the stylesheet declares; `apply` scales them in place.
	 */
	struct Metrics {
		float scale = 1.0f;

		// Frame.
		float sidebarWidth   = 420.0f;
		float controlsHeight = 64.0f;
		float framePadding   = 16.0f;

		// Radii.
		float radiusXs   = 4.0f;
		float radiusSm   = 6.0f;
		float radiusMd   = 8.0f;
		float radiusLg   = 12.0f;
		float radiusFull = 999.0f;

		// Type scale, two steps above the stylesheet's: a desktop panel is read further away than a browser sidebar and has no page zoom to fall back on.
		float fontTitle = 19.0f;
		float fontBody  = 17.0f;
		float fontSmall = 15.0f;
		float fontTiny  = 13.0f;

		// Controls.
		float controlHeight = 30.0f;
		float controlWidth  = 168.0f;
		float rowHeight     = 36.0f;
		float tabHeight     = 30.0f;
		float iconButton    = 34.0f;
		float playButton    = 38.0f;
		float iconGlyph     = 18.0f;
		float toggleWidth   = 36.0f;
		float toggleHeight  = 20.0f;
		float toggleThumb   = 16.0f;
		float sliderHeight  = 24.0f;
		float sliderTrack   = 3.0f;
		float sliderThumb   = 12.0f;
		float volumeWidth   = 100.0f;
		float pickerIcon    = 36.0f;

		// Containers.
		float cardPadding    = 12.0f;
		float sectionPadding = 11.0f;
		float sectionIndent  = 22.0f;
		float chevron        = 12.0f;
	};

	/**
	 * Loads the panel's faces, writes the playground's style into the current ImGui context and scales every length for a display whose device pixel ratio is `scale`.
	 * The panel lays out in physical pixels, so the scale is folded into the metrics rather than left to ImGui.
	 */
	void apply(float scale);

	/**
	 * Returns the lengths the panel lays out with, already scaled for the display.
	 */
	const Metrics& metrics();

	/**
	 * Returns the semibold face titles are set in, or the regular one when the system has no semibold family.
	 */
	ImFont* semibold();
} // namespace example::theme

#endif // MUSIC_LYRIC_PLAYER_EXAMPLE_THEME_H_
