# Each source compiles as is (control target) and must fail with CHECKSUM_EXPECT_COMPILE_ERROR. Entries are "<source>@<regex the build
# output must match>"; a rejected parameter set shows as std::bad_optional_access, a failed assert as the C library's assert handler.
set(compile_fail_tests
    "accumulator_from_temporary@deleted (function|constructor)" "engine_rejected_parameters@bad_optional_access"
    "invalid_parameters@bad_optional_access" "precondition_in_constant_evaluation@(__assert_fail|_wassert)" "string_literal_update@byte_range"
)

foreach (test IN LISTS compile_fail_tests)
  string(REPLACE "@" ";" test "${test}")
  list(GET test 0 name)
  list(GET test 1 expected)

  add_library(compile_fail_${name}_control OBJECT ${name}.cpp)
  target_link_libraries(compile_fail_${name}_control PRIVATE checksum::checksum)

  add_library(compile_fail_${name} OBJECT EXCLUDE_FROM_ALL ${name}.cpp)
  target_link_libraries(compile_fail_${name} PRIVATE checksum::checksum)
  target_compile_definitions(compile_fail_${name} PRIVATE CHECKSUM_EXPECT_COMPILE_ERROR)

  add_test(NAME compile_fail.${name} COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target compile_fail_${name} --config $<CONFIG>)
  # PASS_REGULAR_EXPRESSION ignores the exit code; a successful build prints no diagnostic, so it cannot pass.
  set_tests_properties(compile_fail.${name} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
endforeach ()
