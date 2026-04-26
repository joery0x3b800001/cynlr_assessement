#pragma once
/**
 * @file    DataGenerationBlock.h
 * @brief   Simulates a line-scan camera - produces PixelPair every T nanoseconds.
 *
 * Two modes (switchable at runtime via PipelineConfig::sourceMode):
 *
 *   RandomGenerator - two independent std::mt19937 engines produce uniform
 *                     uint8_t values, emulating an infinite cloth/paper roll.
 *
 *   CsvFile         - reads a pre-built 2-D array from a CSV file (one row per
 *                     line, comma-separated uint values 0-255).  Used for
 *                     deterministic unit testing and submission validation.
 *
 * Timing discipline
 * -----------------
 * The worker thread sleeps for T ns between iterations using a busy-wait loop
 * anchored on steady_clock.  Sleeping with nanosecond precision is platform-
 * dependent; the busy-wait gives tighter latency at the cost of one CPU core,
 * which is acceptable for a real-time scanner.
 * An optional sleep-then-spin hybrid is used: sleep for (T - SPIN_GUARD_NS)
 * then spin for the remainder to avoid overshooting.
 */

#include "IProcessBlock.h"
#include "PixelPair.h"
#include "PipelineConfig.h"
#include "RingBuffer.h"
#include "TimingProfiler.h"

#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <string>

namespace cynlr
{

    class DataGenerationBlock : public IProcessBlock
    {
    public:
        /**
         * @param cfg    Shared pipeline configuration.
         * @param outBuf Ring buffer into which PixelPairs are pushed.
         */
        explicit DataGenerationBlock(
            const PipelineConfig &cfg,
            RingBuffer<PixelPair, PipelineConfig::RING_CAPACITY> &outBuf);

        ~DataGenerationBlock() override;

        // IProcessBlock interface
        bool configure() override;
        void start() override;
        void stop() override;
        void join() override;
        std::string name() const override { return "DataGenerationBlock"; }

        /** @brief Expose profiler for post-run report generation. */
        inline const TimingProfiler &profiler() const noexcept { return profiler_; }

    private:
        void workerLoop();

        // ── CSV helpers ────────────────────────────────────────────────────────
        bool loadCsv(); ///< Parse csvPath_ into csvData_

        // ── Members ───────────────────────────────────────────────────────────
        const PipelineConfig &cfg_;
        RingBuffer<PixelPair, PipelineConfig::RING_CAPACITY> &outBuf_;

        std::thread worker_;
        std::atomic<bool> running_{false};

        // CSV mode storage: flat row-major array, width = cfg_.m
        std::vector<uint8_t> csvData_;
        std::size_t csvRows_{0};

        TimingProfiler profiler_{"DataGenerationBlock"};

        static constexpr uint64_t SPIN_GUARD_NS = 50'000; ///< 50 µs spin guard
    };

} // namespace cynlr
