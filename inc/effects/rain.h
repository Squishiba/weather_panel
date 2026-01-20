#ifndef RAIN_H
#define RAIN_H

#include "display/weather_effect_base.h"
#include "particles/particle.h"
#include "helpers_rand.h"

#include <list>
#include <cmath>
#include <memory>

namespace weather
{

    /**
     * @brief Rain weather effect - renders falling rain droplets
     */
    class RainEffect : public WeatherEffectBase
    {

    private:
        bool freezing_; // True for freezing rain
        Color draw_color_ = kBlue;

    public:
        RainEffect(DisplaySegProperties &seg_properties, bool freezing = false) : WeatherEffectBase(seg_properties), freezing_(freezing)
        {
            // Configure based on rain type
            if (freezing_)
            {
                draw_color_ = kCyan;
            }
            else
            {
                draw_color_ = kBlue;
            }
            max_particles_ = roundf(seg_properties_.get_seg_bounds().w * 0.9f * seg_properties_.get_intensity());
        }

        void update_particles() override
        {
            // Spawn new drops based on spawn rate
            if (particles_.size() < static_cast<size_t>(max_particles_) && get_rand_float() < spawn_rate_)
            {
                particles_.push_back(std::make_unique<Rain>(seg_properties_));
            }
            // Update all particles
            for (const auto &drop : particles_)
            {
                drop->update();
            }
        }

        void draw(pimoroni::PicoZGraphics &graphics) override
        {
            // Draw each raindrop as a line aligned with gravity direction
            for (const auto &drop : particles_)
            {
                if (drop->is_drawable())
                {
                    Position &position = drop->get_positions();
                    graphics.set_pen((position.z * draw_color_.r), (position.z * draw_color_.g), (position.z * draw_color_.b));
                    graphics.set_depth(position.z);
                    // Calculate line endpoint based on gravity direction and drop length
                    auto [start, end] = drop->calc_length();
                    graphics.line(start, end);
                }
            }
        }

        void stop() override
        {
            particles_.clear();
        }
    };

} // namespace weather

#endif // RAIN_H
