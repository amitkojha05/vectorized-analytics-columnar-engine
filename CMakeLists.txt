cmake_minimum_required(VERSION 3.20)
project(ColumnarEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
  add_compile_options(/W4 /permissive-)
  add_compile_options($<$<CONFIG:Release>:/O2>)
  add_compile_options($<$<CONFIG:Release>:/arch:AVX2>)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  add_compile_options($<$<CONFIG:Release>:-O3>)
  add_compile_options($<$<CONFIG:Release>:-march=native>)
  add_compile_options($<$<CONFIG:Release>:-funroll-loops>)
endif()

add_library(columnar_engine
  src/schema.cpp
  src/column.cpp
  src/storage.cpp
  src/scan.cpp
  src/filter.cpp
  src/aggregator.cpp
  src/compression.cpp
  src/query.cpp
  src/batch.cpp
  src/pipeline_executor.cpp
  src/engine_benchmark.cpp
)
target_include_directories(columnar_engine PUBLIC include)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(LZ4 QUIET liblz4)
  if(LZ4_FOUND)
    target_compile_definitions(columnar_engine PRIVATE HAVE_LZ4)
    target_link_libraries(columnar_engine PRIVATE ${LZ4_LIBRARIES})
    target_include_directories(columnar_engine PRIVATE ${LZ4_INCLUDE_DIRS})
  endif()
endif()

if(NOT LZ4_FOUND)
  find_package(lz4 CONFIG QUIET)
  if(TARGET lz4::lz4)
    target_compile_definitions(columnar_engine PRIVATE HAVE_LZ4)
    target_link_libraries(columnar_engine PRIVATE lz4::lz4)
  endif()
endif()

enable_testing()

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(run_tests
  tests/test_column.cpp
  tests/test_storage.cpp
  tests/test_scan.cpp
  tests/test_filter.cpp
  tests/test_aggregator.cpp
  tests/test_compression.cpp
  tests/test_query.cpp
  tests/test_batch.cpp
)
target_link_libraries(run_tests columnar_engine)
if(TARGET GTest::gtest_main)
  target_link_libraries(run_tests GTest::gtest_main)
else()
  target_link_libraries(run_tests gtest_main)
endif()
add_test(NAME all_tests COMMAND run_tests)

find_package(benchmark QUIET)
if(NOT benchmark_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    benchmark
    URL https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip
  )
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(benchmark)
  set(benchmark_FOUND TRUE)
endif()

if(benchmark_FOUND)
  foreach(bench scan compression aggregation)
    add_executable(bench_${bench} bench/bench_${bench}.cpp)
    target_link_libraries(bench_${bench} columnar_engine benchmark::benchmark)
    if(MSVC)
      target_compile_options(bench_${bench} PRIVATE /O2 /arch:AVX2)
    else()
      target_compile_options(bench_${bench} PRIVATE -O3 -march=native)
    endif()
  endforeach()
endif()

add_executable(engine_workloads bench/engine_workloads.cpp)
target_link_libraries(engine_workloads columnar_engine)
