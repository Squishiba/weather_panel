//
// Created by Jared on 1/22/2026.
//

#ifndef WEATHER_PROFILER_H
#define WEATHER_PROFILER_H


#include "pico/time.h"
#include "hardware/pwm.h"
#include <vector>

class Profiler
{

    static constexpr uint32_t slice_num = pwm_gpio_to_slice_num(0);
    std::vector<std::string> logs;

    pwm_config pwm;

    Profiler()
    {
        pwm = pwm_get_default_config();
    }
};


#endif //WEATHER_PROFILER_H