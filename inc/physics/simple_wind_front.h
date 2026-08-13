//
// Created by Jared on 6/19/2026.
//

#ifndef WEATHER_SIMPLE_WIND_FRONT_H
#define WEATHER_SIMPLE_WIND_FRONT_H

#include "../utility/helpers_rand.h"
#include "forcelike.h"
#include "../utility/trig_funcs.h"

class SimpleWindFront : public ForceLike
{

    // Mean of the depth-coupling factor in apply_force() (2 + 3z over z in
    // 0..1), used to publish an honest average push into the spawn drift.
    static constexpr float kMeanDepthCoupling = 3.5f;

    static inline std::array pre_gen_sin_ = generate_sine_table<2048>();
    float sweep_min_x_ = -32.0f; // where a rightward front starts (px, off-segment)
    float sweep_max_x_ = 160.0f; // where a leftward front starts
    float dir_sign_ = 1.0f;      // sweep direction: +1 right, -1 left
    float wavefront_x_ = 0.f;
    float speed_ = 0.f;    // px / second
    float strength_ = 0.f; // current force magnitude
    float peak_ = 0.f;     // target peak
    float life_ = 0.f;     // seconds remaining
    float total_life_ = 0.f;
    float idle_timer_ = 2.0f; // delay before first gust

public:
    // Initialise a fully-formed gust so the very first update() has a valid
    // (non-zero) total_life_ — otherwise the envelope's life_/total_life_ divides
    // by zero and produces NaN.
    //
    // The sweep bounds are where a front spawns (a little off either edge so it
    // arrives rather than pops in). Defaults match the historical full-panel
    // values; effects should pass their own segment's bounds, e.g.
    //   spawn_local_force<SimpleWindFront>(bounds.x - 32.0f, bounds.x_end + 32.0f);
    explicit SimpleWindFront(const float sweep_min_x = -32.0f, const float sweep_max_x = 160.0f) : ForceLike(0.0f, 0.0f), sweep_min_x_(sweep_min_x), sweep_max_x_(sweep_max_x)
    {
        reset();
    }

    /**
	 * @brief Reset the wind front, back to "alive" state.
	 * @param None.
	 * @retval None.
	 */
    void reset()
    {
        dir_sign_ = (rng_class.get_rand_u32() & 1) ? -1.0f : 1.0f;
        speed_ = rng_class.get_rand_float(20.0f, 40.f);
        peak_ = rng_class.get_rand_float(1.0f, 3.f);
        total_life_ = rng_class.get_rand_float(4.0f, 20.0f);
        life_ = total_life_;
        strength_ = 0.0f;
        wavefront_x_ = (dir_sign_ > 0.0f) ? sweep_min_x_ : sweep_max_x_;
        is_alive = true;
        idle_timer_ = rng_class.get_rand_float(2.0f, 7.0f);
    }

    void update(const float dt) override
    {
        if (!is_alive)
        {
            idle_timer_ -= dt;
            if (idle_timer_ <= 0.0f) reset();
            return;
        }
        wavefront_x_ += dir_sign_ * speed_ * dt;
        life_ -= dt;
        // Smooth trapezoid envelope: ramp up over first 30%, hold, decay over last 30%
        const float progress = 1.0f - (life_ / total_life_); // 0.0f to 1.0f
        const uint32_t p_val = std::min(static_cast<size_t>(roundf(progress * pre_gen_sin_.size())), pre_gen_sin_.size() - 1);
        const float env = pre_gen_sin_[p_val];
        strength_ = peak_ * env;

        // Publish the current mean push into the base x_dir/y_dir. The front is
        // registered as a UNIFORM force purely so calculate_combined() (the spawn
        // edge + spawn velocity input) sees this contribution — the spawn edge
        // then tracks each sweep (particles enter from the upwind side while the
        // gust blows) and relaxes back to gravity when it dies. apply_force()
        // below stays position-dependent; only this published average is uniform.
        x_dir = dir_sign_ * strength_ * kMeanDepthCoupling;
        y_dir = 0.0f;

        if (life_ <= 0.0f)
        {
            is_alive = false;
            strength_ = 0.0f;
            x_dir = 0.0f;
        }
    }

    void apply_force(Physical &phys, XYZ &coords, Acceleration &accel) override
    {
        if (!is_alive || strength_ == 0.0f) return;
        apply_one(phys, coords, accel);
    }

    /// Force-major path: the dormant-front test runs ONCE per range (fronts
    /// idle most of the time, so most frames this returns immediately), and
    /// the per-particle body inlines with the wavefront state in registers.
    void apply_range(const ParticleRange &r) override
    {
        if (!is_alive || strength_ == 0.0f) return;
        for (size_t i = 0; i < r.count; ++i)
            apply_one(r.phys_at(i), r.pos_at(i), r.acc_at(i));
    }

private:
    // Single per-particle body shared by both entry points, so the scalar and
    // batched paths can never drift apart.
    inline void apply_one(Physical &phys, XYZ &coords, Acceleration &accel) const
    {
        // How far behind the wavefront is this particle?
        // Positive = particle has been hit; 0 = not yet reached.
        const float behind = (dir_sign_ > 0.0f) ? (wavefront_x_ - coords.x) : (coords.x - wavefront_x_);
        // Soft leading edge: ramp from 0 to full over `falloff` pixels.
        constexpr float falloff = 80.f;
        const float t = std::clamp(behind / falloff, 0.0f, 1.0f);
        if (t == 0.0f) return;
        // Lighter particles (inv_weight = 1/mass) respond more strongly, and
        // nearer ones (z -> 1) are hit harder for parallax. The old factor was
        // (coords.z / (coords.z * 0.2f)) — the z's cancel, so it was a constant
        // 5.0 and the intended depth coupling never happened. This keeps the
        // same 5.0 peak for the nearest particles but scales down with depth.
        accel.x += (dir_sign_ * strength_ * t * phys.inv_weight) * (2.0f + 3.0f * coords.z);
    }
};


#endif //WEATHER_SIMPLE_WIND_FRONT_H