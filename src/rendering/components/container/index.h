#ifndef MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_CONTAINER_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_CONTAINER_INDEX_H_

class SkCanvas;

namespace music_lyric_player::rendering::common {
	struct RenderContext;
} // namespace music_lyric_player::rendering::common

namespace music_lyric_player::rendering::components::container {
	/**
	 * The player's outer frame: fills the background and reserves the horizontal padding the lines lay out within.
	 * A single instance spans the whole viewport and stays fixed while the lines scroll inside it.
	 * Edge fade is planned but not yet ported.
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
	};
} // namespace music_lyric_player::rendering::components::container

#endif // MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_CONTAINER_INDEX_H_
