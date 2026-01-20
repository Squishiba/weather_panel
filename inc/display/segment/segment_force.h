//
// Created by Jared on 1/19/2026.
//

#ifndef WEATHER_SEGMENT_BASE_FORCE_H
#define WEATHER_SEGMENT_BASE_FORCE_H

#include "dsp/controller_functions.h"
#include "segment_geometry.h"

class Force
	{
protected:

	float dir_;

	float prev_dir_ = 999999;


	float mag_;

	float sin_;
	float cos_;

	/**
	 * @brief Update the x and y dir vectors of this force.
	 * If dir_ has not changed since this function was last called, skips
	 * recalculating sin and cos and reuses the old values to save on compute.
	 */
	void update_vector()
		{

		if (dir_ != prev_dir_)
			{
			arm_sin_cos_f32(dir_ - 90.0f, &sin_, &cos_);
			};

		x_dir_ = cos_ * mag_;
		y_dir_ = sin_ * mag_;

		mag_total_ = (std::abs(x_dir_) + std::abs(y_dir_)); // what's the added total
		normalized_x_ = x_dir_ / mag_total_;
		normalized_y_ = y_dir_ / mag_total_;
		prev_dir_ = dir_;
		}

public:
	float x_dir_; // The x component of the force.
	float y_dir_; // The y component of the force.
	float mag_total_; // The magnitude total.
	float normalized_x_; // Normalized X component.
	float normalized_y_; // Normalized Y component.

	Force(const float dir, const float mag) : dir_(dir), mag_(mag)
		{
		update_vector();
		}

	Force() = default;

	/**
	 * @brief Updates the internal magnitude value. X and Y dir
	 * magnitudes are also updated in tandem.
	 * @param new_mag New magnitude value to be applied.
	 */
	void set_magnitude(const float new_mag)
		{
		mag_ = new_mag;
		update_vector();
		}

	float &get_magnitude()
		{
		return mag_;
		}

	/**
	 * @brief Maps this force's direction TO the force's direction.
	 * Useful for calculating the opposite edge of a force. I.e. if gravity is "down",
	 * then the edge span calculated will contain line values that map to the lower most edge.
	 *
	* @param edge_span The edge span to place the results in.
	 * @param bounds The segment's bounds.
	 * @param bounds_expansion How far or in from the provided bounds is valid. Negative values decrease the span, positive increase.
	 */
	void map_force_to_edge(std::vector<Range> &edge_span, const BoundLimits &bounds, const int16_t &bounds_expansion)
		{
		x_dir_ *= -1.0f; // Quick invert.
		y_dir_ *= -1.0f; // Invert.
		map_force_from_edge(edge_span, bounds, bounds_expansion);
		x_dir_ *= -1.0f; // Then we revert the change back!
		y_dir_ *= -1.0f; // Easy peasy.
		}

	/**
	 * @brief Maps a given force's direction FROM the given force's direction.
	 * Useful for spawn point mapping, i.e. if gravity is "down", then the edge
	 * span calculated will contain line values that map to the upper most edge.
	 *
	 * @param edge_span The edge span to place the results in.
	 * @param bounds The segment's bounds.
	 * @param bounds_expansion How far in/out from the provided bounds is valid. Negative values decrease the span, positive increase.
	 * @return None
	 */
	void map_force_from_edge(std::vector<Range> &edge_span, const BoundLimits &bounds, const int16_t &bounds_expansion) const
		{
		float weight_h = std::abs(x_dir_); // horizontal
		float weight_v = std::abs(y_dir_); // vertical
		const float sum = weight_h + weight_v;

		if (sum <= 1e-6f)
			{
			printf("Something went wrong when trying to calculate the weight sum for spawn mapping.");
			}

		// I want to expand the bounds by some amount, but only to where the particle can spawn somewhere where it'll EVENTUALLY be seen.

		weight_h /= sum;
		weight_v /= sum;

		if (weight_h > 0.0f)
			{
			Range s;
			s.space.start_.y = bounds.y;
			s.space.end_.y = (bounds.y_end);
			if (x_dir_ > 0)
				{
				// Right edge
				s.space.start_.x = s.space.end_.x = (bounds.x_end) + bounds_expansion; // Spawn span is entirely on the right edge.
				}
			else
				{
				// Left edge
				s.space.start_.x = s.space.end_.x = bounds.x - bounds_expansion; // Spawn span on the left edge.
				}
			s.weight = weight_h;
			s.z_start = bounds.z_start;
			s.z_end = bounds.z_end;
			edge_span.push_back(s);
			}

		// ---- Horizontal edges (top / bottom) ----
		if (weight_v > 0.0f) // I.e. The spawn span is somewhere on the top axis or bottom axis.
			{
			Range v;
			v.space.start_.x = bounds.x;
			v.space.end_.x = (bounds.x_end);

			if (y_dir_ > 0)
				{
				// Force down → spawn top
				v.space.start_.y = v.space.end_.y = bounds.y - bounds_expansion; // All spawn on top edge.
				}
			else
				{
				// Force up → spawn bottom
				v.space.start_.y = v.space.end_.y = (bounds.y_end) + bounds_expansion; // All spawn on bottom edge.
				}
			v.weight = weight_v;
			v.z_start = bounds.z_start;
			v.z_end = bounds.z_end;
			edge_span.push_back(v);
			}
		}
	};


#endif //WEATHER_SEGMENT_BASE_FORCE_H
