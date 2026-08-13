//
// Created by Jared on 2/7/2026.
//

#ifndef WEATHER_GRAVITY_H
#define WEATHER_GRAVITY_H

#include "forcelike.h"

class Gravity : public ForceLike
	{

	// Single per-particle body shared by both entry points (see apply_range),
	// so the scalar and batched paths can never drift apart.
	inline void apply_one(Physical &phys, Acceleration &accel) const
		{
		accel.x += phys.mass * x_dir;
		accel.y += phys.mass * y_dir;
		}

public:
	Gravity(const float dir, const float mag) : ForceLike(dir, mag)
	{}

	void apply_force(Physical &phys, XYZ &coords, Acceleration &accel) override
		{
		apply_one(phys, accel);
		}

	/// Force-major path: one dispatch for the whole range; the loop body
	/// inlines with x_dir/y_dir held in registers.
	void apply_range(const ParticleRange &r) override
		{
		for (size_t i = 0; i < r.count; ++i)
			apply_one(r.phys_at(i), r.acc_at(i));
		}

	};

struct remove_deleter
	{
	void operator()(Gravity*){}
	};

#endif //WEATHER_GRAVITY_H