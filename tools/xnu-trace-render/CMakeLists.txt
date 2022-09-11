add_executable(xnu-trace-render xnu-trace-render.cpp)

target_link_libraries(xnu-trace-render xnu-single-step-trace argparse fmt)
target_compile_options(xnu-trace-render PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS xnu-trace-render
    RUNTIME DESTINATION bin
)
