#ifndef MISC_H
#define MISC_H

extern "C"
{
#include "arm_math.h"
}

#include "gcem.hpp"

#include "pico/stdlib.h"

#include <string>
#include <map>

// Days of week
constexpr std::string DAYS_OF_WEEK[] = {
    "Saturday", "Sunday", "Monday", "Tuesday",
    "Wednesday", "Thursday", "Friday"};

// Cardinal directions
const std::string DIRS[] = {
    "N", "NE", "E", "SE", "S", "SW", "W", "NW"};

// Precipitation type mapping
inline std::string precip_type_parser(int code)
{
    static const std::map<int, std::string> precipitationType = {
        {0, "N/A"},
        {1, "Rain"},
        {2, "Snow"},
        {3, "Freezing Rain"},
        {4, "Ice Pellets"}};
    auto it = precipitationType.find(code);
    return (it != precipitationType.end()) ? it->second : "N/A";
}

// Weather code mapping (Tomorrow.io codes)
inline std::string weather_code_parser(int code)
{
    static const std::map<int, std::string> weatherCodeDay = {
        {0, "Unknown"},
        {10000, "Clear, Sunny"},
        {11000, "Mostly Clear"},
        {11010, "Partly Cloudy"},
        {11020, "Mostly Cloudy"},
        {10010, "Cloudy"},
        {11030, "Partly Cloudy and Mostly Clear"},
        {21000, "Light Fog"},
        {21010, "Mostly Clear and Light Fog"},
        {21020, "Partly Cloudy and Light Fog"},
        {21030, "Mostly Cloudy and Light Fog"},
        {21060, "Mostly Clear and Fog"},
        {21070, "Partly Cloudy and Fog"},
        {21080, "Mostly Cloudy and Fog"},
        {20000, "Fog"},
        {42040, "Partly Cloudy and Drizzle"},
        {42030, "Mostly Clear and Drizzle"},
        {42050, "Mostly Cloudy and Drizzle"},
        {40000, "Drizzle"},
        {42000, "Light Rain"},
        {42130, "Mostly Clear and Light Rain"},
        {42140, "Partly Cloudy and Light Rain"},
        {42150, "Mostly Cloudy and Light Rain"},
        {42090, "Mostly Clear and Rain"},
        {42080, "Partly Cloudy and Rain"},
        {42100, "Mostly Cloudy and Rain"},
        {40010, "Rain"},
        {42110, "Mostly Clear and Heavy Rain"},
        {42020, "Partly Cloudy and Heavy Rain"},
        {42120, "Mostly Cloudy and Heavy Rain"},
        {42010, "Heavy Rain"},
        {51150, "Mostly Clear and Flurries"},
        {51160, "Partly Cloudy and Flurries"},
        {51170, "Mostly Cloudy and Flurries"},
        {50010, "Flurries"},
        {51000, "Light Snow"},
        {51020, "Mostly Clear and Light Snow"},
        {51030, "Partly Cloudy and Light Snow"},
        {51040, "Mostly Cloudy and Light Snow"},
        {51220, "Drizzle and Light Snow"},
        {51050, "Mostly Clear and Snow"},
        {51060, "Partly Cloudy and Snow"},
        {51070, "Mostly Cloudy and Snow"},
        {50000, "Snow"},
        {51010, "Heavy Snow"},
        {51190, "Mostly Clear and Heavy Snow"},
        {51200, "Partly Cloudy and Heavy Snow"},
        {51210, "Mostly Cloudy and Heavy Snow"},
        {51100, "Drizzle and Snow"},
        {51080, "Rain and Snow"},
        {51140, "Snow and Freezing Rain"},
        {51120, "Snow and Ice Pellets"},
        {60000, "Freezing Drizzle"},
        {60030, "Mostly Clear and Freezing drizzle"},
        {60020, "Partly Cloudy and Freezing drizzle"},
        {60040, "Mostly Cloudy and Freezing drizzle"},
        {62040, "Drizzle and Freezing Drizzle"},
        {62060, "Light Rain and Freezing Drizzle"},
        {62050, "Mostly Clear and Light Freezing Rain"},
        {62030, "Partly Cloudy and Light Freezing Rain"},
        {62090, "Mostly Cloudy and Light Freezing Rain"},
        {62000, "Light Freezing Rain"},
        {62130, "Mostly Clear and Freezing Rain"},
        {62140, "Partly Cloudy and Freezing Rain"},
        {62150, "Mostly Cloudy and Freezing Rain"},
        {60010, "Freezing Rain"},
        {62120, "Drizzle and Freezing Rain"},
        {62200, "Light Rain and Freezing Rain"},
        {62220, "Rain and Freezing Rain"},
        {62070, "Mostly Clear and Heavy Freezing Rain"},
        {62020, "Partly Cloudy and Heavy Freezing Rain"},
        {62080, "Mostly Cloudy and Heavy Freezing Rain"},
        {62010, "Heavy Freezing Rain"},
        {71100, "Mostly Clear and Light Ice Pellets"},
        {71110, "Partly Cloudy and Light Ice Pellets"},
        {71120, "Mostly Cloudy and Light Ice Pellets"},
        {71020, "Light Ice Pellets"},
        {71080, "Mostly Clear and Ice Pellets"},
        {71070, "Partly Cloudy and Ice Pellets"},
        {71090, "Mostly Cloudy and Ice Pellets"},
        {70000, "Ice Pellets"},
        {71050, "Drizzle and Ice Pellets"},
        {71060, "Freezing Rain and Ice Pellets"},
        {71150, "Light Rain and Ice Pellets"},
        {71170, "Rain and Ice Pellets"},
        {71030, "Freezing Rain and Heavy Ice Pellets"},
        {71130, "Mostly Clear and Heavy Ice Pellets"},
        {71140, "Partly Cloudy and Heavy Ice Pellets"},
        {71160, "Mostly Cloudy and Heavy Ice Pellets"},
        {71010, "Heavy Ice Pellets"},
        {80010, "Mostly Clear and Thunderstorm"},
        {80030, "Partly Cloudy and Thunderstorm"},
        {80020, "Mostly Cloudy and Thunderstorm"},
        {80000, "Thunderstorm"}};

    auto it = weatherCodeDay.find(code);
    return (it != weatherCodeDay.end()) ? it->second : "Unknown";
}

