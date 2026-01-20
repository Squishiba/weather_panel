#ifndef BASE_WEATHER_DISPLAY_H
#define BASE_WEATHER_DISPLAY_H

#include "z_buffer.h"
#include "segment/segment_properties.h"
#include <string>

/**
 * @brief Base weather display that handles common elements across all weather types
 *
 * This class renders:
 * - Sky gradient (based on time of day)
 * - Temperature text
 * - Wind direction/speed indicators
 * - Day of week label
 */
class BaseWeatherDisplay
{
private:
    DisplaySegProperties &seg_properties_;
    // Weather data
    float temperature_;
    float wind_speed_;
    float wind_direction_;
    std::string sunrise_time_;
    std::string sunset_time_;
    std::string day_name_;
    int cloud_cover_;

public:
    BaseWeatherDisplay(DisplaySegProperties &segment)
        : seg_properties_(segment), temperature_(0), wind_speed_(0), wind_direction_(0), cloud_cover_(0)
    {
    }

    void update_data(const float temperature, const float wind_speed, const float wind_direction,
                     const std::string &sunrise_time, const std::string &sunset_time,
                     const std::string &day_name, const int cloud_cover)
    {
        temperature_ = temperature;
        wind_speed_ = wind_speed;
        wind_direction_ = wind_direction;
        sunrise_time_ = sunrise_time;
        sunset_time_ = sunset_time;
        day_name_ = day_name;
        cloud_cover_ = cloud_cover;
    }

    void draw(pimoroni::PicoZGraphics &graphics)
    {
        // draw_sky_gradient(graphics);
        // draw_temperature(graphics);
        // draw_wind_indicator(graphics);
        // draw_day_label(graphics);
    }

private:
    void draw_sky_gradient(pimoroni::PicoZGraphics &graphics) const {
        // TODO: Implement sky gradient based on time of day
        // For now, draw a simple blue sky
        pimoroni::Pen sky_pen = graphics.create_pen(100, 150, 255);
        graphics.set_pen(sky_pen);

        const RectMod bounds = seg_properties_.get_seg_bounds();

        graphics.rectangle(bounds);
    }

    void draw_temperature(pimoroni::PicoZGraphics &graphics)
    {
        // TODO: Implement temperature display
        // Need text rendering support
    }

    void draw_wind_indicator(pimoroni::PicoZGraphics &graphics)
    {
        // TODO: Implement wind indicator
        // Could draw an arrow showing direction and speed
    }

    void draw_day_label(pimoroni::PicoZGraphics &graphics)
    {
        // TODO: Implement day label
        // Need text rendering support
    }
};

#endif // BASE_WEATHER_DISPLAY_H
