#ifndef MUSIC_LYRIC_PLAYER_RENDER_UTILS_ANIMATION_EASING_H_
#define MUSIC_LYRIC_PLAYER_RENDER_UTILS_ANIMATION_EASING_H_

#include <algorithm>
#include <cmath>
#include <functional>

namespace music_lyric_player::render::animation {
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
} // namespace music_lyric_player::render::animation

#endif // MUSIC_LYRIC_PLAYER_RENDER_UTILS_ANIMATION_EASING_H_
