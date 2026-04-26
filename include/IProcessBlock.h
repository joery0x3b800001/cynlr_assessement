#pragma once
/**
 * @file    IProcessBlock.h
 * @brief   Abstract interface for all pipeline process blocks.
 *
 * Design intent
 * -------------
 * Every stage in the scanner pipeline (Data Generation, Filter & Threshold,
 * and any future block) implements this interface.  The pipeline harness only
 * knows about IProcessBlock, so new blocks can be added with ZERO changes to
 * existing code - satisfying the modularity and scalability requirements.
 *
 * Lifecycle
 * ---------
 *   configure() → start() → [running: produces/consumes data every T] → stop() → join()
 */

#include <cstdint>
#include <string>

namespace cynlr
{

    /**
     * @interface IProcessBlock
     * Pure-abstract base class for every processing stage.
     */
    class IProcessBlock
    {
    public:
        virtual ~IProcessBlock() = default;

        /**
         * @brief  One-time initialisation called before start().
         * @return true on success, false on configuration error.
         */
        virtual bool configure() = 0;

        /** @brief Spawn the worker thread and begin processing. */
        virtual void start() = 0;

        /** @brief Signal the worker thread to stop (non-blocking). */
        virtual void stop() = 0;

        /** @brief Block until the worker thread has fully exited. */
        virtual void join() = 0;

        /** @brief Human-readable name for logging / diagnostics. */
        virtual std::string name() const = 0;
    };

} // namespace cynlr
