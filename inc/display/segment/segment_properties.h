#ifndef SEG_PROP_H
#define SEG_PROP_H

#include "segment_geometry.h"

#include "physics/particle_physics.h"
#include "physics/gravity.h"
#include "physics/force_handler.h"
#include "force_edge_map.h"

#include "particles/particle_spawn_handler.h"

#include <etl/vector.h>

#include <cstdint>

class DisplaySegProperties
{

    friend class WeatherDisplayHandler;

protected:
    static constexpr int16_t kBoundsExpansion = 30;                            // how far out from nominal segment boundaries can particles exist before reset.
    static constexpr int16_t kSpawnBoundsExpansion = (kBoundsExpansion / 1.5); // how far out from the nominal segment spawning line can particles spawn?

    static constexpr float kInitGravDir = 180.0f; // Initial gravity direction vector.
    static constexpr float kInitGravMag = 9.8f;   // Initial Gravity magnitude value.

    BoundLimits seg_bounds_; // The drawing box of the segment.
    Oob_Limits oob_limits_;  // The outer box where, once crossed, particles are reset.
    Gravity gravity_;        // The gravity properties of this segment.

    float dt_ = 0;        // refresh rate.
    float intensity_ = 0; // weather intensity factor

    uint8_t segment_number_; // Which segment number is this?

public:
    SegmentWideForceHandler force_handler; // Owns the segment-wide forces (gravity + field/local forces).
    ParticleSpawnHandler spawn_handler;    // Handles the spawn locations and directions of particles.

    /**
	 * @brief Construct a new Display Seg Properties object
	 *
	 * @param x_start x axis start point.
	 * @param x_end x axis end point.
	 * @param y_start y axis start point.
	 * @param y_end  y axis end point.
	 * @param segment_num the segment index
	 */
    DisplaySegProperties(const int16_t x_start, const int16_t x_end, const int16_t y_start, const int16_t y_end, const uint8_t segment_num)
        : seg_bounds_(x_start, x_end, y_start, y_end), oob_limits_(x_start, x_end, y_start, y_end, kBoundsExpansion), gravity_(kInitGravDir, kInitGravMag), segment_number_(segment_num),
          force_handler(gravity_), spawn_handler(gravity_, seg_bounds_, oob_limits_)
    {
    }

    DisplaySegProperties(const RectMod &segment_bounds, const uint8_t segment_num)
        : seg_bounds_(segment_bounds), oob_limits_(segment_bounds, kBoundsExpansion), gravity_(kInitGravDir, kInitGravMag), segment_number_(segment_num), force_handler(gravity_),
          spawn_handler(gravity_, seg_bounds_, oob_limits_)
    {
    }

    // Pinned type: this object is self-referential. force_handler borrows gravity_
    // (SegmentWideForceHandler holds a Gravity&), and spawn_handler references
    // seg_bounds_/oob_limits_ (ParticleSpawnHandler holds BoundLimits&/Oob_Limits&).
    // Copying or moving would leave those references dangling into the source
    // object's members, so both are deleted — construct each segment in place.
    DisplaySegProperties(const DisplaySegProperties &) = delete;
    DisplaySegProperties &operator=(const DisplaySegProperties &) = delete;
    DisplaySegProperties(DisplaySegProperties &&) = delete;
    DisplaySegProperties &operator=(DisplaySegProperties &&) = delete;

    /**
	 * @brief Checks if the particle is out of bounds by any metric, by at least
	 * kBoundsExpanse larger than the segment's nominal bounds.
	 * @param position Particle position.
	 *
	 * @return true
	 * @return false
	 */
    [[nodiscard]] bool is_particle_oob(const XYZ &position) const
    {
        return !oob_limits_.contains(pimoroni::Point(position.x, position.y));
    }

    /**
	 * @brief Checks if particle is within the drawing bounds of the segment.
	 *
	 * @return true If particle should be drawn.
	 * @return false
	 */
    [[nodiscard]] bool is_particle_in_segment(const XYZ &position) const
    {
        return seg_bounds_.contains_inclusive(pimoroni::Point(position.x, position.y));
    }

    /**
	 * @brief Update this segment's gravity characteristics.
	 * Also updates appropriate spawning locations.
	 *
	 * @param grav_dir
	 * @param grav_mag
	 */
    void update_gravity(const float grav_dir, const float grav_mag)
    {
        gravity_.set_dir_and_mag(grav_dir, grav_mag);
        refresh_spawn_edge();
    }

    // Bridge: recompute the spawn edge from the current uniform forces.
    // Call whenever the uniform forces change (gravity, wind field on/off).
    void refresh_spawn_edge()
    {
        spawn_handler.update_spawn_edge(force_handler.calculate_combined());
    }

    Gravity &get_gravity()
    {
        return gravity_;
    }

    /**
	 * @brief Resultant of the segment's uniform forces (gravity + any field wind).
	 *
	 * This is the average direction and strength that particles drift. Both the
	 * spawn edge (see refresh_spawn_edge) and the particle spawn velocity are
	 * derived from it, so a particle enters already moving ALONG the resultant
	 * instead of spawning at rest on an edge and visibly ramping up to match the
	 * wind. Uniform forces only — localized forces (gusts) are deliberately
	 * excluded from the drift.
	 *
	 * @return A ForceLike whose x_dir/y_dir are the summed uniform-force vector.
	 */
    [[nodiscard]] ForceLike get_drift_resultant() const
    {
        return force_handler.calculate_combined();
    }

    [[nodiscard]] const float &get_intensity() const
    {
        return intensity_;
    }

    void set_intensity(const float &intensity)
    {
        intensity_ = intensity;
    }

    void set_dt(const float dt)
    {
        dt_ = dt;
    }

    float &get_dt()
    {
        return dt_;
    }

    const float &get_grav_mag()
    {
        return gravity_.get_magnitude();
    }

    bool is_particle_on_ground(const XYZ &position) const
    {
        return spawn_handler.floor_span.is_particle_on_edge(position);
    }

    // Restore spawn edge to match the actual uniform forces (call when wind stops).
    void reset_spawn_direction()
    {
        refresh_spawn_edge();
    }

    [[nodiscard]] auto get_spawn_coords() const
    {
        auto &span_of = spawn_handler.spawn_span;
        auto coords = span_of.get_random_edge_coords();
        return coords;
    }
    [[nodiscard]] auto get_spawn_edge_span() const
    {
        return spawn_handler.spawn_span.edge_span_;
    }

    [[nodiscard]] auto get_spawn_span() const
    {
        return spawn_handler.spawn_span;
    }

    [[nodiscard]] const auto &get_oob_limits() const
    {
        return oob_limits_;
    }

    [[nodiscard]] const auto &get_seg_bounds() const
    {
        return seg_bounds_;
    }
};

#endif
