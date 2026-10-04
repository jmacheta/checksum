foreach (example ipv4_header udp_datagram ttl_decrement)
  add_executable(checksum_example_internet_${example} ${example}.cpp)
  target_link_libraries(checksum_example_internet_${example} PRIVATE checksum::checksum)
endforeach ()
