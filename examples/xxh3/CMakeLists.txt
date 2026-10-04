foreach (example file_hash deduplicate hash_table)
  add_executable(checksum_example_xxh3_${example} ${example}.cpp)
  target_link_libraries(checksum_example_xxh3_${example} PRIVATE checksum::checksum)
endforeach ()
