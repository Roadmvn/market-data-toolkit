#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace mdt {

enum class Side { Buy, Sell };

using OrderId = std::int64_t;

// In-memory limit order book. Queries only: matching is intentionally out of
// scope for this project.
//
// Prices are stored as integer ticks, never as doubles. Price levels are map
// keys and must compare exactly; doubles accumulate representation error
// (421.50 has no exact binary form), which would silently split one price
// level into several.
class OrderBook {
public:
    struct Level {
        std::int64_t price_ticks;
        double total_qty;
        std::size_t order_count;
    };

    // false if the id already exists or qty <= 0.
    bool add(OrderId id, Side side, std::int64_t price_ticks, double qty);

    // false if the id is unknown.
    bool cancel(OrderId id);

    // Changes quantity only, keeps price and side. false if the id is
    // unknown or new_qty <= 0.
    bool modify(OrderId id, double new_qty);

    std::optional<std::int64_t> best_bid() const;
    std::optional<std::int64_t> best_ask() const;
    std::optional<std::int64_t> spread() const;
    std::optional<double> mid() const; // in ticks, fractional

    // Top n_levels aggregated levels, best price first.
    std::vector<Level> depth(Side side, std::size_t n_levels) const;

    std::size_t order_count() const;

private:
    struct LevelData {
        double total_qty = 0.0;
        std::size_t order_count = 0;
    };

    struct OrderInfo {
        Side side;
        std::int64_t price_ticks;
        double qty;
    };

    // std::map trades raw speed for simplicity: O(log n) updates but levels
    // stay sorted, so best price and depth queries are trivial. See the
    // README for what production books use instead.
    std::map<std::int64_t, LevelData, std::greater<>> bids_; // best bid first
    std::map<std::int64_t, LevelData> asks_;                 // best ask first
    std::unordered_map<OrderId, OrderInfo> orders_;          // O(1) id lookup
};

} // namespace mdt
