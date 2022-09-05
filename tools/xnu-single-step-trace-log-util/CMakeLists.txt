add_executable(xnu-single-step-trace-log-util xnu-single-step-trace-log-util.cpp)

target_link_libraries(xnu-single-step-trace-log-util xnu-single-step-trace argparse fmt)
target_compile_options(xnu-single-step-trace-log-util PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS xnu-single-step-trace-log-util
    RUNTIME DESTINATION bin
)
