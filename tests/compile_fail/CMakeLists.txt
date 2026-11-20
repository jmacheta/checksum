# cspell:words TOUPPER
# Each source compiles as is (control target) and must fail with CHECKSUM_EXPECT_COMPILE_ERROR. Entries are "<source>@<regex the build
# output must match>"; a rejected parameter set shows as std::bad_optional_access, a failed assert as the C library's assert handler. The
# sources of every_algorithm_tests build once per algorithm, with CHECKSUM_TEST_<ALGORITHM> choosing the engine of test_engine.hpp.
set(every_algorithm_tests "accumulator_from_temporary@deleted (function|constructor)" "string_literal_update@byte_range"
                          "unavailable_preset@isa_available\\(\\) rejects it"
)
set(crc_tests "engine_rejected_parameters@bad_optional_access" "invalid_parameters@bad_optional_access"
              "precondition_in_constant_evaluation@(__assert_fail|_wassert)"
)
set(algorithms
    adler32
    crc
    fletcher
    fletcher4
    internet
    murmur3
    xxh3
    xxhash
)

set(compile_fail_tests)
foreach (test IN LISTS every_algorithm_tests)
  foreach (algorithm IN LISTS algorithms)
    list(APPEND compile_fail_tests "${test}@${algorithm}")
  endforeach ()
endforeach ()
foreach (test IN LISTS crc_tests)
  list(APPEND compile_fail_tests "${test}@crc")
endforeach ()

foreach (test IN LISTS compile_fail_tests)
  string(REPLACE "@" ";" test "${test}")
  list(GET test 0 source)
  list(GET test 1 expected)
  list(GET test 2 algorithm)
  string(TOUPPER "CHECKSUM_TEST_${algorithm}" selection)
  set(name ${algorithm}_${source})

  add_library(compile_fail_${name}_control OBJECT ${source}.cpp)
  target_link_libraries(compile_fail_${name}_control PRIVATE checksum::checksum)
  target_compile_definitions(compile_fail_${name}_control PRIVATE ${selection})

  add_library(compile_fail_${name} OBJECT EXCLUDE_FROM_ALL ${source}.cpp)
  target_link_libraries(compile_fail_${name} PRIVATE checksum::checksum)
  target_compile_definitions(compile_fail_${name} PRIVATE ${selection} CHECKSUM_EXPECT_COMPILE_ERROR)

  add_test(NAME compile_fail.${name} COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target compile_fail_${name} --config $<CONFIG>)
  # PASS_REGULAR_EXPRESSION ignores the exit code; a successful build prints no diagnostic, so it cannot pass. Runs of the build tool on one build
  # tree must not overlap under ctest --parallel.
  set_tests_properties(compile_fail.${name} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}" RESOURCE_LOCK build_tree)
endforeach ()
