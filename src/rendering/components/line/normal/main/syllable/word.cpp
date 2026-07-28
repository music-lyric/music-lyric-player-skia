#include "rendering/components/line/normal/main/syllable/word.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "include/core/SkBlendMode.h"
#include "include/core/SkBlurTypes.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFont.h"
#include "include/core/SkMaskFilter.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "music_lyric_model.h"
#include "rendering/common/context.h"
#include "rendering/utils/color/parse.h"
#include "rendering/utils/fragment/builder.h"
#include "rendering/utils/length.h"
#include "rendering/utils/shaping/font.h"
#include "rendering/utils/shaping/shaper.h"

namespace music_lyric_player::rendering::components::line::normal::main::syllable {
	namespace {
		constexpr float kGlyphOutset    = 2.0f;
		constexpr float kUnboundedWidth = 1'000'000.0f;
		constexpr float kWidthEpsilon   = 0.01f;

		/**
		 * Layer padding of emphasized words, matching the web word padding that gives the scale pulse and glow room to overflow the glyph box.
		 */
		constexpr float kEmphasizePaddingX = 9.0f;
		constexpr float kEmphasizePaddingY = 5.0f;

		/**
		 * Distance the float offset must move away from a rest point before the word fully leaves the pixel grid.
		 * Blending the snap correction over this range keeps lift-off and touch-down continuous instead of popping half a pixel.
		 */
		constexpr float kSnapBlendRange = 0.5f;

		/**
		 * Returns the local-space corrections that move a draw origin so the word's left edge and baseline land on whole device pixels.
		 * The word's blob keeps free sub-pixel placement for the float animation, so a resting word would otherwise sit at an arbitrary phase and look soft.
		 * Zero when the matrix is not an axis-aligned scale, where device pixel columns have no local-space meaning.
		 */
		SkPoint snapCorrection(const SkCanvas& canvas, float x, float y, float baseline) {
			const SkMatrix matrix = canvas.getLocalToDeviceAs3x3();
			if (!matrix.isScaleTranslate() || matrix.getScaleX() == 0.0f || matrix.getScaleY() == 0.0f) {
				return {0.0f, 0.0f};
			}
			const float deviceX        = matrix.getScaleX() * x + matrix.getTranslateX();
			const float deviceBaseline = matrix.getScaleY() * (y + baseline) + matrix.getTranslateY();
			return {
				(std::round(deviceX) - deviceX) / matrix.getScaleX(),
				(std::round(deviceBaseline) - deviceBaseline) / matrix.getScaleY(),
			};
		}

		/**
		 * Reports whether a sampled cell transform moves the cell at all, letting resting cells skip the canvas save.
		 */
		bool isRestTransform(const animation::Emphasize::Transform& transform) {
			return transform.scale == 1.0f && transform.dx == 0.0f && transform.dy == 0.0f;
		}

		/**
		 * Concats one cell's emphasize transform: scale about the cell center, then shift in the scaled space.
		 * This is the same net matrix as the web `scale() translate()` list with a centered transform origin.
		 */
		void concatCellTransform(SkCanvas* canvas, const utils::fragment::FragmentGroup& cell, const animation::Emphasize::Transform& transform, float x, float y) {
			const float centerX = x + cell.bounds.fLeft + cell.advance * 0.5f;
			const float centerY = y + cell.height * 0.5f;
			canvas->translate(centerX, centerY);
			canvas->scale(transform.scale, transform.scale);
			canvas->translate(transform.dx - centerX, transform.dy - centerY);
		}

		/**
		 * Returns the word's absolute start time, or zero when timing is absent.
		 */
		double wordStart(const music_lyric_model::common::WordNormal& info) {
			const music_lyric_model::common::Time* time = music_lyric_model::common::getWordTime(info);
			return time ? static_cast<double>(time->start) : 0.0;
		}

		/**
		 * Returns the word's non-negative duration.
		 */
		double wordDuration(const music_lyric_model::common::WordNormal& info) {
			return std::max(static_cast<double>(music_lyric_model::common::getWordDuration(info)), 0.0);
		}
	} // namespace

	Word::Word(const music_lyric_model::common::WordNormal& info, uint32_t spacesBefore)
	    : text(info.content),
	      start(wordStart(info)),
	      duration(wordDuration(info)),
	      spaceCount(spacesBefore),
	      stressed(info.stress),
	      floating(this->start, this->duration),
	      emphasizing(this->start, this->duration) {}

	Word::~Word() = default;

