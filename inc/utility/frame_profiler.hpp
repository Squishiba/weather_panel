#ifndef WEATHER_FRAME_PROFILER_H
#define WEATHER_FRAME_PROFILER_H

/**
 * @file frame_profiler.h
 * @brief Per-phase frame-timing datalogger, printed over USB.
 *
 * Splits each rendered frame into named phases (clear, lux read, particle
 * @b update, @b draw, framebuffer @b flush, and the FPS-cap idle) and accumulates
 * how long each takes. Every reporting window (default 1 s) it prints the average
 * and worst-case microseconds per phase, the achieved FPS, and the "busy" time
 * (everything except the idle sleep) with the single-core FPS ceiling that
 * implies. The point is to see on real RP2350 hardware where the frame budget
 * actually goes — e.g. to decide whether splitting physics across the second
 * core (parallelising @b update) is worth it, or whether draw/flush dominates.
 *
 * Usage (all macros are no-ops when WEATHER_PROFILE == 0):
 * @code
 *   PROF_BEGIN(UPDATE); ...physics...   PROF_END(UPDATE);
 *   PROF_BEGIN(DRAW);   ...rasterise... PROF_END(DRAW);
 *   // or, scope-based (times to end of the enclosing block):
 *   { PROF_SCOPE(FLUSH); i75.update(&fb); }
 *   PROF_FRAME_END(particle_count);     // once per frame, after all phases
 * @endcode
 *
 * Clock source is time_us_32() (1 us resolution). Per-frame that quantises small
 * phases, but the reported figures are averages over hundreds of frames, so the
 * quantisation averages out. Swap now_us() for a DWT cycle-count read if you ever
 * need sub-microsecond per-frame numbers.
 */

// 1 = compile the profiler in and log over USB. 0 = every macro below expands to
// nothing, the class is not defined, and it costs zero bytes / zero cycles.
#ifndef WEATHER_PROFILE
#define WEATHER_PROFILE 1
#endif

#if WEATHER_PROFILE

#include "pico/time.h"

#include <cstdint>
#include <cstdio>

/**
 * @brief Accumulates per-phase frame timings and periodically reports them.
 *
 * Single shared instance (Meyers singleton) so the update path (display_segment)
 * and the frame path (weather_display_handler) log into the same accumulators
 * without threading a pointer through every call. Reach it through the PROF_*
 * macros rather than directly.
 *
 * @warning Not thread-safe. Time only from the core that owns the render loop.
 *          If physics is ever moved to core 1, give that core its own instance
 *          (or fold its numbers in during the serial epilog).
 */
class FrameProfiler
{
public:
    /// Phases of one frame. Keep in sync with kPhaseNames and the CSV row below.
    enum Phase : uint8_t
    {
        CLEAR = 0, ///< clear framebuffer + draw segment dividers
        LUX,       ///< ambient-light sensor I2C read
        UPDATE,    ///< particle physics: force update + spawn edge + effect update
        DRAW,      ///< rasterise: base display + effect draw
        FLUSH,     ///< push framebuffer to the HUB75 driver (i75.update)
        GOVERN,    ///< FPS-cap sleep (idle; excluded from "busy")
        PHASE_COUNT
    };

    static FrameProfiler &instance()
    {
        static FrameProfiler profiler;
        return profiler;
    }

    /// Start timing @p p. Pairs with end(p); do not nest the same phase.
    void begin(Phase p)
    {
        start_[p] = now_us();
    }

    /// Stop timing @p p and fold the elapsed time into this window's totals.
    void end(Phase p)
    {
        const uint32_t dt = now_us() - start_[p];
        accum_[p] += dt;
        if (dt > max_[p]) max_[p] = dt;
    }

    /**
     * @brief Close out one frame; emit a report once per window, then reset.
     * @param particle_count Particles drawn this frame, logged for context
     *        (timings are meaningless without the load that produced them).
     */
    void end_frame(uint16_t particle_count = 0)
    {
        ++frames_;
        particle_accum_ += particle_count;

        const uint32_t now = now_us();
        if (now - window_start_ < kReportPeriodUs) return;

        report(now - window_start_);
        reset();
        window_start_ = now_us(); // exclude the report's own printf cost from the next window
    }

private:
    static constexpr uint32_t kReportPeriodUs = 1'000'000; // summarise ~once per second

    FrameProfiler()
    {
        window_start_ = now_us();
    }

