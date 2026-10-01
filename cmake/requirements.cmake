include(${CMAKE_CURRENT_LIST_DIR}/CPM.cmake)

if (CHECKSUM_TESTS)
  CPMAddPackage("gh:google/googletest@1.17.0")
  target_compile_options(gtest PUBLIC $<$<CXX_COMPILER_ID:GNU,Clang>:-Wno-null-dereference>)
endif ()
