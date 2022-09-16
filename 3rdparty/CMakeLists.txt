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

set(BENCHMARK_ENABLE_TESTING OFF)
add_subdirectory(benchmark)

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

