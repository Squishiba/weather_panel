#ifndef THUNDERSTORM_H
#define THUNDERSTORM_H

#include "display/weather_effect_base.h"
#include "misc.h"
#include "pico/rand.h"

namespace weather
{

    /**
     * @brief Thunderstorm effect - renders occasional lightning flashes
     *
     * This is a placeholder implementation. Can be enhanced with:
     * - Actual lightning bolt drawing
     * - Thunder sound effects
     * - Flash timing/randomization
     */
    class ThunderstormEffect : public WeatherEffectBase
    {
    private:
        uint32_t flash_timer_ = 0;
        bool is_flashing_ = false;
        static constexpr uint32_t FLASH_DURATION = 3;      // frames
        static constexpr uint32_t MIN_FLASH_INTERVAL = 60; // frames between flashes
        float intensity_;

    public:
        ThunderstormEffect(DisplaySegProperties &seg_properties, float intensity) : intensity_(intensity), WeatherEffectBase(seg_properties)
        {
        }

        void update_particles() override
        {
            // Update flash timer
            if (is_flashing_)
            {
                flash_timer_++;
                if (flash_timer_ >= FLASH_DURATION)
                {
                    is_flashing_ = false;
                    flash_timer_ = 0;
                }
            }
            else
            {
                // Randomly trigger a flash
                if (get_rand_32() % MIN_FLASH_INTERVAL == 0)
                {
                    is_flashing_ = true;
                    flash_timer_ = 0;
                }
            }
        }

        void draw(pimoroni::PicoZGraphics &graphics) override
        {
            // Draw a white flash across the segment when lightning strikes
            if (is_flashing_)
            {
                graphics.set_pen(pen_white);
                graphics.set_depth((uint8_t)255);
                // Simple flash effect - fill segment with white
                // TODO: Could draw actual lightning bolts here
                graphics.rectangle(seg_properties_.get_seg_bounds());
            }
        }

        void stop() override
        {
            // Nothing to clean up
        }
    };

} // namespace weather

#endif // THUNDERSTORM_H
