/**
 * @file    Pipeline.cpp
 * @brief   Wires DataGenerationBlock → FilterThresholdBlock and manages lifecycle.
 */

#include "Pipeline.h"
#include "DataGenerationBlock.h"
#include "FilterThresholdBlock.h"

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

namespace cynlr
{

    Pipeline::Pipeline(const PipelineConfig &cfg)
        : cfg_(cfg), channel_()
    {
    }

    Pipeline::~Pipeline()
    {
        shutdown();
    }

    // ── Build ──────────────────────────────────────────────────────────────────

    bool Pipeline::build()
    {
        if (built_)
        {
            return true;
        }

        auto finalCallback = outputCallback_ ? outputCallback_ : [](uint64_t, uint8_t, double, int) {};

        // ── Instantiate blocks ─────────────────────────────────────────────────
        auto dataGen = std::make_unique<DataGenerationBlock>(cfg_, channel_);
        auto filter = std::make_unique<FilterThresholdBlock>(cfg_, channel_,
                                                             std::move(finalCallback));

        // Configure both
        if (!dataGen->configure() || !filter->configure())
        {
            std::cerr << "[Pipeline] Configuration failed.\n";
            return false;
        }

        // Ownership transferred to blocks_ vector (order matters for shutdown)
        blocks_.push_back(std::move(dataGen));
        blocks_.push_back(std::move(filter));

        built_ = true;
        std::cout << "[Pipeline] Built with " << blocks_.size() << " blocks.\n";
        return true;
    }

    // ── Run ────────────────────────────────────────────────────────────────────

    void Pipeline::run()
    {
        if (!built_)
        {
            std::cerr << "[Pipeline] Call build() before run().\n";
            return;
        }

        std::cout << "[Pipeline] Starting all blocks...\n\n";

        // Fork: start all blocks (each in its own thread)
        for (auto &block : blocks_)
        {
            block->start();
        }

        // Wait for DataGenerationBlock to finish (it stops after CSV EOS or on stop())
        // FilterThresholdBlock will then drain and stop itself.
        if (auto *dgb = dynamic_cast<DataGenerationBlock *>(blocks_[0].get()))
        {
            dgb->join();
        }

        // Give the filter a moment to drain; its join() is called in shutdown()
        if (auto *ftb = dynamic_cast<FilterThresholdBlock *>(blocks_[1].get()))
        {
            // Signal it to stop after draining
            ftb->stop();
            ftb->join();
        }
    }

    // ── Shutdown ───────────────────────────────────────────────────────────────

    void Pipeline::shutdown()
    {
        if (!built_)
        {
            return;
        }

        std::cout << "\n[Pipeline] Shutting down...\n";

        // Stop in reverse order
        for (auto it = blocks_.rbegin(); it != blocks_.rend(); ++it)
        {
            (*it)->stop();
        }
        for (auto it = blocks_.rbegin(); it != blocks_.rend(); ++it)
        {
            (*it)->join();
        }

        // ── Profiling report ───────────────────────────────────────────────────
        std::cout << "\n══════════════════════════════════════════════════\n";
        std::cout << "  TIMING & PROFILING REPORT";
        std::cout << "\n══════════════════════════════════════════════════\n";

        for (const auto &block : blocks_)
        {
            if (auto *dgb = dynamic_cast<DataGenerationBlock *>(block.get()))
            {
                std::cout << dgb->profiler().report() << "\n";
            }
            else if (auto *ftb = dynamic_cast<FilterThresholdBlock *>(block.get()))
            {
                std::cout << ftb->profiler().report() << "\n";
            }
        }
        std::cout << "══════════════════════════════════════════════════\n\n";

        built_ = false;
    }

} // namespace cynlr
