//
// Created by Jared on 6/19/2026.
//

#ifndef WEATHER_TRIG_FUNCS_H
#define WEATHER_TRIG_FUNCS_H

#include "gcem.hpp"

template <size_t num_points> constexpr std::array<float, num_points> generate_sine_table()
{
    std::array<float, num_points> vals;
    uint32_t index = 0;
    for (auto &itr : vals)
    {
        itr = gcem::sin(index * M_PI / num_points);
        index++;
    }
    return vals;
}

template <size_t num_points> constexpr std::array<float, num_points> generate_cos_table()
{
    std::array<float, num_points> vals;
    uint32_t index = 0;
    for (auto &itr : vals)
    {
        itr = gcem::cos(index * M_PI / num_points);
        index++;
    }
    return vals;
}

/**
 * @brief Full-period (2*pi) sine table, for phase-wrapped waveform lookups.
 *
 * The half-period tables above suit one-shot envelopes (0..pi); a traveling
 * wave needs the full cycle plus cheap wraparound. Pair with wave_lookup(),
 * which maps a phase in CYCLES (1.0 = one full period, any sign) to sin(2*pi*t)
 * with an AND-mask wrap — so num_points must be a power of two.
 */
template <size_t num_points> constexpr std::array<float, num_points> generate_full_sine_table()
{
    static_assert((num_points & (num_points - 1)) == 0, "table size must be a power of two (mask wrap)");
    std::array<float, num_points> vals;
    uint32_t index = 0;
    for (auto &itr : vals)
    {
        itr = gcem::sin(index * 2.0 * M_PI / num_points);
        index++;
    }
    return vals;
}

/**
 * @brief sin(2*pi*cycles) via a full-period table. ~4 ops, no libm.
 *
 * Truncation toward zero mis-wraps negative phases by at most one table step
 * (< 0.4 degrees at 1024 entries) — irrelevant for wind, so no floorf.
 */
template <size_t N> inline float wave_lookup(const std::array<float, N> &table, const float cycles)
{
    return table[static_cast<int32_t>(cycles * static_cast<float>(N)) & (N - 1)];
}


#endif //WEATHER_TRIG_FUNCS_H