#ifndef WEATHER_PARALLEL_EXECUTOR_H
#define WEATHER_PARALLEL_EXECUTOR_H

/**
 * @file parallel_executor.h
 * @brief Dead-simple two-core fork/join for the particle update.
 *
 * The whole multicore story in one place, so the call sites stay readable.
 *
 * Mental model: core 1 is launched ONCE into a worker that waits for work.
 * To "give the other core some work", core 0 stores a job (a plain function
 * pointer + a context pointer), bumps a sequence counter, runs its own half,
 * then waits for core 1 to echo the sequence back. That's fork/join — the two
 * halves run at the same time and run() only returns once both are done.
 *
 * Why a function POINTER and not a lambda: only a captureless function converts
 * to a `void(*)(void*)`. A capturing lambda can't cross to another core, so work
 * is passed as (function + context) instead. In practice both cores call the
 * SAME loop body on different index ranges, so "passing a function" is really
 * just "here's the per-particle work; you do items [0,n/2), I'll do [n/2,n)".
 *
 * Synchronisation: two private atomic sequence counters (g_go / g_done) in
 * shared SRAM, NOT the SIO inter-core FIFO. The FIFO is deliberately avoided:
 * the SDK owns it for its own protocols (the core-1 launch handshake runs
 * through it, and the lockout mechanism installs a FIFO IRQ handler that drains
 * and DISCARDS any entry that isn't its magic word). Tokens we pushed through
 * the FIFO were getting eaten by exactly that kind of consumer, deadlocking
 * both cores in pop_blocking. Counters we own can't be stolen. The RP2350's
 * cores share SRAM with no per-core data cache, so acquire/release atomics are
 * all the ordering needed — the release-store of g_go publishes g_fn/g_ctx,
 * and the acquire-load of g_done makes core 1's writes visible to core 0.
 *
 * Why it's safe without locks (caller's contract):
 *   - The two contexts must touch DISJOINT data. Particle ranges [0,mid) and
 *     [mid,n) are disjoint.
 *   - Anything read in common must not change during the run. Forces are only
 *     READ during apply_force (their per-frame motion happens earlier, on one
 *     core), so that holds.
 *   - RNG-driven work (spawning / respawning) is kept OFF this path — it stays
 *     serial on core 0 in the effect's prolog/epilog.
 *
 * The same code runs in the desktop simulator: its shim provides
 * multicore_launch_core1() via std::thread, and the atomics are plain C++ —
 * so the split, and thus the sim's behaviour, matches the firmware for free.
 */

#include "pico/multicore.h" // multicore_launch_core1 (the ONLY SDK multicore call used)

#include <atomic>
#include <cstddef>
#include <cstdint>

// Platform seam for the two spin-waits. On the RP2350 we pair __wfe()/__sev():
// the waiter dozes until any event, then re-checks its condition (spurious wakes
// are fine — the loops re-test). On the desktop, C++20 atomic wait/notify parks
// the thread in the OS instead of burning a core.
#if defined(__has_include)
#if __has_include("hardware/sync.h")
#include "hardware/sync.h"
#define WEATHER_PARALLEL_ON_RP2 1
#endif
#endif

#ifdef WEATHER_PARALLEL_ON_RP2
// Doze until any event; the caller's loop re-checks the actual condition.
#define WEATHER_PARALLEL_WAIT(var, old) __wfe()
#define WEATHER_PARALLEL_WAKE(var) __sev()
#else
// Block while (var == old); wakes on the matching notify (or spuriously).
#define WEATHER_PARALLEL_WAIT(var, old) (var).wait(old)
#define WEATHER_PARALLEL_WAKE(var) (var).notify_one()
#endif

namespace parallel
{
    using WorkFn = void (*)(void *);

    namespace detail
    {
        // The single outstanding job. Fork/join is serialised (core 0 blocks
        // until core 1 finishes), so one slot is all that's ever needed.
        inline WorkFn g_fn = nullptr;
        inline void *g_ctx = nullptr;
        inline bool g_enabled = true; // runtime on/off; flip to A/B against the profiler
        inline bool g_launched = false;

        // Handshake counters. g_go is bumped by core 0 to fork; core 1 echoes
        // the same value into g_done when its half is finished. Equality-based,
        // so uint32 wraparound is harmless. Must be lock-free: a library lock
        // would deadlock across cores (no OS on the firmware side).
        inline std::atomic<uint32_t> g_go{0};
        inline std::atomic<uint32_t> g_done{0};
        static_assert(std::atomic<uint32_t>::is_always_lock_free, "inter-core handshake requires lock-free 32-bit atomics");

