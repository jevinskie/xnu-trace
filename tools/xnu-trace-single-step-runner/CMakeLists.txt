add_executable(xnu-trace-single-step-runner xnu-trace-single-step-runner.cpp)

target_link_libraries(xnu-trace-single-step-runner xnu-trace argparse fmt)
target_compile_options(xnu-trace-single-step-runner PRIVATE -Wall -Wextra -Wpedantic)

add_custom_command(TARGET xnu-trace-single-step-runner
                   POST_BUILD
                   COMMAND codesign -s - -o linker-signed --entitlements ${CMAKE_CURRENT_SOURCE_DIR}/ent.xml $<TARGET_FILE:xnu-trace-single-step-runner>
)

set_target_properties(xnu-trace-single-step-runner PROPERTIES
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    # XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
)

install(TARGETS xnu-trace-single-step-runner
    RUNTIME DESTINATION bin
)
