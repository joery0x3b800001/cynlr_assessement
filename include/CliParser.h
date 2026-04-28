#pragma once
/**
 * @file    CliParser.h
 * @brief   Minimal command-line argument parser for the scanner pipeline.
 *
 * Usage:
 *   scanner --m 8 --tv 0.5 --t 1000000
 *   scanner --m 8 --tv 0.5 --t 1000000 --csv data/sample.csv
 *
 * --m    <uint>    Number of columns (default: 8)
 * --tv   <double>  Threshold value   (default: 0.5)
 * --t    <uint64>  Period T in ns    (default: 1000000 = 1 ms)
 * --csv  <path>    Path to CSV (enables CsvFile mode)
 * --dump <path>    CSV file for filtered output.
 * --help           Print usage
 */

#include "PipelineConfig.h"
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

namespace cynlr
{
    inline PipelineConfig parseArgs(int argc, char *argv[])
    {
        PipelineConfig cfg;

        std::vector<std::string> args(argv + 1, argv + argc);

        auto nextArg = [&](std::size_t i, const std::string &flag) -> std::string
        {
            if (i + 1 >= args.size())
            {
                throw std::runtime_error("Missing value for flag: " + flag);
            }
            return args[i + 1];
        };

        for (std::size_t i = 0; i < args.size(); ++i)
        {
            const std::string &a = args[i];

            if (a == "--help" || a == "-h")
            {
                std::cout << "CynLr Line-Scan Pipeline\n"
                             "Usage: scanner [options]\n\n"
                             "Options:\n"
                             "  --m   <uint>    Columns / paper width        (default: 8)\n"
                             "  --tv  <double>  Threshold value              (default: 0.5)\n"
                             "  --t   <uint64>  Cycle time T in nanoseconds  (default: 1000000)\n"
                             "  --csv <path>    CSV file for test mode\n"
                             "  --dump <path>    CSV file for filtered output.\n"
                             "  --help          Show this help\n\n"
                             "Examples:\n"
                             "  scanner --m 8 --tv 0.5 --t 500000\n"
                             "  scanner --m 8 --tv 0.5 --t 500000 --csv data/sample.csv\n"
                             "  scanner --m 8 --tv 0.5 --t 500000 --csv data/sample.csv -dump data/out.csv\n";
                std::exit(0);
            }
            else if (a == "--m")
            {
                cfg.m = static_cast<uint32_t>(std::stoul(nextArg(i, a)));
                ++i;
            }
            else if (a == "--tv")
            {
                cfg.thresholdTV = std::stod(nextArg(i, a));
                ++i;
            }
            else if (a == "--t")
            {
                cfg.periodNs = std::stoull(nextArg(i, a));
                ++i;
            }
            else if (a == "--csv")
            {
                cfg.csvPath = nextArg(i, a);
                cfg.sourceMode = DataSourceMode::CsvFile;
                ++i;
            }
            else if (a == "--dump")
            {
                cfg.dumpPath = nextArg(i, a);
                ++i;
            }
            else
            {
                std::cerr << "[Warning] Unknown argument: " << a << "\n";
            }
        }

        // Validate
        if (cfg.periodNs < 100)
        {
            throw std::runtime_error("T must be >= 100 ns per specification.");
        }
        if (cfg.m == 0)
        {
            throw std::runtime_error("m must be > 0.");
        }

        return cfg;
    }
}