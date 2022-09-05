add_executable(xnu-single-step-trace-runner xnu-single-step-trace-runner.cpp)

target_link_libraries(xnu-single-step-trace-runner xnu-single-step-trace argparse fmt)
target_compile_options(xnu-single-step-trace-runner PRIVATE -Wall -Wextra -Wpedantic)

add_custom_command(TARGET xnu-single-step-trace-runner
                   POST_BUILD
                   COMMAND codesign -s - --entitlements ${CMAKE_CURRENT_SOURCE_DIR}/ent.xml $<TARGET_FILE:xnu-single-step-trace-runner>
)

set_target_properties(xnu-single-step-trace-runner PROPERTIES
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    # XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
)

install(TARGETS xnu-single-step-trace-runner
    RUNTIME DESTINATION bin
)