    static uint32_t now_us()
    {
        return time_us_32();
    }

    static const char *name(Phase p)
    {
        static const char *const kPhaseNames[PHASE_COUNT] = {"clear", "lux", "update", "draw", "flush", "govern"};
        return kPhaseNames[p];
    }

    void report(uint32_t window_us) const
    {
        const uint32_t frames = frames_ ? frames_ : 1; // guard the divides
        const float fps = frames_ * 1'000'000.0f / (window_us ? window_us : 1);

        uint64_t busy = 0;
        for (uint8_t p = 0; p < GOVERN; ++p) // busy = all work phases, minus the idle sleep
            busy += accum_[p];
        const uint32_t busy_avg = static_cast<uint32_t>(busy / frames);

        printf("[PROF] frames=%lu fps=%.1f particles=%lu\n", static_cast<unsigned long>(frames_), static_cast<double>(fps), static_cast<unsigned long>(particle_accum_ / frames));

        for (uint8_t p = 0; p < PHASE_COUNT; ++p)
            printf("  %-7s avg %7lu us   max %6lu us\n", name(static_cast<Phase>(p)), static_cast<unsigned long>(accum_[p] / frames), static_cast<unsigned long>(max_[p]));

        printf("  %-7s avg %7lu us   => single-core ceiling %.1f fps\n", "busy", static_cast<unsigned long>(busy_avg), busy_avg ? 1'000'000.0 / static_cast<double>(busy_avg) : 0.0);

        // Machine-readable mirror. Grep '^\[CSV\]' from the serial log; columns:
        // frames,fps,particles,clear,lux,update,draw,flush,govern,busy  (us, window-average)
        printf("[CSV],%lu,%.1f,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n", static_cast<unsigned long>(frames_), static_cast<double>(fps), static_cast<unsigned long>(particle_accum_ / frames),
               static_cast<unsigned long>(accum_[CLEAR] / frames), static_cast<unsigned long>(accum_[LUX] / frames), static_cast<unsigned long>(accum_[UPDATE] / frames),
               static_cast<unsigned long>(accum_[DRAW] / frames), static_cast<unsigned long>(accum_[FLUSH] / frames), static_cast<unsigned long>(accum_[GOVERN] / frames),
               static_cast<unsigned long>(busy_avg));
        printf("\n");
    }

    void reset()
    {
        for (uint8_t p = 0; p < PHASE_COUNT; ++p)
        {
            accum_[p] = 0;
            max_[p] = 0;
        }
        frames_ = 0;
        particle_accum_ = 0;
    }

    uint32_t start_[PHASE_COUNT] = {}; // begin() timestamp per phase
    uint64_t accum_[PHASE_COUNT] = {}; // summed elapsed us this window
    uint32_t max_[PHASE_COUNT] = {};   // worst single-frame us this window
    uint32_t frames_ = 0;
    uint64_t particle_accum_ = 0;
    uint32_t window_start_ = 0;
};

/// RAII timer: times from construction to the end of the enclosing block.
class ProfScope
{
public:
    explicit ProfScope(FrameProfiler::Phase p) : phase_(p)
    {
        FrameProfiler::instance().begin(p);
    }
    ~ProfScope()
    {
        FrameProfiler::instance().end(phase_);
    }
    ProfScope(const ProfScope &) = delete;
    ProfScope &operator=(const ProfScope &) = delete;

private:
    FrameProfiler::Phase phase_;
};

#define PROF_CONCAT_(a, b) a##b
#define PROF_CONCAT(a, b) PROF_CONCAT_(a, b)

#define PROF_SCOPE(phase) ProfScope PROF_CONCAT(prof_scope_, __LINE__)(FrameProfiler::phase)
#define PROF_BEGIN(phase) FrameProfiler::instance().begin(FrameProfiler::phase)
#define PROF_END(phase) FrameProfiler::instance().end(FrameProfiler::phase)
#define PROF_FRAME_END(particle_count) FrameProfiler::instance().end_frame(particle_count)

#else // WEATHER_PROFILE == 0 : compile the profiler out entirely

#define PROF_SCOPE(phase) ((void)0)
#define PROF_BEGIN(phase) ((void)0)
#define PROF_END(phase) ((void)0)
#define PROF_FRAME_END(particle_count) ((void)0)

#endif // WEATHER_PROFILE

#endif // WEATHER_FRAME_PROFILER_H
