idf_component_register(SRCS "test_app_signature.c"
                    "test_firmware_verify.c"
                    INCLUDE_DIRS "."
                    REQUIRES unity app_signature firmware_verify)

target_compile_definitions(${COMPONENT_LIB} PRIVATE TEST_INTEGRATION)