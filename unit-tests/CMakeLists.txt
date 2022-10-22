set(XNUTRACE_UNIT_TEST_SRC
    ARM64Disassembler.cpp
    BitVector.cpp
    EliasFano.cpp
    MinimalPerfectHash.cpp
    RankSelect.cpp
    memmem-chunking.cpp
)

add_executable(xnu-trace-unit-tests ${XNUTRACE_UNIT_TEST_SRC})
target_link_libraries(xnu-trace-unit-tests PRIVATE xnu-trace fmt range-v3 Catch2 Catch2WithMain)
