//
// Created by Jared on 2/7/2026.
//

#ifndef WEATHER_GEOMETRY_H
#define WEATHER_GEOMETRY_H

#include "libraries/pico_graphics/pico_graphics.hpp"
#include "../utility/helpers_rand.h"

/**
 * @brief X, Y, and Z positional coordinates.
 *
 */
struct XYZ
{
	float x;
	float y;
	float z;

	void operator+(const XYZ &coords)
	{
		x += coords.x;
		y += coords.y;
		z += coords.z;
	}
	void operator-(const XYZ &coords)
	{
		x -= coords.x;
		y -= coords.y;
		z -= coords.z;
	}
	void operator*(const XYZ &coords)
	{
		x *= coords.x;
		y *= coords.y;
		z *= coords.z;
	}

	void operator/(const XYZ &coords)
	{
		x /= coords.x;
		y /= coords.y;
		z /= coords.z;
	}
};

struct Line
{
	pimoroni::Point start_;
	pimoroni::Point end_;
	Line(const pimoroni::Point &start, const pimoroni::Point &end) : start_(start), end_(end) {}
	Line(const int16_t x1, const int16_t y1, const int16_t x2, const int16_t y2) : start_(x1, y1), end_(x2, y2) {}
	Line() = default;
};

/**
 * @brief Contains linear range information.
 *
 */
struct Range
{
	Line space; // Contains x and y line information.

	float z_start, z_end;
	float weight;
	float normalized_y; // These might be required since the Y and X values are asymmetric relative to one another.
	float normalized_x; //

	/**
	 * @return A new position.
	 */
	XYZ get_random_coords_in_range() const
	{
		return {
			rng_class.get_rand_float(space.start_.x, space.end_.x),
			rng_class.get_rand_float(space.start_.y, space.end_.y), rng_class.get_rand_float(z_start, z_end)};
	}
};

#endif // WEATHER_GEOMETRY_H