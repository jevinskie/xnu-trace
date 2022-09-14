add_executable(xnu-trace-compressed-file-util xnu-trace-compressed-file-util.cpp)

target_link_libraries(xnu-trace-compressed-file-util xnu-single-step-trace argparse fmt)
target_compile_options(xnu-trace-compressed-file-util PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS xnu-trace-compressed-file-util
    RUNTIME DESTINATION bin
)
