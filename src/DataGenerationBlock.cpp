/**
 * @file    DataGenerationBlock.cpp
 * @brief   Implementation of the line-scan camera emulator.
 */

#include "DataGenerationBlock.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <algorithm>

namespace cynlr
{

    DataGenerationBlock::DataGenerationBlock(
        const PipelineConfig &cfg,
        RingBuffer<PixelPair, PipelineConfig::RING_CAPACITY> &outBuf)
        : cfg_(cfg), outBuf_(outBuf)
    {
    }

    DataGenerationBlock::~DataGenerationBlock()
    {
        stop();
        join();
    }

    bool DataGenerationBlock::configure()
    {
        if (cfg_.sourceMode == DataSourceMode::CsvFile)
        {
            if (!loadCsv())
            {
                std::cerr << "[DataGenerationBlock] Failed to load CSV: "
                          << cfg_.csvPath << "\n";
                return false;
            }
            std::cout << "[DataGenerationBlock] CSV loaded: "
                      << csvRows_ << " rows x " << cfg_.m << " columns.\n";
        }
        else
        {
            std::cout << "[DataGenerationBlock] Mode: RandomGenerator  "
                      << "T=" << cfg_.periodNs << " ns  m=" << cfg_.m << "\n";
        }
        return true;
    }

    void DataGenerationBlock::start()
    {
        running_.store(true, std::memory_order_release);
        worker_ = std::thread(&DataGenerationBlock::workerLoop, this);
    }

    void DataGenerationBlock::stop()
    {
        running_.store(false, std::memory_order_release);
    }

    void DataGenerationBlock::join()
    {
        if (worker_.joinable())
        {
            worker_.join();
        }
    }

    void DataGenerationBlock::workerLoop()
    {
        using Clock = std::chrono::steady_clock;

        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, 255);

        std::size_t csvRow{0};
        std::size_t csvCol{0};

        const uint64_t periodNs = cfg_.periodNs;
        const uint64_t sleepNs [[maybe_unused]] = (periodNs > SPIN_GUARD_NS)
                                                      ? (periodNs - SPIN_GUARD_NS)
                                                      : 0;
        const uint32_t m = cfg_.m;

        std::cout << "[DataGenerationBlock] Worker started.\n";

        auto deadline = Clock::now();

        while (running_.load(std::memory_order_acquire))
        {

            profiler_.begin();

            PixelPair pair;

            if (cfg_.sourceMode == DataSourceMode::RandomGenerator)
            {
                pair.pixel1 = static_cast<uint8_t>(dist(rng));
                pair.pixel2 = static_cast<uint8_t>(dist(rng));
            }
            else
            {
                if (csvRow >= csvRows_)
                {
                    pair.eos = true;
                    while (!outBuf_.push(pair))
                    {
                        std::this_thread::yield();
                    }
                    running_.store(false, std::memory_order_release);
                    break;
                }
                const std::size_t base = csvRow * m + csvCol;
                pair.pixel1 = csvData_[base];
                pair.pixel2 = (csvCol + 1 < m) ? csvData_[base + 1] : 0;

                csvCol += 2;
                if (csvCol >= m)
                {
                    csvCol = 0;
                    ++csvRow;
                }
            }

            while (!outBuf_.push(pair))
            {
                std::this_thread::yield();
            }

            profiler_.end();

            deadline += std::chrono::nanoseconds(periodNs);
            auto wakeTime = deadline - std::chrono::nanoseconds(SPIN_GUARD_NS);
            std::this_thread::sleep_until(wakeTime);
            while (Clock::now() < deadline);
        }

        std::cout << "[DataGenerationBlock] Worker finished.\n";
    }

    bool DataGenerationBlock::loadCsv()
    {
        std::ifstream file(cfg_.csvPath);
        if (!file.is_open())
            return false;

        std::vector<uint8_t> data;
        std::string line;
        std::size_t rows = 0;
        const uint32_t m = cfg_.m;

        while (std::getline(file, line))
        {
            if (line.empty())
                continue;
            std::istringstream ss(line);
            std::string token;
            std::size_t col = 0;
            while (std::getline(ss, token, ',') && col < m)
            {
                // Trim whitespace
                token.erase(0, token.find_first_not_of(" \t\r\n"));
                token.erase(token.find_last_not_of(" \t\r\n") + 1);
                if (token.empty())
                {
                    data.push_back(0);
                }
                else
                {
                    int v = std::stoi(token);
                    data.push_back(static_cast<uint8_t>(
                        std::clamp(v, 0, 255)));
                }
                ++col;
            }
            // Pad short rows
            while (col < m)
            {
                data.push_back(0);
                ++col;
            }
            ++rows;
        }

        if (rows == 0)
            return false;

        csvData_ = std::move(data);
        csvRows_ = rows;
        return true;
    }

} // namespace cynlr
