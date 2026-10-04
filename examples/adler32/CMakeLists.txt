foreach (example resume zlib_trailer)
  add_executable(checksum_example_adler32_${example} ${example}.cpp)
  target_link_libraries(checksum_example_adler32_${example} PRIVATE checksum::checksum)
endforeach ()
