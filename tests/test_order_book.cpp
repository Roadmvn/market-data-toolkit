#include "doctest/doctest.h"

#include "mdt/order_book.hpp"

using mdt::OrderBook;
using mdt::Side;

TEST_CASE("empty book returns nullopt everywhere") {
    OrderBook book;
    CHECK_FALSE(book.best_bid().has_value());
    CHECK_FALSE(book.best_ask().has_value());
    CHECK_FALSE(book.spread().has_value());
    CHECK_FALSE(book.mid().has_value());
    CHECK(book.depth(Side::Buy, 5).empty());
    CHECK(book.depth(Side::Sell, 5).empty());
    CHECK(book.order_count() == 0);
}

TEST_CASE("add updates best prices, spread and mid") {
    OrderBook book;
    CHECK(book.add(1, Side::Buy, 42150, 1.0));
    CHECK(book.add(2, Side::Buy, 42140, 2.0));
    CHECK(book.add(3, Side::Sell, 42160, 0.5));
    CHECK(book.add(4, Side::Sell, 42170, 1.5));

    REQUIRE(book.best_bid().has_value());
    REQUIRE(book.best_ask().has_value());
    CHECK(*book.best_bid() == 42150);
    CHECK(*book.best_ask() == 42160);
    CHECK(*book.spread() == 10);
    CHECK(*book.mid() == doctest::Approx(42155.0));
    CHECK(book.order_count() == 4);
}

TEST_CASE("add rejects duplicate ids and non-positive qty") {
    OrderBook book;
    CHECK(book.add(1, Side::Buy, 100, 1.0));
    CHECK_FALSE(book.add(1, Side::Sell, 200, 1.0)); // duplicate id
    CHECK_FALSE(book.add(2, Side::Buy, 100, 0.0));
    CHECK_FALSE(book.add(2, Side::Buy, 100, -1.0));
    CHECK(book.order_count() == 1);
}

TEST_CASE("depth aggregates orders at the same price") {
    OrderBook book;
    book.add(1, Side::Buy, 42150, 1.0);
    book.add(2, Side::Buy, 42150, 0.5);
    book.add(3, Side::Buy, 42140, 2.0);

    auto levels = book.depth(Side::Buy, 5);
    REQUIRE(levels.size() == 2);
    CHECK(levels[0].price_ticks == 42150); // best bid first
    CHECK(levels[0].total_qty == doctest::Approx(1.5));
    CHECK(levels[0].order_count == 2);
    CHECK(levels[1].price_ticks == 42140);
    CHECK(levels[1].order_count == 1);

    // n_levels caps the result.
    CHECK(book.depth(Side::Buy, 1).size() == 1);
}

TEST_CASE("depth on the ask side is sorted best (lowest) first") {
    OrderBook book;
    book.add(1, Side::Sell, 42170, 1.0);
    book.add(2, Side::Sell, 42160, 1.0);

    auto levels = book.depth(Side::Sell, 5);
    REQUIRE(levels.size() == 2);
    CHECK(levels[0].price_ticks == 42160);
    CHECK(levels[1].price_ticks == 42170);
}

TEST_CASE("cancel removes the order and rejects unknown ids") {
    OrderBook book;
    book.add(1, Side::Buy, 42150, 1.0);
    book.add(2, Side::Buy, 42150, 0.5);

    CHECK(book.cancel(1));
    CHECK_FALSE(book.cancel(1)); // already gone
    CHECK_FALSE(book.cancel(99));
    CHECK(book.order_count() == 1);

    auto levels = book.depth(Side::Buy, 5);
    REQUIRE(levels.size() == 1);
    CHECK(levels[0].total_qty == doctest::Approx(0.5));
    CHECK(levels[0].order_count == 1);
}

TEST_CASE("cancelling the last order at a price erases the level") {
    OrderBook book;
    book.add(1, Side::Buy, 42150, 1.0);
    book.add(2, Side::Buy, 42140, 1.0);
    book.add(3, Side::Sell, 42160, 1.0);

    CHECK(book.cancel(1));
    REQUIRE(book.best_bid().has_value());
    CHECK(*book.best_bid() == 42140); // 42150 level gone

    CHECK(book.cancel(3));
    CHECK_FALSE(book.best_ask().has_value()); // ask side empty again
    CHECK_FALSE(book.spread().has_value());
}

TEST_CASE("modify changes quantity but keeps price and side") {
    OrderBook book;
    book.add(1, Side::Buy, 42150, 1.0);
    book.add(2, Side::Buy, 42150, 2.0);

    CHECK(book.modify(1, 3.0));
    auto levels = book.depth(Side::Buy, 5);
    REQUIRE(levels.size() == 1);
    CHECK(levels[0].price_ticks == 42150);
    CHECK(levels[0].total_qty == doctest::Approx(5.0));
    CHECK(levels[0].order_count == 2);

    CHECK_FALSE(book.modify(99, 1.0)); // unknown id
    CHECK_FALSE(book.modify(1, 0.0));
    CHECK_FALSE(book.modify(1, -2.0));
}
