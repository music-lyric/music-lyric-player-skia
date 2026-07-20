#ifndef MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_CONTAINER_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_CONTAINER_INDEX_H_

class SkCanvas;

namespace music_lyric_player::rendering::common {
	struct RenderContext;
} // namespace music_lyric_player::rendering::common

namespace music_lyric_player::rendering::components::container {
	/**
	 * The player's outer frame: fills the background, reserves the horizontal padding the lines lay out within, and fades the top and bottom edges.
	 * A single instance spans the whole viewport and stays fixed while the lines scroll inside it.
	 */
	class Element {
	public:
		/**
		 * Fills the entire canvas with the configured background colour; call before painting any line.
		 */
		void paintBackground(SkCanvas* canvas, const common::RenderContext& context) const;

		/**
		 * The horizontal padding on each side in logical pixels, resolved against `logicalWidth` and clamped so the content width stays positive.
		 */
		float paddingX(float logicalWidth, const common::RenderContext& context) const;

		/**
		 * Whether an edge fade should be drawn: it is enabled and at least one edge has a positive range at this height.
		 * The caller wraps the lines in a layer only when this holds, so a disabled fade adds no layer cost.
		 */
		bool fadeActive(float logicalHeight, const common::RenderContext& context) const;

		/**
		 * Erases the top and bottom edge alpha of the current layer with a vertical gradient, mirroring the web container mask.
		 * Call after painting the lines into a layer and before restoring it; a no-op when `fadeActive` is false.
		 */
		void paintEdgeFade(SkCanvas* canvas, float logicalWidth, float logicalHeight, const common::RenderContext& context) const;
	};
} // namespace music_lyric_player::rendering::components::container

#endif // MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_CONTAINER_INDEX_H_
