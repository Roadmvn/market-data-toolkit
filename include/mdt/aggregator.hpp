#pragma once

#include <cstddef>
#include <vector>

#include "mdt/trade.hpp"

namespace mdt {

struct WindowStats {
    long long window_start_ms;
    double open;
    double high;
    double low;
    double close;
    double volume;
    double vwap;
    std::size_t trade_count;
};

// Buckets trades into fixed time windows (timestamp_ms / window_ms) and
// computes OHLC, volume and VWAP per window. Binance dumps are time-sorted,
// but the input is sorted defensively anyway.
std::vector<WindowStats> aggregate(const std::vector<Trade>& trades, int window_seconds);

// Volume-weighted average price over the whole trade set, 0 if empty.
double overall_vwap(const std::vector<Trade>& trades);

// Realized volatility as the sample standard deviation of log returns of
// consecutive window closes, returned as a fraction (e.g. 0.0084 = 0.84%).
// Returns 0 for fewer than 3 windows.
double realized_volatility(const std::vector<WindowStats>& windows);

} // namespace mdt
