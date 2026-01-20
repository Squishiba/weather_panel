#ifndef SEG_PROP_H
#define SEG_PROP_H

#include "segment_geometry.h"
#include "segment_gravity.h"

#include "particles/particle_properties.h"
#include "helpers_rand.h"

class DisplaySegProperties
	{

	friend class WeatherDisplayHandler;

protected:
	static constexpr float kInitGravDir = 0.0f; // Initial gravity direction vector.
	static constexpr float kInitGravMag = 9.8f; // Initial Gravity magnitude value.
	static constexpr int16_t kBoundsExpansion = 15; // how far out from nominal segment boundaries can particles exist before reset.
	static constexpr int16_t kSpawnBoundsExpansion = 25; // how far out from the nominal segment spawning line can particles spawn?
	static constexpr uint8_t kMaxWindSwirls = 3;
	BoundLimits seg_bounds_; // The bounding box of this segment.
	Oob_Limits oob_limits_; // The outer box where, once crossed, particles are reset.
	std::vector<Range> spawn_span_; // Points where particles are allowed to spawn.
	std::vector<Range> ground_span_; // What qualifies as the ground?
	GravityProperties gravity_; // The gravity properties of this segment.
	float dt_; // refresh rate.
	float intensity_;

public:
	/**
	 * @brief Construct a new Display Seg Properties object
	 *
	 * @param x_start x axis start point.
	 * @param x_end x axis end point.
	 * @param y_start y axis start point.
	 * @param y_end  y axis end point.
	 */
	DisplaySegProperties(const int16_t x_start, const int16_t x_end, const int16_t y_start, const int16_t y_end) : seg_bounds_(x_start, x_end, y_start, y_end),
		oob_limits_(x_start, x_end, y_start, y_end, kBoundsExpansion),
		gravity_(kInitGravDir, kInitGravMag)
	{
		gravity_.map_force_from_edge(spawn_span_, seg_bounds_, kBoundsExpansion); // generate spawns.
		gravity_.map_force_to_edge(ground_span_, seg_bounds_, 0); // generate the grounds
	}

	DisplaySegProperties(const RectMod &segment_bounds) : seg_bounds_(segment_bounds), oob_limits_(segment_bounds,
																																																 kBoundsExpansion), gravity_(kInitGravDir,
																													kInitGravMag)
	{
		gravity_.map_force_from_edge(spawn_span_, seg_bounds_, kBoundsExpansion); // generate spawns.
		gravity_.map_force_to_edge(ground_span_, seg_bounds_, 0); // generate the grounds
	}

	/**
	 * @brief Checks if the particle is out of bounds by any metric, by at least
	 * kBoundsExpanse larger than the segment's nominal bounds.
	 * @param position Particle position.
	 *
	 * @return true
	 * @return false
	 */
	[[nodiscard]] bool is_particle_oob(const Position &position) const
	{
		return !oob_limits_.contains(pimoroni::Point(position.x, position.y));
	}

	/**
	 * @brief Checks if particle is within the drawing bounds of the segment.
	 *
	 * @return true If particle should be drawn.
	 * @return false
	 */
	[[nodiscard]] bool is_particle_in_segment(const Position &position) const
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
		gravity_.update_gravity(grav_dir, grav_mag);
		gravity_.map_force_from_edge(spawn_span_, seg_bounds_, kBoundsExpansion);
		gravity_.map_force_to_edge(ground_span_, seg_bounds_, 0);
	}

	const GravityProperties &get_gravity() const
	{
		return gravity_;
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

	[[nodiscard]] const float &get_norm_x_grav() const
	{
		return gravity_.normalized_x_;
	}

	[[nodiscard]] const float &get_norm_y_grav() const
	{
		return gravity_.normalized_y_;
	}

	[[nodiscard]] bool is_particle_on_ground(const Position &p) const
	{
		for (auto &ground: ground_span_)
			{
			const Range &g = ground;
			if (p.x >= g.space.start_.x && p.x <= g.space.end_.x &&
					p.y >= g.space.start_.y && p.y <= g.space.end_.y)
				{
				return true;
				}
			}
		return false;
	}

	[[nodiscard]] const std::vector<Range> &get_spawn_ranges() const
	{
		return spawn_span_;
	}

	[[nodiscard]] const RectMod &get_oob_limits() const
	{
		return oob_limits_;
	}

	[[nodiscard]] const RectMod &get_seg_bounds() const
	{
		return seg_bounds_;
	}
	};

#endif
