#ifndef SEG_GRAV_H
#define SEG_GRAV_H

#include "pico/stdlib.h"
#include "segment_force.h"

class GravityProperties : public Force
{

    friend class DebugConsole;

public:

    /**
     * @brief Construct a new Gravity Properties object.
     *
     */
    GravityProperties(const float dir, const float magnitude) : Force(dir, magnitude)
    {}

    /**
     * @brief Updates the internal vectors used in particle movement.
     *
     * @param grav_direction
     * @param grav_magnitude
     */
    void update_gravity(float grav_direction, float grav_magnitude)
    {
        dir_ = std::clamp(grav_direction, 0.0f, 360.0f);
        mag_ = std::clamp(grav_magnitude, -10.0f, 10.0f);
        update_vector();
    }

};

#endif