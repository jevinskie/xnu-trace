add_executable(yield-loop-step-test yield-loop-step-test.cpp)
target_link_libraries(yield-loop-step-test xnu-single-step-trace argparse fmt)

install(TARGETS yield-loop-step-test
    RUNTIME DESTINATION bin
)
