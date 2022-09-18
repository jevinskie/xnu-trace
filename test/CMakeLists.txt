add_executable(yield-loop-step-test yield-loop-step-test.cpp)
target_link_libraries(yield-loop-step-test xnu-single-step-trace argparse fmt)

add_executable(xnu-trace-bench xnu-trace-bench.cpp)
target_link_libraries(xnu-trace-bench xnu-single-step-trace argparse benchmark fmt xxhash-xnu-trace)

install(TARGETS yield-loop-step-test xnu-trace-bench
    RUNTIME DESTINATION bin
)
