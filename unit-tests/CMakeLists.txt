add_executable(xnu-trace-unit-tests xnu-trace-unit-tests.cpp MinimalPerfectHash.cpp)
target_link_libraries(xnu-trace-unit-tests PRIVATE xnu-trace fmt range-v3 Boost::ut)
