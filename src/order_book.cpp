#include "mdt/order_book.hpp"

namespace mdt {

namespace {

// Shared by cancel for both map orderings (bids and asks have different
// comparators, hence the template).
template <typename LevelMap>
void remove_order(LevelMap& levels, std::int64_t price_ticks, double qty) {
    auto it = levels.find(price_ticks);
    it->second.total_qty -= qty;
    it->second.order_count -= 1;
    // Erase empty levels so best_bid/best_ask never point at a ghost price.
    if (it->second.order_count == 0) {
        levels.erase(it);
    }
}

} // namespace

bool OrderBook::add(OrderId id, Side side, std::int64_t price_ticks, double qty) {
    if (qty <= 0.0 || orders_.contains(id)) {
        return false;
    }
    orders_.emplace(id, OrderInfo{side, price_ticks, qty});
    LevelData& level = (side == Side::Buy) ? bids_[price_ticks] : asks_[price_ticks];
    level.total_qty += qty;
    level.order_count += 1;
    return true;
}

bool OrderBook::cancel(OrderId id) {
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return false;
    }
    const OrderInfo& info = it->second;
    if (info.side == Side::Buy) {
        remove_order(bids_, info.price_ticks, info.qty);
    } else {
        remove_order(asks_, info.price_ticks, info.qty);
    }
    orders_.erase(it);
    return true;
}

bool OrderBook::modify(OrderId id, double new_qty) {
    if (new_qty <= 0.0) {
        return false;
    }
    auto it = orders_.find(id);
    if (it == orders_.end()) {
        return false;
    }
    OrderInfo& info = it->second;
    LevelData& level = (info.side == Side::Buy) ? bids_.at(info.price_ticks)
                                                : asks_.at(info.price_ticks);
    level.total_qty += new_qty - info.qty;
    info.qty = new_qty;
    return true;
}

std::optional<std::int64_t> OrderBook::best_bid() const {
    if (bids_.empty()) {
        return std::nullopt;
    }
    return bids_.begin()->first;
}

std::optional<std::int64_t> OrderBook::best_ask() const {
    if (asks_.empty()) {
        return std::nullopt;
    }
    return asks_.begin()->first;
}

std::optional<std::int64_t> OrderBook::spread() const {
    auto bid = best_bid();
    auto ask = best_ask();
    if (!bid || !ask) {
        return std::nullopt;
    }
    return *ask - *bid;
}

std::optional<double> OrderBook::mid() const {
    auto bid = best_bid();
    auto ask = best_ask();
    if (!bid || !ask) {
        return std::nullopt;
    }
    return (static_cast<double>(*bid) + static_cast<double>(*ask)) / 2.0;
}

std::vector<OrderBook::Level> OrderBook::depth(Side side, std::size_t n_levels) const {
    std::vector<Level> result;
    auto collect = [&](const auto& levels) {
        for (const auto& [price, data] : levels) {
            if (result.size() == n_levels) {
                break;
            }
            result.push_back({price, data.total_qty, data.order_count});
        }
    };
    if (side == Side::Buy) {
        collect(bids_);
    } else {
        collect(asks_);
    }
    return result;
}

std::size_t OrderBook::order_count() const {
    return orders_.size();
}

} // namespace mdt
