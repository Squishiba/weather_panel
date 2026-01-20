#ifndef WEATHER_DISPLAY_HANDLER_H
#define WEATHER_DISPLAY_HANDLER_H

#include "AP3216_WE.h"
#include "z_buffer.h"
#include "display_segment.h"

#include "libraries/interstate75/interstate75.hpp"

#include "pico/multicore.h"
#include "pico/time.h"

#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <cmath>

uint8_t global_brightness = 3;

/**
 * @brief Main weather display handler that manages all segments
 *
 * This class:
 * - Divides the display into segments (one per day)
 * - Manages weather data for each segment
 * - Coordinates rendering across multiple cores
 * - Handles frame dividers between segments
 */
class WeatherDisplayHandler
{

private:
    pimoroni::PicoZGraphics &graphics_;           // The framebuffer instance.
    pimoroni::Hub75 &i75_;                        // The HUB75 object instance.
    AP3216_WE lux_meter_;                         // The lux meter add-on, detects brightness of room to dim or brighten display.
    uint32_t prev_time_ = 0;                      // Previous time clocked in us.
    float fps_target_;                            // FPS target.
    float fps_period_;                            // FPS period in s.
    uint32_t fps_period_us_;                      // FPS period in us.
    uint16_t width_;                              // Width of display.
    uint16_t height_;                             // Height of display.
    uint16_t total_particle_count_ = 0;           // Total particle count being drawn onscreen.
    uint8_t num_days_;                            // Number of days to generate.
    std::vector<DisplaySegment> segment_display_; // Stores each display segment.
    std::vector<uint16_t> frame_points_;          // X coordinates where frame dividers are drawn

public:
    WeatherDisplayHandler(pimoroni::PicoZGraphics &graphics, pimoroni::Hub75 &i75, uint8_t num_days, const float target_fps)
        : graphics_(graphics), i75_(i75), width_(graphics.bounds.w), height_(graphics.bounds.h), num_days_(num_days), fps_target_(target_fps)
    {
        printf("Display width: %d, Days: %d\n", width_, num_days);

        set_new_fps_target(target_fps);
        prev_time_ = time_us_32();

        segment_display_.reserve(num_days);

        lux_meter_.init(i2c0);          // initialize the lux sensor
        lux_meter_.setMode(AP3216_ALS); // We only care about the ambient light mode.
        lux_meter_.setLuxRange(RANGE_323);

        uint16_t base_width = width_ / num_days; // Calculate segment widths
        uint16_t remainder = width_ % num_days;
        printf("Base segment width: %d, Remainder pixels: %d\n", base_width, remainder);

        // Distribute remainder pixels to middle segments
        // Calculate which segments get extra pixels (closest to middle)
        float middle = (num_days - 1) / 2.0f;

        // Sort segment indices by distance from middle
        std::vector<int> segments_by_distance(num_days);
        for (uint32_t i = 0; i < num_days; ++i)
        {
            segments_by_distance[i] = i;
        }

        std::sort(segments_by_distance.begin(), segments_by_distance.end(),
                  [middle](uint32_t a, uint32_t b)
                  {
                      return std::abs(a - middle) < std::abs(b - middle);
                  });

        // Select which segments get extra pixels
        std::vector<bool> segments_with_extra(num_days, false);
        for (uint32_t i = 0; i < remainder; ++i)
        {
            segments_with_extra[segments_by_distance[i]] = true;
        }

        // Calculate actual segment widths
        std::vector<int> segment_widths(num_days);
        for (uint32_t i = 0; i < num_days; ++i)
        {
            segment_widths[i] = base_width + (segments_with_extra[i] ? 1 : 0);
        }

        printf("Segment widths: ");
        for (uint32_t w : segment_widths)
        {
            printf("%ld ", w);
        }
        printf("\n");

        // Create segments
        uint32_t current_x = 0;
        for (uint32_t i = 0; i < num_days; ++i)
        {
            uint16_t x_start = current_x;
            uint16_t x_end = current_x + segment_widths[i] - 1; // -1 because end is inclusive

            // Add frame point at boundary (except after last segment)
            // Place divider AFTER the segment, not on its last pixel
            if (i < (num_days - 1))
            {
                frame_points_.push_back(x_end + 1);
            }

            RectMod seg_frame(x_start, 0, segment_widths[i], height_); // Create the rectangle that bounds this segment.
            DisplaySegProperties temp(seg_frame);                      // construct the display segment object from the rectangle above.
            temp.set_dt(fps_period_);                                  // Set the initial delta time.
            segment_display_.emplace_back(temp);                          // Push the segment into the vector array.
            current_x += segment_widths[i] + 1;
        }

        printf("Frame points are at: ");
        for (int fp : frame_points_)
        {
            printf("%d ", fp);
        }

        printf("\n");

        printf("Segment information...");
        uint8_t segment_index = 0;
        for (auto &seg : segment_display_)
        {
            DisplaySegProperties &seg_properties = seg.get_segment_properties();
            printf("\n");
            printf("For segment num: %d \n", segment_index);
            RectMod bounds = seg_properties.get_seg_bounds();

            printf("Segment drawing bounds: \n");
            printf("(x_start: %ld, x_end: %ld, x_width: %ld, y_start: %ld, y_end: %ld ) \n", bounds.x, bounds.x_end, bounds.w, bounds.y, bounds.y_end);

            std::vector<Range> spawn_ranges = seg_properties.get_spawn_ranges();

            uint8_t array_index = 0;
            for (auto &itr : spawn_ranges)
            {
                printf("Spawning range: %d \n", (array_index + 1));
                printf("(x_start: %ld, x_end: %ld, y_start: %ld, y_end: %ld) \n", itr.space.start_.x, itr.space.end_.x, itr.space.start_.y, itr.space.end_.y);
                printf("(weight: %f)", itr.weight);
                printf("\n");
                array_index++;
            }

            RectMod oob_bounds = seg_properties.get_oob_limits();
            printf(" Out of Bounds Limits: \n");
            printf("(x_start: %ld, x_end: %ld, y_start: %ld, y_end: %ld) ", oob_bounds.x, oob_bounds.x_end, oob_bounds.y, oob_bounds.y_end);
            printf("\n");
            segment_index++;
        }
        printf("\n");
    }

