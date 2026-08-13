//
// Created by Jared on 1/19/2026.
//

#ifndef WEATHER_SEGMENT_BASE_FORCE_H
#define WEATHER_SEGMENT_BASE_FORCE_H

#include "particle_physics.h"
#include "../utility/fixed_unique_factory.h"
#include "dsp/controller_functions.h"

#include <cstddef>

/**
 * @brief Type-erased view over a contiguous run of particles (AoS layout).
 *
 * Forces iterate particles WITHOUT knowing the concrete particle type: the
 * three component pointers reference the first particle's members, and each
 * step advances all of them by the same byte stride (sizeof the concrete
 * particle). This is what makes force-major batching possible — the force
 * handler hands each force ONE of these per frame instead of making a virtual
 * apply_force() call per particle per force.
 */
struct ParticleRange
{
    Physical *phys;    ///< first particle's Physical
    XYZ *pos;          ///< first particle's position
    Acceleration *acc; ///< first particle's acceleration accumulator
    size_t stride;     ///< sizeof(ConcreteParticle) — byte step between elements
    size_t count;      ///< number of particles in the range

    Physical &phys_at(const size_t i) const
    {
        return *reinterpret_cast<Physical *>(reinterpret_cast<char *>(phys) + i * stride);
    }
    XYZ &pos_at(const size_t i) const
    {
        return *reinterpret_cast<XYZ *>(reinterpret_cast<char *>(pos) + i * stride);
    }
    Acceleration &acc_at(const size_t i) const
    {
        return *reinterpret_cast<Acceleration *>(reinterpret_cast<char *>(acc) + i * stride);
    }
};

class ForceLike
{
protected:
    float dir_ = 0;
    float prev_dir_ = 999999;

public:
    float x_dir = 0; // The x component of the force.
    float y_dir = 0; // The y component of the force.

    float x_base = 0;
    float y_base = 0;

    float mag = 0;
    bool is_alive = true;


    ForceLike(const float dir, const float mag) : dir_(dir), mag(mag)
    {
        update_vector();
    }

    ForceLike() {}

    virtual ~ForceLike() = default;

    /**
	 * @brief Updates the internal magnitude value. X and Y dir
	 * magnitudes are also updated in tandem.
	 * @param new_mag New magnitude value to be applied.
	 */
    void set_magnitude(const float new_mag)
    {
        mag = new_mag;
        update_vector();
    }

    virtual void update(const float dt) {}

    /**
		 * @brief Update the x and y dir vectors of this force.
		 * If dir_ has not changed since this function was last called, skips
		 * recalculating sin and cos and reuses the old values to save on compute.
		 */
    void update_vector()
    {

        if (dir_ != prev_dir_)
        {
            arm_sin_cos_f32(dir_ - 90.0f, &y_base, &x_base);
        };

        x_dir = x_base * mag;
        y_dir = y_base * mag;

        prev_dir_ = dir_;
    }

    /**
	 *
	 * @return Magnitude of the force.
	 */
    float &get_magnitude()
    {
        return mag;
    }

    float &get_dir()
    {
        return dir_;
    }

    virtual void apply_force(Physical &phys, XYZ &coords, Acceleration &accel)
    {
        // a = F / m. x_dir/y_dir are the already-signed force components, so use
        // them directly — squaring (the old `x_dir * x_dir`) discarded the sign
        // and made every force push in the +x/+y direction regardless of dir.
        accel.x += x_dir * phys.inv_weight;
        accel.y += y_dir * phys.inv_weight;
    };

    /**
		 * @brief Apply this force to a whole particle range (force-major path).
		 *
		 * The force handler calls this ONCE per force per range instead of
		 * making a virtual apply_force() call per particle — one dispatch per
		 * frame per core, and the concrete force's inner loop can inline,
		 * hoist its per-range work (early-outs, constants) and pipeline.
		 *
		 * This default simply loops apply_force(), so a force that doesn't
		 * override it keeps working unchanged (just without the batching win).
		 * Overrides should hoist their whole-force early-outs (is_alive,
		 * zero strength, ...) to the top — one test per frame, not per
		 * particle.
		 */
    virtual void apply_range(const ParticleRange &r)
    {
        for (size_t i = 0; i < r.count; ++i)
            apply_force(r.phys_at(i), r.pos_at(i), r.acc_at(i));
    };

    /**
			 * @brief Updates the internal vectors used in particle movement.
			 * @param dir New direction, 0 to 360 degrees.
			 */
    void set_direction(const float dir)
    {
        dir_ = std::clamp(dir, -360.0f, 360.0f);
        update_vector();
    }

    /**
		 *  Update the direction and magnitude of the force.
		 * @param dir Direction,  -360 - 360.
		 * @param mag New magnitude of the force.
		 */
    void set_dir_and_mag(const float dir, const float mag)
    {
        dir_ = std::clamp(dir, -360.0f, 360.0f);
        this->mag = mag;
        update_vector();
    }

    void set_mag(const float mag)
    {
        this->mag = mag;
        update_vector();
    }
};

#endif //WEATHER_SEGMENT_BASE_FORCE_H
