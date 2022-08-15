add_executable(xnu-single-step-trace-util xnu-single-step-trace-util.cpp)

target_link_libraries(xnu-single-step-trace-util xnu-single-step-trace argparse fmt)

add_custom_command(TARGET xnu-single-step-trace-util
                   POST_BUILD
                   COMMAND codesign -s - --entitlements ${CMAKE_CURRENT_SOURCE_DIR}/ent.xml $<TARGET_FILE:xnu-single-step-trace-util>
)

set_target_properties(xnu-single-step-trace-util PROPERTIES
    XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    # XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
)
