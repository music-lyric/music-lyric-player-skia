#include "rendering/components/container/index.h"

#include <algorithm>
#include <utility>

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkShader.h"
#include "include/core/SkSpan.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkGradient.h"
#include "rendering/common/context.h"
#include "rendering/utils/color/parse.h"
#include "rendering/utils/length.h"

namespace music_lyric_player::rendering::components::container {
	namespace {
		/**
		 * The resolved top and bottom fade ranges in logical pixels, each clamped to be non-negative.
		 */
		struct FadeExtent {
			float top;
			float bottom;
		};

		/**
		 * Resolves the fade config's top and bottom ranges against the viewport height, falling back to the defaults.
		 */
		FadeExtent resolveFade(const config::container::FadeConfig& fade, const config::container::FadeConfig& fallback, float logicalHeight) {
			const double base   = logicalHeight;
			const float  top    = static_cast<float>(std::max(resolveLength(fade.top, fallback.top, base), 0.0));
			const float  bottom = static_cast<float>(std::max(resolveLength(fade.bottom, fallback.bottom, base), 0.0));
			return {top, bottom};
		}
	} // namespace

	void Element::paintBackground(SkCanvas* canvas, const common::RenderContext& context) const {
		const config::Root& cfg = context.config;
		// clear ignores the canvas transform, so the fill always covers the whole surface.
		canvas->clear(utils::color::resolve(cfg.container.backgroundColor, config::Default.container.backgroundColor));
	}

	float Element::paddingX(float logicalWidth, const common::RenderContext& context) const {
		const config::Root& cfg     = context.config;
		const float         padding = static_cast<float>(resolveLength(cfg.container.paddingX, config::Default.container.paddingX, logicalWidth));
		// Clamp so both sides together never exceed the viewport, keeping the content width positive.
		return std::min(padding, logicalWidth * 0.5f);
	}

	bool Element::fadeActive(float logicalHeight, const common::RenderContext& context) const {
		const config::container::Root& cfg = context.config.container;
		if (!cfg.fade.enabled || logicalHeight <= 0.0f) {
			return false;
		}
		const FadeExtent extent = resolveFade(cfg.fade, config::Default.container.fade, logicalHeight);
		return extent.top > 0.0f || extent.bottom > 0.0f;
	}

	void Element::paintEdgeFade(SkCanvas* canvas, float logicalWidth, float logicalHeight, const common::RenderContext& context) const {
		if (logicalHeight <= 0.0f) {
			return;
		}
		const config::container::Root& cfg    = context.config.container;
		const FadeExtent               extent = resolveFade(cfg.fade, config::Default.container.fade, logicalHeight);

		// Gradient stop fractions along the height: opaque between the edges, transparent at them.
		float top    = std::clamp(extent.top / logicalHeight, 0.0f, 1.0f);
		float bottom = std::clamp(1.0f - extent.bottom / logicalHeight, 0.0f, 1.0f);
		// A tall enough fade on both sides can cross over; collapse the overlap to a single point so the opaque band stays valid.
		if (top > bottom) {
			top    = (top + bottom) * 0.5f;
			bottom = top;
		}

		// Build strictly increasing stops (the API drops non-monotonic ones): transparent edges bracket an opaque band.
		// Only the source alpha matters under kDstIn, so plain white and transparent suffice.
		constexpr SkColor4f kTransparent{0.0f, 0.0f, 0.0f, 0.0f};
		constexpr SkColor4f kWhite{1.0f, 1.0f, 1.0f, 1.0f};
		SkColor4f           colors[4];
		float               positions[4];
		int                 count = 0;
		if (top > 0.0f) {
			// Fade in from the top edge; without it the band already starts opaque at 0.
			positions[count] = 0.0f;
			colors[count]    = kTransparent;
			++count;
		}
		positions[count] = top;
		colors[count]    = kWhite;
		++count;
		if (bottom > top) {
			// A distinct plateau end; equal to top when the band collapsed to a point.
			positions[count] = bottom;
			colors[count]    = kWhite;
			++count;
		}
		if (bottom < 1.0f) {
			// Fade out to the bottom edge; without it the band stays opaque through 1.
			positions[count] = 1.0f;
			colors[count]    = kTransparent;
			++count;
		}

		const SkPoint   points[] = {{0.0f, 0.0f}, {0.0f, logicalHeight}};
		sk_sp<SkShader> shader    = SkShaders::LinearGradient(
            points,
            {{SkSpan<const SkColor4f>(colors, count), SkSpan<const float>(positions, count), SkTileMode::kClamp}, {}});
		if (!shader) {
			return;
		}

		SkPaint paint;
		paint.setShader(std::move(shader));
		// kDstIn multiplies the layer's alpha by the gradient alpha, erasing the top and bottom edges.
		paint.setBlendMode(SkBlendMode::kDstIn);
		canvas->drawRect(SkRect::MakeWH(logicalWidth, logicalHeight), paint);
	}
} // namespace music_lyric_player::rendering::components::container
