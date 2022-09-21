add_executable(xnu-trace-log-util xnu-trace-log-util.cpp)

target_link_libraries(xnu-trace-log-util xnu-trace argparse fmt benchmark)
target_compile_options(xnu-trace-log-util PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS xnu-trace-log-util
    RUNTIME DESTINATION bin
)
