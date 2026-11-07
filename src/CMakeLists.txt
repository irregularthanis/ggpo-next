include(CMakeSources.cmake)

add_library(GGPO
	${GGPO_LIB_SRC}
)
add_library(GGPO::GGPO ALIAS GGPO)

add_common_flags(GGPO)

target_include_directories(GGPO
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
	PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/lib/ggpo
)

if(NOT WIN32)
	message(FATAL_ERROR
		"The GGPO SDK currently supports only the Windows platform layer. "
		"Linux and Apple adapters are planned follow-up work.")

else()
	include(platform/windows/Platform.cmake)
	ggpo_configure_platform(GGPO)
endif()

set_target_properties(GGPO PROPERTIES VERSION ${PROJECT_VERSION})

# Install
install(
	DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
	DESTINATION ./
	FILES_MATCHING
	PATTERN "*.h"
	PATTERN "*.hpp"
)

install(
	TARGETS GGPO
	EXPORT GGPOTargets
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	INCLUDES DESTINATION include
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/GGPOConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/GGPOConfig.cmake
	INSTALL_DESTINATION lib/cmake/GGPO
)
write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/GGPOConfigVersion.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion
)
install(
	EXPORT GGPOTargets
	FILE GGPOTargets.cmake
	NAMESPACE GGPO::
	DESTINATION lib/cmake/GGPO
)
install(
	FILES
		${CMAKE_CURRENT_BINARY_DIR}/GGPOConfig.cmake
		${CMAKE_CURRENT_BINARY_DIR}/GGPOConfigVersion.cmake
	DESTINATION lib/cmake/GGPO
)

if(MSVC)
	if(BUILD_SHARED_LIBS)
		install(
			FILES $<TARGET_PDB_FILE:GGPO>
			DESTINATION lib
			OPTIONAL
		)
	else()
		## NOTE: This is rather hacky, since CMake doesn't allow to install the PDB files for
		##	     static libraries, so we get the debug PDB file and copy it there.
		install(
			FILES "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG}/GGPO.pdb"
			DESTINATION lib
			OPTIONAL
		)
	endif()
else()
## TODO place the symbols install when GGPO became cross-platform
endif()
