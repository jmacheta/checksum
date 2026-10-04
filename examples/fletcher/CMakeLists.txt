foreach (example compile_time file_checksum frame_check)
  add_executable(checksum_example_fletcher_${example} ${example}.cpp)
  target_link_libraries(checksum_example_fletcher_${example} PRIVATE checksum::checksum)
endforeach ()
