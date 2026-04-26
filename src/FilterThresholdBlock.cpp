/**
 * @file    FilterThresholdBlock.cpp
 * @brief   Implementation of the Gaussian filter + threshold block.
 */

#include "FilterThresholdBlock.h"

#include <iostream>
#include <thread>
#include <numeric>
#include <cassert>

namespace cynlr
{

    // ── constexpr definition (ODR) ─────────────────────────────────────────────
    constexpr std::array<double, 9> FilterThresholdBlock::FILTER_WINDOW;

    // ── Constructor / Destructor ───────────────────────────────────────────────

    FilterThresholdBlock::FilterThresholdBlock(
        const PipelineConfig &cfg,
        RingBuffer<PixelPair, PipelineConfig::RING_CAPACITY> &inBuf,
        OutputCallback onOutput)
        : cfg_(cfg), inBuf_(inBuf), onOutput_(std::move(onOutput))
    {
    }

    FilterThresholdBlock::~FilterThresholdBlock()
    {
        stop();
        join();
    }

    // ── IProcessBlock interface ────────────────────────────────────────────────

    bool FilterThresholdBlock::configure()
    {
        // Pre-fill history with zeros (border handling: treat pre-stream as 0)
        history_.fill(0);
        histPos_ = 0;
        lookahead_.clear();
        elementIndex_ = 0;

        std::cout << "[FilterThresholdBlock] TV=" << cfg_.thresholdTV
                  << "  filter window: 9-tap Gaussian\n";
        return true;
    }

    void FilterThresholdBlock::start()
    {
        running_.store(true, std::memory_order_release);
        worker_ = std::thread(&FilterThresholdBlock::workerLoop, this);
    }

    void FilterThresholdBlock::stop()
    {
        running_.store(false, std::memory_order_release);
    }

    void FilterThresholdBlock::join()
    {
        if (worker_.joinable())
        {
            worker_.join();
        }
    }

    // ── Static helpers ─────────────────────────────────────────────────────────

    double FilterThresholdBlock::applyFilter(
        const std::array<double, 9> &window) noexcept
    {
        double result = 0.0;
        result += window[0] * FILTER_WINDOW[0];
        result += window[1] * FILTER_WINDOW[1];
        result += window[2] * FILTER_WINDOW[2];
        result += window[3] * FILTER_WINDOW[3];
        result += window[4] * FILTER_WINDOW[4];
        result += window[5] * FILTER_WINDOW[5];
        result += window[6] * FILTER_WINDOW[6];
        result += window[7] * FILTER_WINDOW[7];
        result += window[8] * FILTER_WINDOW[8];
        return result;
    }

    int FilterThresholdBlock::applyThreshold(double filteredValue) const noexcept
    {
        double tv = cfg_.thresholdTV;

#if defined(__APPLE__) || defined(__linux__)
        int result;
#if defined(__aarch64__)
        // ARM64 (Apple Silicon) Branchless Comparison
        __asm__(
            "fcmp %d[val], %d[tv]\n\t"
            "cset %w[res], ge\n\t"
            : [res] "=r"(result)
            : [val] "w"(filteredValue), [tv] "w"(tv)
            : "cc");
        return result;

#elif defined(__x86_64__)
        // x86_64 (Intel Mac/Linux) Branchless Comparison
        __asm__(
            "comisd %[tv], %[val];"
            "setae %%al;"
            "movzx %%al, %[res];"
            : [res] "=r"(result)
            : [val] "x"(filteredValue), [tv] "x"(tv)
            : "cc", "rax");
        return result;
#endif

#elif defined(_WIN32) || defined(_WIN64)
        // Windows MSVC: Inline assembly isn't supported for x64.
        // We use a logical evaluation that compilers usually optimize to
        // a conditional set (SETCC) instruction, avoiding a jump.
        return (filteredValue >= tv) ? 1 : 0;

#else
        // Fallback for other platforms
        return (filteredValue >= tv) ? 1 : 0;
#endif
    }

    // ── Worker thread ──────────────────────────────────────────────────────────
    /**
     * Strategy for the look-ahead filter
     * ------------------------------------
     * We cannot filter element K until we have K+1 … K+4 (future values).
     * The approach:
     *   1. Buffer incoming elements in `lookahead_`.
     *   2. Once lookahead has ≥ (HALF_WINDOW+1) elements, the front element
     *      has its full future context - extract it and compute the filter.
     *   3. History (past context) is maintained in a circular array of size 4.
     *
     * This introduces a latency of HALF_WINDOW elements (4 pixels), which is
     * unavoidable for a causal implementation of this non-causal filter.
     * Memory overhead: 4 (history) + up to 4 (lookahead) = 8 uint8 values.
     */
    void FilterThresholdBlock::workerLoop()
    {
        std::cout << "[FilterThresholdBlock] Worker started.\n";

        bool producerDone = false;

        auto processElement = [&](uint8_t elem)
        {
            // 1. Push into look-ahead queue
            lookahead_.push_back(elem);

            // 2. Can we filter the front element?
            //    We need HALF_WINDOW future elements after the candidate.
            //    Candidate is at lookahead_[0]; its future elements are [1..4].
            //    So we need lookahead_.size() > HALF_WINDOW.
            while (lookahead_.size() > static_cast<std::size_t>(HALF_WINDOW))
            {

                profiler_.begin();

                // Build the 9-element window
                std::array<double, 9> window;

                // Past 4 elements from circular history
                // histPos_ points to the OLDEST entry in history_
                for (int i = 0; i < HALF_WINDOW; ++i)
                {
                    window[i] = static_cast<double>(
                        history_[(histPos_ + i) % HALF_WINDOW]);
                }
                // Current (K) and future 4
                for (int i = 0; i <= HALF_WINDOW; ++i)
                {
                    window[HALF_WINDOW + i] = static_cast<double>(lookahead_[i]);
                }

                // 3. Filter
                double filtered = applyFilter(window);

                // 4. Threshold
                int thresholded = applyThreshold(filtered);

                uint8_t rawVal = lookahead_.front();

                // 5. Output
                if (onOutput_)
                {
                    onOutput_(elementIndex_, rawVal, filtered, thresholded);
                }

                // 6. Advance history
                history_[histPos_] = rawVal;
                histPos_ = (histPos_ + 1) % HALF_WINDOW;

                lookahead_.pop_front();
                ++elementIndex_;

                uint64_t iterNs = profiler_.end();

                // Track successive-pixel throughput separately
                throughputProf_.begin();
                // (the 'end' will be called at the next iteration's begin)
                (void)iterNs;
            }
        };

        // Flush: when producer is done, pad with zeros to drain the look-ahead
        auto flushRemaining = [&]()
        {
            for (int i = 0; i < HALF_WINDOW; ++i)
            {
                processElement(0); // zero-pad the right border
            }
        };

        while (running_.load(std::memory_order_acquire) || !inBuf_.empty())
        {

            auto item = inBuf_.pop();
            if (!item.has_value())
            {
                std::this_thread::yield();
                continue;
            }

            const PixelPair &pair = item.value();

            if (pair.eos)
            {
                producerDone = true;
                flushRemaining();
                break;
            }

            processElement(pair.pixel1);
            processElement(pair.pixel2);
        }

        // If producer stopped without EOS (stop() called externally), drain
        if (!producerDone)
        {
            flushRemaining();
        }

        std::cout << "[FilterThresholdBlock] Worker finished. "
                  << "Processed " << elementIndex_ << " elements.\n";
    }

} // namespace cynlr
