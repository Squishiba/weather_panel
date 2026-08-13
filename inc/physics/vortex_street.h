//
// Created by Jared on 7/12/2026.
//

#ifndef WEATHER_VORTEX_STREET_H
#define WEATHER_VORTEX_STREET_H

#include "../utility/helpers_rand.h"
#include "../utility/trig_funcs.h"
#include "forcelike.h"

#include <algorithm>

/**
 * @brief Persistent chain of counter-rotating vortices (non-uniform force).
 *
 * The classic "leaves swirling behind a truck" look: a few immortal vortices
 * drift slowly along a line, alternating spin direction, wrapping back to the
 * start when they leave the far edge. Each vortex applies WindGust's swirl
 * math — a rotational push perpendicular to the radial offset with parabolic
 * falloff — but unlike WindGust they never die, so the swirling is constant.
 * Adjacent counter-rotating pairs hand particles to each other, which is what
 * makes the motion read as true circulation rather than a wavy drift.
 *
 * Cost: per particle, one radius test per vortex (2 subs + 2 muls + compare);
 * the full swirl math only runs for particles actually inside a core.
 *
 * Parallel-safety (parallel_executor.h contract): vortex positions move ONLY
 * in update(dt) (once per frame, core 0, before the particle split);
 * apply_force() is pure w.r.t. the force, so both cores may call it freely.
 *
 * Register with spawn_local_force<VortexStreet>(...) — position-dependent,
 * so it must stay non-uniform (never biases the spawn edge).
 */
class VortexStreet : public ForceLike
{

public:
    static constexpr uint8_t kMaxVortices = 4;

private:
    static inline std::array full_sin_ = generate_full_sine_table<1024>();

    struct Vortex
    {
        float x;
        float y;
        float spin; // signed strength: alternating +/- along the chain
    };

    Vortex vortices_[kMaxVortices];
    uint8_t count_;

    // Geometry (fixed at spawn)
    float line_y_; // resting chain line (absolute px)
    float x_min_;  // wrap bounds (px, a little outside the segment)
    float x_max_;
    float radius2_; // vortex core radius^2
    float inv_radius2_;
    float drift_speed_; // px/s along +x (sign allowed)

    // Animated state (update() only)
    float bob_phase_ = 0.0f;
    float bob_speed_; // cycles/s (slow)
    float bob_amp_;   // px of vertical bobbing

public:
    /**
	 * @param line_y      Chain line, absolute framebuffer y (px).
	 * @param x_min       Left wrap bound (px) — start a little off-segment.
	 * @param x_max       Right wrap bound (px).
	 * @param radius      Vortex core radius (px).
	 * @param spin_mag    Swirl strength (sign alternates per vortex).
	 * @param drift_speed Chain drift along x, px/s (negative drifts left).
	 * @param count       Vortices in the chain (clamped to kMaxVortices).
	 */
    VortexStreet(const float line_y, const float x_min, const float x_max, const float radius, const float spin_mag, const float drift_speed, const uint8_t count = 3)
        : ForceLike(0.0f, 0.0f), count_(std::clamp<uint8_t>(count, 1, kMaxVortices)), line_y_(line_y), x_min_(x_min), x_max_(std::max(x_max, x_min + 1.0f)), radius2_(radius * radius),
          inv_radius2_(1.0f / std::max(radius * radius, 1.0f)), drift_speed_(drift_speed), bob_speed_(rng_class.get_rand_float(0.03f, 0.08f)), bob_amp_(radius * 0.4f)
    {
        // Evenly space the chain across the span, alternating spin so adjacent
        // vortices counter-rotate (that hand-off is the "street" look).
        const float span = x_max_ - x_min_;
        for (uint8_t i = 0; i < count_; ++i)
        {
            vortices_[i].x = x_min_ + span * (static_cast<float>(i) + 0.5f) / count_;
            vortices_[i].y = line_y_;
            vortices_[i].spin = (i & 1) ? -spin_mag : spin_mag;
        }
        is_alive = true;
    }

    /// Drift the chain and bob it vertically. Once per frame on core 0 only.
    void update(const float dt) override
    {
        bob_phase_ += bob_speed_ * dt;
        if (bob_phase_ >= 1.0f) bob_phase_ -= 1.0f;

        const float span = x_max_ - x_min_;
        for (uint8_t i = 0; i < count_; ++i)
        {
            vortices_[i].x += drift_speed_ * dt;
            // Wrap around the span, preserving spacing.
            if (vortices_[i].x > x_max_) vortices_[i].x -= span;
            if (vortices_[i].x < x_min_) vortices_[i].x += span;
            // Stagger the bob per vortex (quarter-cycle apart) so the chain
            // undulates instead of moving as a rigid bar.
            vortices_[i].y = line_y_ + bob_amp_ * wave_lookup(full_sin_, bob_phase_ + 0.25f * static_cast<float>(i));
        }
    }

    void apply_force(Physical &phys, XYZ &coords, Acceleration &accel) override
    {
        apply_one(phys, coords, accel);
    }

    /// Force-major path: one dispatch per range; the vortex array and radii
    /// stay register/cache-hot across the whole particle run.
    void apply_range(const ParticleRange &r) override
    {
        for (size_t i = 0; i < r.count; ++i)
            apply_one(r.phys_at(i), r.pos_at(i), r.acc_at(i));
    }

private:
    // Single per-particle body shared by both entry points, so the scalar and
    // batched paths can never drift apart.
    inline void apply_one(Physical &phys, XYZ &coords, Acceleration &accel) const
    {
        for (uint8_t i = 0; i < count_; ++i)
        {
            const float dx = coords.x - vortices_[i].x;
            const float dy = coords.y - vortices_[i].y;
            const float dist2 = dx * dx + dy * dy;
            if (dist2 > radius2_) continue;

            // Parabolic falloff from the core, with depth coupling so near
            // particles (z -> 1) circulate visibly harder than far ones.
            const float falloff = (1.0f - dist2 * inv_radius2_) * (0.4f + 0.6f * coords.z);
            const float strength = vortices_[i].spin * falloff * phys.inv_weight;

            // Rotational push: perpendicular to the radial offset (-dy, dx),
            // so particles orbit the core instead of being blown through it.
            accel.x += -dy * strength;
            accel.y += dx * strength;
        }
    }
};

#endif // WEATHER_VORTEX_STREET_H
