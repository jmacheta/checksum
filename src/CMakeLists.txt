add_library(checksum STATIC)
add_library(checksum::checksum ALIAS checksum)
set_target_properties(checksum PROPERTIES EXPORT_NAME checksum)

target_compile_features(checksum PUBLIC cxx_std_23)

target_sources(
  checksum
  PRIVATE crc/lut_braided.cpp crc/lut_byte.cpp crc/lut_nibble.cpp crc/lut_none.cpp crc/lut_sliced.cpp internet/sum_loop.cpp
  PUBLIC FILE_SET
         HEADERS
         BASE_DIRS
         include
         FILES
         include/checksum/byte_range.hpp
         include/checksum/crc.hpp
         include/checksum/crc_catalog.hpp
         include/checksum/internet.hpp
)

target_sources(checksum PRIVATE FILE_SET private_headers TYPE HEADERS BASE_DIRS include_private)
if (NOT CHECKSUM_ACCELERATION)
  target_compile_definitions(checksum PRIVATE CHECKSUM_ACCELERATION=0)
endif ()
