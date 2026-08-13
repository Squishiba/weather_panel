#ifndef SNOW_H
#define SNOW_H

#include "pico/stdlib.h"
#include "../utility/helpers_rand.h"

#include "weather_effect_base.h"
#include "particles/particlelike.h"

#include <cmath>

namespace weather
{

    class SnowEffect : public ParticleEffectBase<SnowEffect, Snow>
    {
        static constexpr float kParticleCountMultiplier = 8.0f;
        static constexpr float MM_TO_INCH_CONST = 0.0393700787f;
        static constexpr float STICK_PROBABILITY = 0.3f;

        using particle_type = Snow;

        uint16_t accumulation_depth_pixels_ = 0;
        uint16_t current_accumulation_height_ = 0;
        bool is_ice_ = false;

        // Particles this effect would like; the shared pool decides how many it
        // actually gets (see ParticleEffectBase). No local cap — the arena bounds it.
        static uint16_t desired_count(DisplaySegProperties &seg)
            {
            return static_cast<uint16_t>(
                roundf(seg.get_seg_bounds().w * seg.get_intensity() * kParticleCountMultiplier));
            }

    public:

        // with_front: occasional gust fronts (on by default on hardware; the
        // desktop sim passes its checkbox through so it can be A/B'd).
        SnowEffect(DisplaySegProperties &seg_properties, const float accumulation,
                   const bool is_ice = false, const bool with_front = true)
            : ParticleEffectBase(seg_properties, desired_count(seg_properties)), is_ice_(is_ice)
            {
            particle_color_ = kWhite;
            accumulation_depth_pixels_ = depth_to_pixels(accumulation, seg_properties_.get_seg_bounds().y_end);

            // Occasional wind front: self-scheduling (idles 2-7 s, sweeps, dies,
            // re-idles), so spawning it once gives intermittent gust fronts for
            // the effect's whole life. Registered UNIFORM so the mean push it
            // publishes steers the spawn edge during a sweep (particles enter
            // from the upwind side while the gust blows). Sweep bounds start a
            // little off either segment edge so the front arrives rather than
            // popping in.
            // Also gated by g_ambient_winds_enabled (console `winds` command),
            // so a gravity-only profiling baseline needs no special build.
            if (with_front && g_ambient_winds_enabled)
                {
                const auto &bounds = seg_properties_.get_seg_bounds();
                spawn_uniform_force<SimpleWindFront>(bounds.x - 32.0f, bounds.x_end + 32.0f);
                }
            }

        // No draw_effects override — uses the base single-pixel default.

        static uint16_t depth_to_pixels(const float snow_depth, const int16_t display_height)
            {
            const float inches = snow_depth * MM_TO_INCH_CONST;
            const float height_div = (display_height / 4.0f);
            return static_cast<uint16_t>(roundf((inches / height_div) * 100.0f));
            }

        uint16_t get_accumulation_height() const { return current_accumulation_height_; }
        uint16_t get_target_accumulation()  const { return accumulation_depth_pixels_; }
    };

} // namespace weather

#endif // SNOW_H
