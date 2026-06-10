# market-data-toolkit

Small C++20 toolkit built to learn the market-data side of HFT: a fast Binance tick-data parser and an in-memory limit order book.

## Features

- CSV parser for Binance public trade dumps (data.binance.vision), built on `std::from_chars` - no locale, no allocations, no exceptions on the hot path
- Time-window aggregator: OHLC, volume, VWAP per window, overall VWAP, realized volatility
- In-memory limit order book with integer-tick prices: add / cancel / modify, best bid/ask, spread, mid, aggregated depth
- `tickstats` CLI to summarize a trades file, `book_demo` REPL to play with the book
- Unit tests with doctest, CI on Linux and macOS

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Requires CMake 3.20+ and a C++20 compiler. On toolchains whose standard library lacks floating-point `std::from_chars` (Apple's libc++), the parser falls back to `strtod` automatically.

## Usage

### tickstats

```
./build/tickstats data/sample_trades.csv --window 60
```

```
window start (UTC)         open      high       low     close      volume      vwap  trades
2026-01-01 00:00:00    42154.18  42154.18  42079.03  42082.09     70.1966  42104.61      53
2026-01-01 00:01:00    42072.84  42136.53  42021.54  42021.83     78.1041  42077.96      60
2026-01-01 00:02:00    42033.86  42061.42  42008.18  42021.37     52.7435  42042.82      37

trades: 150  malformed lines: 0  overall vwap: 42078.05  realized volatility: 0.1006%
```

### book_demo

Interactive, but also works with piped input:

```
printf 'A 1 B 421.50 0.5\nA 2 S 421.60 1.2\nP\nC 1\nP\nQ\n' | ./build/book_demo
```

```
book_demo - in-memory limit order book
Commands:
  A <id> <B|S> <price> <qty>   add a limit order
  C <id>                       cancel an order
  M <id> <qty>                 modify order quantity
  P                            print top 5 levels and summary
  D <n>                        print top n levels and summary
  H                            show this help
  Q                            quit
Prices use 2 decimals, e.g. A 1 B 421.50 0.5
> added order 1
> added order 2
>       BIDS qty @ price   |   price @ qty ASKS
     0.500 @    421.50   |      421.60 @      1.200
best bid: 421.50  best ask: 421.60  spread: 0.10  mid: 421.55  orders: 2
> cancelled order 1
>       BIDS qty @ price   |   price @ qty ASKS
                         |      421.60 @      1.200
best bid: n/a  best ask: 421.60  spread: n/a  mid: n/a  orders: 1
> bye
```

## Getting real data

`data/sample_trades.csv` is a small synthetic sample so everything runs out of the box. For real data:

```
scripts/download_data.sh BTCUSDT 2024-01-15
./build/tickstats data/BTCUSDT-trades-2024-01-15.csv --window 300
```

The script pulls the daily spot trades dump from data.binance.vision and unzips it into `data/`.

## Design notes

- **`std::from_chars` for parsing.** It reads numbers straight out of the input buffer: no locale lookup, no `std::string` allocation, no exceptions. On multi-million-line dumps this is significantly faster than `stringstream` or `std::stod`. Malformed lines are counted and skipped, never thrown on.
- **Integer ticks for prices.** Order book levels are keyed by `int64_t` ticks, never doubles. Price keys must compare exactly; doubles carry representation error (421.50 has no exact binary form) and would silently split one level into several.
- **`std::map` for the book sides.** Bids use `std::greater` so `begin()` is always the best price on both sides, and depth queries are a simple in-order walk. The trade-off is O(log n) per update and pointer-chasing through tree nodes. Real HFT books avoid this with contiguous price-level arrays indexed by tick offset, or intrusive structures tuned for cache locality - worth doing, but not the point of this project.
- **Microsecond normalization.** Some Binance dumps use microsecond timestamps; the parser detects values above 10^15 and normalizes them to milliseconds.
- **Out of scope: matching engine.** The book answers queries only (best, spread, mid, depth). Crossing and order matching are intentionally left for the next project.

## Tests

```
ctest --test-dir build --output-on-failure
```

Unit tests (doctest) cover the parser edge cases, hand-computed aggregation fixtures and the full order book life cycle.
