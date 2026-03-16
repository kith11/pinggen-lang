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
    add_test(
        NAME pinggen-check-invalid-string-plus-mismatch
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_string_plus_mismatch
    )
    set_tests_properties(pinggen-check-invalid-string-plus-mismatch PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-array-mixed-literal
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_array_mixed_literal
    )
    set_tests_properties(pinggen-check-invalid-array-mixed-literal PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-array-size-mismatch
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_array_size_mismatch
    )
    set_tests_properties(pinggen-check-invalid-array-size-mismatch PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-array-index-type
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_array_index_type
    )
    set_tests_properties(pinggen-check-invalid-array-index-type PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-array-assign-immutable
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_array_assign_immutable
    )
    set_tests_properties(pinggen-check-invalid-array-assign-immutable PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-array-operator
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_array_operator
    )
    set_tests_properties(pinggen-check-invalid-array-operator PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-run-invalid-array-bounds
        COMMAND pinggen run ${CMAKE_SOURCE_DIR}/examples/runtime_invalid_array_bounds
    )
    set_tests_properties(pinggen-run-invalid-array-bounds PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-unknown-impl
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_unknown_impl
    )
    set_tests_properties(pinggen-check-invalid-unknown-impl PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-duplicate-method
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_duplicate_method
    )
    set_tests_properties(pinggen-check-invalid-duplicate-method PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-missing-self
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_missing_self
    )
    set_tests_properties(pinggen-check-invalid-missing-self PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-self-outside-method
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_self_outside_method
    )
    set_tests_properties(pinggen-check-invalid-self-outside-method PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-method-on-non-struct
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_method_on_non_struct
    )
    set_tests_properties(pinggen-check-invalid-method-on-non-struct PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-unknown-method
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_unknown_method
    )
    set_tests_properties(pinggen-check-invalid-unknown-method PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-self-field-write
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_self_field_write
    )
    set_tests_properties(pinggen-check-invalid-self-field-write PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-mut-method-immutable
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_mut_method_immutable
    )
    set_tests_properties(pinggen-check-invalid-mut-method-immutable PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-mut-method-temporary
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_mut_method_temporary
    )
    set_tests_properties(pinggen-check-invalid-mut-method-temporary PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-mut-method-field-receiver
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_mut_method_field_receiver
    )
    set_tests_properties(pinggen-check-invalid-mut-method-field-receiver PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-mut-self-position
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_mut_self_position
    )
    set_tests_properties(pinggen-check-invalid-mut-self-position PROPERTIES WILL_FAIL TRUE)
endif()
