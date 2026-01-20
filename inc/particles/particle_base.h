#ifndef PARTICLE_BASE_H
#define PARTICLE_BASE_H

#include "display/segment/segment_properties.h"
#include "particle_properties.h"

class ParticleBase
	{
protected:
	DisplaySegProperties &seg_properties_;
	Velocity velocities_; // The particle's velocities in x, y, and z directions.
	Position positions_; // The immediate particle's position in x, y and z.
	Acceleration accel_;

	virtual void update_physics() = 0;
	virtual void reset() = 0;

public:
	virtual void update() = 0;

	virtual ~ParticleBase() = default;

	ParticleBase(DisplaySegProperties &seg_properties) : seg_properties_(seg_properties) {}

	[[nodiscard]] bool is_drawable() const { return seg_properties_.is_particle_in_segment(positions_); }

	Position &get_positions()
		{
		return positions_;
		}

	Acceleration &get_acceleration()
		{
		return accel_;
		}

	[[nodiscard]] std::pair<pimoroni::Point, pimoroni::Point> calc_length() const
		{
		int16_t end_x = (positions_.x + (velocities_.x * 2.0f));
		int16_t end_y = (positions_.y + (velocities_.y * 2.0f));
		int16_t start_x = positions_.x;
		int16_t start_y = positions_.y;
		return {pimoroni::Point(start_x, start_y), pimoroni::Point(end_x, end_y)};
		}
	};





/**
 * @brief Generate the particle's spawn positions, given the span.
 * @param spawn_span The given spans to try and spawn on.
 * @param positions The particle position struct to place the new positions in.
 */
void set_spawn_point(const std::vector<Range> &spawn_span, Position &positions)
	{
	float r = get_rand_float();
	float acc = 0.0f;
	for (auto &spawn: spawn_span)
		{
		acc += spawn.weight;
		if (r <= acc)
			{
			positions = spawn.get_random_point_in_range();
			}
		}
	}

/**
     * @brief Generates the initial particle velocities based off of particle weight
     * and the given gravity information.
     *
     */
void set_initial_velocities(Velocity &velocities, const float &particle_weight, const GravityProperties &gravity)
	{
	velocities.x = particle_weight * gravity.x_dir_;
	velocities.y = particle_weight * gravity.y_dir_;
	velocities.z = 0.0f; // No Z gravity currently
	}


#endif
