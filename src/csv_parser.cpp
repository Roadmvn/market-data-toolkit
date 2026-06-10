#include "mdt/csv_parser.hpp"

#include <array>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace mdt {

namespace {

// std::from_chars is used for every numeric field: it parses straight from
// the buffer with no locale lookup, no allocation and no exceptions, which
// makes it considerably faster than stringstream or std::stod on large dumps.
template <typename T>
bool parse_number(std::string_view field, T& out) {
    const char* first = field.data();
    const char* last = first + field.size();
    auto [ptr, ec] = std::from_chars(first, last, out);
    return ec == std::errc{} && ptr == last;
}

#if !(defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L)
// Apple's libc++ still ships std::from_chars for integers only, so double
// fields fall back to strtod on a null-terminated stack copy. The program
// never calls setlocale, so the decimal separator stays '.'.
bool parse_number(std::string_view field, double& out) {
    std::array<char, 64> buf;
    if (field.empty() || field.size() >= buf.size()) {
        return false;
    }
    std::memcpy(buf.data(), field.data(), field.size());
    buf[field.size()] = '\0';
    char* end = nullptr;
    out = std::strtod(buf.data(), &end);
    return end == buf.data() + field.size();
}
#endif

bool parse_bool(std::string_view field, bool& out) {
    if (field == "true" || field == "True" || field == "TRUE" || field == "1") {
        out = true;
        return true;
    }
    if (field == "false" || field == "False" || field == "FALSE" || field == "0") {
        out = false;
        return true;
    }
    return false;
}

} // namespace

std::optional<Trade> parse_line(std::string_view line) {
    // Strip a trailing \r so files with Windows line endings parse cleanly.
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);
    }

    // Exactly 7 comma-separated fields, no quoting in Binance dumps.
    std::array<std::string_view, 7> fields;
    std::size_t count = 0;
    std::size_t start = 0;
    while (true) {
        std::size_t comma = line.find(',', start);
        std::string_view field = (comma == std::string_view::npos)
                                     ? line.substr(start)
                                     : line.substr(start, comma - start);
        if (count == fields.size()) {
            return std::nullopt; // too many fields
        }
        fields[count++] = field;
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
    if (count != fields.size()) {
        return std::nullopt; // too few fields
    }

    Trade trade{};
    if (!parse_number(fields[0], trade.id) ||
        !parse_number(fields[1], trade.price) ||
        !parse_number(fields[2], trade.qty) ||
        !parse_number(fields[3], trade.quote_qty) ||
        !parse_number(fields[4], trade.timestamp_ms) ||
        !parse_bool(fields[5], trade.is_buyer_maker)) {
        return std::nullopt;
    }

    // Some Binance dumps switched to microsecond timestamps. Anything above
    // 10^15 cannot be a millisecond timestamp (that would be year 33658), so
    // treat it as microseconds and normalize.
    if (trade.timestamp_ms > 1'000'000'000'000'000LL) {
        trade.timestamp_ms /= 1000;
    }

    return trade;
}

ParseResult parse_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open file: " + path);
    }

    ParseResult result;
    std::string line;
    bool first_line = true;
    while (std::getline(file, line)) {
        if (first_line) {
            first_line = false;
            // Header detection: a data line always starts with a trade id.
            if (!line.empty() && !std::isdigit(static_cast<unsigned char>(line[0]))) {
                continue;
            }
        }
        if (line.empty()) {
            continue;
        }
        if (auto trade = parse_line(line)) {
            result.trades.push_back(*trade);
        } else {
            ++result.malformed_lines;
        }
    }
    return result;
}

} // namespace mdt
