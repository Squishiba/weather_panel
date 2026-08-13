//
// Created by Jared on 6/14/2026.
//

#ifndef WEATHER_FORCE_HANDLER_H
#define WEATHER_FORCE_HANDLER_H

#include "gravity.h" // Gravity, ForceLike, pool_ptr (via forcelike.h)
#include "etl/vector.h"

#include <utility>

// Owns the segment-wide forces. Two read-only consumers depend on it: the
// particle update loop (apply each force) and the spawn manager (which only
// wants the combined resultant direction). It does NOT depend on the segment,
// nor on the concrete force catalog (force_list.h) — callers build forces with
// the factory and hand the finished pool_ptr in. That keeps this header free of
// the force types, which is what lets it live inside DisplaySegProperties
// without a circular include through WindGust.
//
// The two lists are applied to particles IDENTICALLY (see apply_forces); the
// split controls exactly one thing — who feeds calculate_combined(), i.e. the
// spawn edge and spawn velocity:
//   - uniform_forces_     contribute their x_dir/y_dir to the drift resultant.
//                         Truly-uniform forces (gravity, wind field) get this
//                         for free from ForceLike. Position-dependent forces
//                         may ALSO register here if they publish an honest
//                         MEAN push into x_dir/y_dir each update() — e.g.
//                         SimpleWindFront (spawn edge follows each sweep) and
//                         WindStream (coverage-scaled jet drift).
//   - non_uniform_forces_ never contribute to the spawn direction. For purely
//                         rotational forces (WindGust, VortexStreet) this is
//                         physically right: their mean push is ~zero.
// So the registration choice answers one question: "should where/how particles
// SPAWN lean with this force?"
class SegmentWideForceHandler
{

    static constexpr uint16_t kMaxUniform = 16;
    static constexpr uint16_t kMaxNonUniform = 64;

    etl::vector<pool_ptr<ForceLike>, kMaxUniform> uniform_forces_;
    etl::vector<pool_ptr<ForceLike>, kMaxNonUniform> non_uniform_forces_;

    Gravity &gravity_; // Segment gravity — borrowed, mutate via the original.

public:
    explicit SegmentWideForceHandler(Gravity &gravity) : gravity_(gravity)
    {
        // Gravity is uniform and always present. Non-owning borrow: it lives in
        // DisplaySegProperties, so the no-op deleter never frees it.
        uniform_forces_.emplace_back(pool_ptr<ForceLike>(&gravity, [](void *) {}));
    }

    // Store a uniform (field-wide) force. Returns false if full.
    // Contributes to both particle application and the spawn direction.
    bool add_uniform(pool_ptr<ForceLike> force)
    {
        if (uniform_forces_.full()) return false;
        uniform_forces_.push_back(std::move(force));
        return true;
    }

    // Store a non-uniform (localized) force. Returns false if full.
    // Applied per-particle but excluded from the spawn direction.
    bool add_non_uniform(pool_ptr<ForceLike> force)
    {
        if (non_uniform_forces_.full()) return false;
        non_uniform_forces_.push_back(std::move(force));
        return true;
    }

    // Drop every runtime-spawned force, keeping only the permanent gravity
    // borrow (always element 0, emplaced in the ctor). MUST be called when a
    // segment's weather changes: the segment (and this handler) outlives the
    // effects, and effects re-spawn their forces on construction — without
    // this, every weather start stacked ANOTHER copy of each self-spawned
    // force (e.g. gust fronts) into the lists, forever. Destroying the
    // pool_ptrs returns their slots to the shared force pool.
    void clear_spawned()
    {
        while (uniform_forces_.size() > 1)
            uniform_forces_.pop_back();
        non_uniform_forces_.clear();
    }

    // Advance every force's own internal state by dt. Called ONCE per frame
    // (not per particle) before the particle loop runs. Time-varying forces
    // (WindGust motion, SimpleWindFront envelope, etc.) do nothing without this.
    //
    // update() is deliberately not gated on is_alive: a force that has gone
    // dormant may still need to tick — e.g. to count down a respawn timer and
    // bring itself back. The is_alive gate belongs only on apply_forces().
    void update(const float dt)
    {
        for (const auto &force : uniform_forces_)
            force->update(dt);
        for (const auto &force : non_uniform_forces_)
            force->update(dt);
    }

    // Apply every live force to one particle. Kept for one-off callers; the hot
    // path is apply_forces_range() below.
    void apply_forces(Physical &phys, XYZ &coords, Acceleration &accel)
    {
        for (const auto &force : uniform_forces_)
            if (force->is_alive) force->apply_force(phys, coords, accel);
        for (const auto &force : non_uniform_forces_)
            if (force->is_alive) force->apply_force(phys, coords, accel);
    }

    // Force-major hot path: each live force runs ONCE over the whole particle
    // range (see ForceLike::apply_range). Cuts the per-particle virtual
    // dispatch from (particles x forces) indirect calls per frame to (forces)
    // per range, and lets each force's inner loop inline and hoist its
    // per-range work. Called from the effect's update_range() on BOTH cores
    // with disjoint ranges — forces must keep apply_range() pure w.r.t. the
    // force (the parallel_executor.h contract).
    void apply_forces_range(const ParticleRange &range)
    {
        for (const auto &force : uniform_forces_)
            if (force->is_alive) force->apply_range(range);
        for (const auto &force : non_uniform_forces_)
            if (force->is_alive) force->apply_range(range);
    }

    // Sum the live UNIFORM forces into a single resultant vector. This is the bridge
    // value handed to the spawn manager — the average direction particles drift.
    ForceLike calculate_combined() const
    {
        ForceLike combined_force;
        for (const auto &force : uniform_forces_)
            if (force->is_alive)
            {
                combined_force.x_dir += force->x_dir;
                combined_force.y_dir += force->y_dir;
            }
        return combined_force;
    }

    Gravity &get_gravity() const
    {
        return gravity_;
    }
};

#endif //WEATHER_FORCE_HANDLER_H
