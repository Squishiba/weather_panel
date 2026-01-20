#ifndef PARTICLE_PROP_H
#define PARTICLE_PROP_H

#include "pico/stdlib.h"

constexpr float UINT8_T_INVERSE = 1.0f / UINT8_MAX;

/**
 * @brief Particle position struct.
 *
 */
struct Position
{
    float x;
    float y;
    float z;
};

/**
 * @brief Particle Velocity struct.
 *
 */
struct Velocity : public Position
{};

/**
 * @brief Particle Acceleration struct.
 */
struct Acceleration : public Position
{};

/**
 * @brief Particle physical properties.
 *
 */
struct Physical
{
    float weight; // Particle weight.
    float drag;   // Particle drag.
    float terminal_velocity;
    float gravity_x_constant;
    float gravity_y_constant;
    uint16_t length; // How long the particle is.
    uint16_t size;   // How fat the particle is.
};

#endif