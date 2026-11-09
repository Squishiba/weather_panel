#ifndef WEATHER_PARTICLE_ARENA_H
#define WEATHER_PARTICLE_ARENA_H

/**
 * @file particle_arena.h
 * @brief One fixed pool of particle memory shared by every segment.
 *
 * Previously each effect embedded its own `etl::vector<ParticleType, 3000>` by
 * value, so N segments reserved N × the full capacity — three snow segments alone
 * blew the RP2350's ~520 KB SRAM. Instead, all effects now carve a contiguous
 * block out of this single fixed arena and derive their particle count from what
 * the pool can actually grant. Total particle RAM is therefore bounded by
 * kParticlePoolBytes regardless of how many segments/effects exist.
 *
 * The allocator is a trivial first-fit free-list with coalescing: allocations are
 * few (≤ one particle effect per segment) and rare (only when the weather
 * changes), so nothing fancier is warranted. Each effect gets ONE contiguous
 * block — this keeps the per-frame update loop cache-linear and keeps the future
 * two-core split (halving a segment's particle array by index) viable, which a
 * node/free-list-of-particles pool would wreck.
 *
 * Never panics: if a request can't be met it returns nullptr / a short block and
 * the caller spawns fewer particles. Graceful degradation, never OOM.
 */

#include <cstddef>
#include <cstdint>

#include "etl/vector.h"

// Total shared budget. ~3000 particles at ~56 B each ≈ 164 KiB. Deliberately
// excludes the WiFi/lwIP stack (not linked yet); revisit if that changes.
inline constexpr size_t kParticlePoolParticles = 3000;
inline constexpr size_t kParticlePoolAssumedParticleBytes = 56; // current sizeof(Snow)/sizeof(Rain)
inline constexpr size_t kParticlePoolBytes = kParticlePoolParticles * kParticlePoolAssumedParticleBytes;

// Fair-share divisor: the ceiling on any single effect is kParticlePoolBytes /
// this, so one busy segment can't starve the others. Set to the number of
// segments (days) that share the pool.
inline constexpr size_t kParticlePoolFairShareSegments = 3;

/**
 * @brief Fixed-capacity block allocator backing all particle storage.
 *
 * Not thread-safe: only the render core allocates/frees (weather changes happen
 * on the same core that runs the loop). If particle work ever moves to core 1,
 * keep allocation on core 0.
 */
class ParticleArena
{
public:
    ParticleArena()
    {
        blocks_.push_back(Block{0, kParticlePoolBytes, true});
    }

    /**
     * @brief Carve a contiguous block of at least @p bytes from the pool.
     * @return Pointer into the arena, or nullptr if no free block is large
     *         enough (caller degrades gracefully — it never panics).
     */
    void *allocate(size_t bytes)
    {
        bytes = align_up(bytes);
        if (bytes == 0) return nullptr;

        for (size_t i = 0; i < blocks_.size(); ++i)
        {
            Block &b = blocks_[i];
            if (!b.free || b.size < bytes) continue;

            b.free = false;
            const size_t leftover = b.size - bytes;
            if (leftover > 0 && !blocks_.full())
            {
                b.size = bytes;
                blocks_.insert(blocks_.begin() + i + 1, Block{b.offset + bytes, leftover, true});
            }
            // If the block table is full we keep the whole block (minor internal
            // waste) rather than fail — still correct, freed whole on release.
            return &buffer_[b.offset];
        }
        return nullptr;
    }

    /// Return a block previously handed out by allocate(). nullptr is ignored.
    void deallocate(void *ptr)
    {
        if (ptr == nullptr) return;

        const size_t offset = static_cast<size_t>(static_cast<std::byte *>(ptr) - buffer_);
        for (size_t i = 0; i < blocks_.size(); ++i)
        {
            if (!blocks_[i].free && blocks_[i].offset == offset)
            {
                blocks_[i].free = true;
                coalesce();
                return;
            }
        }
        // Unknown pointer: ignore defensively.
    }

    /// Bytes in the largest single free block (what a new effect can actually get).
    size_t largest_free() const
    {
        size_t best = 0;
        for (const Block &b : blocks_)
            if (b.free && b.size > best) best = b.size;
        return best;
    }

    /// Total free bytes across all free blocks (diagnostics only).
    size_t bytes_free() const
    {
        size_t total = 0;
        for (const Block &b : blocks_)
            if (b.free) total += b.size;
        return total;
    }

private:
    static constexpr size_t kAlign = 8;      // covers alignof of any particle (≤ 8)
    static constexpr size_t kMaxBlocks = 24; // ≤ ~2×(effects) + 1; plenty for a few segments

    struct Block
    {
        size_t offset;
        size_t size;
        bool free;
    };

    static size_t align_up(size_t n)
    {
        return (n + (kAlign - 1)) & ~(kAlign - 1);
    }

    // Merge adjacent free blocks. The list is kept in offset order and gapless,
    // so neighbours in the list are physically adjacent in the buffer.
    void coalesce()
    {
        for (size_t i = 0; i + 1 < blocks_.size();)
        {
            if (blocks_[i].free && blocks_[i + 1].free)
            {
                blocks_[i].size += blocks_[i + 1].size;
                blocks_.erase(blocks_.begin() + i + 1);
            }
            else
            {
                ++i;
            }
        }
    }

    alignas(kAlign) std::byte buffer_[kParticlePoolBytes];
    etl::vector<Block, kMaxBlocks> blocks_;
};

/// The one shared arena. Lazily constructed in .bss (function-local static avoids
/// any static-init-order surprises).
inline ParticleArena &particle_arena()
{
    static ParticleArena arena;
    return arena;
}

#endif // WEATHER_PARTICLE_ARENA_H
