#ifndef ZBUFF_H
#define ZBUFF_H

#include "libraries/pico_graphics/pico_graphics.hpp"

#include <type_traits>

#pragma once

// Color definitions (RGB888 format)
struct Color
{
    uint8_t r, g, b;
    constexpr Color(uint8_t red, uint8_t green, uint8_t blue)
        : r(red), g(green), b(blue) {}
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

namespace pimoroni
{

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
    class PicoZGraphics : public PicoGraphics_PenRGB888
    {
        uint8_t depth_;
        bool is_depth_active;

    public:
        PicoZGraphics(const uint16_t width, const uint16_t height, void *frame_buffer, uint16_t layers = 1)
            : PicoGraphics_PenRGB888(width, height, frame_buffer, layers)
        {
            is_depth_active = false;
            depth_ = 0;
        }
        void enable_depth()
        {
            is_depth_active = true;
        }

        void disable_depth()
        {
            is_depth_active = false;
            depth_ = 0;
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

    public:
        __attribute__((optimize("O3")))
        void set_pixel(const Point &p) override
        {
            // Bounds check - critical for preventing buffer overruns
            if (p.x < 0 || p.x >= bounds.w || p.y < 0 || p.y >= bounds.h)
                return;

            auto *buf = static_cast<uint32_t *>(frame_buffer);
            buf += this->layer_offset;

            uint32_t &pixel = buf[p.y * bounds.w + p.x];

            if (!is_depth_active)
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
        void set_pixel_span(const Point &p, uint l) override
        {
            auto *buf = static_cast<uint32_t *>(frame_buffer);
            buf += this->layer_offset;
            buf = &buf[p.y * bounds.w + p.x];

            const uint32_t color_with_depth = color | (depth_ << 24);

            if (!is_depth_active)
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
    };
}

#endif