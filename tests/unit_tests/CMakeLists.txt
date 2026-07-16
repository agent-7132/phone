idf_component_register(SRCS "test_ui_utils.c"
                    "test_utils.c"
                    INCLUDE_DIRS "."
                    REQUIRES unity ui_utils utils)

target_compile_definitions(${COMPONENT_LIB} PRIVATE TEST_UNIT)