#ifndef PARTICLE_PROP_H
#define PARTICLE_PROP_H

#include "pico/stdlib.h"
#include "geometry/geometry.h"

#include <cstdint>

/**
 * @brief 16-bit storage float for "cold" per-particle scalars.
 *
 * Use this for values that are set once (at spawn/reset) and only read back
 * afterwards, where full fp32 precision is wasted. Arithmetic still happens in
 * fp32 — the Cortex-M33 FPU converts float16<->float with single hardware VCVT
 * instructions (verified: no soft-float calls), so a `float16` field just halves
 * the storage. fp16 gives ~3 significant digits / max +/-65504.
 *
 * Requires -mfp16-format=ieee, set on the weather target in CMakeLists.txt.
 * Falls back to plain float if that flag is ever removed, so the code still
 * builds (just without the size saving).
 *
 * Do NOT use for accumulators (position, velocity) — those need fp32 range and
 * precision across frames.
 */
#if defined(__ARM_FP16_FORMAT_IEEE)
using float16 = __fp16;
#else
using float16 = float;
#endif

/**
 * @brief Particle Velocity struct.
 *
 */
struct Velocity : public XYZ
{
};

/**
 * @brief Particle Acceleration struct.
 */
struct Acceleration : public XYZ
{
};

/**
 * @brief Particle physical properties.
 *
 * The scalar properties are stored as `float16` (16-bit): each is a constant for a
 * particle's life (assigned in reset()/spawn()) and read back in fp32, so fp16's
 * precision is ample for drag (~0.1-0.3), mass (~0.3-4), inv_weight (~0.25-3.3)
 * and a 0-1 hardness/bounce coefficient. This packs Physical to 12 bytes (from
 * 20). Positions/velocities live in ParticleLike and stay fp32.
 */
struct Physical
{
    float16 mass = 0;       // Particle mass.
    float16 drag = 0;       // Particle drag.
    float16 hardness = 0;   // How "hard" the particle is (bounce coefficient).
    float16 inv_weight = 0; // Inverse mass (1/mass), precomputed.
    uint8_t length = 1;     // How long the particle is.
    uint8_t size = 1;       // How fat the particle is.
};

#endif