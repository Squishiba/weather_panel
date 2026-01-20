#ifndef PARTICLE_H
#define PARTICLE_H

#include "helpers_rand.h"
#include "../display/segment/segment_properties.h"
#include "particle_base.h"

class Particle : public ParticleBase
	{
protected:
	Physical physical_; // The particle's physical properties.

	void update_physics() override
		{
		accel_.x += physical_.gravity_x_constant - (physical_.drag * velocities_.x);
		accel_.y += physical_.gravity_y_constant - (physical_.drag * velocities_.y);

		velocities_.x += accel_.x * seg_properties_.get_dt();
		velocities_.y += accel_.y * seg_properties_.get_dt();

		positions_.x += velocities_.x * ((positions_.z) * 1.2f) * seg_properties_.get_dt();
		positions_.y += velocities_.y * ((positions_.z) * 1.2f) * seg_properties_.get_dt();
		positions_.z = std::clamp(positions_.z + (velocities_.z), 0.2f, 1.0f);

		accel_.x = 0;
		accel_.y = 0;
		}

public:
	Particle(DisplaySegProperties &seg_properties, const float terminal_velocity) : ParticleBase(seg_properties)
		{
		physical_.terminal_velocity = terminal_velocity;
		physical_.drag = seg_properties_.get_grav_mag() / physical_.terminal_velocity;
		}

	void update() override
		{
		update_physics();
		if (seg_properties_.is_particle_oob(positions_))
			{
			reset();
			}
		}
	};

class Rain : public Particle
	{
public:
	explicit Rain(DisplaySegProperties &seg_properties) : Particle(seg_properties, 24.0f)
		{
		Rain::reset();
		}

	void reset() override
		{
		// Weight range: 0.7 - 1.0 (heavy rain drops have high weight)
		physical_.weight = get_rand_float(0.7f, 1.0f);
		const GravityProperties grav = seg_properties_.get_gravity();
		physical_.gravity_x_constant = physical_.weight * grav.x_dir_;
		physical_.gravity_y_constant = physical_.weight * grav.y_dir_;
		set_initial_velocities(velocities_, physical_.weight, seg_properties_.get_gravity());
		set_spawn_point(seg_properties_.get_spawn_ranges(), positions_);
		}
	};

class Snow : public Particle
	{
public:
	explicit Snow(DisplaySegProperties &seg_properties) : Particle(seg_properties, 9.5f)
		{
		Snow::reset();
		}

	// Reset particle with new values...
	void reset() override
		{
		physical_.weight = get_rand_float(0.25f, 0.4f);
		const GravityProperties grav = seg_properties_.get_gravity();
		physical_.gravity_x_constant = physical_.weight * grav.x_dir_;
		physical_.gravity_y_constant = physical_.weight * grav.y_dir_;
		set_initial_velocities(velocities_, physical_.weight, seg_properties_.get_gravity());
		set_spawn_point(seg_properties_.get_spawn_ranges(), positions_);
		}
	};

#endif // PARTICLE_H
