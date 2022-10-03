add_custom_command(OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/mach_exc.h
    ${CMAKE_CURRENT_BINARY_DIR}/mach_excServer.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_excUser.c
    COMMAND mig ${CMAKE_CURRENT_SOURCE_DIR}/mach_exc.defs
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mach_exc.defs
)

add_library(xnu-trace-mach-exc STATIC ${CMAKE_CURRENT_BINARY_DIR}/mach_excServer.c ${CMAKE_CURRENT_BINARY_DIR}/mach_exc.h)
target_compile_options(xnu-trace-mach-exc PRIVATE -Wall -Wextra -Wpedantic)

target_include_directories(xnu-trace-mach-exc
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)


set(INSTTRACE_SRC
    ARM64InstrHistogram.cpp
    common-internal.h
    CompressedFile.cpp
    dyld.cpp
    exception_handlers.cpp
    FridaStalker.cpp
    log_structs.cpp
    mach.cpp
    macho.cpp
    MachORegions.cpp
    MinimalPerfectHash.cpp
    proc.cpp
    Signpost.cpp
    Symbols.cpp
    ThreadPool.cpp
    TraceLog.cpp
    utils.cpp
    VMRegions.cpp
    XNUCommpageTime.cpp
    XNUTracer.cpp
)

set(INSTTRACE_HDR_STANDALONE
    Atomic.h
    drcov.h
    xnu-trace.h
    xnu-trace-c.h
)

set(INSTTRACE_HDR)
foreach(HDR ${INSTTRACE_HDR_STANDALONE})
    set(HDR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/xnu-trace/${HDR}")
    list(APPEND INSTTRACE_HDR ${HDR})
endforeach()

foreach(SRC ${INSTTRACE_SRC})
    get_filename_component(HDR_NAME ${SRC} NAME_WLE)
    set(HDR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/xnu-trace/${HDR_NAME}.h")
    if(EXISTS ${HDR})
        list(APPEND INSTTRACE_HDR ${HDR})
    endif()
endforeach()

set(XNUTRACE_PUBLIC_LIBS
    interval-tree
    frida-gum
    absl::flat_hash_map
    thread-pool
    static_vector
)

set(XNUTRACE_PRIVATE_LIBS
    bn-arm64-disasm
    CoreSymbolication
    fmt
    libzstd_static
    mbedcrypto
    xxhash-xnu-trace
    xnu-trace-mach-exc
    fastmod
    range-v3
    simde
)

# INSTTRACE_HDR added for Xcode project generation
add_library(xnu-trace STATIC ${INSTTRACE_SRC} ${INSTTRACE_HDR})
add_library(xnu-trace-shared SHARED ${INSTTRACE_SRC} ${INSTTRACE_HDR})
set_target_properties(xnu-trace xnu-trace-shared PROPERTIES PUBLIC_HEADER "${INSTTRACE_HDR}")

target_link_libraries(xnu-trace
PUBLIC
    ${XNUTRACE_PUBLIC_LIBS}
PRIVATE
    ${XNUTRACE_PRIVATE_LIBS}
)
target_compile_options(xnu-trace PRIVATE -Wall -Wextra -Wpedantic)

target_include_directories(xnu-trace
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(xnu-trace-shared
PUBLIC
    ${XNUTRACE_PUBLIC_LIBS}
PRIVATE
    ${XNUTRACE_PRIVATE_LIBS}
)
target_compile_options(xnu-trace-shared PRIVATE -Wall -Wextra -Wpedantic)

target_include_directories(xnu-trace-shared
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)


install(TARGETS xnu-trace xnu-trace-shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    PUBLIC_HEADER DESTINATION include/xnu-trace
)

# target_compile_options(xnu-trace PRIVATE -Xclang -fdump-record-layouts)

