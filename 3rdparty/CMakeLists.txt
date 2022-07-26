add_subdirectory(arch-arm64/disassembler)

add_subdirectory(argparse)

option(ZSTD_LEGACY_SUPPORT "" OFF)
option(ZSTD_BUILD_PROGRAMS "" ON)
option(ZSTD_BUILD_STATIC "" ON)
option(ZSTD_BUILD_SHARED "" OFF)
add_subdirectory(zstd/build/cmake)
target_include_directories(libzstd_static
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zstd/lib>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zstd/lib/common>
)

add_subdirectory(CoreSymbolication)

add_subdirectory(fmt)

option(INT_TREE_ENABLE_TESTS "" OFF)
add_subdirectory(interval-tree)

option(ENABLE_PROGRAMS "" OFF)
option(ENABLE_TESTING "" OFF)
add_subdirectory(mbedtls)

# add_subdirectory(perf-macos)

add_subdirectory(abseil)
# add_subdirectory(compact_vector)
add_subdirectory(fastmod)
add_subdirectory(range-v3)

add_library(static_vector INTERFACE static_vector/include/experimental/fixed_capacity_vector)
target_include_directories(static_vector INTERFACE static_vector/include)
target_compile_definitions(static_vector INTERFACE FCV_NDEBUG)

add_library(thread-pool INTERFACE)
target_include_directories(thread-pool INTERFACE thread-pool)

add_library(xxhash-xnu-trace INTERFACE)
target_include_directories(xxhash-xnu-trace INTERFACE xxhash-scoped-include)

add_library(simde INTERFACE)
target_include_directories(simde INTERFACE simde-scoped-include)

find_package(Boost CONFIG REQUIRED headers)
set_target_properties(Boost::headers PROPERTIES IMPORTED_GLOBAL ON)

add_library(icecream INTERFACE)
target_include_directories(icecream INTERFACE icecream-scoped-include)

add_subdirectory(Catch2)

set(BENCHMARK_ENABLE_TESTING OFF)
add_subdirectory(benchmark)
add_subdirectory(nanobench)

cmake_policy(SET CMP0135 NEW)
include(FetchContent)
FetchContent_Declare(
    frida-gum-prebuilt
    FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER
    URL https://github.com/frida/frida/releases/download/15.2.2/frida-gum-devkit-15.2.2-macos-arm64.tar.xz
)
FetchContent_MakeAvailable(frida-gum-prebuilt)
FetchContent_GetProperties(frida-gum-prebuilt SOURCE_DIR FRIDA_GUM_SRC_DIR)

add_library(frida-gum STATIC IMPORTED GLOBAL)
set_target_properties(frida-gum PROPERTIES IMPORTED_LOCATION ${FRIDA_GUM_SRC_DIR}/libfrida-gum.a)
target_include_directories(frida-gum INTERFACE ${FRIDA_GUM_SRC_DIR})
