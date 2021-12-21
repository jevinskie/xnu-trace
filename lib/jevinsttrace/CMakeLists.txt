
set(INSTTRACE_SRC
    jevinsttrace.cpp
)

set(INSTTRACE_HDR_STANDALONE
)

set(INSTTRACE_HDR)
foreach(HDR ${INSTTRACE_HDR_STANDALONE})
    set(HDR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/jevinsttrace/${HDR}")
    list(APPEND INSTTRACE_HDR ${HDR})
endforeach()

foreach(SRC ${INSTTRACE_SRC})
    string(REGEX REPLACE "\.cpp$" ".h" HDR ${SRC})
    set(HDR "${CMAKE_CURRENT_SOURCE_DIR}/../../include/jevinsttrace/${HDR}")
    list(APPEND INSTTRACE_HDR ${HDR})
endforeach()

list(APPEND INSTTRACE_SRC mach_excServer.c)

# INSTTRACE_HDR added for Xcode project generation
add_library(jevinsttrace ${INSTTRACE_SRC} ${INSTTRACE_HDR})


target_link_libraries(jevinsttrace PUBLIC fmt)
target_compile_definitions(jevinsttrace PUBLIC USE_FMT=1)
target_compile_options(jevinsttrace PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(jevinsttrace PRIVATE -fno-rtti -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables)

target_include_directories(jevinsttrace PUBLIC ../../include)

# target_compile_options(jevinsttrace PRIVATE -Xclang -fdump-record-layouts)