	void Word::layout(const common::RenderContext& context) {
		this->measuredWidth    = 0.0f;
		this->measuredHeight   = 0.0f;
		this->measuredBaseline = 0.0f;
		this->group            = {};
		this->cells.clear();
		this->useCells = false;

		if (!context.shaper || !context.fontMgr || this->text.empty()) {
			return;
		}

		const config::Root& cfg = context.config;
		const float size = static_cast<float>(std::max(resolveLength(cfg.line.normal.main.syllable.font.size, config::Default.line.normal.main.syllable.font.size), 1.0));
		// The word floats vertically, so its glyphs keep free sub-pixel placement; the paint path snaps them back onto the grid whenever the word rests.
		const SkFont font = utils::shaping::buildBodyFont(context.fontMgr, cfg.line.normal.main.syllable.font.family.value(), size, true);

		const char*       utf8  = this->text.c_str();
		const std::size_t bytes = this->text.size();

		const utils::shaping::ShapedText shaped = utils::shaping::shapeText(*context.shaper, context.unicode, context.fontMgr, font, utf8, bytes, kUnboundedWidth);
		// The word is shaped unbounded, so makeTextGroup collapses it to one fragment; ceil/epsilon stay component-local for row packing.
		this->group = utils::fragment::makeTextGroup(shaped, utf8);
		if (!this->group) {
			return;
		}
		this->measuredWidth    = std::ceil(std::max(this->group.advance, 1.0f) + kWidthEpsilon);
		this->measuredBaseline = this->group.ascent;
		this->measuredHeight   = this->group.height;

		// The per-character split only happens for stressed words with at least one emphasize sub-effect enabled, so plain words keep the single cached blob.
		const auto& emphasize = cfg.line.normal.main.syllable.word.animation.emphasize;
		this->useCells        = this->stressed && emphasize.enabled.value() &&
			(emphasize.effects.main.enabled.value() || emphasize.effects.glow.enabled.value() || emphasize.effects.floating.enabled.value());
		if (this->useCells) {
			this->cells    = utils::fragment::makeClusterGroups(shaped, utf8);
			this->useCells = !this->cells.empty();
		}
	}

	void Word::setPosition(float x, float y) {
		this->x = x;
		this->y = y;
	}

	void Word::paintGroup(SkCanvas* canvas, float x, float y, SkColor color, const std::vector<animation::Emphasize::Transform>* transforms) const {
		// Cells keep their absolute in-word glyph positions, so an untransformed cell issues the same draw parameters as the single blob.
		if (this->useCells) {
			for (std::size_t i = 0; i < this->cells.size(); ++i) {
				const utils::fragment::FragmentGroup&  cell      = this->cells[i];
				const animation::Emphasize::Transform* transform = transforms && i < transforms->size() ? &(*transforms)[i] : nullptr;
				if (!transform || isRestTransform(*transform)) {
					cell.paint(canvas, x, y, color);
					continue;
				}
				canvas->save();
				concatCellTransform(canvas, cell, *transform, x, y);
				cell.paint(canvas, x, y, color);
				canvas->restore();
			}
			return;
		}
		this->group.paint(canvas, x, y, color);
	}

	void Word::paintGlow(SkCanvas* canvas, float x, float y, const GlowPaint& glow, double opacity, const std::vector<animation::Emphasize::Transform>* transforms) const {
		if (!this->useCells || !transforms || glow.radius <= 0.0f) {
			return;
		}

		// The CSS blur radius equals two standard deviations, so the sigma is half the sampled radius; the filter respects the CTM so a scaled cell scales its halo too.
		const sk_sp<SkMaskFilter> blur = SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, glow.radius * 0.5f);
		if (!blur) {
			return;
		}

