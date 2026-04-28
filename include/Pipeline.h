#pragma once
/**
 * @file    Pipeline.h
 * @brief   Owns and orchestrates all IProcessBlock instances.
 *
 * Scalability design
 * ------------------
 * Pipeline holds a std::vector<std::unique_ptr<IProcessBlock>>.
 * Adding a third block (e.g. a future defect-classification stage) requires:
 *   1. Instantiate the new block.
 *   2. Call pipeline.addBlock(std::move(newBlock)).
 * Zero modifications to existing block code.
 *
 * The Pipeline is responsible for:
 *   • Connecting blocks via a shared RingBuffer (currently one buffer between
 *     DataGen → FilterThreshold; future buffers added per new stage).
 *   • Calling configure() → start() on all blocks in order.
 *   • Calling stop() → join() on all blocks in reverse order on shutdown.
 *   • Printing the profiling report after all blocks have joined.
 */

#include "IProcessBlock.h"
#include "PipelineConfig.h"
#include "PixelPair.h"
#include "RingBuffer.h"
#include "cynlr_export.h"

#include <vector>
#include <memory>
#include <functional>

namespace cynlr
{

    class CYNLR_API Pipeline
    {
    public:
        using OutputCallback = std::function<void(uint64_t idx, uint8_t raw, double filtered, int thresholded)>;

        explicit Pipeline(const PipelineConfig &cfg);
        ~Pipeline();

        /** @brief Build and wire all blocks. Must be called before run(). */
        bool build();

        /** @brief Start all blocks. Returns when the pipeline finishes (EOS received). */
        void run();

        /** @brief Stop all blocks and print timing report. */
        void shutdown();

        void setOutputCallback(OutputCallback cb) { outputCallback_ = std::move(cb); }

        const PipelineConfig &config() const noexcept { return cfg_; }

    private:
        const PipelineConfig &cfg_;

        // Shared communication channel between DataGen and FilterThreshold
        RingBuffer<PixelPair, PipelineConfig::RING_CAPACITY> channel_;

        // Ordered list of all process blocks (add future blocks here)
        std::vector<std::unique_ptr<IProcessBlock>> blocks_;

        OutputCallback outputCallback_;

        bool built_{false};
    };

}