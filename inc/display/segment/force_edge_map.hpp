#ifndef WEATHER_SPAWN_RANGES_H
#define WEATHER_SPAWN_RANGES_H

#include "physics/forcelike.hpp"
#include "geometry/geometry.hpp"
#include "segment_geometry.hpp"  // BoundLimits
#include "dsp/fast_math_functions.h"

#include <etl/vector.h>

struct ForceEdgeMap
{

    etl::vector<Range, 4> edge_span_;

    /**
	 * @brief Maps this force's direction TO the force's direction.
	 * Useful for calculating the opposite edge of a force. I.e. if gravity is "down",
	 * then the edge span calculated will contain line values that map to the lowermost edge.
	 * @param bounds Bounds of the segment.
	 * @param force What force to use in this mapping.
	 * @param bounds_expansion How far or in from the provided bounds is valid. Negative values decrease the span, positive increase.
	 */
    void map_force_to_edge(const BoundLimits &bounds, ForceLike &force, const int16_t &bounds_expansion)
    {
        force.x_dir = -force.x_dir; // Quick invert.
        force.y_dir = -force.y_dir;
        map_force_from_edge(bounds, force, bounds_expansion);
        force.x_dir = -force.x_dir; // Quick invert.
        force.y_dir = -force.y_dir;
    }

    /**
	 * @brief Maps a given force's direction FROM the given force's direction.
	 * Useful for spawn point mapping, i.e. if gravity is "down", then the edge
	 * span calculated will contain line values that map to the uppermost edge.
	 * @param bounds Bounds of the segment.
	 * @param force What forces to use in this mapping.
	 * @param bounds_expansion How far in/out from the provided bounds is valid. Negative values decrease the span, positive increase.
	 * @return None
	 */
    void map_force_from_edge(const BoundLimits &bounds, const ForceLike &force, const int16_t &bounds_expansion)
    {

        auto &x_dir_ = force.x_dir;
        auto &y_dir_ = force.y_dir;

        // Flux weighting: the spawn rate through an edge is proportional to the
        // drift component crossing it TIMES the edge's length (influx = |v.n| x
        // face area). Segments here are much taller than wide, so the side edge
        // must count for more than the raw component ratio — without the length
        // factor, a wind field visibly failed to move spawns to the side edge
        // (9% at wind 1 / gravity 9.8, where the correct flux share is ~24%).
        const float side_len = static_cast<float>((bounds.y_end - bounds.y) + 2 * bounds_expansion);
        const float top_len = static_cast<float>((bounds.x_end - bounds.x) + 2 * bounds_expansion);

        float weight_h = std::abs(x_dir_) * side_len; // through left/right edge
        float weight_v = std::abs(y_dir_) * top_len;  // through top/bottom edge
        const float sum = weight_h + weight_v;

        edge_span_.clear();

        weight_h /= sum;
        weight_v /= sum;

        if (weight_h > 0.0f)
        {
            Range s;
            s.space.start_.y = bounds.y - bounds_expansion;
            s.space.end_.y = (bounds.y_end) + bounds_expansion;
            if (x_dir_ > 0)
            {
                // Force going right → spawn on LEFT edge
                s.space.start_.x = s.space.end_.x = bounds.x - bounds_expansion;
            }
            else
            {
                // Force going left → spawn on RIGHT edge
                s.space.start_.x = s.space.end_.x = (bounds.x_end) + bounds_expansion;
            }
            s.weight = weight_h;
            s.z_start = bounds.z_start;
            s.z_end = bounds.z_end;
            edge_span_.push_back(s);
        }

        // ---- Horizontal edges (top / bottom) ----
        if (weight_v > 0.0f) // I.e. The spawn span is somewhere on the top axis or bottom axis.
        {
            Range v;
            v.space.start_.x = bounds.x - bounds_expansion;
            v.space.end_.x = (bounds.x_end) + bounds_expansion;

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
            edge_span_.push_back(v);
        }
    }

    void map_force_from_perpendicular(const BoundLimits &bounds, ForceLike &force, const int16_t &bounds_expansion)
    {
        force.x_dir = -force.x_dir;
        force.y_dir = -force.y_dir;
        map_force_to_perpendicular(bounds, force, bounds_expansion);
        force.x_dir = -force.x_dir;
        force.y_dir = -force.y_dir;
    }

    /**
	 * @brief Maps a given force's direction approximately perpendicular from its origin.
	 * @param bounds The bounds of the segment.
	 * @param force What forces to use in this mapping.
	 * @param bounds_expansion How far in/out from the provided bounds is valid. Negative values decrease the span, positive increase.
	 * @return None
	 */
    void map_force_to_perpendicular(const BoundLimits &bounds, const ForceLike &force, const int16_t &bounds_expansion)
    {

        const auto &x_dir_ = -force.x_dir;
        const auto &y_dir_ = force.y_dir;

        float weight_h = std::abs(x_dir_); // horizontal
        float weight_v = std::abs(y_dir_); // vertical

        const float sum = weight_h + weight_v;

        edge_span_.clear();

        weight_h /= sum;
        weight_v /= sum;

        if (weight_h > 0.0f)
        {
            Range s;
            s.space.start_.y = bounds.y - bounds_expansion;
            s.space.end_.y = (bounds.y_end) + bounds_expansion;
            if (x_dir_ > 0)
            {
                // Force going right → spawn on LEFT edge
                s.space.start_.x = s.space.end_.x = bounds.x - bounds_expansion;
            }
            else
            {
                // Force going left → spawn on RIGHT edge
                s.space.start_.x = s.space.end_.x = (bounds.x_end) + bounds_expansion;
            }
            s.weight = weight_h;
            s.z_start = bounds.z_start;
            s.z_end = bounds.z_end;
            edge_span_.push_back(s);
        }

        // ---- Horizontal edges (top / bottom) ----
        if (weight_v > 0.0f) // I.e. The spawn span is somewhere on the top axis or bottom axis.
        {
            Range v;

            v.space.start_.x = bounds.x - bounds_expansion;
            v.space.end_.x = (bounds.x_end) + bounds_expansion;

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
            edge_span_.push_back(v);
        }
    }

    [[nodiscard]] bool is_particle_on_edge(const XYZ &p) const
    {
        for (auto &span : edge_span_)
        {
            const Range &g = span;
            if (p.x >= g.space.start_.x && p.x <= g.space.end_.x && p.y >= g.space.start_.y && p.y <= g.space.end_.y)
            {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] XYZ get_random_edge_coords() const
    {
        if (edge_span_.empty())
        {
            // No spawn ranges defined - return origin as fallback
            return {0.0f, 0.0f, 0.5f};
        }

        const float r = rng_class.get_rand_float();
        float acc = 0.0f;
        for (const auto &spawn : edge_span_)
        {
            acc += spawn.weight;
            if (r <= acc)
            {
                return spawn.get_random_coords_in_range();
            }
        }
        // Fallback: use last range if floating-point precision caused us to miss
        return edge_span_.back().get_random_coords_in_range();
    }
};


#endif //WEATHER_SPAWN_RANGES_H