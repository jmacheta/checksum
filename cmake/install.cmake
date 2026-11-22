include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS checksum EXPORT checksum FILE_SET HEADERS)
install(EXPORT checksum FILE checksumConfig.cmake NAMESPACE checksum:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/checksum)

write_basic_package_version_file(checksumConfigVersion.cmake COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/checksumConfigVersion.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/checksum)
