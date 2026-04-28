#pragma once
/**
 * @file    TimingProfiler.h
 * @brief   Lightweight, header-only nanosecond timing utility.
 *
 * Uses std::chrono::steady_clock (monotonic, no NTP jumps) to measure:
 *   • Per-iteration latency
 *   • Running min / max / mean
 *
 * Designed to be cheap enough to leave enabled in release builds so that
 * the submission profiling report can be auto-generated from live data.
 */

#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <sstream>
#include <iomanip>

namespace cynlr
{
    class TimingProfiler
    {
    public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        explicit TimingProfiler(std::string label)
            : label_(std::move(label)) {}

        /** Mark the start of one timed interval. */
        void begin() noexcept { t0_ = Clock::now(); }

        /**
         * Mark the end of one interval and accumulate statistics.
         * @return Elapsed nanoseconds for this interval.
         */
        uint64_t end() noexcept
        {
            const uint64_t ns = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    Clock::now() - t0_)
                    .count());

            ++count_;
            totalNs_ += ns;
            if (ns < minNs_)
                minNs_ = ns;
            if (ns > maxNs_)
                maxNs_ = ns;
            return ns;
        }

        uint64_t count() const noexcept { return count_; }
        uint64_t minNs() const noexcept { return minNs_; }
        uint64_t maxNs() const noexcept { return maxNs_; }
        double meanNs() const noexcept
        {
            return count_ ? static_cast<double>(totalNs_) / count_ : 0.0;
        }
        uint64_t totalNs() const noexcept { return totalNs_; }
        const std::string &label() const noexcept { return label_; }

        /** @brief Human-readable summary for the profiling report. */
        std::string report() const
        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1);
            oss << "[" << label_ << "] "
                << "iterations=" << count_
                << "  min=" << minNs_ << " ns"
                << "  mean=" << meanNs() << " ns"
                << "  max=" << maxNs_ << " ns"
                << "  total=" << totalNs_ / 1'000'000 << " ms";
            return oss.str();
        }

    private:
        std::string label_;
        TimePoint t0_{};
        uint64_t count_{0};
        uint64_t totalNs_{0};
        uint64_t minNs_{std::numeric_limits<uint64_t>::max()};
        uint64_t maxNs_{0};
    };
}