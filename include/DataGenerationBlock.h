#pragma once
/**
 * @file    DataGenerationBlock.h
 * @brief   Simulates a line-scan camera - produces PixelPair every T nanoseconds.
 */

#include "IProcessBlock.h"
#include "PixelPair.h"
#include "PipelineConfig.h"
#include "RingBuffer.h"
#include "TimingProfiler.h"
#include "cynlr_export.h"

#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <string>

namespace cynlr
{
    class CYNLR_API DataGenerationBlock : public IProcessBlock
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
        inline std::string name() const override { return "DataGenerationBlock"; }

        /** @brief Expose profiler for post-run report generation. */
        inline const TimingProfiler &profiler() const noexcept { return profiler_; }

    private:
        void workerLoop();

        bool loadCsv();

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
}