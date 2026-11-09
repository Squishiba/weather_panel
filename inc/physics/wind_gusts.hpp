#ifndef SEG_WIND_H
#define SEG_WIND_H

#include "../utility/helpers_rand.hpp"
#include "etl/vector.h"

#include "physics/gravity.hpp"

#include "physics/wind_field_force.hpp"
#include "particles/particle_base.hpp"
#include "display/segment/segment_properties.hpp"  // DisplaySegProperties
#include "pico/time.h"

class WindGust : public ForceLike, public ParticleLike
{

    friend class DebugConsole;
    friend class WeatherEffectBase;

protected:
    static constexpr float kBaseWindDir = 90.0f;
    static constexpr float kUpperWindDir = kBaseWindDir + 25.0f;
    static constexpr float kLowerWindDir = kBaseWindDir - 25.0f;
    static constexpr int16_t kWindBoundExpansion = 10;
    static constexpr uint16_t radius_span_min = 15;
    static constexpr uint16_t radius_span_max = 25;

    DisplaySegProperties &seg_properties_;

    ForceEdgeMap wind_spawn_span_1; // Points where wind is allowed to spawn.
    ForceEdgeMap wind_spawn_span_2;

    float intensity_factor_;
    float inv_radius2_;

    uint32_t wind_chance_;
    uint32_t respawn_counter_ = 0;
    uint32_t respawn_set_;
    uint16_t radius_;
    uint16_t radius2_;

    bool is_alive_;
    bool is_reset_ = false;

    /**
	 * @brief Generate a new wind centerpoint.
	 *
	 * WindGust-specific respawn — unrelated to ParticleLike::reset() (which is no
	 * longer virtual), so this is a plain hiding method, not an override.
	 */
    void reset()
    {
        // Don't need a lifetime, the center of the wind gust will travel across the field of view and then reset / eventually respawn.
        mag = rng_class.get_rand_float(intensity_factor_ * 0.9f, intensity_factor_ * 1.3f); // Calculate how strong this gust of wind will be
        dir_ = rng_class.get_rand_float(kLowerWindDir, kUpperWindDir);
        is_alive_ = true;
        radius_ = rng_class.get_rand_u32(radius_span_min, radius_span_max);
        radius2_ = radius_ * radius_;
        inv_radius2_ = 1.0f / radius2_;
        velocities_.x = x_dir * rng_class.get_rand_float(65.0f, 80.0f);
        velocities_.y = y_dir * rng_class.get_rand_float(65.0f, 80.0f);
        update_vector();
    }

    void p_wind_move(const float dt)
    {
        positions_.x += velocities_.x * ((positions_.z) * 1.2f) * dt;
        positions_.y += velocities_.y * ((positions_.z) * 1.2f) * dt;
        positions_.z = std::clamp(positions_.z + (velocities_.z), 0.2f, 1.0f);
    }

public:
    static inline uint8_t num_gusts = 0;

    explicit WindGust(DisplaySegProperties &seg_properties) : ForceLike(0, 0), ParticleLike(), seg_properties_(seg_properties)
    {
        num_gusts++;
        intensity_factor_ = seg_properties.get_intensity() * 15.0f;
        WindGust::reset();
        wind_chance_ = (INT32_MAX - 1) * (intensity_factor_ * 0.25f);
        respawn_set_ = rng_class.get_rand_u32(2000, 3500);
        auto &bounds = seg_properties.get_seg_bounds();
        wind_spawn_span_1.map_force_to_perpendicular(bounds, seg_properties_.get_gravity(), 15);
    }

    ~WindGust() override {}

    void apply_force(Physical &phys, XYZ &coords, Acceleration &accel) override // from ForceLike
    {
        if (!is_alive_) return; // Dormant gust waiting to respawn — exerts no force.

        // Signed offset of the particle from the gust centre.
        const float dis_x = coords.x - positions_.x;
        const float dis_y = coords.y - positions_.y;
        const float dis_z = coords.z - positions_.z;

        // Distance squared from centre (sign-independent).
        const float dist_sq = dis_x * dis_x + dis_y * dis_y;

        // Only apply force within the gust radius.
        if (dist_sq > radius2_) return;

        // Falloff: strongest at the centre, fading to zero at the radius, then
        // scaled by how close in depth the particle sits to the gust.
        const float depth_factor = std::abs(dis_z);
        const float dist_factor = (1.0f - (dist_sq * inv_radius2_)) * depth_factor;
        const float strength = mag * dist_factor * 0.1f;

        // Rotational (swirl) push: perpendicular to the radial vector, i.e.
        // (-dis_y, dis_x). Using the SIGNED offsets makes particles circulate
        // around the centre. The old code used abs()'d offsets and fed accel back
        // into itself, so every particle was shoved the same way instead.
        accel.x += -dis_y * strength;
        accel.y += dis_x * strength;
    }

    // Advance the gust's motion/respawn state. This overrides ForceLike::update
    // (the 1-arg force tick), NOT ParticleLike's 2-arg update — the segment force
    // loop drives forces through a ForceLike pointer, so the motion logic has to
    // live here to actually run each frame.
    void update(const float dt) override // from ForceLike
    {
        if (is_alive_ == false)
        {
            respawn_counter_++;
            if (is_reset_ == false)
            {
                velocities_.x = 0.0f;
                velocities_.y = 0.0f;
                velocities_.z = 0.0f;
                is_reset_ = true;
            }
            if (respawn_counter_ >= respawn_set_)
            {
                reset();
                respawn_counter_ = 0;
                is_reset_ = false;
                respawn_set_ = rng_class.get_rand_u32(2000, 3400);
            }
        }

        else if (is_alive_ == true)
        {
            p_wind_move(dt);
            if (seg_properties_.is_particle_oob(positions_))
            {
                is_alive_ = false;
            }
        }
    }
};

#endif
