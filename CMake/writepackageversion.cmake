# Include a version file in source packages, so that the version derived
# from the git repository becomes available for builds from source tarballs.

if(NOT CPACK_INSTALL_CMAKE_PROJECTS)
	file(WRITE "${CPACK_TEMPORARY_DIRECTORY}/VERSION"
		"${CPACK_SOURCE_PACKAGE_FILE_NAME}")
endif()
