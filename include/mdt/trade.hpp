#pragma once

namespace mdt {

// One executed trade from a Binance spot trades dump.
// timestamp_ms is always milliseconds since epoch (the parser normalizes
// microsecond dumps).
struct Trade {
    long long id;
    double price;
    double qty;
    double quote_qty;
    long long timestamp_ms;
    bool is_buyer_maker;
};

} // namespace mdt
