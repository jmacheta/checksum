# Bare-metal check of detect_isa() and every dispatch engine, run in QEMU system mode with semihosting where the preset names
# an emulator. The preset's flags define CHECKSUM_EXPECTED_ISA and CHECKSUM_EXPECTED_BUILT.
enable_language(ASM)

add_executable(checksum_bare_metal isa_check.cpp startup.S)
target_compile_features(checksum_bare_metal PRIVATE cxx_std_23)
target_include_directories(checksum_bare_metal PRIVATE ${PROJECT_SOURCE_DIR}/src/include_private)
target_link_libraries(checksum_bare_metal PRIVATE checksum::checksum)

if (CMAKE_CROSSCOMPILING_EMULATOR)
  add_test(NAME bare_metal.isa_check COMMAND checksum_bare_metal)
  set_tests_properties(bare_metal.isa_check PROPERTIES TIMEOUT 300)
endif ()
