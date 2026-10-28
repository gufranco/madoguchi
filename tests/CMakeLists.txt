cmake_minimum_required(VERSION 3.24)

project(MadoguchiTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
  URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  arduinojson
  URL https://github.com/bblanchon/ArduinoJson/archive/refs/tags/v7.4.3.tar.gz
  URL_HASH SHA256=a2a8a277fc3cd0404499199bf1468682ea3688f354be0eaf6af8648366911257
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR headers-only
)
FetchContent_MakeAvailable(googletest arduinojson)

option(MADO_COVERAGE "Instrument the library for coverage" OFF)

set(MADO_STRICT_FLAGS
  -Wall -Wextra -Wpedantic -Wshadow -Wpointer-arith -Wcast-qual -Wcast-align
  -Wconversion -Wsign-conversion -Wmissing-declarations -Wold-style-cast
  -Wundef -Wnull-dereference -Wdouble-promotion -Werror
)

add_library(mado_support STATIC support/FakeClock.cpp support/FakeClient.cpp support/TestApp.cpp)
target_include_directories(mado_support PUBLIC shim support ../src)
target_compile_options(mado_support PRIVATE -Wall -Wextra -Wpedantic -Werror)

file(GLOB MADO_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp)
add_executable(mado_tests ${MADO_TEST_SOURCES})
target_link_libraries(mado_tests PRIVATE mado_support GTest::gtest_main)
target_include_directories(mado_tests SYSTEM PRIVATE ${arduinojson_SOURCE_DIR}/src)
target_compile_options(mado_tests PRIVATE ${MADO_STRICT_FLAGS})
if(MADO_COVERAGE)
  target_compile_options(mado_tests PRIVATE --coverage -O0)
  target_link_options(mado_tests PRIVATE --coverage)
endif()

add_executable(mado_cxx11_check cxx11_check.cpp support/FakeClock.cpp)
set_target_properties(mado_cxx11_check PROPERTIES CXX_STANDARD 11 CXX_EXTENSIONS OFF)
target_include_directories(mado_cxx11_check PRIVATE shim support ../src)
target_compile_options(mado_cxx11_check PRIVATE ${MADO_STRICT_FLAGS})

enable_testing()
include(GoogleTest)
gtest_discover_tests(mado_tests)

file(GLOB MADO_EXAMPLE_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/examples/*_test.cpp)
if(MADO_EXAMPLE_TEST_SOURCES)
  add_executable(mado_example_tests ${MADO_EXAMPLE_TEST_SOURCES})
  target_link_libraries(mado_example_tests PRIVATE mado_support GTest::gtest_main)
  target_compile_options(mado_example_tests PRIVATE ${MADO_STRICT_FLAGS})
  gtest_discover_tests(mado_example_tests)
endif()

file(GLOB MADO_EXAMPLES LIST_DIRECTORIES true CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/../examples/*)
foreach(example IN LISTS MADO_EXAMPLES)
  file(GLOB example_tests CONFIGURE_DEPENDS ${example}/tests/*.test.cpp)
  if(example_tests)
    get_filename_component(example_name ${example} NAME)
    string(TOLOWER ${example_name} example_target)
    add_executable(mado_example_${example_target} ${example_tests})
    target_link_libraries(mado_example_${example_target} PRIVATE mado_support GTest::gtest_main)
    target_compile_options(mado_example_${example_target} PRIVATE ${MADO_STRICT_FLAGS})
    gtest_discover_tests(mado_example_${example_target} TEST_PREFIX "${example_name}.")
  endif()
endforeach()
add_test(NAME cxx11_check COMMAND mado_cxx11_check)

set(MADO_FUZZ_TARGETS request rest files json text)
option(MADO_FUZZ "Build libFuzzer targets; needs clang" OFF)
foreach(target IN LISTS MADO_FUZZ_TARGETS)
  add_executable(mado_fuzz_replay_${target} fuzz/${target}_fuzz.cpp fuzz/replay_main.cpp)
  target_link_libraries(mado_fuzz_replay_${target} PRIVATE mado_support)
  target_compile_options(mado_fuzz_replay_${target} PRIVATE ${MADO_STRICT_FLAGS})
  add_test(NAME fuzz_replay_${target}
           COMMAND mado_fuzz_replay_${target} ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/${target})
  if(MADO_FUZZ)
    add_executable(mado_fuzz_${target} fuzz/${target}_fuzz.cpp)
    target_link_libraries(mado_fuzz_${target} PRIVATE mado_support)
    target_compile_options(mado_fuzz_${target} PRIVATE ${MADO_STRICT_FLAGS}
                           -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
    target_link_options(mado_fuzz_${target} PRIVATE -fsanitize=fuzzer,address,undefined)
  endif()
endforeach()

add_executable(mado_host_server host_server/main.cpp host_server/PosixClient.cpp)
target_include_directories(mado_host_server PRIVATE shim support ../src)
target_compile_options(mado_host_server PRIVATE -Wall -Wextra -Wpedantic -Werror)
