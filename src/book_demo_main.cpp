#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "mdt/order_book.hpp"

namespace {

// Prices are entered as decimals and stored as integer ticks with scale 100,
// i.e. 1 tick = 0.01 (2 decimals, enough for USDT pairs in this demo).
constexpr double kTickScale = 100.0;

std::int64_t to_ticks(double price) {
    return static_cast<std::int64_t>(std::llround(price * kTickScale));
}

double to_price(std::int64_t ticks) {
    return static_cast<double>(ticks) / kTickScale;
}

void print_help() {
    std::cout << "Commands:\n"
              << "  A <id> <B|S> <price> <qty>   add a limit order\n"
              << "  C <id>                       cancel an order\n"
              << "  M <id> <qty>                 modify order quantity\n"
              << "  P                            print top 5 levels and summary\n"
              << "  D <n>                        print top n levels and summary\n"
              << "  H                            show this help\n"
              << "  Q                            quit\n"
              << "Prices use 2 decimals, e.g. A 1 B 421.50 0.5\n";
}

void print_book(const mdt::OrderBook& book, std::size_t n_levels) {
    const auto bids = book.depth(mdt::Side::Buy, n_levels);
    const auto asks = book.depth(mdt::Side::Sell, n_levels);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "      BIDS qty @ price   |   price @ qty ASKS\n";
    const std::size_t rows = std::max(bids.size(), asks.size());
    if (rows == 0) {
        std::cout << "      (empty book)\n";
    }
    for (std::size_t i = 0; i < rows; ++i) {
        if (i < bids.size()) {
            std::cout << std::setw(10) << std::setprecision(3) << bids[i].total_qty
                      << " @ " << std::setw(9) << std::setprecision(2)
                      << to_price(bids[i].price_ticks);
        } else {
            std::cout << std::setw(22) << " ";
        }
        std::cout << "   |   ";
        if (i < asks.size()) {
            std::cout << std::setw(9) << std::setprecision(2)
                      << to_price(asks[i].price_ticks) << " @ " << std::setw(10)
                      << std::setprecision(3) << asks[i].total_qty;
        }
        std::cout << "\n";
    }

    std::cout << std::setprecision(2) << "best bid: ";
    if (auto bid = book.best_bid()) {
        std::cout << to_price(*bid);
    } else {
        std::cout << "n/a";
    }
    std::cout << "  best ask: ";
    if (auto ask = book.best_ask()) {
        std::cout << to_price(*ask);
    } else {
        std::cout << "n/a";
    }
    std::cout << "  spread: ";
    if (auto spread = book.spread()) {
        std::cout << to_price(*spread);
    } else {
        std::cout << "n/a";
    }
    std::cout << "  mid: ";
    if (auto mid = book.mid()) {
        std::cout << *mid / kTickScale;
    } else {
        std::cout << "n/a";
    }
    std::cout << "  orders: " << book.order_count() << "\n";
}

// Returns false when the session should end (Q command).
bool handle_command(mdt::OrderBook& book, const std::string& line) {
    std::istringstream input(line);
    std::string command;
    if (!(input >> command) || command.size() != 1) {
        std::cout << "unknown command, type H for help\n";
        return true;
    }

    switch (std::toupper(static_cast<unsigned char>(command[0]))) {
    case 'A': {
        mdt::OrderId id;
        std::string side_token;
        double price;
        double qty;
        if (!(input >> id >> side_token >> price >> qty) ||
            (side_token != "B" && side_token != "S")) {
            std::cout << "usage: A <id> <B|S> <price> <qty>\n";
            return true;
        }
        const mdt::Side side = side_token == "B" ? mdt::Side::Buy : mdt::Side::Sell;
        if (book.add(id, side, to_ticks(price), qty)) {
            std::cout << "added order " << id << "\n";
        } else {
            std::cout << "add rejected: duplicate id or non-positive qty\n";
        }
        return true;
    }
    case 'C': {
        mdt::OrderId id;
        if (!(input >> id)) {
            std::cout << "usage: C <id>\n";
            return true;
        }
        if (book.cancel(id)) {
            std::cout << "cancelled order " << id << "\n";
        } else {
            std::cout << "cancel rejected: unknown id\n";
        }
        return true;
    }
    case 'M': {
        mdt::OrderId id;
        double qty;
        if (!(input >> id >> qty)) {
            std::cout << "usage: M <id> <qty>\n";
            return true;
        }
        if (book.modify(id, qty)) {
            std::cout << "modified order " << id << "\n";
        } else {
            std::cout << "modify rejected: unknown id or non-positive qty\n";
        }
        return true;
    }
    case 'P':
        print_book(book, 5);
        return true;
    case 'D': {
        std::size_t n_levels;
        if (!(input >> n_levels) || n_levels == 0) {
            std::cout << "usage: D <n>\n";
            return true;
        }
        print_book(book, n_levels);
        return true;
    }
    case 'H':
        print_help();
        return true;
    case 'Q':
        return false;
    default:
        std::cout << "unknown command, type H for help\n";
        return true;
    }
}

} // namespace

int main() {
    std::cout << "book_demo - in-memory limit order book\n";
    print_help();

    mdt::OrderBook book;
    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }
        if (!handle_command(book, line)) {
            break;
        }
    }
    std::cout << "bye\n";
    return 0;
}
