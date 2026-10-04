foreach (example compile_time file_hash cache_key)
  add_executable(checksum_example_xxhash_${example} ${example}.cpp)
  target_link_libraries(checksum_example_xxhash_${example} PRIVATE checksum::checksum)
endforeach ()
