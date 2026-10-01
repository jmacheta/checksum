add_library(checksum STATIC)
add_library(checksum::checksum ALIAS checksum)
set_target_properties(checksum PROPERTIES EXPORT_NAME checksum)

target_compile_features(checksum PUBLIC cxx_std_23)

target_sources(
  checksum
  PRIVATE crc/crc_runtime.cpp
  PUBLIC FILE_SET
         HEADERS
         BASE_DIRS
         include
         FILES
         include/checksum/crc.hpp
         include/checksum/crc/catalog.hpp
         include/checksum/crc/engine.hpp
         include/checksum/crc/parameters.hpp
         include/checksum/crc/polynomial.hpp
         include/checksum/crc/table.hpp
  PRIVATE FILE_SET
          private_headers
          TYPE
          HEADERS
          BASE_DIRS
          include_private
          FILES
          include_private/checksum_private/crc_slicing.hpp
)
