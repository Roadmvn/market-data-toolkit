cmake_minimum_required(VERSION 3.20)
project(market_data_toolkit CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_library(mdt
    src/csv_parser.cpp
    src/aggregator.cpp
    src/order_book.cpp
)
target_include_directories(mdt PUBLIC include)

add_executable(tickstats src/tickstats_main.cpp)
target_link_libraries(tickstats PRIVATE mdt)

add_executable(book_demo src/book_demo_main.cpp)
target_link_libraries(book_demo PRIVATE mdt)

enable_testing()

add_executable(mdt_tests
    tests/test_csv_parser.cpp
    tests/test_aggregator.cpp
    tests/test_order_book.cpp
)
target_link_libraries(mdt_tests PRIVATE mdt)
target_include_directories(mdt_tests PRIVATE third_party)
add_test(NAME mdt_tests COMMAND mdt_tests)