/**
 * @brief Convert degrees to cardinal directions.
 *
 * @param deg
 * @return std::string
 */
inline std::string deg_to_cardinal(float deg)
{
    // Normalize so 360 maps back to 0
    deg = fmod(deg, 360.0f);
    // Add 22.5 to push boundaries to middle of slice
    int idx = static_cast<int>((deg + 22.5f) / 45.0f) % 8;
    return DIRS[idx];
}

template <typename T>
inline T map_values(T val, T left_min, T left_max, T right_min, T right_max)
{
    T left_span = left_max - left_min;
    T right_span = right_max - right_min;
    T val_scaled = (val - left_min) / left_span;
    return right_min + (val_scaled * right_span);
}

/**
 * @brief Pregenerate a sin waveform table.
 *
 * @tparam num_var Number of points.
 * @param num_periods Number of periods in table.
 * @param randomness Amplitude of table.
 * @return constexpr std::array<float, num_var>
 */
template <size_t num_var>
constexpr std::array<float, num_var> gen_sin_table(const float &num_periods, const float &offset, const float &amplitude)
{
    std::array<float, num_var> array = {0};
    float per_val = (static_cast<float>(num_var) / (num_periods + 1));
    int index = 0;
    for (auto &itr : array)
    {
        const float angle = (PI * static_cast<float>(index)) / per_val;
        itr = (gcem::sin(angle) * amplitude) + offset;
        index++;
    }
    return array;
}

/**
 * @brief Pregenerate a cos waveform table.
 *
 * @tparam num_var Number of points.
 * @param num_periods Number of periods in table.
 * @param randomness Amplitude of table.
 * @return constexpr std::array<float, num_var>
 */
template <size_t num_var>
constexpr std::array<float, num_var> gen_cos_table(const float &num_periods, const float &offset, const float &amplitude)
{
    std::array<float, num_var> array = {0};
    float per_val = (static_cast<float>(num_var) / (num_periods + 1));
    int index = 0;
    for (auto &itr : array)
    {
        const float angle = (PI * static_cast<float>(index)) / per_val;
        itr = (gcem::cos(angle) * amplitude) + offset;
        index++;
    }
    return array;
}

#endif // MISC_H
