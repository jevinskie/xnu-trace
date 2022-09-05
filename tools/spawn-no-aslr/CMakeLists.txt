add_executable(spawn-no-aslr spawn-no-aslr.cpp)

target_link_libraries(spawn-no-aslr)
target_compile_options(spawn-no-aslr PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS spawn-no-aslr
    RUNTIME DESTINATION bin
)
