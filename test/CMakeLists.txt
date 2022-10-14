add_executable(yield-loop-step-test yield-loop-step-test.cpp)
target_link_libraries(yield-loop-step-test xnu-trace argparse fmt)

add_executable(xnu-trace-bench xnu-trace-bench.cpp)
target_link_libraries(xnu-trace-bench xnu-trace argparse benchmark fmt xxhash-xnu-trace)

add_executable(xnu-trace-nanobench xnu-trace-nanobench.cpp)
target_link_libraries(xnu-trace-nanobench xnu-trace argparse nanobench fmt xxhash-xnu-trace)


add_executable(xnu-fast-timeout xnu-fast-timeout.cpp)
target_link_libraries(xnu-fast-timeout xnu-trace fmt)

add_executable(xnu-trace-mph xnu-trace-mph.cpp)
target_link_libraries(xnu-trace-mph xnu-trace fmt)

add_executable(hana-101 hana-101.cpp)
target_link_libraries(hana-101 fmt Boost::headers)

add_executable(vtable-opt vtable-opt.cpp)
target_link_libraries(vtable-opt xnu-trace benchmark)

install(TARGETS yield-loop-step-test xnu-trace-bench xnu-trace-nanobench xnu-fast-timeout xnu-trace-mph
    RUNTIME DESTINATION bin
)
