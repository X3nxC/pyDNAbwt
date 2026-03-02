cmake_minimum_required(VERSION 3.15)
project(dnabwt_c C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(dnabwt_core STATIC
    bwt/bwt_transform.c
    bwt/bwt_inverse.c
    bwt/bwt_search.c
    code/dna_rle.c
    code/sentinel.c
    io/file_reader.c
    io/file_writer.c
    util/alloc.c
    util/log.c
    util/progress.c
    util/signal.c
    util/status.c
)

target_include_directories(dnabwt_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
