#ifndef WEATHER_EFFECT_BASE_H
#define WEATHER_EFFECT_BASE_H

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>

#include "../display/segment/segment_properties.h"
#include "../display/z_buffer.h"
#include "../utility/fixed_unique_factory.h"
#include "../utility/parallel_executor.h"
#include "physics/force_list.h"
#include "particles/particle_types.h"
#include "particles/particle_arena.h"

#include "etl/vector.h"
#include "pfr.hpp"


// Shared, fixed-capacity object pool backing every runtime-spawned force.
// Sized to the largest type in force_types; all segments/effects draw from this
// one pool. To make a new force type spawnable through spawn_uniform_force() /
// spawn_local_force(), add it to force_types in physics/force_list.h.
inline constexpr size_t kForcePoolCapacity = 64;
using ForceFactory = typename UnpackFactory<kForcePoolCapacity, force_types>::type;

// Master switch for the SELF-SPAWNING ambient wind features (gust fronts today;
// wind-coupled streams if/when they're wired in). Effects consult it at
// construction, so flipping it takes effect on the next weather (re)start.
// Exists so profiling can get a gravity-only baseline at runtime — the debug
// console's `winds` command toggles it — without reflashing a special build.
inline bool g_ambient_winds_enabled = true;


/**
 * @brief Interface for all weather effects.
 * Any effect (particle-based or not) must implement this interface.
 */
class IWeatherEffect
{

public:
    virtual ~IWeatherEffect() = default;
    virtual void update() = 0;
    virtual uint16_t get_particle_count() const { return 0; }
    virtual void draw(PicoZGraphicsBase &graphics) = 0;
    virtual void stop() = 0;

    // Debug only: when true, draw particles even if they're outside the segment's
    // drawing bounds (used by the desktop sim to visualise the spawn/OOB region).
    virtual void set_debug_draw_all(bool /*on*/) {}
};

/**
 * @brief CRTP base for particle-based weather effects.
 *
 * ParticleType is the concrete particle class (Rain, Snow, etc.) — stored directly
 * by value so the compiler can resolve update(), reset(), and draw_effects() without
 * virtual dispatch.
 *
 * Storage comes from the shared ParticleArena, not an inline fixed vector: the
 * derived constructor passes up the DESIRED particle count, and the base grabs a
 * contiguous block for min(desired, per-segment fair share, space actually free).
 * max_particles_ is set to what the pool granted, so total particle RAM is bounded
 * by kParticlePoolBytes no matter how many segments spawn effects. The block is
 * returned to the pool in the destructor.
 *
 * Derived classes override draw_effects(PicoZGraphicsBase&, ParticleType&) as a plain
 * (non-virtual) method. If they don't, the default single-pixel draw is used.
 */
template<class Derived, class ParticleType>
class ParticleEffectBase : public IWeatherEffect
{

protected:

    static constexpr float kSpawnRateDivider = 3.0f;

    // A block borrowed from the shared ParticleArena (declared before particles_
    // and max_particles_ so both can be initialised from it).
    struct Storage
    {
        void *ptr = nullptr;
        size_t count = 0;
    };

    DisplaySegProperties &seg_properties_;
    Storage block_;
    etl::vector_ext<ParticleType> particles_; // lives in block_, capacity = block_.count
    float spawn_rate_;
    uint16_t max_particles_;
    Color particle_color_;
    bool debug_draw_all_ = false; // sim debug: bypass the in-segment draw clip.

    // Reserve pool space for this effect. Granted = min(what it wants, a fair
    // per-segment share, whatever contiguous space is free). Returns {nullptr, 0}
    // if the pool is exhausted — the effect then simply renders nothing.
    static Storage acquire_block(size_t desired)
        {
        const size_t fair  = (kParticlePoolBytes / kParticlePoolFairShareSegments) / sizeof(ParticleType);
        const size_t avail = particle_arena().largest_free() / sizeof(ParticleType);
        const size_t granted = std::min(std::min(desired, fair), avail);

        Storage s;
        if (granted > 0)
            {
            s.ptr = particle_arena().allocate(granted * sizeof(ParticleType));
            s.count = (s.ptr != nullptr) ? granted : 0;
            }
        return s;
        }

    // desired_particles: the count the effect would like; the pool decides the rest.
    ParticleEffectBase(DisplaySegProperties &seg_props, uint16_t desired_particles)
        : seg_properties_(seg_props)
        , block_(acquire_block(desired_particles))
        , particles_(block_.ptr, block_.count)
        , spawn_rate_(seg_props.get_intensity() / kSpawnRateDivider)
        , max_particles_(static_cast<uint16_t>(block_.count))
        {
        }

