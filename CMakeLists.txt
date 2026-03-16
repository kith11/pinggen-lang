cmake_minimum_required(VERSION 3.20)
project(pinggen LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(pinggen
    src/main.cpp
    src/token.cpp
    src/diagnostics.cpp
    src/lexer.cpp
    src/parser.cpp
    src/sema.cpp
    src/llvm_ir.cpp
    src/project.cpp
)

target_include_directories(pinggen PRIVATE include)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME pinggen-check-example
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/hello
    )
    add_test(
        NAME pinggen-build-example
        COMMAND pinggen build ${CMAKE_SOURCE_DIR}/examples/hello
    )
    add_test(
        NAME pinggen-check-invalid-arity
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_wrong_arity
    )
    set_tests_properties(pinggen-check-invalid-arity PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-return
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_missing_return
    )
    set_tests_properties(pinggen-check-invalid-return PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-duplicate-param
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_duplicate_param
    )
    set_tests_properties(pinggen-check-invalid-duplicate-param PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-if-condition
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_if_condition
    )
    set_tests_properties(pinggen-check-invalid-if-condition PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-string-compare
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_string_compare
    )
    set_tests_properties(pinggen-check-invalid-string-compare PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-bool-assign
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_bool_assign
    )
    set_tests_properties(pinggen-check-invalid-bool-assign PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-while-condition
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_while_condition
    )
    set_tests_properties(pinggen-check-invalid-while-condition PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-while-return
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_while_missing_return
    )
    set_tests_properties(pinggen-check-invalid-while-return PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-struct-duplicate-field
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_struct_duplicate_field
    )
    set_tests_properties(pinggen-check-invalid-struct-duplicate-field PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-struct-field-type
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_struct_field_type
    )
    set_tests_properties(pinggen-check-invalid-struct-field-type PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-field-assign-immutable
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_field_assign_immutable
    )
    set_tests_properties(pinggen-check-invalid-field-assign-immutable PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-break-outside-loop
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_break_outside_loop
    )
    set_tests_properties(pinggen-check-invalid-break-outside-loop PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-continue-outside-loop
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_continue_outside_loop
    )
    set_tests_properties(pinggen-check-invalid-continue-outside-loop PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-logical-operands
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_logical_operands
    )
    set_tests_properties(pinggen-check-invalid-logical-operands PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-relational-operands
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_relational_operands
    )
    set_tests_properties(pinggen-check-invalid-relational-operands PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-modulo-operands
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_modulo_operands
    )
    set_tests_properties(pinggen-check-invalid-modulo-operands PROPERTIES WILL_FAIL TRUE)
endif()
