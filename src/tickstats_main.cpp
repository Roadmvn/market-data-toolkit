#include <charconv>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

#include "mdt/aggregator.hpp"
#include "mdt/csv_parser.hpp"

namespace {

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " <csv_path> [--window N]\n"
              << "  N is the aggregation window in seconds (default 60).\n";
}

std::string format_utc(long long timestamp_ms) {
    std::time_t seconds = static_cast<std::time_t>(timestamp_ms / 1000);
    std::tm tm_utc{};
    gmtime_r(&seconds, &tm_utc);
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_utc);
    return buffer;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    const std::string csv_path = argv[1];
    int window_seconds = 60;
    if (argc == 4) {
        if (std::strcmp(argv[2], "--window") != 0) {
            print_usage(argv[0]);
            return 1;
        }
        const char* arg = argv[3];
        const char* end = arg + std::strlen(arg);
        auto [ptr, ec] = std::from_chars(arg, end, window_seconds);
        if (ec != std::errc{} || ptr != end || window_seconds <= 0) {
            std::cerr << "Error: --window expects a positive integer.\n";
            return 1;
        }
    }

    mdt::ParseResult parsed;
    try {
        parsed = mdt::parse_file(csv_path);
    } catch (const std::runtime_error& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    const auto windows = mdt::aggregate(parsed.trades, window_seconds);

    std::cout << std::left << std::setw(21) << "window start (UTC)" << std::right
              << std::setw(10) << "open" << std::setw(10) << "high"
              << std::setw(10) << "low" << std::setw(10) << "close"
              << std::setw(12) << "volume" << std::setw(10) << "vwap"
              << std::setw(8) << "trades" << "\n";

    std::cout << std::fixed;
    for (const auto& w : windows) {
        std::cout << std::left << std::setw(21) << format_utc(w.window_start_ms)
                  << std::right << std::setprecision(2)
                  << std::setw(10) << w.open << std::setw(10) << w.high
                  << std::setw(10) << w.low << std::setw(10) << w.close
                  << std::setprecision(4) << std::setw(12) << w.volume
                  << std::setprecision(2) << std::setw(10) << w.vwap
                  << std::setw(8) << w.trade_count << "\n";
    }

    const double vwap = mdt::overall_vwap(parsed.trades);
    const double vol = mdt::realized_volatility(windows);
    std::cout << "\n"
              << "trades: " << parsed.trades.size()
              << "  malformed lines: " << parsed.malformed_lines
              << std::setprecision(2) << "  overall vwap: " << vwap
              << std::setprecision(4) << "  realized volatility: " << vol * 100.0
              << "%\n";
    return 0;
}
