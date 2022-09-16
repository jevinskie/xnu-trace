
set(INSTTRACE_SRC
    ARM64InstrHistogram.cpp
    common.h
    CompressedFile.cpp
    dyld.cpp
    exception_handlers.cpp
    FridaStalker.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/mach_excServer.c
    mach.cpp
    macho.cpp
    MachORegions.cpp
    proc.cpp
    Symbols.cpp
    TraceLog.cpp
    utils.cpp
    VMRegions.cpp
    XNUTracer.cpp
)

set(INSTTRACE_HDR_STANDALONE
    xnu-single-step-trace.h
    xnu-single-step-trace-c.h
)

set(INSTTRACE_HDR)
foreach(HDR ${INSTTRACE_HDR_STANDALONE})
    set(HDR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/xnu-single-step-trace/${HDR}")
    list(APPEND INSTTRACE_HDR ${HDR})
endforeach()

foreach(SRC ${INSTTRACE_SRC})
    get_filename_component(HDR_NAME ${SRC} NAME_WLE)
    set(HDR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/xnu-single-step-trace/${HDR_NAME}.h")
    if(EXISTS ${HDR})
        list(APPEND INSTTRACE_HDR ${HDR})
    endif()
endforeach()

add_custom_command(OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/mach_exc.h
    ${CMAKE_CURRENT_BINARY_DIR}/mach_excServer.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_excUser.c
    COMMAND mig ${CMAKE_CURRENT_SOURCE_DIR}/mach_exc.defs
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mach_exc.defs
)

# INSTTRACE_HDR added for Xcode project generation
add_library(xnu-single-step-trace SHARED ${INSTTRACE_SRC} ${INSTTRACE_HDR})
set_target_properties(xnu-single-step-trace PROPERTIES PUBLIC_HEADER "${INSTTRACE_HDR}")

target_link_libraries(xnu-single-step-trace
PUBLIC
    interval-tree
    frida-gum
PRIVATE
    bn-arm64-disasm
    CoreSymbolication
    fmt
    libzstd_static
    mbedcrypto
    perf-macos
)
target_compile_options(xnu-single-step-trace PRIVATE -Wall -Wextra -Wpedantic)

target_include_directories(xnu-single-step-trace
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

install(TARGETS xnu-single-step-trace
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/xnu-single-step-trace
)

# target_compile_options(xnu-single-step-trace PRIVATE -Xclang -fdump-record-layouts)

