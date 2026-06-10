#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "mdt/trade.hpp"

namespace mdt {

struct ParseResult {
    std::vector<Trade> trades;
    std::size_t malformed_lines = 0;
};

// Parses one line of a Binance spot trades CSV:
//   id,price,qty,quote_qty,time,is_buyer_maker,is_best_match
// Returns std::nullopt for malformed lines (wrong field count, bad numbers).
std::optional<Trade> parse_line(std::string_view line);

// Parses a whole file. Skips a header line if present, counts malformed
// lines instead of throwing. Throws std::runtime_error only when the file
// cannot be opened.
ParseResult parse_file(const std::string& path);

} // namespace mdt
