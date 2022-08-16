add_subdirectory(argparse)

option(CAPSTONE_ARCHITECTURE_DEFAULT "" OFF)
option(CAPSTONE_ARM64_SUPPORT "" ON)
option(CAPSTONE_BUILD_CSTOOL "" ON)
add_subdirectory(capstone)

add_subdirectory(fmt)
