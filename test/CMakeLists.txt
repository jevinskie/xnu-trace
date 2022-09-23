add_executable(yield-loop-step-test yield-loop-step-test.cpp)
target_link_libraries(yield-loop-step-test xnu-trace argparse fmt)

add_executable(xnu-trace-bench xnu-trace-bench.cpp)
target_link_libraries(xnu-trace-bench xnu-trace argparse benchmark fmt xxhash-xnu-trace)

add_executable(xnu-fast-timeout xnu-fast-timeout.cpp)
target_link_libraries(xnu-fast-timeout xnu-trace fmt)

add_executable(xnu-trace-mph xnu-trace-mph.cpp)
target_link_libraries(xnu-trace-mph xnu-trace fmt)

install(TARGETS yield-loop-step-test xnu-trace-bench xnu-fast-timeout xnu-trace-mph
    RUNTIME DESTINATION bin
)
