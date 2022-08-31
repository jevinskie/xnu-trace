add_executable(xnu-single-step-trace-log-util xnu-single-step-trace-log-util.cpp)

target_link_libraries(xnu-single-step-trace-log-util xnu-single-step-trace argparse fmt)
