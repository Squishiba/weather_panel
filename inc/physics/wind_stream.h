//
// Created by Jared on 7/12/2026.
//

#ifndef WEATHER_WIND_STREAM_H
#define WEATHER_WIND_STREAM_H

#include "../display/segment/segment_properties.h" // live drift resultant (no include cycle: seg_properties knows force_handler, not force_list)
#include "../utility/helpers_rand.h"
#include "../utility/trig_funcs.h"
#include "forcelike.h"

#include <algorithm>
#include <cmath>

/**
 * @brief Wind-driven jet stream that occasionally curls over itself in a loop.
 *
 * NOT a standalone force: the stream is a dependent feature of the segment's
 * overall wind. Every frame it reads the horizontal component of the segment's
 * uniform drift resultant (wind field + any live gust front) and scales itself
 * from it:
 *
 *   - The JET: a band of flow with a parabolic profile and a slowly meandering
 *     centreline. Its acceleration = flow_ref * (wind_x / kReferenceWind), so
 *     the band follows the wind's direction AND strength — calm air means a
 *     dead stream, and a front sweeping through visibly whips it up.
 *
 *   - The LOOP (roller): when the wind is above kMinWindFrac of reference, a
 *     vortex periodically spawns at the upwind end and rides downstream,
 *     spin matched so the stream CURLS BACK OVER ITSELF (top of the loop moves
 *     against the flow, closing the curl onto its own tail). Every loop rolls
 *     fresh RNG — strength, speed, radius, and how far it travels before
 *     dissipating — so no two crossings repeat. Loop strength/speed also scale
 *     with the wind at spawn time.
 *
 * The *_ref tuning knobs are the values that apply when the wind equals
 * kReferenceWind; everything scales linearly around that point.
 *
 * Registration: NON-uniform (spawn_local_force). A narrow band must not steer
 * the segment-wide spawn edge/velocity — segment-wide winds (WindFieldForce,
 * SimpleWindFront) own that. This force never writes x_dir/y_dir.
 *
 * Parallel-safety (parallel_executor.h contract): all mutable state (wind
 * sampling, RNG, roller motion) advances ONLY in update(dt) — once per frame on
 * core 0, in the same serial phase that moves the forces — and apply_force()
 * is pure w.r.t. the force, so both cores may call it concurrently.
 */
