//
// Created by Jared on 2/13/2026.
//

#ifndef WEATHER_BASIC_FORCE_H
#define WEATHER_BASIC_FORCE_H

#include "forcelike.h"

class WindFieldForce : public ForceLike
{

public:
    bool is_swirler = false;

    WindFieldForce(const float dir, const float mag) : ForceLike(dir, mag)
    {
        if (const auto num = rng_class.get_rand_u32(); num > (UINT32_MAX * 0.75f))
        {
            is_swirler = true;
        }
    }

    WindFieldForce() : ForceLike(0, 0) {}

    ~WindFieldForce() override = default;

    void apply_force(Physical &phys, XYZ &coords, Acceleration &accel) override
    {
        // a = F / m. Use the signed components directly; the old
        // `x_dir * x_dir` squared away the sign so a leftward field wrongly
        // pushed particles right.
        accel.x += x_dir * phys.inv_weight;
        accel.y += y_dir * phys.inv_weight;
    }

    /// Force-major path: uniform field, so the loop is just two fused MACs per
    /// particle with the components in registers.
    void apply_range(const ParticleRange &r) override
    {
        for (size_t i = 0; i < r.count; ++i)
        {
            Physical &phys = r.phys_at(i);
            Acceleration &accel = r.acc_at(i);
            accel.x += x_dir * phys.inv_weight;
            accel.y += y_dir * phys.inv_weight;
        }
    }
};

#endif //WEATHER_BASIC_FORCE_H