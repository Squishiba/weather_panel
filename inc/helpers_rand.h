#ifndef HELP_RAN_H
#define HELP_RAN_H

#include "pico/rand.h"
#include <cmath>
#include <array>

constexpr float kUint32MaxInv = 1.0f / UINT32_MAX;
constexpr float kUint16MaxInv = 1.0f / UINT16_MAX;

class RNG
{
    static constexpr uint16_t kRNGTableSize = 512;
    std::array<uint16_t, kRNGTableSize> rng_table_;
    uint16_t table_counter_;

public:
    RNG()
    {
        for (auto it = rng_table_.begin(); it < rng_table_.end(); it += 4)
        {
            uint64_t value = get_rand_64();
            *it = value & 0xFFFF;               // Bits 0-15
            *(it + 1) = (value >> 16) & 0xFFFF; // Bits 16-31
            *(it + 2) = (value >> 32) & 0xFFFF; // Bits 32-47
            *(it + 3) = (value >> 48) & 0xFFFF; // Bits 48-63
        }
    }

    /**
     * @brief Get a random float between 0 and 1.0f.
     *
     * @return float
     */
    float get_random_float()
    {
        table_counter_ = (table_counter_ + 1) % (kRNGTableSize - 1);
        return (rng_table_[table_counter_] * kUint16MaxInv);
    }

    uint16_t get_random_uint16()
    {
        table_counter_ = (table_counter_ + 1) % (kRNGTableSize - 1);
        return (rng_table_[table_counter_]);
    }

    /**
     * @brief Get a random float between the two values given.
     *
     * @param start
     * @param end
     * @return float
     */
    float get_float_interval(float start, float end)
    {
        float rng = get_random_float();
        start = std::clamp(start, 0.0f, end);
        end = std::clamp(end, start, MAXFLOAT);
        float slope = (end - start);
        return (start + (slope * rng));
    }
};

// extern RNG rng_class;

/**
 * @brief Get a random float between 0 and 1.0f
 *
 * @return float
 */
inline float get_rand_float()
{
    return (get_rand_32()) / static_cast<float>(UINT32_MAX);
}

/**
 * @brief Get the rand float, mapped to between start and end.
 *
 * @param start
 * @param end
 * @return float
 */
inline float get_rand_float(float start, float end)
{
    const uint32_t num = get_rand_32();
    start = std::clamp(start, std::numeric_limits<float>::min(), end);
    end = std::clamp(end, start, MAXFLOAT);
    const float slope = (float)(end - start) / (UINT32_MAX);
    return (start + (slope * num));
}

/**
 * @brief Get a random number between the two values given.
 *
 * @param start First number.
 * @param end Second number.
 * @return uint32_t
 */
inline uint32_t get_rand_uint32(uint32_t start, uint32_t end)
{
    const uint32_t num = get_rand_32();
    start = std::clamp(start, (uint32_t)0, end);
    end = std::clamp(end, start, UINT32_MAX);
    const float slope = (float)(end - start) / UINT32_MAX;
    return (start + roundf(slope * (num)));
}

#endif