#pragma once
#pragma once

/**
 * @file    RingBuffer.h
 * @brief   High-Performance, Lock-Free Single-Producer / Single-Consumer (SPSC) Ring Buffer.
 *
 * Why this design?
 * ----------------
 * The pipeline architectural requirements specify exactly one producer (DataGenerationBlock)
 * and one consumer (FilterThresholdBlock). A SPSC queue eliminates the need for mutexes
 * or heavy-weight synchronization primitives, providing:
 * • Zero Lock Contention: Eliminates kernel-level context switching, enabling the
 * deterministic latency required for <100 ns successive-pixel throughput.
 * • Bounded Memory: Fixed capacity ensures the system stays within the 'm'
 * element memory constraint.
 * • Wait-Free Progress: Guaranteed progress for both producer and consumer
 * without blocking, critical for real-time system stability.
 *
 * Memory order rationale
 * ----------------------
 * This implementation utilizes C++11 Acquire-Release semantics to establish a
 * "happens-before" relationship between threads without the cost of full
 * sequential consistency:
 * • Push (Producer): Writes data to storage, then performs a 'Release' store on head_.
 * This ensures all previous writes (the pixel data) are visible to the consumer.
 * • Pop (Consumer): Performs an 'Acquire' load on head_. This synchronizes with
 * the producer’s release, ensuring the consumer sees the valid data before reading.
 * • Result: Total data integrity with zero UB and minimal cache-coherency traffic.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * OPTIMISATION INVENTORY
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * 1. POWER-OF-2 CAPACITY → BITMASK WRAPPING
 * Replaces expensive modulo (%) operations with a bitwise AND (&).
 * • Old: (head + 1) % Capacity  (8-40 cycles depending on CPU)
 * • New: (head + 1) & MASK      (1 cycle)
 *
 * 2. PLACEMENT NEW + ALIGNED RAW STORAGE
 * Utilizes `std::byte` storage with `alignas(T)` to implement an Object Pool.
 * Avoids the overhead of default-constructing every slot in a `std::array`.
 * For trivially-copyable types like PixelPair, the compiler optimizes the
 * push operation into a single store.
 *
 * 3. CACHE-LINE ISOLATION (False Sharing Prevention)
 * Forces `head_` and `tail_` atomics onto separate 64-byte cache lines
 * using `std::hardware_destructive_interference_size`. This prevents the
 * CPU from "fighting" over a single cache line, drastically reducing
 * L1 cache misses in high-frequency loops.
 *
 * 4. BATCH SEMANTICS (Amortized Overhead)
 * `push_batch` and `pop_batch` allow transferring N items using only ONE
 * Acquire/Release fence pair. This significantly reduces the overhead
 * of atomic synchronization when processing bulk pixel rows.
 *
 * @tparam T         Element type. Optimized for trivially copyable structures.
 * @tparam Capacity  MUST be a power of two (enforced via static_assert).
 */

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <type_traits>

namespace cynlr
{

    template <typename T, std::size_t Capacity>
    class RingBuffer
    {
        static_assert(Capacity >= 2 && (Capacity & (Capacity - 1u)) == 0,
                      "RingBuffer Capacity must be a power of two");

        static constexpr std::size_t MASK = Capacity - 1u;

        // ── Cache-line padded atomic: fills exactly 64 bytes ──────────────────
        struct alignas(64) PaddedAtomic
        {
            std::atomic<std::size_t> v{0};
            char _pad[std::hardware_destructive_interference_size - sizeof(std::atomic<std::size_t>)];
            PaddedAtomic() noexcept : v(0) { (void)_pad; }
        };

    public:
        RingBuffer() = default;

        ~RingBuffer()
        {
            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                std::size_t t = tail_.v.load(std::memory_order_relaxed);
                const std::size_t h = head_.v.load(std::memory_order_relaxed);
                while (t != h)
                {
                    slot(t)->~T();
                    t = (t + 1u) & MASK;
                }
            }
        }

        RingBuffer(const RingBuffer &) = delete;
        RingBuffer &operator=(const RingBuffer &) = delete;

        // ── Single push (producer) ─────────────────────────────────────────────
        [[nodiscard]] bool push(const T &item) noexcept
        {
            const std::size_t h = head_.v.load(std::memory_order_relaxed);
            const std::size_t next = (h + 1u) & MASK; // bitmask, no div
            if (next == tail_.v.load(std::memory_order_acquire))
                return false;
            ::new (slot(h)) T(item); // placement new
            head_.v.store(next, std::memory_order_release);
            return true;
        }

        // ── Single pop (consumer) ──────────────────────────────────────────────
        [[nodiscard]] std::optional<T> pop() noexcept
        {
            const std::size_t t = tail_.v.load(std::memory_order_relaxed);
            if (t == head_.v.load(std::memory_order_acquire))
                return std::nullopt;
            T *p = slot(t);
            T item(*p);
            if constexpr (!std::is_trivially_destructible_v<T>)
                p->~T();
            tail_.v.store((t + 1u) & MASK, std::memory_order_release);
            return item;
        }

        // ── Batch push: ONE fence pair for N items ─────────────────────────────
        std::size_t push_batch(const T *items, std::size_t n) noexcept
        {
            std::size_t h = head_.v.load(std::memory_order_relaxed);
            std::size_t tCur = tail_.v.load(std::memory_order_acquire);
            std::size_t free = (Capacity - 1u - ((h - tCur) & MASK));
            std::size_t count = (n < free) ? n : free;
            for (std::size_t i = 0; i < count; ++i)
            {
                ::new (slot(h)) T(items[i]);
                h = (h + 1u) & MASK;
            }
            if (count)
                head_.v.store(h, std::memory_order_release);
            return count;
        }

        // ── Batch pop: ONE fence pair for N items ──────────────────────────────
        std::size_t pop_batch(T *out, std::size_t n) noexcept
        {
            std::size_t t = tail_.v.load(std::memory_order_relaxed);
            std::size_t hCur = head_.v.load(std::memory_order_acquire);
            std::size_t avail = (hCur - t) & MASK;
            std::size_t count = (n < avail) ? n : avail;
            for (std::size_t i = 0; i < count; ++i)
            {
                T *p = slot(t);
                out[i] = *p;
                if constexpr (!std::is_trivially_destructible_v<T>)
                    p->~T();
                t = (t + 1u) & MASK;
            }
            if (count)
                tail_.v.store(t, std::memory_order_release);
            return count;
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return tail_.v.load(std::memory_order_acquire) ==
                   head_.v.load(std::memory_order_acquire);
        }
        [[nodiscard]] std::size_t size() const noexcept
        {
            return (head_.v.load(std::memory_order_acquire) -
                    tail_.v.load(std::memory_order_acquire)) &
                   MASK;
        }
        static constexpr std::size_t capacity() noexcept { return Capacity; }

    private:
        // Raw byte pool – objects live here via placement new
        alignas(T) std::byte storage_[sizeof(T) * Capacity];

        PaddedAtomic head_; // written only by producer
        PaddedAtomic tail_; // written only by consumer

        T *slot(std::size_t i) noexcept
        {
            return std::launder(reinterpret_cast<T *>(storage_ + i * sizeof(T)));
        }
    };

} // namespace cynlr