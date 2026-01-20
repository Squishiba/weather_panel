#ifndef SEG_WIND_H
#define SEG_WIND_H

#include "helpers_rand.h"

#include "display/segment/segment_force.h"
#include "particles/particle_base.h"
#include "display/segment/segment_properties.h"

#include "pico/multicore.h"
#include "pico/sync.h"

class WindGust : public Force, public ParticleBase
	{
	friend class DebugConsole;
	friend class WeatherEffectBase;

protected:

	static constexpr float kBaseWindDir = -90.0f;
	static constexpr float kUpperWindDir = kBaseWindDir + 5.0f;
	static constexpr float kLowerWindDir = kBaseWindDir - 5.0f;
	static constexpr int16_t kWindBoundExpansion = 5;

	std::vector<Range> wind_spawn_span_; // Points where wind is allowed to spawn.

	uint32_t wind_chance_;
	float intensity_factor_;
	float gust_point_;
	float radius_;
	float radius2_;
	float inv_radius2_;
	bool is_alive_;

	/**
	 * @brief Generate a new wind centerpoint.
	 */
	void reset() override
		{
		// Don't need a lifetime, the center of the wind gust will travel across the field of view and then reset / eventually respawn.
		mag_ = get_rand_float(2.0f, intensity_factor_); // Calculate how strong this gust of wind will be
		dir_ = get_rand_float(kLowerWindDir, kUpperWindDir);
		update_vector();
		set_spawn_point(wind_spawn_span_, positions_);
		is_alive_ = true;
		radius_ = get_rand_float(intensity_factor_, intensity_factor_ * 9.0f);
		radius2_ = radius_ * radius_;
		inv_radius2_ = 1.0f / radius2_;

		velocities_.x = x_dir_ * 0.75f;
		velocities_.y = y_dir_ * 0.75f;
		}

	void p_wind_move()
		{
		positions_.x += velocities_.x * ((positions_.z) * 1.3f) * seg_properties_.get_dt();
		positions_.y += velocities_.y * ((positions_.z) * 1.3f) * seg_properties_.get_dt();
		positions_.z = std::clamp(positions_.z + (velocities_.z), 0.2f, 1.0f);
		}

private:
	using ParticleBase::is_drawable;
	using ParticleBase::calc_length;

	/**
	 * @brief does nothing.
	 */
	void update_physics() override {};

public:
	/**
	 * @param seg_properties The display segment properties reference.
	 * @param intensity [in] Intensity of the wind.
	 */
	WindGust(DisplaySegProperties &seg_properties) : ParticleBase(seg_properties)
		{
		intensity_factor_ = seg_properties.get_intensity() * 5.0f;
		reset();
		map_force_from_edge(wind_spawn_span_, seg_properties_.get_seg_bounds(), kWindBoundExpansion); // Creation of the valid spawn edge.
		wind_chance_ = UINT16_MAX * (intensity_factor_ * 0.01f);
		}

	~WindGust() override {};
	/**
	 *
	 * @param position Particle position to apply wind to.
	 * @param accel The particle's acceleration values.
	 */
	void apply_wind(const Position &position, Acceleration& accel) const
		{
		// Calculate offset from center
		const float dx = abs(position.x - positions_.x);
		const float dy = abs(position.y - positions_.y);

		const float dz = abs(position.z - positions_.z);

		// Calculate distance squared from center
		const float dist_sq = dx * dx + dy * dy;

		// Only apply force within radius
		if (dist_sq > radius2_)
			return;

		// Distance falloff: stronger near center, weaker at edges
		// Takes into account Depth as well.
		const float dist_factor = (1.0f - (dist_sq * inv_radius2_)) * dz;

		// Create perpendicular force for swirling (rotate offset by 90 degrees)
		// For clockwise swirl: tangent = (-dy, dx)
		// For counter-clockwise: tangent = (dy, -dx)

		// Combine all strength factors
		const float strength = mag_ * dist_factor;

		// Apply swirling force
		accel.x += -dy * strength;
		accel.y += dx * strength;
		}

	/**
	 * Check if the gust is alive.
	 * @return True if gust is alive, else false.
	 */
	bool is_gust_alive() const
		{
		return is_alive_;
		}

	/*
	* @brief Updates the wind process.
	*/
	void update() override
		{
		if (is_alive_ == false && (get_rand_32() > wind_chance_))
			{
			reset();
			}
		else if (is_alive_ == true)
			{
			p_wind_move();
			if (seg_properties_.is_particle_oob(positions_))
				{
				is_alive_ = false;
				}
			}
		}
	};

#endif
