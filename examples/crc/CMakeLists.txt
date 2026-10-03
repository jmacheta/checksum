foreach (example compile_time file_checksum custom_crc runtime_parameters modbus_frame hardware_strategy)
  add_executable(checksum_example_crc_${example} ${example}.cpp)
  target_link_libraries(checksum_example_crc_${example} PRIVATE checksum::checksum)
endforeach ()
