#ifndef HELP_RAN_H
#define HELP_RAN_H

#include "pico/rand.h"
#include <cmath>
#include <array>
#include <algorithm>
#include <random>

constexpr float kUint32MaxInv = 1.0f / UINT32_MAX;
constexpr float kUint16MaxInv = 1.0f / UINT16_MAX;

class RNG_Base
{
public:
    virtual float get_rand_float(float start, float end) = 0;
    virtual float get_rand_float() = 0;
    virtual uint32_t get_rand_u32(uint32_t start, uint32_t end) = 0;
};

/**
 * @brief Hardware supported random number generation class.
 * Uses the pi pico's TRNG to generate random numbers.
 */
class H_RNG : public RNG_Base
{

    H_RNG() {}


    /**
     * @brief Get a random float value between the two given.
     * @param start Starting float.
     * @param end Ending float.
     * @return float
     */
    float get_rand_float(float start, float end) override
    {
        if (start == end)
        {
            return start;
        }
        start = std::clamp(start, -(MAXFLOAT), end);
        end = std::clamp(end, start, MAXFLOAT);
        uint32_t tmp = get_rand_32();
        float slope = (float)(end - start) / UINT32_MAX;
        return start + (slope * (tmp));
    }


    /**
     * @brief Get a random float between 0 and 1.0f.
     * @return float
     */
    float get_rand_float() override
    {
        return static_cast<float>(get_rand_32()) / UINT32_MAX;
    }


    /**
     * @brief Gets a random number between the two values given.
     * @param start Starting num.
     * @param end  Ending num.
     * @return uint32_t
     */
    uint32_t get_rand_u32(uint32_t start, uint32_t end) override
    {
        if (start == end)
        {
            return start;
        }
        start = std::clamp(start, static_cast<uint32_t>(0), end);
        end = std::clamp(end, start, UINT32_MAX);
        uint32_t tmp = get_rand_32();
        float slope = (float)(end - start) / UINT32_MAX;
        return start + roundf(slope * (tmp));
    }
};

/**
 * @brief Software based pseudo random number generator.
 * Does use the pi pico's TRNG for initial seed value, else, all software based.
 */
class S_RNG : public RNG_Base
{

    std::random_device rd;
    std::mt19937 rng{static_cast<std::mt19937::result_type>(get_rand_32())};
    std::uniform_int_distribution<uint32_t> distribution;
    std::uniform_real_distribution<float> distribution_f;

public:
    S_RNG()
    {
        distribution = std::uniform_int_distribution<uint32_t>(0, UINT32_MAX);
        distribution_f = std::uniform_real_distribution<float>(0, 1.0f);
    }

    /**
     * @brief Get the rand float, mapped to between start and end.
     *
     * @param start
     * @param end
     * @return float
     */
    float get_rand_float(float start, float end) override
    {
        if (start == end)
        {
            return start;
        }
        start = std::clamp(start, -(MAXFLOAT), end);
        end = std::clamp(end, start, MAXFLOAT);
        std::uniform_real_distribution<float> distribf(start, end);
        return distribf(rng);
    }

    /**
     * Get a random float between 0 and 1.0f.
     * @return
     */
    float get_rand_float() override
    {
        return distribution_f(rng);
    }

    /**
     * @brief Get a random number between the two values given.
     *
     * @param start First number.
     * @param end Second number.
     * @return uint32_t
     */
    uint32_t get_rand_u32(uint32_t start, uint32_t end) override
    {
        if (start == end)
        {
            return start;
        }
        start = std::clamp(start, static_cast<uint32_t>(0), end);
        end = std::clamp(end, start, UINT32_MAX);
        std::uniform_int_distribution<uint32_t> distribution(start, end);
        return distribution(rng);
    }
    uint32_t get_rand_u32()
    {
        return distribution(rng);
    }
};


extern S_RNG rng_class;


#endif