#ifndef MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_INDEX_H_

#include <memory>
#include <vector>

#include "rendering/components/line/normal/main/syllable/animation/mask.h"
#include "rendering/utils/animation/tween.h"

class SkCanvas;

#include "music_lyric_model.h"

namespace music_lyric_player::rendering::common {
	struct RenderContext;
} // namespace music_lyric_player::rendering::common

namespace music_lyric_player::rendering::components::line::normal::main::syllable {
	class Word;

	/**
	 * A word-timed normal-line body that caches and manually places one paragraph per word.
	 */
	class Element {
	public:
		/**
		 * Builds timed word components and preserves explicit spaces between them.
		 */
		explicit Element(const music_lyric_model::parsed::Line& info);

		/**
		 * Destroys cached word components where their concrete type is complete.
		 */
		~Element();

		/**
		 * Shapes words, wraps them into rows, and aligns every row within the available width.
		 */
		void layout(float width, const common::RenderContext& context);

		/**
		 * Paints every cached word with karaoke reveal and float animation; `played` selects the dimmer played word opacity.
		 */
		void paint(SkCanvas* canvas, float x, float y, double now, bool active, bool played, const common::RenderContext& context) const;

		/**
		 * Returns the total laid-out row height in logical pixels.
		 */
		float height() const;

	private:
		std::vector<std::unique_ptr<Word>> words;
		mutable animation::Mask            mask;
		float                              width          = 0.0f;
		float                              measuredHeight = 0.0f;

		// The line-wide word opacity, eased across state flips: it snaps to the active brightness on activation and eases over 0.8s to the normal or played brightness otherwise, mirroring the web `.word` `transition: opacity 0.8s ease`.
		mutable ::music_lyric_player::rendering::animation::Tween<float> wordOpacity;
		mutable bool                                                    wordFadeReady   = false;
		mutable bool                                                    wordWasActive   = false;
		mutable float                                                   wordOpacityGoal = 0.0f;
	};
} // namespace music_lyric_player::rendering::components::line::normal::main::syllable

#endif // MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_NORMAL_MAIN_SYLLABLE_INDEX_H_