    /**
     * @brief Update weather data for all segments
     *
     * @param weather_data Map containing weather intervals data
     */
    void update_weather(const std::vector<std::map<std::string, std::string>> &weather_intervals)
    {
        for (size_t i = 0; i < weather_intervals.size() && i < segment_display_.size(); ++i)
        {
            segment_display_[i].update_state(weather_intervals[i]);
            printf("  Day %zu: %s\n", i + 1, segment_display_[i].weather_state().c_str());
        }
    }

    /**
     * @brief Get the total particle count of the display.
     *
     * @return uint16_t
     */
    [[nodiscard]] uint16_t get_total_particle_count() const
    {
        return total_particle_count_;
    }

    /**
     * @brief Set a new FPS target for the display to try and hit.
     *
     * @param fps_target FPS to hit.
     */
    void set_new_fps_target(const float fps_target)
    {
        fps_period_us_ = roundf((1.0f / (fps_target)) * 1'000'000.0f);
        fps_period_ = (1.0f / fps_target);
    }

    /**
     * @brief Set the magnitude of gravity of all segments to a different
     * value.
     *
     * @param gravity float, can be any number.
     */
    void set_new_gravity(const float gravity)
    {
        for (auto &segment : segment_display_)
        {
            segment.seg_properties_.gravity_.set_magnitude(gravity);
        }
    }

    /**
     * @brief Refresh and update the display (main rendering function)
     *
     * This should be called every frame. It:
     * 1. Clears the framebuffer
     * 2. Draws frame dividers
     * 3. Draws all segments (using dual-core rendering if multiple segments)
     * 4. Updates the physical display
     */
    void refresh_and_update_display()
    {

        graphics_.clear_framebuffer();

        // Draw frame dividers
        graphics_.set_pen(pen_white);
        graphics_.set_depth((uint8_t)255);
        for (int point : frame_points_)
        {
            graphics_.line(pimoroni::Point(point, 0), pimoroni::Point(point, height_));
        }

        float lux_bright = lux_meter_.getAmbientLight();

        i75_.brightness = std::clamp(static_cast<int>(lux_bright * lux_bright / 2.0f), 2, 10);

        uint16_t total_particles = 0;
        for (auto &segment : segment_display_)
        {
            segment.draw_seg(graphics_);
            total_particles += segment.get_total_particle_count();
        }

        total_particle_count_ = total_particles;
        // Update physical display

        i75_.update(&graphics_);

        uint32_t current_time = time_us_32();
        uint32_t delta = current_time - prev_time_;

        if (delta < fps_period_us_)
        {
            uint32_t time_wait = fps_period_us_ - delta;
            sleep_us(time_wait);
        }

        prev_time_ = time_us_32();
    }
};

#endif // WEATHER_DISPLAY_HANDLER_H
