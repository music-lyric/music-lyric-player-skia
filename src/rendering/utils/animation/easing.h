#ifndef MUSIC_LYRIC_PLAYER_RENDERING_UTILS_ANIMATION_EASING_H_
#define MUSIC_LYRIC_PLAYER_RENDERING_UTILS_ANIMATION_EASING_H_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "rendering/utils/length.h"

namespace music_lyric_player::rendering::animation {
	/**
	 * Maps normalised progress in 0..1 to an eased 0..1, reshaping an animation's rate over time.
	 * Any callable of this shape qualifies, so the analytic presets below and `CubicBezier` interchange.
	 */
	using Easing = ::std::function<float(float)>;

	/**
	 * Linear easing: returns progress unchanged after clamping to 0..1.
	 */
	inline float linear(float t) {
		return ::std::clamp(t, 0.0f, 1.0f);
	}

	/**
	 * Cubic ease-in-out over normalised 0..1 progress; the input is clamped to the range.
	 */
	inline float inOutCubic(float t) {
		t = ::std::clamp(t, 0.0f, 1.0f);
		return t < 0.5f ? 4.0f * t * t * t : 1.0f - ::std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
	}

	/**
	 * Cubic Bezier easing defined by its two control points, mirroring CSS `cubic-bezier(x1,y1,x2,y2)`.
	 * Endpoints are fixed at (0,0) and (1,1), and `x1`/`x2` are clamped to 0..1 so x stays a function of the curve parameter.
	 * `operator()` solves x==t for the curve parameter with Newton-Raphson (bisection fallback), then reads y off the curve.
	 */
	class CubicBezier {
	public:
		/**
		 * Builds the curve from control points `(x1,y1)` and `(x2,y2)`, precomputing its polynomial coefficients.
		 */
		CubicBezier(float x1, float y1, float x2, float y2) {
			x1 = ::std::clamp(x1, 0.0f, 1.0f);
			x2 = ::std::clamp(x2, 0.0f, 1.0f);
			// Polynomial form B(p) = ((a*p + b)*p + c)*p for each axis (P0 = 0, P3 = 1).
			this->cx = 3.0f * x1;
			this->bx = 3.0f * (x2 - x1) - this->cx;
			this->ax = 1.0f - this->cx - this->bx;
			this->cy = 3.0f * y1;
			this->by = 3.0f * (y2 - y1) - this->cy;
			this->ay = 1.0f - this->cy - this->by;
		}

		/**
		 * Eases normalised progress `t` (clamped to 0..1); the endpoints short-circuit to avoid solving.
		 */
		float operator()(float t) const {
			t = ::std::clamp(t, 0.0f, 1.0f);
			if (t <= 0.0f || t >= 1.0f) {
				return t;
			}
			return sampleY(solveForParam(t));
		}

	private:
		/**
		 * Evaluates x at curve parameter `p`.
		 */
		float sampleX(float p) const {
			return ((this->ax * p + this->bx) * p + this->cx) * p;
		}

		/**
		 * Evaluates y at curve parameter `p`.
		 */
		float sampleY(float p) const {
			return ((this->ay * p + this->by) * p + this->cy) * p;
		}

		/**
		 * Derivative dx/dp at parameter `p`, used by the Newton step.
		 */
		float slopeX(float p) const {
			return (3.0f * this->ax * p + 2.0f * this->bx) * p + this->cx;
		}

		/**
		 * Finds the parameter whose x equals `t`: a few Newton-Raphson steps, then bisection if they stall.
		 */
		float solveForParam(float t) const {
			constexpr float kEpsilon = 1e-6f;

			float p = t; // x(p) ~ p makes progress itself a good initial guess.
			for (int i = 0; i < 8; ++i) {
				const float x = sampleX(p) - t;
				if (::std::fabs(x) < kEpsilon) {
					return p;
				}
				const float d = slopeX(p);
				if (::std::fabs(d) < kEpsilon) {
					break;
				}
				p -= x / d;
			}

			float lo = 0.0f;
			float hi = 1.0f;
			p        = t;
			while (lo < hi) {
				const float x = sampleX(p);
				if (::std::fabs(x - t) < kEpsilon) {
					break;
				}
				if (x < t) {
					lo = p;
				} else {
					hi = p;
				}
				p = (lo + hi) * 0.5f;
			}
			return p;
		}

		float ax = 0.0f;
		float bx = 0.0f;
		float cx = 0.0f;
		float ay = 0.0f;
		float by = 0.0f;
		float cy = 0.0f;
	};

	/**
	 * Resolves a CSS timing-function string into an `Easing`.
	 * Accepts `linear`, `ease`, `ease-in`, `ease-out`, `ease-in-out`, or `cubic-bezier(x1, y1, x2, y2)`.
	 * An unrecognised or malformed value falls back to `ease`.
	 */
	inline Easing resolveEasing(const ::std::string& value) {
		const ::std::string_view token = detail::trimLength(value);

		// cubic-bezier(x1, y1, x2, y2): parse the four control coordinates.
		constexpr ::std::string_view kFunc = "cubic-bezier";
		if (token.size() > kFunc.size() && token.substr(0, kFunc.size()) == kFunc) {
			const ::std::size_t open = token.find('(');
			if (open != ::std::string_view::npos && token.back() == ')') {
				const ::std::string_view body = token.substr(open + 1, token.size() - open - 2);
				double                   coords[4];
				int                      count = 0;
				::std::size_t            start = 0;
				bool                     ok    = true;
				while (ok && count < 4) {
					const ::std::size_t           comma = body.find(',', start);
					const ::std::string_view      piece = body.substr(start, comma == ::std::string_view::npos ? ::std::string_view::npos : comma - start);
					const ::std::optional<double> number = detail::parseNumber(detail::trimLength(piece));
					if (!number.has_value()) {
						ok = false;
						break;
					}
					coords[count++] = *number;
					if (comma == ::std::string_view::npos) {
						break;
					}
					start = comma + 1;
				}
				if (ok && count == 4) {
					return CubicBezier(static_cast<float>(coords[0]), static_cast<float>(coords[1]), static_cast<float>(coords[2]), static_cast<float>(coords[3]));
				}
			}
			return CubicBezier(0.25f, 0.1f, 0.25f, 1.0f); // malformed cubic-bezier falls back to `ease`
		}

		if (token == "linear") {
			return linear;
		}
		if (token == "ease-in") {
			return CubicBezier(0.42f, 0.0f, 1.0f, 1.0f);
		}
		if (token == "ease-out") {
			return CubicBezier(0.0f, 0.0f, 0.58f, 1.0f);
		}
		if (token == "ease-in-out") {
			return CubicBezier(0.42f, 0.0f, 0.58f, 1.0f);
		}
		// `ease` and every unrecognised value.
		return CubicBezier(0.25f, 0.1f, 0.25f, 1.0f);
	}
} // namespace music_lyric_player::rendering::animation

#endif // MUSIC_LYRIC_PLAYER_RENDERING_UTILS_ANIMATION_EASING_H_