    // Default draw: single pixel. Override in Derived with the same signature
    // (non-virtual) to customise per-particle rendering.
    void draw_effects(PicoZGraphicsBase &graphics, ParticleType &particle)
        {
        graphics.pixel(pimoroni::Point(particle.get_positions().x, particle.get_positions().y));
        }

    // Forces are segment-wide (owned by DisplaySegProperties), so registering one
    // here makes it apply for every effect sharing this segment.
    //
    // These low-level overloads take an already-built pool_ptr. Prefer the
    // spawn_uniform_force()/spawn_local_force() helpers below, which build the
    // force from the shared pool and register it in a single call.
    bool add_uniform_force(pool_ptr<ForceLike> force) const
        {
        const bool ok = seg_properties_.force_handler.add_uniform(std::move(force));
        seg_properties_.refresh_spawn_edge(); // a uniform force changes the drift direction
        return ok;
        }

    bool add_non_uniform_force(pool_ptr<ForceLike> force) const
        {
        return seg_properties_.force_handler.add_non_uniform(std::move(force));
        }

    /**
     * @brief Build a uniform (field-wide) force in the shared pool and register it.
     *
     * Uniform forces act on every particle identically and feed the spawn-edge
     * (drift) direction. The type must be listed in force_types
     * (physics/force_list.h). The returned observer pointer stays valid for the
     * force's lifetime — the pool slot is stable — so it can be used to tune the
     * force at runtime (e.g. change magnitude). One-line usage:
     * @code
     *   spawn_uniform_force<WindFieldForce>(90.0f, 4.0f); // dir, mag
     * @endcode
     *
     * @tparam ForceT Concrete force type (must be in force_types).
     * @param args Constructor arguments forwarded to ForceT.
     * @return Non-owning pointer to the spawned force, or nullptr if the
     *         segment's uniform-force table is full.
     */
    template<class ForceT, class... Args>
    ForceT *spawn_uniform_force(Args &&...args) const
        {
        auto owning = ForceFactory::template make_unique<ForceT>(std::forward<Args>(args)...);
        ForceT *observer = owning.get();
        if (!seg_properties_.force_handler.add_uniform(std::move(owning)))
            return nullptr; // table full — owning frees its pool slot on return.
        seg_properties_.refresh_spawn_edge(); // a new uniform force changes the drift direction.
        return observer;
        }

    /**
     * @brief Build a non-uniform (localized) force in the shared pool and register it.
     *
     * Localized forces (e.g. gusts within a radius) are applied per-particle
     * but never affect the spawn direction. Same ownership and return
     * semantics as spawn_uniform_force().
     *
     * @tparam ForceT Concrete force type (must be in force_types).
     * @param args Constructor arguments forwarded to ForceT.
     * @return Non-owning pointer to the spawned force, or nullptr if the
     *         segment's non-uniform-force table is full.
     */
    template<class ForceT, class... Args>
    ForceT *spawn_local_force(Args &&...args) const
        {
        auto owning = ForceFactory::template make_unique<ForceT>(std::forward<Args>(args)...);
        ForceT *observer = owning.get();
        if (!seg_properties_.force_handler.add_non_uniform(std::move(owning)))
            return nullptr; // table full — owning frees its pool slot on return.
        return observer;
        }

public:

    ~ParticleEffectBase() override
        {
        // vector_ext destroys the live particles but never frees the backing
        // memory (it doesn't own it). Clear first so element destructors run
        // while the block is still valid, then hand the block back to the pool.
        particles_.clear();
        if (block_.ptr != nullptr)
            particle_arena().deallocate(block_.ptr);
        }

    // Cap on how many out-of-bounds particles one core records per frame. The
    // steady-state recycle flux is a few dozen per frame at most; anything that
    // overflows this (e.g. the frame after a gravity flip throws hundreds OOB
    // at once) is simply caught on a later frame — OOB particles are invisible
    // (the draw clip skips them), so deferring their respawn has no visual cost.
    static constexpr uint16_t kMaxOobMarks = 64;

    // Per-particle work for one index range, type-erased so &update_range can be
    // handed to core 1 as a plain function pointer (see parallel_executor.h).
    // Both cores run this same body on disjoint ranges.
    //
    // Each context also collects the indices of particles that ended the step
    // out of bounds. The OOB *test* is a pure position check, so it's safe on
    // the parallel path; only the RNG-driven *respawn* must stay serial, and it
    // now touches just these recorded few instead of scanning every particle.
    struct UpdateCtx
        {
        ParticleType *arr;
        size_t start;
        size_t end;
        SegmentWideForceHandler *forces;
        float dt;
        float grav;
        const DisplaySegProperties *seg;  // read-only: is_particle_oob() bounds
        uint16_t oob[kMaxOobMarks];       // indices into arr, filled by this core only
        uint16_t oob_count;
        };

