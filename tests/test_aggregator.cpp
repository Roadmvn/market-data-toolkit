#include "doctest/doctest.h"

#include <vector>

#include "mdt/aggregator.hpp"

namespace {

mdt::Trade make_trade(long long timestamp_ms, double price, double qty) {
    return {0, price, qty, price * qty, timestamp_ms, false};
}

// Two 60s windows with hand-computed expectations:
//   window 0: prices 100, 110, 90, 105 -> O=100 H=110 L=90 C=105
//             volume 6, vwap = (100*1 + 110*2 + 90*1 + 105*2) / 6 = 620/6
//   window 1: single trade at 106
std::vector<mdt::Trade> fixture() {
    return {
        make_trade(1000, 100.0, 1.0),
        make_trade(2000, 110.0, 2.0),
        make_trade(3000, 90.0, 1.0),
        make_trade(59000, 105.0, 2.0),
        make_trade(60000, 106.0, 1.0),
    };
}

} // namespace

TEST_CASE("aggregate computes OHLC, volume and vwap per window") {
    auto windows = mdt::aggregate(fixture(), 60);
    REQUIRE(windows.size() == 2);

    CHECK(windows[0].window_start_ms == 0);
    CHECK(windows[0].open == doctest::Approx(100.0));
    CHECK(windows[0].high == doctest::Approx(110.0));
    CHECK(windows[0].low == doctest::Approx(90.0));
    CHECK(windows[0].close == doctest::Approx(105.0));
    CHECK(windows[0].volume == doctest::Approx(6.0));
    CHECK(windows[0].vwap == doctest::Approx(620.0 / 6.0));
    CHECK(windows[0].trade_count == 4);

    CHECK(windows[1].window_start_ms == 60000);
    CHECK(windows[1].open == doctest::Approx(106.0));
    CHECK(windows[1].close == doctest::Approx(106.0));
    CHECK(windows[1].volume == doctest::Approx(1.0));
    CHECK(windows[1].trade_count == 1);
}

TEST_CASE("aggregate buckets exactly at window boundaries") {
    std::vector<mdt::Trade> trades = {
        make_trade(59999, 100.0, 1.0), // last ms of window 0
        make_trade(60000, 101.0, 1.0), // first ms of window 1
    };
    auto windows = mdt::aggregate(trades, 60);
    REQUIRE(windows.size() == 2);
    CHECK(windows[0].window_start_ms == 0);
    CHECK(windows[1].window_start_ms == 60000);
}

TEST_CASE("aggregate sorts unsorted input defensively") {
    std::vector<mdt::Trade> trades = {
        make_trade(60000, 106.0, 1.0),
        make_trade(2000, 110.0, 2.0),
        make_trade(1000, 100.0, 1.0),
    };
    auto windows = mdt::aggregate(trades, 60);
    REQUIRE(windows.size() == 2);
    CHECK(windows[0].open == doctest::Approx(100.0));
    CHECK(windows[0].close == doctest::Approx(110.0));
    CHECK(windows[1].open == doctest::Approx(106.0));
}

TEST_CASE("aggregate handles empty input") {
    CHECK(mdt::aggregate({}, 60).empty());
}

TEST_CASE("overall_vwap matches hand-computed value") {
    auto trades = fixture();
    // (620 + 106*1) / 7
    CHECK(mdt::overall_vwap(trades) == doctest::Approx(726.0 / 7.0));
    CHECK(mdt::overall_vwap({}) == doctest::Approx(0.0));
}

TEST_CASE("realized_volatility matches a hand-computed series") {
    auto stats = [](double close) {
        return mdt::WindowStats{0, 0.0, 0.0, 0.0, close, 0.0, 0.0, 0};
    };
    // closes 100, 102, 101, 103: sample stddev of log returns
    // ln(1.02), ln(101/102), ln(103/101) computed externally.
    std::vector<mdt::WindowStats> windows = {stats(100.0), stats(102.0),
                                             stats(101.0), stats(103.0)};
    CHECK(mdt::realized_volatility(windows) == doctest::Approx(0.0170655063).epsilon(1e-6));

    // Constant growth ratio: every log return equal, zero deviation.
    std::vector<mdt::WindowStats> constant = {stats(100.0), stats(110.0), stats(121.0)};
    CHECK(mdt::realized_volatility(constant) == doctest::Approx(0.0));
}

TEST_CASE("realized_volatility returns 0 for fewer than 3 windows") {
    auto stats = [](double close) {
        return mdt::WindowStats{0, 0.0, 0.0, 0.0, close, 0.0, 0.0, 0};
    };
    CHECK(mdt::realized_volatility({}) == doctest::Approx(0.0));
    CHECK(mdt::realized_volatility({stats(100.0)}) == doctest::Approx(0.0));
    CHECK(mdt::realized_volatility({stats(100.0), stats(105.0)}) == doctest::Approx(0.0));
}
