#ifndef PARTICLE_BASE_H
#define PARTICLE_BASE_H

#include "physics/particle_physics.h"
#include "physics/gravity.h"
#include "particle_tuning.h"
#include "utility/fixed_unique_factory.h"

class ParticleBaseTag
{
public:
	static constexpr bool is_particle = true;
};

/**
 * @brief Common state and physics for every particle type (Rain, Snow, ...).
 *
 * Particles are stored by value in an effect's etl::vector and are never used
 * through a ParticleLike pointer/reference, so this type is deliberately
 * NON-virtual — update()/reset()/spawn() are resolved statically by the CRTP
 * effect. Dropping the vtable saves 4 bytes per particle.
 *
 * The per-type drag/mass ranges and spawn bias live in a single shared
 * ParticleTuning (see particle_tuning.h); each particle keeps only a pointer to
 * it (tuning_) rather than its own copy of the four range floats. That saves
 * 12 bytes per particle and means runtime edits to a tuning struct immediately
 * affect every live particle of that type.
 */
class ParticleLike : public ParticleBaseTag
{

protected:
	// Shared, non-owning tuning for this particle's type. Null only for the
	// default-constructed mixin case (WindGust), which never calls reset()/spawn().
	const ParticleTuning *tuning_ = nullptr;

	Velocity velocities_; // The particle's velocities in x, y, and z directions.
	XYZ positions_;		  // The immediate particle's position in x, y and z.
	Acceleration accel_;  // Particle's accumulated acceleration.
	Physical physical_;	  // The particle's physical properties.

	/**
	 * @brief Construct a particle bound to its type's shared tuning.
	 * @param spawn_coords Initial position.
	 * @param particle_color Render colour.
	 * @param tuning Shared tuning for this particle type; stored by pointer, not copied.
	 */
	ParticleLike(const XYZ &spawn_coords, const Color particle_color, const ParticleTuning &tuning)
		: tuning_(&tuning), positions_(spawn_coords), color(particle_color)
	{
	}

	ParticleLike() = default;

public:
	Color color; // Particle's color.

	void update(const float &dt, const float gravity_mag)
	{
		accel_.x -= (physical_.drag * velocities_.x);
		accel_.y -= (physical_.drag * velocities_.y);
		velocities_.x += accel_.x * dt;
		velocities_.y += accel_.y * dt;
		positions_.x += velocities_.x * ((positions_.z) * 1.2f) * dt;

		positions_.y += velocities_.y * ((positions_.z) * 1.2f) * dt;
		positions_.z = std::clamp(positions_.z + (velocities_.z), 0.2f, 1.0f);
		accel_.x = 0;
		accel_.y = 0;
		accel_.z = 0;
	}

	/**
	 * @brief Re-randomize the particle's physical properties (drag, mass) from
	 * its shared tuning and clear accumulated acceleration.
	 *
	 * Note: this does NOT seed velocity — use spawn() for a full (re)spawn that
	 * also gives the particle its initial terminal velocity.
	 */
	void reset()
	{
		physical_.drag = rng_class.get_rand_float(tuning_->drag_min, tuning_->drag_max);
		physical_.mass = rng_class.get_rand_float(tuning_->mass_min, tuning_->mass_max);
		physical_.inv_weight = 1.0f / physical_.mass;
		// Clear the accumulator. (The old code read each axis back after writing
		// it — `accel_.x = accel_.y * 0.01f; accel_.y = accel_.x * 0.01f;` — which
		// just propagated stale/garbage values.)
		accel_.x = 0.0f;
		accel_.y = 0.0f;
		accel_.z = 0.0f;
	}

	/**
	 * @brief Full (re)spawn: re-randomize physical properties then seed velocity.
	 *
	 * Used both when a particle is first created and when it goes out of bounds
	 * and is recycled. The velocity is seeded ALONG the segment's drift resultant
	 * (gravity + any uniform wind), which is the same vector the spawn edge is
	 * computed from — so a particle enters already moving in the direction it will
	 * actually travel, instead of spawning at rest on an edge and visibly ramping
	 * up to match the wind. It also never carries stale velocity from a prior life.
	 *
	 * @param drift The combined uniform-force resultant (see
	 *              DisplaySegProperties::get_drift_resultant()).
	 */
	void spawn(const ForceLike &drift)
	{
		reset();
		setup_initial(drift, tuning_->spawn_vx, tuning_->spawn_vy);
	}

	~ParticleLike() = default;

	void set_positions(const XYZ &coords)
	{
		positions_ = coords;
	}

	XYZ &get_positions()
	{
		return positions_;
	}

	Acceleration &get_acceleration()
	{
		return accel_;
	}

	Physical &get_physical()
	{
		return physical_;
	}

	[[nodiscard]] std::pair<pimoroni::Point, pimoroni::Point> calc_length() const
	{
		const int16_t end_x = (positions_.x + (velocities_.x * 0.03f));
		const int16_t end_y = (positions_.y + (velocities_.y * 0.03f));
		const int16_t start_x = positions_.x;
		const int16_t start_y = positions_.y;
		return {pimoroni::Point(start_x, start_y), pimoroni::Point(end_x, end_y)};
	}

	/**
	 * @brief Seed the particle's velocity to terminal velocity along a force
	 *        vector, plus an optional per-type spawn bias.
	 *
	 * The velocity is aligned with @p drift (x_dir/y_dir), so its angle matches
	 * the net direction the particle will drift. Passing the combined uniform-force
	 * resultant (rather than gravity alone) is what makes wind-blown particles
	 * spawn already travelling at the wind's angle instead of ramping into it.
	 *
	 * @param drift Force vector to align the initial velocity with (gravity, or
	 *              the combined uniform-force resultant).
	 * @param extra_vx Additional x-velocity at spawn (per-type bias from tuning).
	 * @param extra_vy Additional y-velocity at spawn.
	 */
	void setup_initial(const ForceLike &drift, const float extra_vx = 0.f, const float extra_vy = 0.f)
	{
		accel_.x = 0;
		accel_.y = 0;
		accel_.z = 0;
		velocities_.x = (2.0f * physical_.mass * drift.x_dir / physical_.drag) + extra_vx;
		velocities_.y = (2.0f * physical_.mass * drift.y_dir / physical_.drag) + extra_vy;
		velocities_.z = 0.0f;
	}
};

#endif
