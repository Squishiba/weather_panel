#ifndef SEG_GEO_H
#define SEG_GEO_H

#include "../../utility/helpers_rand.h"
#include "libraries/pico_graphics/pico_graphics.hpp"
#include "geometry/geometry.h"

/**
 * @brief Modified rectangle struct that additionally
 * keeps track of end points.
 */
struct RectMod : public pimoroni::Rect
{
    int32_t x_end;
    int32_t y_end;

    RectMod(const int32_t x, const int32_t y, const int32_t w, const int32_t h) : pimoroni::Rect(x, y, w, h), x_end(x + w - 1), y_end(y + h - 1) {}
    RectMod(const pimoroni::Point &tl, const pimoroni::Point &br) : pimoroni::Rect(tl, br), x_end(br.x), y_end(br.y) {}

    RectMod() = default;

    [[nodiscard]] bool contains_inclusive(const pimoroni::Point &p) const
    {
        return p.x >= x && p.y >= y && p.x <= x_end && p.y <= y_end;
    }

    [[nodiscard]] bool contains_inclusive(const XYZ &p) const
    {
        return p.x >= x && p.y >= y && p.x <= x_end && p.y <= y_end;
    }
};

/**
 * @brief The segment bounding box that particle drawing/showing occurs in.
 *
 */
struct BoundLimits : public RectMod
{

    uint32_t pixel_count;
    float z_start = 0.1f;
    float z_end = 1.0f;

    BoundLimits(const int16_t x_start, const int16_t x_end, const int16_t y_start, const int16_t y_end) : RectMod(x_start, y_start, (x_end - x_start), (y_end - y_start)), pixel_count(w * h) {}

    explicit BoundLimits(const RectMod &space) : RectMod(space), pixel_count(w * h) {}

    BoundLimits() = default;
};

struct Oob_Limits : public RectMod
{

    const int16_t bounds_expansion_val;

    Oob_Limits(const pimoroni::Point &tl, const pimoroni::Point &br, const int16_t oob_limits)
        : RectMod(tl.x - oob_limits, tl.y - oob_limits, br.x - (tl.x - oob_limits) + oob_limits, br.y - (tl.y - oob_limits) + oob_limits), bounds_expansion_val(oob_limits)
    {
    }

    Oob_Limits(const RectMod &oob, const int16_t oob_limits) : RectMod((oob.x - oob_limits), (oob.y - oob_limits), (oob.w + oob_limits * 2), (oob.h + oob_limits * 2)), bounds_expansion_val(oob_limits)
    {
    }

    Oob_Limits(const int16_t x_start, const int16_t x_end, const int16_t y_start, const int16_t y_end, const int16_t oob_limits)
        : RectMod(x_start - oob_limits, y_start - oob_limits, (x_end - x_start + oob_limits * 2), (y_end - y_start + oob_limits * 2)), bounds_expansion_val(oob_limits)
    {
    }
};

#endif
