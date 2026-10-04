foreach (example compile_time file_checksum zfs_block)
  add_executable(checksum_example_fletcher4_${example} ${example}.cpp)
  target_link_libraries(checksum_example_fletcher4_${example} PRIVATE checksum::checksum)
endforeach ()
