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
target_compile_definitions(pinggen PRIVATE PINGGEN_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")

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
        NAME pinggen-run-example
        COMMAND pinggen run ${CMAKE_SOURCE_DIR}/examples/hello
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
        NAME pinggen-check-invalid-for-bound-type
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_for_bound_type
    )
    set_tests_properties(pinggen-check-invalid-for-bound-type PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-for-assign-loop-var
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_for_assign_loop_var
    )
    set_tests_properties(pinggen-check-invalid-for-assign-loop-var PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-for-scope
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_for_scope
    )
    set_tests_properties(pinggen-check-invalid-for-scope PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-for-return
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_for_missing_return
    )
    set_tests_properties(pinggen-check-invalid-for-return PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-for-missing-in
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_for_missing_in
    )
    set_tests_properties(pinggen-check-invalid-for-missing-in PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-enum-duplicate
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_enum_duplicate
    )
    set_tests_properties(pinggen-check-invalid-enum-duplicate PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-enum-duplicate-variant
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_enum_duplicate_variant
    )
    set_tests_properties(pinggen-check-invalid-enum-duplicate-variant PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-enum-unknown-type
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_enum_unknown_type
    )
    set_tests_properties(pinggen-check-invalid-enum-unknown-type PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-enum-unknown-variant
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_enum_unknown_variant
    )
    set_tests_properties(pinggen-check-invalid-enum-unknown-variant PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-enum-compare-mismatch
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_enum_compare_mismatch
    )
    set_tests_properties(pinggen-check-invalid-enum-compare-mismatch PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-enum-operator
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_enum_operator
    )
    set_tests_properties(pinggen-check-invalid-enum-operator PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-enum-impl
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_enum_impl
    )
    set_tests_properties(pinggen-check-invalid-enum-impl PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-match-non-enum
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_match_non_enum_subject
    )
    set_tests_properties(pinggen-check-invalid-match-non-enum PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-match-missing-variant
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_match_missing_variant
    )
    set_tests_properties(pinggen-check-invalid-match-missing-variant PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-match-duplicate-arm
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_match_duplicate_arm
    )
    set_tests_properties(pinggen-check-invalid-match-duplicate-arm PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-match-wrong-enum
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_match_wrong_enum_arm
    )
    set_tests_properties(pinggen-check-invalid-match-wrong-enum PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-match-unknown-variant
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_match_unknown_variant
    )
    set_tests_properties(pinggen-check-invalid-match-unknown-variant PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-match-non-block-arm
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_match_non_block_arm
    )
    set_tests_properties(pinggen-check-invalid-match-non-block-arm PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-payload-enum-wrong-arity
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_payload_enum_wrong_arity
    )
    set_tests_properties(pinggen-check-invalid-payload-enum-wrong-arity PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-payload-enum-wrong-type
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_payload_enum_wrong_type
    )
    set_tests_properties(pinggen-check-invalid-payload-enum-wrong-type PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-payload-enum-call-plain
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_payload_enum_call_plain_variant
    )
    set_tests_properties(pinggen-check-invalid-payload-enum-call-plain PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-payload-enum-missing-argument
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_payload_enum_missing_argument
    )
    set_tests_properties(pinggen-check-invalid-payload-enum-missing-argument PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-payload-enum-equality
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_payload_enum_equality
    )
    set_tests_properties(pinggen-check-invalid-payload-enum-equality PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-payload-match-missing-binding
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_payload_match_missing_binding
    )
    set_tests_properties(pinggen-check-invalid-payload-match-missing-binding PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-plain-match-binding
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_plain_match_binding
    )
    set_tests_properties(pinggen-check-invalid-plain-match-binding PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-fs-read-missing-import
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_fs_read_missing_import
    )
    set_tests_properties(pinggen-check-invalid-fs-read-missing-import PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-fs-read-arity
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_fs_read_arity
    )
    set_tests_properties(pinggen-check-invalid-fs-read-arity PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-fs-read-type
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_fs_read_type
    )
    set_tests_properties(pinggen-check-invalid-fs-read-type PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-fs-result-without-import
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_fs_result_without_import
    )
    set_tests_properties(pinggen-check-invalid-fs-result-without-import PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-fs-write-missing-import
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_fs_write_missing_import
    )
    set_tests_properties(pinggen-check-invalid-fs-write-missing-import PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-fs-write-arity
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_fs_write_arity
    )
    set_tests_properties(pinggen-check-invalid-fs-write-arity PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-fs-write-path-type
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_fs_write_path_type
    )
    set_tests_properties(pinggen-check-invalid-fs-write-path-type PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-fs-write-content-type
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_fs_write_content_type
    )
    set_tests_properties(pinggen-check-invalid-fs-write-content-type PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-fs-write-result-without-import
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_fs_write_result_without_import
    )
    set_tests_properties(pinggen-check-invalid-fs-write-result-without-import PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-fs-exists-missing-import
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_fs_exists_missing_import
    )
    set_tests_properties(pinggen-check-invalid-fs-exists-missing-import PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-fs-exists-arity
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_fs_exists_arity
    )
    set_tests_properties(pinggen-check-invalid-fs-exists-arity PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-fs-exists-type
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_fs_exists_type
    )
    set_tests_properties(pinggen-check-invalid-fs-exists-type PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-env-get-missing-import
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_env_get_missing_import
    )
    set_tests_properties(pinggen-check-invalid-env-get-missing-import PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-env-get-arity
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_env_get_arity
    )
    set_tests_properties(pinggen-check-invalid-env-get-arity PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-env-get-type
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_env_get_type
    )
    set_tests_properties(pinggen-check-invalid-env-get-type PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-env-result-without-import
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_env_result_without_import
    )
    set_tests_properties(pinggen-check-invalid-env-result-without-import PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-con-empty
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_con_empty
    )
    set_tests_properties(pinggen-check-invalid-con-empty PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-con-non-call
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_con_non_call
    )
    set_tests_properties(pinggen-check-invalid-con-non-call PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-con-unsafe-call
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_con_unsafe_call
    )
    set_tests_properties(pinggen-check-invalid-con-unsafe-call PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-con-mut-method
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_con_mut_method
    )
    set_tests_properties(pinggen-check-invalid-con-mut-method PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-con-nested
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_con_nested
    )
    set_tests_properties(pinggen-check-invalid-con-nested PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-tuple-destructure-arity
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_tuple_destructure_arity
    )
    set_tests_properties(pinggen-check-invalid-tuple-destructure-arity PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-con-mixed-void
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_con_mixed_void
    )
    set_tests_properties(pinggen-check-invalid-con-mixed-void PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-str-len-missing-import
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_str_len_missing_import
    )
    set_tests_properties(pinggen-check-invalid-str-len-missing-import PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-str-len-arity
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_str_len_arity
    )
    set_tests_properties(pinggen-check-invalid-str-len-arity PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-str-len-type
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_str_len_type
    )
    set_tests_properties(pinggen-check-invalid-str-len-type PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-missing-module
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_missing_module
    )
    set_tests_properties(pinggen-check-invalid-missing-module PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-module-cycle
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_module_cycle
    )
    set_tests_properties(pinggen-check-invalid-module-cycle PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME pinggen-check-invalid-std-import-item
        COMMAND pinggen check ${CMAKE_SOURCE_DIR}/examples/invalid_std_import_item
    )
    set_tests_properties(pinggen-check-invalid-std-import-item PROPERTIES WILL_FAIL TRUE)
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
        NAME pinggen-run-fs-read-success
        COMMAND pinggen run ${CMAKE_SOURCE_DIR}/examples/runtime_fs_read_success
    )
    add_test(
        NAME pinggen-run-fs-read-missing
        COMMAND pinggen run ${CMAKE_SOURCE_DIR}/examples/runtime_fs_read_missing
    )
    add_test(
        NAME pinggen-run-fs-write-success
        COMMAND pinggen run ${CMAKE_SOURCE_DIR}/examples/runtime_fs_write_success
    )
    add_test(
        NAME pinggen-run-fs-write-missing
        COMMAND pinggen run ${CMAKE_SOURCE_DIR}/examples/runtime_fs_write_missing
    )
    add_test(
        NAME pinggen-run-fs-exists-success
        COMMAND pinggen run ${CMAKE_SOURCE_DIR}/examples/runtime_fs_exists_success
    )
    add_test(
        NAME pinggen-run-fs-exists-missing
        COMMAND pinggen run ${CMAKE_SOURCE_DIR}/examples/runtime_fs_exists_missing
    )
    add_test(
        NAME pinggen-run-env-get-success
        COMMAND pinggen run ${CMAKE_SOURCE_DIR}/examples/runtime_env_get_success
    )
    add_test(
        NAME pinggen-run-env-get-missing
        COMMAND pinggen run ${CMAKE_SOURCE_DIR}/examples/runtime_env_get_missing
    )
    add_test(
        NAME pinggen-run-con-success
        COMMAND pinggen run ${CMAKE_SOURCE_DIR}/examples/runtime_con_success
    )
    add_test(
        NAME pinggen-codegen-con-single
        COMMAND powershell -NoProfile -Command
                "& '$<TARGET_FILE:pinggen>' build '${CMAKE_SOURCE_DIR}/examples/codegen_con_single'; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; $text = Get-Content -Raw '${CMAKE_SOURCE_DIR}/examples/codegen_con_single/build/codegen_con_single.ll'; if (([regex]::Matches($text, 'call ptr @pinggen_con_group_create')).Count -ne 1) { throw 'expected one group create'; }; if (([regex]::Matches($text, 'call void @pinggen_con_spawn')).Count -ne 1) { throw 'expected one spawn'; }; if (([regex]::Matches($text, 'call void @pinggen_con_wait')).Count -ne 1) { throw 'expected one wait'; }; if (([regex]::Matches($text, 'define void @pinggen_con_task_')).Count -ne 1) { throw 'expected one task thunk'; }"
    )
    add_test(
        NAME pinggen-codegen-con-multi
        COMMAND powershell -NoProfile -Command
                "& '$<TARGET_FILE:pinggen>' build '${CMAKE_SOURCE_DIR}/examples/hello'; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; $text = Get-Content -Raw '${CMAKE_SOURCE_DIR}/examples/hello/build/hello.ll'; if (([regex]::Matches($text, 'call ptr @pinggen_con_group_create')).Count -ne 1) { throw 'expected one group create'; }; if (([regex]::Matches($text, 'call void @pinggen_con_spawn')).Count -ne 2) { throw 'expected two spawns'; }; if (([regex]::Matches($text, 'call void @pinggen_con_wait')).Count -ne 1) { throw 'expected one wait'; }; if (([regex]::Matches($text, 'define void @pinggen_con_task_')).Count -ne 2) { throw 'expected two task thunks'; }; if (([regex]::Matches($text, 'call i64 @Job__label_len')).Count -lt 1) { throw 'expected safe method call thunk'; }"
    )
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
