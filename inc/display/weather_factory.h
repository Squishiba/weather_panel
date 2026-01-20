#ifndef WEATHER_FACTORY_H
#define WEATHER_FACTORY_H

#include "segment/segment_properties.h"
#include "weather_effect_base.h"
#include "effects/rain.h"
#include "effects/snow.h"
#include "effects/thunderstorm.h"
#include "effects/clouds.h"

#include <vector>
#include <memory>
#include <string>
#include <algorithm>

/**
 * @brief Factory for creating weather effect instances based on weather codes
 *
 * This class analyzes weather data and creates appropriate weather effects
 * to render for a given weather condition.
 */
class WeatherEffectFactory
{
public:
    /**
     * @brief Create weather effects based on weather code and description
     *
     * @param weather_code Tomorrow.io weather code (e.g., 40010 for Rain)
     * @param weather_description Human-readable description
     * @param x_start Segment X start coordinate
     * @param x_end Segment X end coordinate
     * @param height Display height
     * @param precip_type Precipitation type (N/A, Rain, Snow, etc.)
     * @param precip_intensity Precipitation intensity (0.0 to 3.0+)
     * @param snow_accumulation Snow accumulation in mm
     * @param ice_accumulation Ice accumulation in mm
     * @param cloud_cover Cloud cover percentage (0-100)
     * @return Vector of unique pointers to weather effects
     */
    static std::vector<std::unique_ptr<WeatherEffectBase>> create_effects(
        int weather_code,
        const std::string &weather_description,
        DisplaySegProperties &seg_properties,
        const std::string &precip_type,
        float snow_accumulation,
        float ice_accumulation,
        int cloud_cover)
    {
        std::vector<std::unique_ptr<WeatherEffectBase>> effects;

        // Convert description to lowercase for comparison
        std::string desc_lower = weather_description;
        std::transform(desc_lower.begin(), desc_lower.end(), desc_lower.begin(),
                       [](unsigned char c)
                       { return std::tolower(c); });

        // 1. Check for cloud effects (background layer)
        // TODO: Enable clouds when fully implemented
        // if (cloud_cover > 30 || desc_lower.find("cloud") != std::string::npos ||
        //     desc_lower.find("fog") != std::string::npos) {
        //     effects.push_back(std::make_unique<CloudsEffect>(x_start, x_end, height, cloud_cover));
        // }

        // 2. Check for precipitation effects (foreground layer)
        if (precip_type == "Rain" || desc_lower.find("rain") != std::string::npos ||
            desc_lower.find("drizzle") != std::string::npos)
        {
            bool freezing = (desc_lower.find("freezing") != std::string::npos);
            effects.push_back(std::make_unique<weather::RainEffect>(seg_properties, freezing));
        }
        else if (precip_type == "Snow" || desc_lower.find("snow") != std::string::npos ||
                 desc_lower.find("flurries") != std::string::npos)
        {
            effects.push_back(std::make_unique<weather::SnowEffect>(seg_properties, snow_accumulation, false));
        }
        else if (precip_type == "Ice Pellets" || desc_lower.find("ice") != std::string::npos)
        {
            effects.push_back(std::make_unique<weather::SnowEffect>(seg_properties, ice_accumulation, true));
        }
        // 3. Check for thunderstorm effects (top layer)
        if (desc_lower.find("thunderstorm") != std::string::npos)
        {
            float intensity = 1.0f;

            if (desc_lower.find("severe") != std::string::npos)
            {
                intensity += 2.0f;
            }

            if (desc_lower.find("light") != std::string::npos)
            {
                intensity -= 0.5f;
            }

            effects.push_back(std::make_unique<weather::ThunderstormEffect>(seg_properties, intensity));
        }

        // If no effects were added, it's clear weather (just base display will render)

        return effects;
    }

    /**
     * @brief Parse weather code to extract information
     *
     * @param weather_code Tomorrow.io weather code
     * @return Struct with flags for different weather components
     */
    struct WeatherCodeInfo
    {
        bool is_clear;
        bool is_cloudy;
        bool is_fog;
        bool is_rain;
        bool is_snow;
        bool is_freezing;
        bool is_ice;
        bool is_thunderstorm;
    };

    static WeatherCodeInfo parse_weather_code(int weather_code)
    {
        std::string code_str = std::to_string(weather_code);

        WeatherCodeInfo info;
        info.is_clear = (code_str.rfind("100", 0) == 0);
        info.is_cloudy = (code_str.rfind("110", 0) == 0) || (code_str.rfind("102", 0) == 0);
        info.is_fog = (code_str[0] == '2');
        info.is_rain = (code_str[0] == '4');
        info.is_snow = (code_str[0] == '5');
        info.is_freezing = (code_str[0] == '6');
        info.is_ice = (code_str[0] == '7');
        info.is_thunderstorm = (code_str[0] == '8');

        return info;
    }
};

#endif // WEATHER_FACTORY_H
