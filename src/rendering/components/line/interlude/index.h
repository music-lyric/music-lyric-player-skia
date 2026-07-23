#ifndef MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_INTERLUDE_INDEX_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_INTERLUDE_INDEX_H_

#include <array>
#include <cstddef>

#include "music_lyric_model.h"
#include "rendering/components/line/base/index.h"
#include "rendering/utils/animation/tween.h"

class SkCanvas;

namespace music_lyric_player::rendering::common {
	struct RenderContext;
} // namespace music_lyric_player::rendering::common

namespace music_lyric_player::rendering::components::line::interlude {
	/**
	 * An instrumental-gap component that paints three dots and fills them in sequence over the line duration.
	 */
	class Element : public base::Element {
	public:
		/**
		 * Builds an interlude indicator and caches its content-time animation range.
		 */
		Element(int index, const music_lyric_model::parsed::Line& info);

		/**
		 * Resolves the configured dot geometry and measured line height.
		 */
		void layout(float width, const common::RenderContext& context) override;

		/**
		 * Paints the dots at their content-time opacity and active-state scale; the played state carries no extra styling here.
		 */
		void paint(SkCanvas* canvas, float x, float y, double now, bool active, bool played, const common::RenderContext& context) const override;

	private:
		/**
		 * Samples the active-state scale transition at the render clock time.
		 */
		float stateScale(double now, bool active) const;

		/**
		 * Samples one dot's content-timed fill or inactive-state fade.
		 */
		float dotOpacity(std::size_t index, double now, double currentTime, bool active, bool deactivating, float normalOpacity, float activeOpacity) const;

		static constexpr std::size_t kDotCount             = 3;
		static constexpr double      kOpacityFadeDuration  = 300.0;
		static constexpr double      kScaleDuration        = 600.0;
		static constexpr double      kScaleActivationDelay = 100.0;
		static constexpr float       kInactiveScale        = 0.8f;
		static constexpr float       kActiveScale          = 1.0f;

		double start   = 0.0;
		double slice   = 0.0;
		float  dotSize = 0.0f;

		mutable std::array<animation::Tween<float>, kDotCount> opacityTweens;
		mutable animation::Tween<float>                        scaleTween{kInactiveScale};
		mutable bool                                           opacityReady    = false;
		mutable bool                                           animationActive = false;
	};
} // namespace music_lyric_player::rendering::components::line::interlude

#endif // MUSIC_LYRIC_PLAYER_RENDERING_COMPONENTS_LINE_INTERLUDE_INDEX_H_
