/**
 * @file    main.cpp
 * @brief   Entry point for the CynLr line-scan scanner pipeline.
 */

#include "CliParser.h"
#include "Pipeline.h"

#include <iostream>
#include <fstream>
#include <csignal>
#include <atomic>
#include <iomanip>

// Global pipeline pointer for graceful Ctrl-C shutdown
static cynlr::Pipeline *g_pipeline{nullptr};
static std::atomic<bool> g_interrupted{false};

static void signalHandler(int /*signum*/)
{
    std::cout << "\n[main] Interrupt received - stopping pipeline...\n";
    g_interrupted.store(true);
    if (g_pipeline)
    {
        g_pipeline->shutdown();
    }
    std::exit(0);
}

int main(int argc, char *argv[])
{
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::cout << "╔══════════════════════════════════════════════════╗\n"
              << "║   CynLr Line-Scan Scanner Pipeline               ║\n"
              << "║   Evaluation 1 - Programming Fundamentals C++    ║\n"
              << "╚══════════════════════════════════════════════════╝\n\n";

    cynlr::PipelineConfig cfg;
    try
    {
        cfg = cynlr::parseArgs(argc, argv);
    }
    catch (const std::exception &ex)
    {
        std::cerr << "[Error] " << ex.what() << "\n";
        return 1;
    }

    // If dumpPath is set in cfg, we prepare an output file stream.
    std::unique_ptr<std::ofstream> dumpFile;
    if (!cfg.dumpPath.empty())
    {
        dumpFile = std::make_unique<std::ofstream>(cfg.dumpPath);
        if (dumpFile->is_open())
        {
            *dumpFile << "index,raw_pixel,filtered_value,threshold_status\n";
            std::cout << "[main] Dumping data to: " << cfg.dumpPath << "\n";
        }
    }

    std::cout << "Configuration:\n"
              << "  m (columns)      = " << cfg.m << "\n"
              << "  TV (threshold)   = " << cfg.thresholdTV << "\n"
              << "  T  (period ns)   = " << cfg.periodNs << "\n"
              << "  Mode             = "
              << (cfg.sourceMode == cynlr::DataSourceMode::CsvFile
                      ? "CsvFile (" + cfg.csvPath + ")"
                      : "RandomGenerator")
              << "\n\n";

    cynlr::Pipeline pipeline(cfg);
    g_pipeline = &pipeline;

    // We register a callback that both prints to console (first 20) and
    // writes to the dump file if active.
    pipeline.setOutputCallback([&](uint64_t idx, uint8_t raw, double filtered, int threshold)
                               {
        // Console output (throttled to first 20 for readability)
        if (idx < 20)
        {
            std::cout << "  " << std::setw(6) << idx << " | " 
                      << std::setw(3) << (int)raw << " | " 
                      << std::setw(10) << std::fixed << std::setprecision(4) << filtered << " | " 
                      << (threshold ? "TRIGGER" : "       ") << "\n";
        }

        // CSV File dump logic
        if (dumpFile && dumpFile->is_open())
        {
            *dumpFile << idx << "," << (int)raw << "," << filtered << "," << threshold << "\n";
        } });

    if (!pipeline.build())
    {
        std::cerr << "[Error] Pipeline build failed.\n";
        return 2;
    }

    std::cout << "Output Log:\n";
    std::cout << "  Index  | Raw | Filtered   | Thresholded\n";
    std::cout << "  -------+-----+------------+------------\n";

    pipeline.run(); // blocks until EOS or Ctrl-C

    if (dumpFile)
    {
        dumpFile->close();
    }

    pipeline.shutdown();

    return 0;
}