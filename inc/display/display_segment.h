#ifndef DISPLAY_SEGMENT_H
#define DISPLAY_SEGMENT_H

#include "segment/segment_geometry.h"
#include "base_weather_display.h"
#include "weather_factory.h"
#include "weather_effect_base.h"

#include <vector>
#include <memory>
#include <string>
#include <map>

/**
 * @brief Represents a single segment of the display showing one day's weather
 *
 * This class uses composition: it contains a base display and a list of weather effects.
 * The base display handles common elements (sky, temperature, etc.) while
 * weather effects handle precipitation, clouds, storms, etc.
 * This class also contains important information for particle simulation, where
 * each segment has specific spawning characteristics and physics.
 */
class DisplaySegment
{

    friend class WeatherDisplayHandler;

private:
    DisplaySegProperties seg_properties_;
    std::string weather_state_;
    std::map<std::string, std::string> weather_info_;
    // Composition: Base display + weather effects
    BaseWeatherDisplay base_display_;
    std::vector<std::unique_ptr<WeatherEffectBase>> weather_effects_;
    uint32_t prev_time_ = 0;
    uint16_t particle_count_ = 0;

public:
    DisplaySegment(DisplaySegProperties properties) : seg_properties_(properties), base_display_(seg_properties_)
    {
    }

    /**
     * @brief Update the weather state with new weather data
     *
     * This is called when new weather data arrives from the API.
     * It creates appropriate weather effects based on the data.
     */
    void update_state(const std::map<std::string, std::string> &day_weather)
    {
        weather_info_ = day_weather;

        // Extract weather description
        const auto it = day_weather.find("weather_description");
        weather_state_ = (it != day_weather.end()) ? it->second : "Unknown";

        // Update base display with common data
        const float temperature = get_float_value("temperature", 0.0f);
        const float wind_speed = get_float_value("windSpeed", 0.0f);
        const float wind_direction = get_float_value("windDirection", 0.0f);
        std::string sunrise_time = get_string_value("sunriseTime", "");
        std::string sunset_time = get_string_value("sunsetTime", "");
        std::string day_name = get_string_value("day", "");
        const int cloud_cover = get_int_value("cloudCover", 0);

        base_display_.update_data(temperature, wind_speed, wind_direction,
                                  sunrise_time, sunset_time, day_name, cloud_cover);

        // Clean up old weather effects
        for (const auto &effect : weather_effects_)
        {
            effect->stop();
        }
        weather_effects_.clear();

        // Create new weather effects based on current weather
        int weather_code = get_int_value("weatherCodeDay", 10000); // Default to clear
        std::string precip_type = get_string_value("precipitationType", "N/A");
        float precip_intensity = get_float_value("precipitationIntensity", 0.0f);
        float snow_accumulation = get_float_value("snowAccumulation", 0.0f);
        float ice_accumulation = get_float_value("iceAccumulation", 0.0f);

        seg_properties_.set_intensity(precip_intensity);

        weather_effects_ = WeatherEffectFactory::create_effects(
            weather_code, weather_state_, seg_properties_,
            precip_type, snow_accumulation,
            ice_accumulation, cloud_cover);
    }

    /**
     * @brief Draw this segment to the display
     *
     * Rendering order:
     * 1. Update particles (physics, spawning, cleanup)
     * 2. Base display (sky, temperature, wind, day label)
     * 3. Weather effects in order (clouds, precipitation, storms)
     */
    void draw_seg(pimoroni::PicoZGraphics &graphics)
    {

        const uint32_t current_time = time_us_32();
        const uint32_t delta = current_time - prev_time_;
        seg_properties_.set_dt((delta / 1'000'000.0f));

        // Update particle positions before drawing
        uint16_t particle_count_temp = 0;
        for (const auto &effect : weather_effects_)
        {
            effect->update_particles();
            particle_count_temp += effect->get_particle_count();
        }

        prev_time_ = time_us_32();

        particle_count_ = particle_count_temp;

        // Draw base display first
        base_display_.draw(graphics);

        // Then draw each weather effect in order
        for (const auto &effect : weather_effects_)
        {
            effect->draw(graphics);
        }
    }

    [[nodiscard]] const std::string &weather_state() const { return weather_state_; }
    [[nodiscard]] uint16_t get_total_particle_count() const { return particle_count_; }
    [[nodiscard]] RectMod seg_bounds() const { return seg_properties_.get_seg_bounds(); }
    DisplaySegProperties &get_segment_properties() { return seg_properties_; }

private:
    // Helper functions to extract typed values from weather_info_
    // Simple string to float conversion without exceptions
    static float safe_stof(const std::string &str, float default_val)
    {
        if (str.empty())
            return default_val;

        char *end;
        float result = strtof(str.c_str(), &end);

        // If no conversion happened or string was invalid, return default
        if (end == str.c_str() || *end != '\0')
        {
            return default_val;
        }
        return result;
    }

    // Simple string to int conversion without exceptions
    static int safe_stoi(const std::string &str, int default_val)
    {
        if (str.empty())
            return default_val;

        char *end;
        long result = strtol(str.c_str(), &end, 10);

        // If no conversion happened or string was invalid, return default
        if (end == str.c_str() || *end != '\0')
        {
            return default_val;
        }
        return static_cast<int>(result);
    }

    float get_float_value(const std::string &key, float default_val) const
    {
        auto it = weather_info_.find(key);
        if (it != weather_info_.end())
        {
            return safe_stof(it->second, default_val);
        }
        return default_val;
    }

    int get_int_value(const std::string &key, int default_val) const
    {
        auto it = weather_info_.find(key);
        if (it != weather_info_.end())
        {
            return safe_stoi(it->second, default_val);
        }
        return default_val;
    }

    std::string get_string_value(const std::string &key, const std::string &default_val) const
    {
        auto it = weather_info_.find(key);
        return (it != weather_info_.end()) ? it->second : default_val;
    }
};

#endif // DISPLAY_SEGMENT_H
