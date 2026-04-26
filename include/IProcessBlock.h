#pragma once
/**
 * @file    IProcessBlock.h
 * @brief   Abstract interface for all pipeline process blocks.
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
