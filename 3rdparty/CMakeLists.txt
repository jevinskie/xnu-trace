add_subdirectory(argparse)

option(CAPSTONE_ARCHITECTURE_DEFAULT "" OFF)
option(CAPSTONE_ARM64_SUPPORT "" ON)
option(CAPSTONE_BUILD_CSTOOL "" ON)
add_subdirectory(capstone)

add_subdirectory(CoreSymbolication)

add_subdirectory(fmt)

option(INT_TREE_ENABLE_TESTS "" OFF)
add_subdirectory(interval-tree)

set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)
option(LIEF_PYTHON_API "" OFF)
option(LIEF_C_API "" OFF)
option(LIEF_EXAMPLES "" OFF)
option(LIEF_LOGGING "" ON)
option(LIEF_LOGGING_DEBUG "" ON)
option(LIEF_ENABLE_JSON "" OFF)
option(LIEF_ELF "" OFF)
option(LIEF_PE "" OFF)
option(LIEF_MACHO "" ON)
option(LIEF_DEX "" OFF)
option(LIEF_ART "" OFF)
add_subdirectory(LIEF)


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