        // Enable THIS core's FPU (Cortex-M33 CPACR: CP10 & CP11 = full access).
        // Core 0 gets this from the runtime startup, but a freshly launched
        // core 1 may come up with the FPU disabled — its first FP instruction
        // would then take a NOCP UsageFault. Idempotent, so harmless if the
        // runtime already enabled it. DSB+ISB are architecturally required
        // before the next FP instruction.
        inline void enable_fpu_this_core()
        {
#ifdef WEATHER_PARALLEL_ON_RP2
            volatile uint32_t *const cpacr = reinterpret_cast<volatile uint32_t *>(0xE000ED88u);
            *cpacr |= (0xFu << 20);
            __asm volatile("dsb 0xf" ::: "memory");
            __asm volatile("isb 0xf" ::: "memory");
#endif
        }

        /**
         * @brief Core 1's resident loop: wait for a fork, run the job, echo done.
         *
         * `seen` tracks the last sequence number this core completed; a fork is
         * "g_go moved past seen". The acquire-load of g_go synchronises with
         * run()'s release-store, making g_fn/g_ctx (written before the store)
         * visible here without any explicit barrier.
         */
        inline void core1_worker()
        {
            enable_fpu_this_core(); // core 1 must enable its own FPU before any FP work

            uint32_t seen = 0;
            for (;;)
            {
                uint32_t go;
                while ((go = g_go.load(std::memory_order_acquire)) == seen)
                    WEATHER_PARALLEL_WAIT(g_go, seen); // doze; loop re-checks

                seen = go;
                g_fn(g_ctx);                                   // run core 1's half
                g_done.store(seen, std::memory_order_release); // publish results + done
                WEATHER_PARALLEL_WAKE(g_done);
            }
        }

        inline void ensure_launched()
        {
            if (!g_launched)
            {
                // Force core 1 back into its bootrom "wait for launch" state
                // before launching. The launch handshake only works from that
                // state; after a re-flash or debugger restart WITHOUT a full
                // power cycle, core 1 can still be running a previous run's
                // worker, in which case launch_core1 wedges forever pushing a
                // handshake no one is listening to. Reset makes launch
                // unconditional-safe. (No-op in the desktop shim.)
                multicore_reset_core1();
                multicore_launch_core1(core1_worker); // once, on first split
                g_launched = true;
            }
        }
    } // namespace detail

    /// Turn use of core 1 on/off at runtime. When off, run() does both halves
    /// inline on the calling core — identical result, one code path.
    inline void set_enabled(bool on)
    {
        detail::g_enabled = on;
    }
    inline bool enabled()
    {
        return detail::g_enabled;
    }

    /// Below this many items the fork/join sync costs more than it saves.
    inline constexpr size_t kMinItemsForSplit = 64;

    /**
     * @brief Run fn(here) on this core and fn(other) on core 1, concurrently.
     * @param items Size hint; below kMinItemsForSplit both halves run inline.
     *
     * Returns only once both halves have finished. `here` and `other` must
     * describe disjoint work (see file header for the safety contract).
     *
     * Handshake: publish the job, release-store a new sequence number into
     * g_go (fork), run our own half, then wait for core 1 to echo that number
     * into g_done (join). No possibility of a lost wake-up: the launch-vs-fork
     * race is covered because core 1 CHECKS g_go before it ever waits, and on
     * the RP2350 __sev() latches the event register even if it fires between
     * core 1's check and its __wfe().
     */
    inline void run(WorkFn fn, void *here, void *other, size_t items)
    {
        if (!detail::g_enabled || items < kMinItemsForSplit)
        {
            fn(here);
            fn(other);
            return;
        }

        detail::ensure_launched();

        detail::g_fn = fn;
        detail::g_ctx = other;
        const uint32_t seq = detail::g_go.load(std::memory_order_relaxed) + 1u;
        detail::g_go.store(seq, std::memory_order_release); // fork: publishes job too
        WEATHER_PARALLEL_WAKE(detail::g_go);

        fn(here); // core 0 runs its half meanwhile

        uint32_t done;
        while ((done = detail::g_done.load(std::memory_order_acquire)) != seq)
            WEATHER_PARALLEL_WAIT(detail::g_done, done); // join: wait for core 1
    }
} // namespace parallel

#endif // WEATHER_PARALLEL_EXECUTOR_H
