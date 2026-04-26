#pragma once
/**
 * @file    PipelineConfig.h
 * @brief   Aggregates all runtime-configurable parameters for the pipeline.
 *
 * Centralising configuration in one POD struct means:
 *   • Every block reads from the same source-of-truth.
 *   • Adding a new parameter later touches only this header, not block code.
 *   • Unit tests can construct configs programmatically without touching CLI.
 */

#include <cstdint>
#include <string>

namespace cynlr
{

    /** Operation mode for the Data Generation Block. */
    enum class DataSourceMode
    {
        RandomGenerator, ///< Infinite stream of random uint8 pairs (production)
        CsvFile          ///< Read from a 2-D CSV file (test / validation)
    };

    struct PipelineConfig
    {
        // ── User Inputs (Section: Summary of User Inputs) ──────────────────────
        uint32_t m{8};                ///< Number of columns (paper width)
        double thresholdTV{0.5};      ///< Threshold Value (TV)
        uint64_t periodNs{1'000'000}; ///< Process time T in nanoseconds (>=100 ns)

        // ── Data source ────────────────────────────────────────────────────────
        DataSourceMode sourceMode{DataSourceMode::RandomGenerator};
        std::string csvPath{}; ///< Path to CSV when in CsvFile mode
        std::string dumpPath{}; ///< Path to export raw/filtered results to CSV

        // ── Ring buffer tuning ─────────────────────────────────────────────────
        // Keep outstanding pairs ≤ m to honour the "max memory ≤ m" requirement.
        // The ring buffer capacity is set statically; here we document the intent.
        // RingBuffer<PixelPair, RING_CAPACITY> where RING_CAPACITY >= m + filter_lookahead
        static constexpr std::size_t RING_CAPACITY = 256; ///< Must be > max(m)+filter half-window
    };

} // namespace cynlr
