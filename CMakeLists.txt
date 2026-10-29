cmake_minimum_required(VERSION 3.25)
project(order_book LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OB_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(OB_SANITIZERS "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(OB_NATIVE "Tune for the build machine (-march=native)" OFF)

# ---------------------------------------------------------------------------
# ob_options: link every target against this to get warnings / sanitizers.
#   target_link_libraries(<target> PRIVATE ob_options)
# ---------------------------------------------------------------------------
add_library(ob_options INTERFACE)

target_compile_options(ob_options INTERFACE
    -Wall -Wextra -Wpedantic
    -Wshadow -Wconversion -Wsign-conversion
    -Wold-style-cast -Wcast-align -Wnull-dereference
    -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough
    -Wnon-virtual-dtor -Woverloaded-virtual
    $<$<BOOL:${OB_WARNINGS_AS_ERRORS}>:-Werror>
)

if(OB_SANITIZERS)
    target_compile_options(ob_options INTERFACE
        -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    target_link_options(ob_options INTERFACE -fsanitize=address,undefined)
endif()

if(OB_NATIVE)
    target_compile_options(ob_options INTERFACE -march=native)
endif()

# Keep frame pointers in profiling builds so `perf record -g` gives usable stacks.
target_compile_options(ob_options INTERFACE
    $<$<CONFIG:RelWithDebInfo>:-fno-omit-frame-pointer>)

# ---------------------------------------------------------------------------
# Targets — add your own below, e.g.
#   add_executable(<name> src/<file>.cpp)
#   target_link_libraries(<name> PRIVATE ob_options)
# ---------------------------------------------------------------------------

add_executable(counter src/tools/message_counter.cc)
target_link_libraries(counter PRIVATE ob_options)
target_include_directories(counter PRIVATE src)

enable_testing()

# ---------------------------------------------------------------------------
# Tests — GoogleTest, fetched at configure time.
# SYSTEM marks its headers as system headers, so our strict warnings
# (-Wold-style-cast etc. + -Werror) don't fire on code we don't own.
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    SYSTEM
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(parser_test tests/parser_test.cc)
target_link_libraries(parser_test PRIVATE ob_options GTest::gtest_main)
target_include_directories(parser_test PRIVATE src)
gtest_discover_tests(parser_test)