class WindStream : public ForceLike
	{

	/// wind_x at which the *_ref knobs apply exactly as given.
	static constexpr float kReferenceWind = 1.5f;
	/// Below this fraction of reference wind, no loops spawn (the jet itself
	/// just fades toward zero with the wind).
	static constexpr float kMinWindFrac = 0.10f;

	static inline std::array full_sin_ = generate_full_sine_table<1024>();

	const DisplaySegProperties &seg_; // wind source: uniform drift resultant

	// --- Geometry (fixed at spawn) ---
	float base_center_y_;  // resting centreline (absolute px)
	float half_width_;     // band half-height (px)
	float inv_half_width_;
	float x_min_;          // band span (px, slightly off-segment)
	float x_max_;

	// --- Reference-wind tuning (fixed at spawn; see class comment) ---
	float flow_ref_;          // band accel at reference wind (magnitude)
	float loop_strength_ref_; // peak tangential accel at reference wind
	float loop_speed_ref_;    // loop travel speed at reference wind, px/s
	float loop_pull_;         // inward pull, fraction of tangential
	float mean_interval_;     // mean seconds between loops

	// --- Animated state (update() only — see parallel-safety note) ---
	float flow_accel_ = 0.0f;  // live: flow_ref_ scaled+signed by the wind
	float center_y_;           // current (meandering) centreline
	float meander_phase_ = 0.0f;
	float meander_speed_;      // cycles/s (slow)
	float meander_amp_;        // px of vertical wander

	// Roller: parameters are rolled fresh at each spawn and frozen for that
	// loop's lifetime (a mid-flight wind change doesn't retarget it).
	bool  roller_active_ = false;
	float roller_x_ = 0.0f;
	float roller_start_x_ = 0.0f;
	float roller_dir_ = 1.0f;        // travel direction, sign of flow at spawn
	float roller_spin_sign_ = -1.0f; // curl-back: opposite the flow (see update)
	float roller_strength_ = 0.0f;   // envelope-scaled, 0 when dormant
	float roller_peak_ = 0.0f;
	float roller_speed_ = 0.0f;      // px/s
	float roller_travel_ = 1.0f;     // distance before dissipating (px)
	float roller_radius2_ = 1.0f;
	float inv_roller_radius2_ = 1.0f;
	float inv_roller_radius_ = 1.0f;
	float idle_timer_;

public:

	/**
	 * @param seg             Owning segment — supplies the live wind (uniform
	 *                        drift resultant) the stream scales from.
	 * @param center_y        Band centreline, absolute framebuffer y (px).
	 * @param half_width      Band half-height (px); jet force is zero outside.
	 * @param flow_ref        Band acceleration at reference wind (magnitude —
	 *                        direction always follows the wind).
	 * @param x_min           Band start (px) — a little off the segment edge.
	 * @param x_max           Band end (px).
	 * @param loop_strength   Peak loop acceleration at reference wind.
	 * @param loop_speed      Loop travel speed at reference wind (px/s).
	 * @param loop_interval_s Mean seconds between loops (randomized +/-40%).
	 * @param loop_pull       Inward pull as a fraction of the tangential push
	 *                        (0 = pure swirl, particles sling out; ~1 = orbits
	 *                        close tight and particles ride inside the curl).
	 */
	WindStream(const DisplaySegProperties &seg,
	           const float center_y, const float half_width, const float flow_ref,
	           const float x_min, const float x_max,
	           const float loop_strength, const float loop_speed, const float loop_interval_s,
	           const float loop_pull = 1.0f)
		: seg_(seg)
		, base_center_y_(center_y)
		, half_width_(std::max(half_width, 1.0f))
		, inv_half_width_(1.0f / std::max(half_width, 1.0f))
		, x_min_(x_min)
		, x_max_(std::max(x_max, x_min + 1.0f))
		, flow_ref_(std::fabs(flow_ref))
		, loop_strength_ref_(loop_strength)
		, loop_speed_ref_(std::max(loop_speed, 1.0f))
		, loop_pull_(loop_pull)
		, mean_interval_(std::max(loop_interval_s, 0.5f))
		, center_y_(center_y)
		, meander_speed_(rng_class.get_rand_float(0.02f, 0.06f))
		, meander_amp_(half_width * 0.5f)
		// First loop arrives sooner than the mean so the effect shows itself.
		, idle_timer_(mean_interval_ * rng_class.get_rand_float(0.3f, 0.8f))
		{
		is_alive = true;
		}

	/// Sample the wind, advance the meander, and run the roller lifecycle.
	/// Once per frame, core 0 only (this is where all the RNG happens).
	void update(const float dt) override
		{
		// Live wind coupling: horizontal component of the segment's uniform
		// drift (wind field + active front; gravity is vertical and drops out).
		const float wind_x = seg_.get_drift_resultant().x_dir;
		const float scale = wind_x / kReferenceWind; // signed
		flow_accel_ = flow_ref_ * scale * 0.1f;             // jet follows the wind

		meander_phase_ += meander_speed_ * dt;
		if (meander_phase_ >= 1.0f) meander_phase_ -= 1.0f;
		center_y_ = base_center_y_ + meander_amp_ * wave_lookup(full_sin_, meander_phase_);

		if (!roller_active_)
			{
			idle_timer_ -= dt;
			if (idle_timer_ > 0.0f)
				return;

			if (std::fabs(scale) < kMinWindFrac)
				{
				idle_timer_ = 0.5f; // calm air: no loop yet, re-check shortly
				return;
				}

			// --- Spawn a loop: roll fresh parameters so no two repeat. ---
			const float mag = std::fabs(scale);
			roller_dir_       = (flow_accel_ >= 0.0f) ? 1.0f : -1.0f;
			// Curl-back spin (screen coords, +y down): positive spin in the
			// (-dy,+dx) swirl is visually clockwise (top moves +x), so the top
			// of the loop moves AGAINST the flow when spin opposes it.
			roller_spin_sign_ = -roller_dir_;
			roller_peak_  = loop_strength_ref_ * mag * rng_class.get_rand_float(0.65f, 1.35f);
			roller_speed_ = loop_speed_ref_    * mag * rng_class.get_rand_float(0.70f, 1.30f);

			const float radius = half_width_ * rng_class.get_rand_float(1.3f, 1.9f);
			roller_radius2_     = radius * radius;
			inv_roller_radius2_ = 1.0f / roller_radius2_;
			inv_roller_radius_  = 1.0f / radius;

			// Not every loop survives the whole crossing — some dissipate
			// mid-segment, which breaks the metronome feel.
			roller_travel_  = (x_max_ - x_min_) * rng_class.get_rand_float(0.55f, 1.0f);
			roller_start_x_ = (roller_dir_ > 0.0f) ? x_min_ : x_max_;
			roller_x_       = roller_start_x_;
			roller_active_  = true;
			return;
			}

		// --- Advance the active loop (frozen params, live position). ---
		roller_x_ += roller_dir_ * roller_speed_ * dt;
		const float progress = std::fabs(roller_x_ - roller_start_x_) / roller_travel_;
		if (progress >= 1.0f)
			{
			roller_active_   = false;
			roller_strength_ = 0.0f;
			idle_timer_      = mean_interval_ * rng_class.get_rand_float(0.6f, 1.4f);
			return;
			}
		// Smooth in/out over the crossing: sin(pi * progress).
		roller_strength_ = roller_peak_ * wave_lookup(full_sin_, 0.5f * progress);
		}

	void apply_force(Physical &phys, XYZ &coords, Acceleration &accel) override
		{
		apply_one(phys, coords, accel);
		}

	/// Force-major path: one dispatch per range, and a becalmed stream (no
	/// flow, no live loop) skips the WHOLE range with a single test.
	void apply_range(const ParticleRange &r) override
		{
		if (flow_accel_ == 0.0f && roller_strength_ <= 0.0f)
			return;
		for (size_t i = 0; i < r.count; ++i)
			apply_one(r.phys_at(i), r.pos_at(i), r.acc_at(i));
		}

private:

	// Single per-particle body shared by both entry points, so the scalar and
	// batched paths can never drift apart.
	inline void apply_one(Physical &phys, XYZ &coords, Acceleration &accel) const
		{
		// Depth coupling shared by jet and roller: nearer particles (z -> 1)
		// are caught harder, so the stream reads with parallax.
		const float grip = phys.inv_weight * (coords.z);

		// --- Jet: parabolic band flow (dead when the wind is dead) ---
		const float dyb = (coords.y - center_y_) * inv_half_width_; // -1..1 inside
		if (dyb > -1.0f && dyb < 1.0f)
			accel.x += flow_accel_ * (1.0f - dyb * dyb) * grip;

		// --- Loop: traveling roller vortex (radius pokes outside the band, so
		// this test is independent of the band test above) ---
		if (roller_strength_ > 0.0f)
			{
			const float dx = coords.x - roller_x_;
			const float dy = coords.y - center_y_;
			const float dist2 = dx * dx + dy * dy;
			if (dist2 < roller_radius2_)
				{
				// Tangential push perpendicular to the radial offset, normalized
				// by radius so roller_peak_ IS the peak acceleration. Spin sign
				// folds the stream back over itself (see update()).
				const float falloff = 1.0f - dist2 * inv_roller_radius2_;
				const float base = roller_strength_ * falloff * inv_roller_radius_ * grip;
				const float tang = roller_spin_sign_ * base;
				accel.x += -dy * tang;
				accel.y += dx * tang;

				// Centripetal pull toward the core. A pure tangential push does
				// no inward work — particles pick up speed and sling straight
				// out of the swirl. This inward term (independent of spin sign)
				// closes the orbits so particles get caught and carried INSIDE
				// the curl. Force ~ r*(1-r^2/R^2): zero at the core, so it's a
				// smooth well, not a black hole.
				accel.x -= dx * base * loop_pull_;
				accel.y -= dy * base * loop_pull_;
				}
			}
		}

	};

#endif // WEATHER_WIND_STREAM_H
