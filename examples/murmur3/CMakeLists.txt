foreach (example compile_time bloom_filter deduplicate)
  add_executable(checksum_example_murmur3_${example} ${example}.cpp)
  target_link_libraries(checksum_example_murmur3_${example} PRIVATE checksum::checksum)
endforeach ()
