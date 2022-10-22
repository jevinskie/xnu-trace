add_executable(capstone-arm64-enumerate-mem-insn capstone-arm64-enumerate-mem-insn.cpp)

target_link_libraries(capstone-arm64-enumerate-mem-insn PRIVATE bn-arm64-disasm fmt frida-gum)
target_compile_options(capstone-arm64-enumerate-mem-insn PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS capstone-arm64-enumerate-mem-insn
    RUNTIME DESTINATION bin
)
