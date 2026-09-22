#ifndef SPRING_DYNAMICS_H
#define SPRING_DYNAMICS_H

#include <cmath>
#include <godot_cpp/core/math.hpp>

namespace godot {

/**
 * @brief Frame-rate-independent smoothing factor for an exponential approach.
 *
 * Returns the lerp `t` in `current = lerp(current, target, t)` that makes a
 * simple `a.lerp(b, t)` decay at a fixed rate regardless of frame time, i.e.
 * `1 - e^(-rate * dt)`. Prefer this over the common `MIN(1, dt * k)` hack, which
 * is only a first-order approximation and drifts (over-smooths at low fps,
 * under-smooths at high fps). `rate` is roughly "how many e-folds per second".
 */
inline float spring_damp_factor(float p_rate, float p_delta) {
	if (p_rate <= 0.0f || p_delta <= 0.0f) {
		return 0.0f;
	}
	return 1.0f - std::exp(-p_rate * p_delta);
}

/**
 * @brief Second-Order Dynamics (Spring-Damper system)
 * Implementation based on Keijiro Takahashi's and t3ssel8r's work.
 * Can be used for smoothing Camera, Doors, Punching Bags, etc.

 * [Instant "Game Feel" Tutorial - Secrets of Springs Explained](https://www.youtube.com/watch?v=bFOAipGJGA0)
 * [3Blue1Brown | The Physics of Euler's Formula | Laplace Transform
 Prelude](https://www.youtube.com/watch?v=-j8PzkZ70Lg)
 * [Example Second-Order ODE: Spring-Mass-Damper](https://www.youtube.com/watch?v=r1eWerqrcqo)
 * [Understanding Vibration and Resonance](https://www.youtube.com/watch?v=vLaFAKnaRJU)
 */
template <typename T> struct SpringDynamics {
	T current = T();
	T velocity = T();
	T target = T();

	void reset(T p_value) {
		current = p_value;
		target = p_value;
		velocity = T(); // zero for both float and Vector types
	}

	/**
	 * Advance the spring by `p_delta` seconds.
	 *
	 * @param p_frequency Natural frequency in Hz (speed of response).
	 * @param p_damping   Damping ratio (1 = critically damped, <1 bouncy, >1 sluggish).
	 * @param p_response  Initial response / anticipation (0 = none, >0 overshoots
	 *                    toward a moving target, <0 anticipates before reacting).
	 *
	 * Stability: the semi-implicit integrator can diverge when the frame time is
	 * large relative to the spring period. We clamp `k2` to the smallest value
	 * that keeps the poles inside the unit circle for this `p_delta`, so the
	 * spring stays stable at any frame rate (hitches, low-end mobile) instead of
	 * exploding. This is the standard t3ssel8r clamp.
	 */
	void
	step(float p_delta,
		 float p_frequency,
		 float p_damping,
		 float p_response) {
		if (p_delta <= 0.0f || p_frequency <= 0.0f)
			return;

		const float two_pi_f = 2.0f * (float)Math::PI * p_frequency;

		// Dynamics constants.
		float k1 = p_damping / ((float)Math::PI * p_frequency);
		float k2 = 1.0f / (two_pi_f * two_pi_f);
		float k3 = p_response * p_damping / two_pi_f;

		// Clamp k2 so the discrete system cannot go unstable for this delta.
		const float k2_stable = p_delta * p_delta * 0.5f + p_delta * k1 * 0.5f;
		if (k2 < k2_stable) {
			k2 = k2_stable;
		}
		if (k2 < p_delta * k1) {
			k2 = p_delta * k1;
		}

		// Estimate target velocity (x_prime).
		T x_prime = (target - current) / p_delta;

		// Semi-implicit update: advance position with the *old* velocity, then the
		// velocity with the *new* position (this ordering is what the clamp above
		// keeps stable).
		current += velocity * p_delta;
		velocity += p_delta * (target + k3 * x_prime - current - k1 * velocity) / k2;
	}
};

/**
 * @brief Analytical Damped Spring
 * Implementation based on Ryan Juckett's analytical solution.
 * Provides perfect stability even at low frame rates.
 * https://www.ryanjuckett.com/damped-springs/
 */
template <typename T> struct AnalyticalSpring {
	T current = T();
	T velocity = T();
	T target = T();

	void reset(T p_value) {
		current = p_value;
		target = p_value;
		velocity = T();
	}

	/**
	 * @param p_delta Time step
	 * @param p_frequency Frequency in Hertz (converted to rad/s internally)
	 * @param p_damping Damping ratio (1.0 = critically damped)
	 */
	void
	step(float p_delta,
		 float p_frequency,
		 float p_damping) {
		if (p_delta <= 0.0f)
			return;

		// Convert Hz to Angular Frequency (rad/s)
		float omega = 2.0f * Math::PI * p_frequency;
		float zeta = p_damping;
		const float epsilon = 0.0001f;

		if (omega < epsilon)
			return;

		float posPosCoef, posVelCoef, velPosCoef, velVelCoef;

		if (zeta > 1.0f + epsilon) {
			// Over-damped
			float za = -omega * zeta;
			float zb = omega * std::sqrt(zeta * zeta - 1.0f);
			float z1 = za - zb;
			float z2 = za + zb;

			float e1 = std::exp(z1 * p_delta);
			float e2 = std::exp(z2 * p_delta);

			float invTwoZb = 1.0f / (2.0f * zb);
			float e1_Over_TwoZb = e1 * invTwoZb;
			float e2_Over_TwoZb = e2 * invTwoZb;
			float z1e1_Over_TwoZb = z1 * e1_Over_TwoZb;
			float z2e2_Over_TwoZb = z2 * e2_Over_TwoZb;

			posPosCoef = e1_Over_TwoZb * z2 - z2e2_Over_TwoZb + e2;
			posVelCoef = -e1_Over_TwoZb + e2_Over_TwoZb;
			velPosCoef = (z1e1_Over_TwoZb - z2e2_Over_TwoZb + e2) * z2;
			velVelCoef = -z1e1_Over_TwoZb + z2e2_Over_TwoZb;
		} else if (zeta < 1.0f - epsilon) {
			// Under-damped
			float omegaZeta = omega * zeta;
			float alpha = omega * std::sqrt(1.0f - zeta * zeta);

			float expTerm = std::exp(-omegaZeta * p_delta);
			float cosTerm = std::cos(alpha * p_delta);
			float sinTerm = std::sin(alpha * p_delta);

			float invAlpha = 1.0f / alpha;
			float expSin = expTerm * sinTerm;
			float expCos = expTerm * cosTerm;
			float expOmegaZetaSin_Over_Alpha = expTerm * omegaZeta * sinTerm * invAlpha;

			posPosCoef = expCos + expOmegaZetaSin_Over_Alpha;
			posVelCoef = expSin * invAlpha;
			velPosCoef = -expSin * alpha - omegaZeta * expOmegaZetaSin_Over_Alpha;
			velVelCoef = expCos - expOmegaZetaSin_Over_Alpha;
		} else {
			// Critically damped
			float expTerm = std::exp(-omega * p_delta);
			float timeExp = p_delta * expTerm;
			float timeExpFreq = timeExp * omega;

			posPosCoef = timeExpFreq + expTerm;
			posVelCoef = timeExp;
			velPosCoef = -omega * timeExpFreq;
			velVelCoef = -timeExpFreq + expTerm;
		}

		T oldPos = current - target;
		T oldVel = velocity;

		current = target + oldPos * posPosCoef + oldVel * posVelCoef;
		velocity = oldPos * velPosCoef + oldVel * velVelCoef;
	}
};

} // namespace godot

#endif // SPRING_DYNAMICS_H
