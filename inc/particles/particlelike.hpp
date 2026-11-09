#ifndef PARTICLE_H
#define PARTICLE_H

#include "../utility/helpers_rand.hpp"
#include "particle_base.hpp"
#include "particle_tuning.hpp"
#include "display/z_buffer.hpp"

class Rain : public ParticleLike
{
public:
    // drift = segment's combined uniform-force resultant; the particle spawns
    // moving along it (see ParticleLike::spawn) so it enters at the wind's angle.
    explicit Rain(const XYZ &spawn_coords, const ForceLike &drift, const Color particle_color) : ParticleLike(spawn_coords, particle_color, rain_tuning)
    {
        // Bind to the shared rain tuning (above) then spawn with drift-aligned velocity.
        spawn(drift);
    }
};

class Snow : public ParticleLike
{
public:
    // drift = segment's combined uniform-force resultant; see Rain above.
    explicit Snow(const XYZ &spawn_coords, const ForceLike &drift, const Color particle_color) : ParticleLike(spawn_coords, particle_color, snow_tuning)
    {
        // Bind to the shared snow tuning (above) then spawn with drift-aligned velocity.
        spawn(drift);
    }
};


#endif // PARTICLE_H
