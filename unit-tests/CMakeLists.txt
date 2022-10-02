set(XNUTRACE_UNIT_TEST_SRC
    MinimalPerfectHash.cpp
    memmem-chunking.cpp
)

add_executable(xnu-trace-unit-tests xnu-trace-unit-tests.cpp ${XNUTRACE_UNIT_TEST_SRC})
target_link_libraries(xnu-trace-unit-tests PRIVATE xnu-trace fmt range-v3 Boost::ut)

set(XNUTRACE_UNIT_TEST_CATCH2_SRC
    MinimalPerfectHash-catch2.cpp
    memmem-chunking-catch2.cpp
)

add_executable(xnu-trace-unit-tests-catch2 ${XNUTRACE_UNIT_TEST_CATCH2_SRC})
target_link_libraries(xnu-trace-unit-tests-catch2 PRIVATE xnu-trace fmt range-v3 Catch2 Catch2WithMain)
