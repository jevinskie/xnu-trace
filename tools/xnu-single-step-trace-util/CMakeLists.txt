add_executable(xnu-single-step-trace-util xnu-single-step-trace-util.cpp)

target_link_libraries(xnu-single-step-trace-util xnu-single-step-trace argparse fmt)
