//
// Created by Jared on 5/24/2026.
//

#ifndef WEATHER_FORCE_LIST_H
#define WEATHER_FORCE_LIST_H

#include "typelist.hpp"
#include "gravity.hpp"
#include "wind_gusts.hpp"
#include "wind_field_force.hpp"
#include "simple_wind_front.hpp"
#include "wind_stream.hpp"
#include "vortex_street.hpp"

// Catalogue of every concrete force that can be spawned from the shared force
// pool (see ForceFactory / spawn_uniform_force / spawn_local_force). To make a
// new force type spawnable at runtime, define it and add it to this list — the
// pool is sized automatically to the largest type here.
using force_types = tl::type_list<Gravity, WindGust, WindFieldForce, SimpleWindFront, WindStream, VortexStreet>;

#endif //WEATHER_FORCE_LIST_H