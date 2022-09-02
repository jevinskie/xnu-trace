
set(INSTTRACE_SRC
    xnu-single-step-trace.cpp
)

set(INSTTRACE_HDR_STANDALONE
)

set(INSTTRACE_HDR)
foreach(HDR ${INSTTRACE_HDR_STANDALONE})
    set(HDR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/xnu-single-step-trace/${HDR}")
    list(APPEND INSTTRACE_HDR ${HDR})
endforeach()

foreach(SRC ${INSTTRACE_SRC})
    string(REGEX REPLACE "\.cpp$" ".h" HDR ${SRC})
    set(HDR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/xnu-single-step-trace/${HDR}")
    list(APPEND INSTTRACE_HDR ${HDR})
endforeach()

list(APPEND INSTTRACE_SRC mach_excServer.c)

add_custom_command(OUTPUT
    ${CMAKE_CURRENT_BINARY_DIR}/mach_exc.h
    ${CMAKE_CURRENT_BINARY_DIR}/mach_excServer.c
    ${CMAKE_CURRENT_BINARY_DIR}/mach_excUser.c
    COMMAND mig ${CMAKE_CURRENT_SOURCE_DIR}/mach_exc.defs
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mach_exc.defs
)

# INSTTRACE_HDR added for Xcode project generation
add_library(xnu-single-step-trace ${INSTTRACE_SRC} ${INSTTRACE_HDR})


target_link_libraries(xnu-single-step-trace PRIVATE capstone CoreSymbolication fmt LIEF::LIEF)
target_compile_options(xnu-single-step-trace PRIVATE -Wall -Wextra -Wpedantic)

target_include_directories(xnu-single-step-trace
    PUBLIC ../../include
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

# target_compile_options(xnu-single-step-trace PRIVATE -Xclang -fdump-record-layouts)

