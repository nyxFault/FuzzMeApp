cmake_minimum_required(VERSION 3.22.1)
project(fuzzme_native)

add_library(
        fuzzme
        SHARED
        native-lib.cpp)

find_library(log-lib log)

target_link_libraries(
        fuzzme
        ${log-lib})
