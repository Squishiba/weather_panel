#ifndef PARTICLE_TUNING_H
#define PARTICLE_TUNING_H

struct ParticleTuning
{
    float drag_min, drag_max;
    float mass_min, mass_max;
    float spawn_vx = 0.f; // extra x-velocity added at spawn (wind bias)
    float spawn_vy = 0.f; // extra y-velocity added at spawn
};

// Defaults match the original hardcoded values in particle.h so firmware
// behaviour is unchanged unless something writes to these at runtime.
inline ParticleTuning rain_tuning{0.1f, 0.23f, 2.8f, 4.0f};
inline ParticleTuning snow_tuning{0.15f, 0.30f, 0.30f, 0.5f};

#endif // PARTICLE_TUNING_H
