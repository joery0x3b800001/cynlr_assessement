#pragma once

/**
 * @file    RingBuffer.h
 * @brief   Lock-Free Single-Producer / Single-Consumer Ring Buffer.
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