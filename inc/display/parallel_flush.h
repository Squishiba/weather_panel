#ifndef WEATHER_PARALLEL_FLUSH_H
#define WEATHER_PARALLEL_FLUSH_H

/**
 * @file parallel_flush.h
 * @brief Two-core framebuffer -> HUB75 back-buffer flush.
 *
 * The profiler showed Hub75::update() (the FLUSH phase) is the single biggest
 * chunk of frame work (~2.7 ms at -O2): for all 16k pixels it gamma-corrects
 * three channels through GAMMA_10BIT and computes the driver's interlace/
 * square-arrangement remap. None of that work is a memcpy, so DMA can't do it —
 * but every ROW is completely independent, so it splits perfectly across the
 * two cores with parallel::run() (the same fork/join the particle update uses).
 *
 * This header replicates the driver's 128x128-canvas -> 256x64-panel branch of
 * Hub75::copy_to_back_buffer() (pimoroni-pico/drivers/hub75/hub75.cpp) over a
 * row RANGE, and parallel_hub75_flush() forks the range across both cores.
 * Everything it touches on Hub75 is public (back_buffer, shifts, dims), so the
 * vendored driver stays unmodified.
 *
 * Race-safety (parallel::run's contract): the canvas is read-only during the
 * flush, and the row mapping is injective — canvas rows {y, y+32, y+64, y+96}
 * tile disjoint even/odd + left/right quarters of each 512-entry output group —
 * so any row split writes disjoint back-buffer entries. No locks needed.
 *
 * If the geometry or pen type ever stops matching the fast path, it falls back
 * to the driver's own (serial) Hub75::update().
 */

#include "drivers/hub75/hub75.hpp"
#include "../utility/parallel_executor.h"

#include <cstdint>

namespace weather
{

    /// One core's share of the flush: convert canvas rows [y0, y1).
    struct FlushCtx
    {
        pimoroni::Pixel *dst;  ///< HUB75 back buffer (gamma'd 10:10:10 pixels)
        const uint32_t *fb;    ///< canvas framebuffer, 0xDDRRGGBB per pixel
        int y0;                ///< first canvas row (inclusive)
        int y1;                ///< last canvas row (exclusive)
        int canvas_w;          ///< canvas width in pixels (= panel_w / 2)
        int panel_w;           ///< HUB75 driver width  (e.g. 256)
        int panel_h;           ///< HUB75 driver height (e.g. 64)
        uint r_shift;          ///< colour-order shifts, copied from the driver
        uint g_shift;
        uint b_shift;
    };

    /**
     * @brief Convert one range of canvas rows into the HUB75 back buffer.
     *
     * Same transform as the driver's copy_to_back_buffer() square-arrangement
     * branch: bottom half of the canvas maps to the right half of the panel,
     * and each pair of rows (N, N + panel_h/2) is interleaved even/odd because
     * the panel scans its top and bottom halves simultaneously. The 0xDDRRGGBB
     * depth byte is simply ignored, exactly as the driver skips it.
     *
     * Type-erased (void*) so it can cross to core 1 as a plain function
     * pointer — see parallel_executor.h.
     */
    inline void flush_rows(void *v)
    {
        auto *c = static_cast<const FlushCtx *>(v);

        for (int y = c->y0; y < c->y1; ++y)
        {
            // Row remap (hoisted out of the pixel loop; constant per row).
            int sy = y;
            int basex = 0;
            if (sy >= c->panel_h) // bottom half of canvas -> right half of panel
            {
                sy -= c->panel_h;
                basex = c->panel_w / 2; // assumes panel_w / 2 is even (driver does too)
            }
            int offsety = c->panel_w * 2;
            if (sy >= c->panel_h / 2) // interleave: odd entries are the lower scan half
            {
                sy -= c->panel_h / 2;
                offsety *= sy;
                offsety += 1;
            }
            else
            {
                offsety *= sy;
            }

            const uint32_t *src = c->fb + static_cast<size_t>(y) * c->canvas_w;
            for (int x = 0; x < c->canvas_w; ++x)
            {
                const uint32_t px = src[x]; // 0xDDRRGGBB (one load vs the driver's byte walk)
                const uint32_t b = px & 0xFFu;
                const uint32_t g = (px >> 8) & 0xFFu;
                const uint32_t r = (px >> 16) & 0xFFu;

                c->dst[offsety + (x + basex) * 2] = pimoroni::Pixel(
                    (pimoroni::GAMMA_10BIT[b] << c->b_shift) |
                    (pimoroni::GAMMA_10BIT[g] << c->g_shift) |
                    (pimoroni::GAMMA_10BIT[r] << c->r_shift));
            }
        }
    }

    /**
     * @brief Flush the graphics framebuffer to the HUB75 back buffer on BOTH cores.
     *
     * Drop-in replacement for i75.update(&graphics): core 0 converts the top
     * half of the canvas rows while core 1 converts the bottom half, then
     * joins. Falls back to the driver's serial update() if the pen type or
     * geometry doesn't match the fast path this file replicates.
     */
    inline void parallel_hub75_flush(pimoroni::Hub75 &hub, pimoroni::PicoGraphics &graphics)
    {
        const int cw = graphics.bounds.w;
        const int ch = graphics.bounds.h;

        if (graphics.pen_type != pimoroni::PicoGraphics::PEN_RGB888 ||
            cw != static_cast<int>(hub.width / 2) ||
            ch != static_cast<int>(hub.height * 2))
        {
            hub.update(&graphics); // unknown layout: let the driver handle it
            return;
        }

        const int mid = ch / 2;
        FlushCtx upper{hub.back_buffer, static_cast<const uint32_t *>(graphics.frame_buffer),
                       mid, ch, cw, static_cast<int>(hub.width), static_cast<int>(hub.height),
                       hub.r_shift, hub.g_shift, hub.b_shift};
        FlushCtx lower{hub.back_buffer, static_cast<const uint32_t *>(graphics.frame_buffer),
                       0, mid, cw, static_cast<int>(hub.width), static_cast<int>(hub.height),
                       hub.r_shift, hub.g_shift, hub.b_shift};

        // core 0: upper rows, core 1: lower rows. Items = pixels, so tiny
        // panels would just run inline (same threshold as the physics split).
        parallel::run(&flush_rows, &upper, &lower, static_cast<size_t>(cw) * ch);
    }

} // namespace weather

#endif // WEATHER_PARALLEL_FLUSH_H
