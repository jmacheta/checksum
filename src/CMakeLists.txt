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
          murmur3/block_loop.cpp
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
         include/checksum/internet.hpp
         include/checksum/murmur3.hpp
         include/checksum/xxhash.hpp
)

target_sources(checksum PRIVATE FILE_SET private_headers TYPE HEADERS BASE_DIRS include_private)
if (NOT CHECKSUM_ACCELERATION)
  target_compile_definitions(checksum PRIVATE CHECKSUM_ACCELERATION=0)
endif ()
