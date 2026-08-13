#ifndef RAIN_H
#define RAIN_H

#include "weather_effect_base.h"
#include "particles/particlelike.h"

namespace weather
{

    class RainEffect : public ParticleEffectBase<RainEffect, Rain>
    {
        static constexpr float kParticleCountMultiplier = 2.5f;

        using particle_type = Rain;

        // Desired particle count; the shared pool caps what's actually granted
        // (see ParticleEffectBase). No local cap — the arena bounds it.
        static uint16_t desired_count(DisplaySegProperties &seg)
        {
            return static_cast<uint16_t>(roundf(seg.get_seg_bounds().w * seg.get_intensity()) * kParticleCountMultiplier);
        }

    public:
        // with_front: occasional gust fronts (on by default on hardware; the
        // desktop sim passes its checkbox through so it can be A/B'd).
        explicit RainEffect(DisplaySegProperties &seg_properties, const bool freezing = false, const bool with_front = true) : ParticleEffectBase(seg_properties, desired_count(seg_properties))
        {
            particle_color_ = freezing ? kCyan : kBlue;

            // Occasional wind front (see snow.h for the full rationale): spawns
            // once, then self-schedules intermittent gust sweeps forever.
            // Uniform, so the spawn edge follows each sweep.
            // Also gated by g_ambient_winds_enabled (console `winds` command),
            // so a gravity-only profiling baseline needs no special build.
            if (with_front && g_ambient_winds_enabled)
            {
                const auto &bounds = seg_properties_.get_seg_bounds();
                spawn_uniform_force<SimpleWindFront>(bounds.x - 32.0f, bounds.x_end + 32.0f);
            }
        }

        // Non-virtual: resolved at compile time via CRTP. Takes Rain& directly
        // so calc_length() needs no virtual dispatch either.
        void draw_effects(PicoZGraphicsBase &graphics, Rain &particle)
        {
            auto [start, end] = particle.calc_length();
            graphics.line(start, end);
        }
    };

} // namespace weather

#endif // RAIN_H
