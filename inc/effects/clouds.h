#ifndef CLOUDS_H
#define CLOUDS_H

#include "display/weather_effect_base.h"

namespace weather
{

    /**
     * @brief Clouds effect - renders cloud coverage
     *
     * This is a placeholder implementation. Could be enhanced with:
     * - Actual cloud sprites/shapes
     * - Movement/animation
     * - Variable density based on cloud_cover percentage
     */
    class CloudsEffect : public WeatherEffectBase
    {

    private:
        float cloud_cover_;

    public:
        CloudsEffect(DisplaySegProperties seg_properties, float cloud_cover)
            : WeatherEffectBase(seg_properties), cloud_cover_(cloud_cover)
        {
        }

        void update_particles() override
        {
            // No particles to update for static clouds
            // Could add cloud movement here
        }

        void draw(pimoroni::PicoZGraphics &graphics) override
        {
            // Simple cloud representation: semi-transparent gray overlay
            // TODO: Implement actual cloud rendering
            // For now, this is a placeholder
        }

        void stop() override
        {
            // Nothing to clean up
        }
    };

} // namespace weather

#endif // CLOUDS_H
