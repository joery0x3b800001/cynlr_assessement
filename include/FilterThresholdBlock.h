#pragma once
/**
 * @file    FilterThresholdBlock.h
 * @brief   Consumes PixelPairs, applies a 9-tap Gaussian filter, then thresholds.
 *
 * Algorithm summary (per element k)
 * -----------------------------------
 *  1. Collect window: [k-4, k-3, k-2, k-1, k, k+1, k+2, k+3, k+4]
 *     • Past 4 values stored in a circular history buffer.
 *     • Future 4 values buffered in a look-ahead queue.
 *  2. Dot-product with the Gaussian filter window (9 constants from spec).
 *  3. Compare filtered value against TV:
 *       filtered >= TV  →  output 1
 *       filtered <  TV  →  output 0
 *
 * Memory constraint adherence
 * ----------------------------
 * The look-ahead queue holds at most 4 future elements at any moment, and
 * the history buffer holds 4 past elements.  Total extra memory per logical
 * element stream: 8 uint8 values - well within the "≤ m" budget.
 *
 * The block processes elements one at a time (not pair-at-a-time) so that
 * the filter window is contiguous across pair boundaries - exactly as the
 * line-scan model requires.
 *
 * Throughput
 * ----------
 * The 9-multiply + 9-add convolution is ~18 FP ops per element.  On modern
 * x86 this executes in <10 ns, comfortably under the 100 ns threshold.
 */

#include "IProcessBlock.h"
#include "PixelPair.h"
#include "PipelineConfig.h"
#include "RingBuffer.h"
#include "TimingProfiler.h"

#include <thread>
#include <atomic>
#include <array>
#include <deque>
#include <cstdint>
#include <string>
#include <functional>

namespace cynlr
{

    /** Signature of the output callback delivered to external consumers. */
    using OutputCallback = std::function<void(uint64_t elementIndex, uint8_t rawValue,
                                              double filteredValue, int thresholded)>;

    class FilterThresholdBlock : public IProcessBlock
    {
    public:
        /**
         * @param cfg     Shared pipeline configuration.
         * @param inBuf   Ring buffer from which PixelPairs are consumed.
         * @param onOutput Called (from worker thread) for every thresholded output.
         */
        explicit FilterThresholdBlock(
            const PipelineConfig &cfg,
            RingBuffer<PixelPair, PipelineConfig::RING_CAPACITY> &inBuf,
            OutputCallback onOutput = nullptr);

        ~FilterThresholdBlock() override;

        // IProcessBlock interface
        bool configure() override;
        void start() override;
        void stop() override;
        void join() override;
        inline std::string name() const override { return "FilterThresholdBlock"; }

        inline const TimingProfiler &profiler() const noexcept { return profiler_; }

        // ── Gaussian filter window (9 elements, symmetric) ─────────────────────
        // Values taken verbatim from the problem statement.
        static constexpr std::array<double, 9> FILTER_WINDOW = {
            0.00025177,  // K-4
            0.008666992, // K-3
            0.078025818, // K-2
            0.24130249,  // K-1
            0.343757629, // K   (centre)
            0.24130249,  // K+1
            0.078025818, // K+2
            0.008666992, // K+3
            0.000125885  // K+4  (note: asymmetric tail per spec)
        };
        static constexpr int HALF_WINDOW = 4; ///< Elements on each side of K

    private:
        void workerLoop();

        /**
         * @brief Apply the Gaussian filter to the current 9-element window.
         * @param window  Exactly 9 elements: [K-4 ... K+4]
         * @return Filtered (smoothed) value as double.
         */
        static double applyFilter(const std::array<double, 9> &window) noexcept;

        /**
         * @brief Threshold the filtered value against TV.
         * @return 1 if filtered >= TV, else 0.
         */
        int applyThreshold(double filteredValue) const noexcept;

        // ── Members ───────────────────────────────────────────────────────────
        const PipelineConfig &cfg_;
        RingBuffer<PixelPair, PipelineConfig::RING_CAPACITY> &inBuf_;
        OutputCallback onOutput_;

        std::thread worker_;
        std::atomic<bool> running_{false};

        // Per-element sliding window state
        std::array<uint8_t, HALF_WINDOW> history_{}; ///< K-4 … K-1 (circular)
        int histPos_{0};                             ///< Next write position in history_
        std::deque<uint8_t> lookahead_;              ///< Buffered future elements

        uint64_t elementIndex_{0}; ///< Global monotonic element counter

        TimingProfiler profiler_{"FilterThresholdBlock"};
        TimingProfiler throughputProf_{"SuccessivePixelThroughput"};
    };

} // namespace cynlr