		SkPaint paint;
		paint.setAntiAlias(true);
		paint.setMaskFilter(blur);
		for (std::size_t i = 0; i < this->cells.size(); ++i) {
			const animation::Emphasize::Transform* transform = i < transforms->size() ? &(*transforms)[i] : nullptr;
			if (!transform || transform->glow <= 0.0f) {
				continue;
			}
			// The halo alpha comes from the pulse and the path opacity; the config color's own alpha is discarded like the web rgba template.
			paint.setColor(utils::color::withOpacity(SkColorSetA(glow.color, 0xFF), static_cast<double>(transform->glow) * opacity));

			const utils::fragment::FragmentGroup& cell = this->cells[i];
			const bool                            rest = isRestTransform(*transform);
			if (!rest) {
				canvas->save();
				concatCellTransform(canvas, cell, *transform, x, y);
			}
			for (const utils::fragment::GlyphFragment& fragment : cell.fragments) {
				if (!fragment.blob) {
					continue;
				}
				canvas->drawTextBlob(fragment.blob, x + fragment.origin.fX, y + fragment.origin.fY, paint);
			}
			if (!rest) {
				canvas->restore();
			}
		}
	}

	void Word::paintReveal(SkCanvas*                            canvas,
		float                                               x,
		float                                               y,
		float                                               progress,
		float                                               feather,
		SkColor                                             unsungColor,
		SkColor                                             sungColor,
		const std::vector<animation::Emphasize::Transform>* transforms,
		const GlowPaint*                                    glow) const {
		if (!this->group) {
			return;
		}

		// Use the ceil'd layout box rather than group.bounds so the mask saveLayer size stays unchanged.
		const SkRect textBounds = SkRect::MakeXYWH(x, y, this->measuredWidth, this->measuredHeight);
		// Emphasized cells scale and shift past the glyph outset, so their layer grows by the web word padding; the mask geometry itself stays on the unpadded box.
		const SkRect drawBounds = this->useCells ? textBounds.makeOutset(kEmphasizePaddingX, kEmphasizePaddingY) : textBounds.makeOutset(kGlyphOutset, kGlyphOutset);

		// The glow wipes in its own layer beneath the body: kDstIn multiplies only the halo's alpha with the same band, keeping the glow color the kSrcIn body pass would overwrite.
		if (glow) {
			canvas->saveLayer(&drawBounds, nullptr);
			this->paintGlow(canvas, x, y, *glow, 1.0, transforms);
			animation::Mask::apply(canvas, drawBounds, textBounds, progress, feather, unsungColor, sungColor, SkBlendMode::kDstIn);
			canvas->restore();
		}

		canvas->saveLayer(&drawBounds, nullptr);
		// Opaque coverage in the sung color's rgb: kSrcIn replaces the rgb anyway, and keeping the paint luminance of the direct draws keeps the glyph masks' gamma identical, so entering or leaving the reveal cannot shift the word's weight.
		this->paintGroup(canvas, x, y, SkColorSetA(sungColor, 0xFF), transforms);
		animation::Mask::apply(canvas, drawBounds, textBounds, progress, feather, unsungColor, sungColor);
		canvas->restore();
	}

	void Word::paint(SkCanvas*           canvas,
		float                        lineX,
		float                        lineY,
		double                       now,
		bool                         active,
		bool                         maskEnabled,
		float                        maskProgress,
		float                        maskFeather,
		float                        inactiveOpacity,
		const common::RenderContext& context) const {
		if (!this->group) {
			return;
		}

		const config::Root& cfg             = context.config;
		const auto&         animationConfig = cfg.line.normal.main.syllable.word.animation;
		const double        fromValue =
			std::isfinite(animationConfig.floating.from) ? animationConfig.floating.from : config::Default.line.normal.main.syllable.word.animation.floating.from;
		const double toValue =
			std::isfinite(animationConfig.floating.to) ? animationConfig.floating.to : config::Default.line.normal.main.syllable.word.animation.floating.to;
		const float offset =
			this->floating.sample(context.currentTime, now, active, animationConfig.floating.enabled, static_cast<float>(fromValue), static_cast<float>(toValue));
		// The float rests either at `from` (unsung or settled back) or at `to` (fully risen), so the grid correction is measured against the nearer rest point.
		const float rest = std::abs(offset - static_cast<float>(fromValue)) <= std::abs(offset - static_cast<float>(toValue)) ? static_cast<float>(fromValue)
																      : static_cast<float>(toValue);
		const float rawX = lineX + this->x;
		const float rawY = lineY + this->y + offset;
		// Measuring at the rest position keeps the correction constant while the word moves; blending it out over the first half pixel of travel makes a resting word pixel-crisp yet lets it leave and rejoin the grid without a visible pop.
		const SkPoint correction = snapCorrection(*canvas, rawX, lineY + this->y + rest, this->measuredBaseline);
		const float   blend      = std::clamp(1.0f - std::abs(offset - rest) / kSnapBlendRange, 0.0f, 1.0f);
		const float   drawX      = rawX + correction.fX;
		const float   drawY      = rawY + correction.fY * blend;

		// Emphasize transforms are sampled once per frame and shared by every paint path below; plain words skip the whole pipeline.
		const std::vector<animation::Emphasize::Transform>* transforms = nullptr;
		GlowPaint                                           glowPaint;
		bool                                                glowLit = false;
		if (this->useCells) {
			const auto&                              emphasizeConfig = animationConfig.emphasize;
			const animation::Emphasize::MainSettings mainSettings{
				emphasizeConfig.effects.main.enabled.value(),
				static_cast<float>(emphasizeConfig.effects.main.scale.value()),
				static_cast<float>(emphasizeConfig.effects.main.offsetHorizontal.value()),
				static_cast<float>(emphasizeConfig.effects.main.offsetVertical.value()),
				emphasizeConfig.effects.main.easingRise.value(),
				emphasizeConfig.effects.main.easingFall.value(),
			};
			// The background amplitude multiplier stays unwired until background lines are ported.
			const animation::Emphasize::FloatSettings floatSettings{
				emphasizeConfig.effects.floating.enabled.value(),
				static_cast<float>(emphasizeConfig.effects.floating.amplitude.value()),
				emphasizeConfig.effects.floating.duration.scale.value(),
				emphasizeConfig.effects.floating.duration.lead.value(),
				emphasizeConfig.effects.floating.easing.value(),
			};
			// An unparseable glow color disables the whole sub-effect, mirroring the web soft kill switch.
			std::optional<SkColor> glowColor;
			if (emphasizeConfig.effects.glow.enabled.value()) {
				glowColor = utils::color::tryResolve(emphasizeConfig.effects.glow.color.value());
			}
			const animation::Emphasize::GlowSettings glowSettings{
				glowColor.has_value(),
				static_cast<float>(emphasizeConfig.effects.glow.maxRadius.value()),
				static_cast<float>(emphasizeConfig.effects.glow.maxAlpha.value()),
				emphasizeConfig.effects.glow.easingRise.value(),
				emphasizeConfig.effects.glow.easingFall.value(),
			};
			transforms = &this->emphasizing.sample(context.currentTime,
				now,
				active,
				emphasizeConfig.minDuration.value(),
				emphasizeConfig.disablePlaybackRate.value(),
				this->cells.size(),
				mainSettings,
				floatSettings,
				glowSettings);
			if (glowColor.has_value() && this->emphasizing.glowRadius() > 0.0f) {
				glowPaint = GlowPaint{*glowColor, this->emphasizing.glowRadius()};
				glowLit   = true;
			}
		}

		const SkColor normalColor   = utils::color::resolve(cfg.line.normal.main.syllable.style.normal.color, config::Default.line.normal.main.syllable.style.normal.color);
		const SkColor activeColor   = utils::color::resolve(cfg.line.normal.main.syllable.style.active.color, config::Default.line.normal.main.syllable.style.active.color);
		const double  normalOpacity = cfg.line.normal.main.syllable.style.normal.opacity;
		const double  activeOpacity = cfg.line.normal.main.syllable.style.active.opacity;

		// Inactive lines paint the whole word in the normal state color; the opacity is eased by the owning element so a deactivating line fades out (web `.word` `transition: opacity 0.8s ease`) instead of snapping.
		if (!active) {
			if (glowLit) {
				this->paintGlow(canvas, drawX, drawY, glowPaint, inactiveOpacity, transforms);
			}
			this->paintGroup(canvas, drawX, drawY, utils::color::withOpacity(normalColor, inactiveOpacity), transforms);
			return;
		}

		// The active line switches the rgb to the active color at once; the mask only wipes alpha, from the normal opacity (unsung) up to the active opacity (sung), so the hue never changes.
		const SkColor sungColor   = utils::color::withOpacity(activeColor, activeOpacity);
		const SkColor unsungColor = utils::color::withOpacity(activeColor, normalOpacity);

		if (!maskEnabled || maskProgress >= 1.0f) {
			if (glowLit) {
				this->paintGlow(canvas, drawX, drawY, glowPaint, activeOpacity, transforms);
			}
			this->paintGroup(canvas, drawX, drawY, sungColor, transforms);
			return;
		}
		if (maskProgress <= 0.0f) {
			if (glowLit) {
				this->paintGlow(canvas, drawX, drawY, glowPaint, normalOpacity, transforms);
			}
			this->paintGroup(canvas, drawX, drawY, unsungColor, transforms);
			return;
		}
		this->paintReveal(canvas, drawX, drawY, maskProgress, maskFeather, unsungColor, sungColor, transforms, glowLit ? &glowPaint : nullptr);
	}

	animation::Mask::Input Word::maskInput() const {
		return animation::Mask::Input{this->start, this->duration, this->measuredWidth, this->measuredHeight};
	}

	uint32_t Word::spacesBefore() const {
		return this->spaceCount;
	}

	float Word::width() const {
		return this->measuredWidth;
	}

	float Word::height() const {
		return this->measuredHeight;
	}

	float Word::baseline() const {
		return this->measuredBaseline;
	}
} // namespace music_lyric_player::rendering::components::line::normal::main::syllable
