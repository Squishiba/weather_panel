#ifndef WEATHER_EFFECT_BASE_H
#define WEATHER_EFFECT_BASE_H

#include "particles/particle_base.h"
#include "segment/segment_properties.h"
#include "z_buffer.h"

#include <list>
#include <memory>

#include "particles/wind_gusts.h"

class WindGust;
/**
 * @brief Base class for all weather effects (rain, snow, clouds, thunderstorms, etc.)
 *
 * This provides a common interface that all weather effects must implement.
 * The DisplaySegment will call update_particles() and draw() on each effect.
 */
class WeatherEffectBase
{

protected:

    DisplaySegProperties &seg_properties_;               // Reference to the segment properties.
    WindGust windgust_;
    float spawn_rate_;                                   // How quickly new particles are spawned.
    uint16_t max_particles_;                             // The maximum number of particles for this segment.
    std::list<std::unique_ptr<ParticleBase>> particles_; // List of particles.

public:
    WeatherEffectBase(DisplaySegProperties &seg_props) : seg_properties_(seg_props), windgust_(seg_props), spawn_rate_((seg_props.get_intensity() / 3.0f))
    {
        windgust_.reset();
    }

    virtual ~WeatherEffectBase() = default;


    /**
     * @brief Update particle positions, spawn new particles, remove old ones.
     * Called once per frame before drawing.
     */
    virtual void update_particles() = 0;

    /**
     * @brief Draw the weather effect to the display.
     * @param graphics Reference to the PicoGraphics object for drawing
     */
    virtual void draw(pimoroni::PicoZGraphics &graphics) = 0;

    /**
     * @brief Stop the effect (cleanup, stop background threads if any).
     */
    virtual void stop() {};

    /**
     * @brief Get the particle count for this segment.
     *
     * @return uint16_t
     */
    [[nodiscard]] uint16_t get_particle_count() const
    {
        return particles_.size();
    }
};

#endif // WEATHER_EFFECT_BASE_H
