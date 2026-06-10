#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "mdt/csv_parser.hpp"

namespace {

// Writes content to a unique temp file and removes it on scope exit.
struct TempCsv {
    std::filesystem::path path;

    explicit TempCsv(const std::string& content) {
        path = std::filesystem::temp_directory_path() /
               ("mdt_test_" + std::to_string(::getpid()) + "_" +
                std::to_string(counter++) + ".csv");
        std::ofstream out(path);
        out << content;
    }

    ~TempCsv() { std::filesystem::remove(path); }

    static inline int counter = 0;
};

} // namespace

TEST_CASE("parse_line accepts a valid Binance trades line") {
    auto trade = mdt::parse_line("123456,42000.50,0.25,10500.125,1767225600000,true,true");
    REQUIRE(trade.has_value());
    CHECK(trade->id == 123456);
    CHECK(trade->price == doctest::Approx(42000.50));
    CHECK(trade->qty == doctest::Approx(0.25));
    CHECK(trade->quote_qty == doctest::Approx(10500.125));
    CHECK(trade->timestamp_ms == 1767225600000LL);
    CHECK(trade->is_buyer_maker == true);
}

TEST_CASE("parse_line accepts all boolean variants") {
    const char* truthy[] = {"true", "True", "TRUE", "1"};
    const char* falsy[] = {"false", "False", "FALSE", "0"};
    for (const char* value : truthy) {
        auto trade = mdt::parse_line(std::string("1,1.0,1.0,1.0,1000,") + value + ",true");
        REQUIRE(trade.has_value());
        CHECK(trade->is_buyer_maker == true);
    }
    for (const char* value : falsy) {
        auto trade = mdt::parse_line(std::string("1,1.0,1.0,1.0,1000,") + value + ",true");
        REQUIRE(trade.has_value());
        CHECK(trade->is_buyer_maker == false);
    }
    CHECK_FALSE(mdt::parse_line("1,1.0,1.0,1.0,1000,yes,true").has_value());
}

TEST_CASE("parse_line normalizes microsecond timestamps to milliseconds") {
    auto micros = mdt::parse_line("1,1.0,1.0,1.0,1767225600000000,true,true");
    REQUIRE(micros.has_value());
    CHECK(micros->timestamp_ms == 1767225600000LL);

    auto millis = mdt::parse_line("1,1.0,1.0,1.0,1767225600000,true,true");
    REQUIRE(millis.has_value());
    CHECK(millis->timestamp_ms == 1767225600000LL);
}

TEST_CASE("parse_line rejects malformed lines") {
    CHECK_FALSE(mdt::parse_line("").has_value());
    CHECK_FALSE(mdt::parse_line("1,2,3").has_value());                          // too few fields
    CHECK_FALSE(mdt::parse_line("1,2,3,4,5,true,true,extra").has_value());      // too many fields
    CHECK_FALSE(mdt::parse_line("abc,1.0,1.0,1.0,1000,true,true").has_value()); // bad id
    CHECK_FALSE(mdt::parse_line("1,oops,1.0,1.0,1000,true,true").has_value());  // bad price
    CHECK_FALSE(mdt::parse_line("1,1.0,1.0,1.0,nope,true,true").has_value());   // bad timestamp
}

TEST_CASE("parse_file skips a header line") {
    TempCsv file("id,price,qty,quote_qty,time,is_buyer_maker,is_best_match\n"
                 "1,100.0,1.0,100.0,1000,true,true\n"
                 "2,101.0,2.0,202.0,2000,false,true\n");
    auto result = mdt::parse_file(file.path.string());
    CHECK(result.trades.size() == 2);
    CHECK(result.malformed_lines == 0);
    CHECK(result.trades[0].id == 1);
}

TEST_CASE("parse_file counts malformed lines without throwing") {
    TempCsv file("1,100.0,1.0,100.0,1000,true,true\n"
                 "garbage line\n"
                 "2,101.0,2.0,202.0,2000,false,true\n"
                 "3,broken,2.0,202.0,3000,false,true\n");
    auto result = mdt::parse_file(file.path.string());
    CHECK(result.trades.size() == 2);
    CHECK(result.malformed_lines == 2);
}

TEST_CASE("parse_file throws on a missing file") {
    CHECK_THROWS_AS(mdt::parse_file("/nonexistent/path/trades.csv"), std::runtime_error);
}