    static void update_range(void *v)
        {
        auto *c = static_cast<UpdateCtx *>(v);
        c->oob_count = 0;
        if (c->start >= c->end)
            return; // empty range (n==1 split): building the view would deref past-the-end

        // --- Pass 1 (force-major): every live force runs ONCE over this whole
        // range via a type-erased strided view, instead of being virtually
        // dispatched per particle per force. Forces only accumulate into accel
        // and read pos/phys; integration (pass 2) consumes accel afterwards —
        // so the two-pass split is behaviourally identical to the old fused
        // loop, just without (particles x forces) indirect calls.
        const ParticleRange range{
            &c->arr[c->start].get_physical(),
            &c->arr[c->start].get_positions(),
            &c->arr[c->start].get_acceleration(),
            sizeof(ParticleType),
            c->end - c->start};
        c->forces->apply_forces_range(range);

        // --- Pass 2: integrate + record OOB.
        for (size_t i = c->start; i < c->end; ++i)
            {
            c->arr[i].update(c->dt, c->grav);

            // Record (don't respawn) OOB particles: each core writes only its
            // own context, so this is race-free. Overflow just waits a frame.
            if (c->oob_count < kMaxOobMarks &&
                c->seg->is_particle_oob(c->arr[i].get_positions()))
                c->oob[c->oob_count++] = static_cast<uint16_t>(i);
            }
        }

    void update() override
        {
        // Combined uniform-force resultant (gravity + any field wind), computed
        // once per frame. New and recycled particles are seeded to move along it
        // so they enter at the correct drift angle/speed rather than ramping up
        // from rest on the edges. It's the same vector the spawn edge is built
        // from, so spawn position and spawn velocity stay consistent.
        const ForceLike drift = seg_properties_.get_drift_resultant();

        if (particles_.size() < static_cast<size_t>(max_particles_) && rng_class.get_rand_float() < spawn_rate_)
            particles_.emplace_back(seg_properties_.get_spawn_coords(), drift, particle_color_);

        // --- Parallel middle: force application + integration, split by index
        // across the two cores. Both halves run the same update_range(); the
        // RNG-driven spawn (prolog, above) and recycle (epilog, below) stay off
        // this path, so the cores only ever touch disjoint particles and
        // read-only forces — no locks needed. When core 1 is disabled or the
        // count is tiny, parallel::run() just does both halves inline.
        const size_t n = particles_.size();
        if (n > 0)
            {
            const size_t mid = n / 2;
            UpdateCtx upper{particles_.data(), mid, n, &seg_properties_.force_handler,
                            seg_properties_.get_dt(), seg_properties_.get_grav_mag(), &seg_properties_};
            UpdateCtx lower{particles_.data(), 0, mid, &seg_properties_.force_handler,
                            seg_properties_.get_dt(), seg_properties_.get_grav_mag(), &seg_properties_};
            parallel::run(&update_range, &upper, &lower, n); // core 0: upper half, core 1: lower half

            // --- Serial epilog (core 0 only): respawn the OOB particles the
            // parallel pass recorded. Re-spawning rolls the RNG (drag/mass +
            // drift-aligned velocity), so it must stay off the parallel path —
            // but it now touches only the recorded few instead of re-scanning
            // every particle, which was ~40% of the whole update phase.
            for (const UpdateCtx *c : {&upper, &lower})
                {
                for (uint16_t k = 0; k < c->oob_count; ++k)
                    {
                    ParticleType &particle = particles_[c->oob[k]];
                    // Move it back to a spawn edge and fully re-spawn it so it
                    // re-enters already moving along the drift, never carrying
                    // stale velocity from its last life.
                    particle.set_positions(seg_properties_.get_spawn_coords());
                    particle.spawn(drift);
                    }
                }
            }
        }

    void draw(PicoZGraphicsBase &graphics) override
        {
        for (auto &particle : particles_)
            {
            if (debug_draw_all_ || seg_properties_.is_particle_in_segment(particle.get_positions()))
                {
                XYZ &pos = particle.get_positions();
                graphics.set_pen(pos.z * particle.color.r, pos.z * particle.color.g, pos.z * particle.color.b);
                graphics.set_depth(pos.z);
                static_cast<Derived *>(this)->draw_effects(graphics, particle);
                }
            }
        }

    uint16_t get_particle_count() const override
        {
        return static_cast<uint16_t>(particles_.size());
        }

    void set_debug_draw_all(bool on) override { debug_draw_all_ = on; }

    void stop() override
        {
        particles_.clear();
        // Forces are owned by the segment (and may be shared by other effects),
        // so they are not cleared here.
        }

};

#endif // WEATHER_EFFECT_BASE_H
