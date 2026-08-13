#ifndef ZBUFF_H
#define ZBUFF_H

#include "libraries/pico_graphics/pico_graphics.hpp"

#include <type_traits>
#include <memory>

#pragma once

// Color definitions (RGB888 format)
struct Color
{
    uint8_t r, g, b;
};

inline pimoroni::PicoGraphics_PenRGB888 color_creator(1, 1, nullptr, 1);

// Standard colors
constexpr Color kWhite{255, 255, 255};
constexpr Color kBlack{0, 0, 0};
constexpr Color kRed{255, 0, 0};
constexpr Color kCyan{200, 220, 255};
constexpr Color kBlue{150, 180, 220};

inline int32_t color_to_pen(const Color color)
{
    return color_creator.create_pen(color.r, color.g, color.b);
}

const int pen_black = color_to_pen(kBlack);
const int pen_red = color_to_pen(kRed);
const int pen_white = color_to_pen(kWhite);
const int pen_cyan = color_to_pen(kCyan);
const int pen_blue = color_to_pen(kBlue);

    class PicoZGraphicsBase : public pimoroni::PicoGraphics_PenRGB888
        {
    protected:

        uint8_t depth_;
        bool is_depth_active_;

    public:

        void enable_depth()
            {
            is_depth_active_ = true;
            }

        void disable_depth()
            {
            is_depth_active_ = false;
            depth_ = 0;
            }

        /**
         * @brief Automatically clears out the framebuffer
         * to black, with 0 depth.
         *
         */
        void clear_framebuffer()
            {
            disable_depth();
            set_pen(pen_black);
            this->clear();
            enable_depth();
            }

        template <typename T>
        void set_depth(const T &depth)
            {
            if constexpr (std::is_integral_v<T>)
                {
                depth_ = static_cast<uint8_t>(depth);
                }
            else if constexpr (std::is_floating_point_v<T>)
                {
                depth_ = static_cast<uint8_t>(depth * UINT8_MAX);
                }
            }

        using PicoGraphics_PenRGB888::PicoGraphics_PenRGB888;
        virtual ~PicoZGraphicsBase() = default;
        virtual void set_pixel(const pimoroni::Point&) = 0;
        virtual void set_pixel_span(const pimoroni::Point&, uint) = 0;
        };

    /**
     * @brief Modified PicoGraphics_PenRGB888 derivative that adds depth testing.
     * Stores depth in the high byte of the 32-bit color (0xDDRRGGBB format).
     * This eliminates the need for a separate z-buffer, saving memory and improving cache efficiency.
     * The Hub75 driver skips this high byte when copying to the display, so it won't affect rendering.
     *
     * Format: 0xDDRRGGBB
     *   DD = Depth (bits 24-31)
     *   RR = Red   (bits 16-23)
     *   GG = Green (bits 8-15)
     *   BB = Blue  (bits 0-7)
     */
    enum Panel_Rotation
        {
        ROTATE_0,
        ROTATE_90,
        ROTATE_180,
        ROTATE_270
        };

    template<Panel_Rotation ROTATION>
    class PicoZGraphics final : public PicoZGraphicsBase
        {

    public:
        PicoZGraphics(const uint16_t width, const uint16_t height, void *frame_buffer, uint16_t layers = 1)
            : PicoZGraphicsBase(width, height, frame_buffer, layers)
            {
            is_depth_active_ = false;
            depth_ = 0;
            }

        constexpr inline size_t remap_index(const pimoroni::Point& p) const
            {
            if constexpr (ROTATION == ROTATE_0)
                {
                return p.y * bounds.w + p.x;
                }
            else if constexpr (ROTATION == ROTATE_90)
                {
                return p.x * bounds.w + (bounds.w - 1 - p.y);
                }
            else if constexpr (ROTATION == ROTATE_180)
                {
                return (bounds.h - 1 - p.y) * bounds.w
                     + (bounds.w - 1 - p.x);
                }
            else // ROTATE_270
                {
                return (bounds.h - 1 - p.x) * bounds.w
                     + p.y;
                }
            }
    public:
        __attribute__((optimize("O3")))
        void set_pixel(const pimoroni::Point &p) override
            {
            // Bounds check - critical for preventing buffer overruns
            if (p.x < 0 || p.x >= bounds.w || p.y < 0 || p.y >= bounds.h)
                return;

            auto *buf = static_cast<uint32_t *>(frame_buffer);
            buf += this->layer_offset;

            uint32_t &pixel = buf[remap_index(p)];

            if (!is_depth_active_)
                {
                // No depth testing - just write color with depth in high byte
                pixel = color | (depth_ << 24);
                }
            else
                {
                // Extract existing depth from high byte
                uint8_t existing_depth = (pixel >> 24) & 0xFF;

                // Only write if new depth is greater
                if (depth_ > existing_depth)
                    {
                    pixel = color | (depth_ << 24);
                    }
                }
            }
        __attribute__((optimize("O3")))
        void set_pixel_span(const pimoroni::Point &p, uint l) override
            {
            if constexpr (ROTATION == Panel_Rotation::ROTATE_0)
                {
                auto *buf = static_cast<uint32_t *>(frame_buffer);
                buf += this->layer_offset;
                buf = &buf[p.y * bounds.w + p.x];
                const uint32_t color_with_depth = color | (depth_ << 24);
                if (!is_depth_active_)
                    {
                    // Depth testing disabled - always write (used for clearing)
                    while (l--)
                        {
                        *buf++ = color_with_depth;
                        }
                    }
                else
                    {
                    // Depth testing enabled - check before writing
                    while (l--)
                        {
                        if (uint8_t existing_depth = (*buf >> 24) & 0xFF; depth_ > existing_depth)
                            {
                            *buf = color_with_depth;
                            }
                        buf++;
                        }
                    }
                }
            else if constexpr (ROTATION == Panel_Rotation::ROTATE_180)
                {
                auto* buf = static_cast<uint32_t*>(frame_buffer);
                buf += this->layer_offset;

                size_t idx = remap_index(p);
                const uint32_t color_with_depth = color | (depth_ << 24);

                while (l--)
                    {
                    uint32_t& pixel = buf[idx];
                    if (!is_depth_active_ || depth_ > ((pixel >> 24) & 0xFF))
                        pixel = color_with_depth;

                    idx--; // reverse direction
                    }
                }
            else
                {
                // 90 / 270: fallback
                for (uint i = 0; i < l; ++i)
                    {
                    set_pixel({p.x + int(i), p.y});
                    }
                }
            }
        };

        std::unique_ptr<PicoZGraphicsBase> make_graphics(const Panel_Rotation rot, uint16_t w, uint16_t h, void* fb)
            {
            switch (rot)
                {
                case ROTATE_0:
                    return std::make_unique<PicoZGraphics<ROTATE_0>>(w, h, fb);
                case ROTATE_90:
                    return std::make_unique<PicoZGraphics<ROTATE_90>>(w, h, fb);
                case ROTATE_180:
                    return std::make_unique<PicoZGraphics<ROTATE_180>>(w, h, fb);
                case ROTATE_270:
                    return std::make_unique<PicoZGraphics<ROTATE_270>>(w, h, fb);
                }
            return nullptr;
            }

#endif