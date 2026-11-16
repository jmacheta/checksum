add_library(checksum STATIC)
add_library(checksum::checksum ALIAS checksum)
set_target_properties(checksum PROPERTIES EXPORT_NAME checksum)

target_compile_features(checksum PUBLIC cxx_std_23)

target_sources(
  checksum
  PRIVATE adler32/sum_loop.cpp
          crc/lut_braided.cpp
          crc/lut_byte.cpp
          crc/lut_nibble.cpp
          crc/lut_none.cpp
          crc/lut_sliced.cpp
          fletcher/sum_loop.cpp
          fletcher4/sum_loop.cpp
          internet/sum_loop.cpp
          internet/sum_runtime.cpp
          isa/detect.cpp
          murmur3/block_loop.cpp
          xxh3/stripe_loop.cpp
          xxhash/stripe_loop.cpp
  PUBLIC FILE_SET
         HEADERS
         BASE_DIRS
         include
         FILES
         include/checksum/adler32.hpp
         include/checksum/byte_range.hpp
         include/checksum/crc.hpp
         include/checksum/crc_catalog.hpp
         include/checksum/fletcher.hpp
         include/checksum/fletcher4.hpp
         include/checksum/hash128.hpp
         include/checksum/internet.hpp
         include/checksum/isa.hpp
         include/checksum/murmur3.hpp
         include/checksum/xxh3.hpp
         include/checksum/xxhash.hpp
)

target_sources(checksum PRIVATE FILE_SET private_headers TYPE HEADERS BASE_DIRS include_private)
if (NOT CHECKSUM_ACCELERATION)
  target_compile_definitions(checksum PRIVATE CHECKSUM_ACCELERATION=0)
endif ()

# The Internet checksum loop for the internet_loop_for<isa_x86_avx2> strategy: this source alone gets -mavx2, so it
# compiles the AVX2 kernel whatever the build's flags. CHECKSUM_INTERNET_X86_AVX2 tells the headers it is there.
if (CHECKSUM_ACCELERATION AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$" AND CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
  set_source_files_properties(internet/sum_loop_x86_avx2.cpp PROPERTIES COMPILE_OPTIONS "-mavx2")
  target_sources(checksum PRIVATE internet/sum_loop_x86_avx2.cpp)
  target_compile_definitions(checksum PUBLIC CHECKSUM_INTERNET_X86_AVX2=1)
endif ()
