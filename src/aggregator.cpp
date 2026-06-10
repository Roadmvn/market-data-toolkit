#include "mdt/aggregator.hpp"

#include <algorithm>
#include <cmath>

namespace mdt {

std::vector<WindowStats> aggregate(const std::vector<Trade>& trades, int window_seconds) {
    std::vector<WindowStats> windows;
    if (trades.empty() || window_seconds <= 0) {
        return windows;
    }

    std::vector<Trade> sorted = trades;
    std::stable_sort(sorted.begin(), sorted.end(),
                     [](const Trade& a, const Trade& b) { return a.timestamp_ms < b.timestamp_ms; });

    const long long window_ms = static_cast<long long>(window_seconds) * 1000;
    long long current_bucket = -1;
    double notional = 0.0; // sum of price * qty for the current window

    for (const Trade& trade : sorted) {
        const long long bucket = trade.timestamp_ms / window_ms;
        if (bucket != current_bucket) {
            current_bucket = bucket;
            notional = 0.0;
            windows.push_back({bucket * window_ms, trade.price, trade.price,
                               trade.price, trade.price, 0.0, 0.0, 0});
        }
        WindowStats& w = windows.back();
        w.high = std::max(w.high, trade.price);
        w.low = std::min(w.low, trade.price);
        w.close = trade.price;
        w.volume += trade.qty;
        w.trade_count += 1;
        notional += trade.price * trade.qty;
        w.vwap = w.volume > 0.0 ? notional / w.volume : 0.0;
    }
    return windows;
}

double overall_vwap(const std::vector<Trade>& trades) {
    double notional = 0.0;
    double volume = 0.0;
    for (const Trade& trade : trades) {
        notional += trade.price * trade.qty;
        volume += trade.qty;
    }
    return volume > 0.0 ? notional / volume : 0.0;
}

double realized_volatility(const std::vector<WindowStats>& windows) {
    // r_i = ln(close_i / close_{i-1});  vol = sqrt(sum((r_i - mean)^2) / (m - 1))
    // with m = number of returns. Sample (not population) deviation, since the
    // windows are a sample of the price process. At least 3 windows are needed
    // for 2 returns, the minimum for a sample deviation.
    if (windows.size() < 3) {
        return 0.0;
    }

    std::vector<double> returns;
    returns.reserve(windows.size() - 1);
    for (std::size_t i = 1; i < windows.size(); ++i) {
        returns.push_back(std::log(windows[i].close / windows[i - 1].close));
    }

    double mean = 0.0;
    for (double r : returns) {
        mean += r;
    }
    mean /= static_cast<double>(returns.size());

    double sum_sq = 0.0;
    for (double r : returns) {
        sum_sq += (r - mean) * (r - mean);
    }
    return std::sqrt(sum_sq / static_cast<double>(returns.size() - 1));
}

} // namespace mdt
