include(CMakeSources.cmake)

# Linkage selection.  Leaving BUILD_SHARED_LIBS unset builds both libraries so
# one install can serve dynamic and static consumers.  Passing it explicitly
# keeps the historical single-flavor build used by existing scripts.
if(DEFINED BUILD_SHARED_LIBS)
	if(BUILD_SHARED_LIBS)
		set(GGPO_BUILD_SHARED_DEFAULT ON)
		set(GGPO_BUILD_STATIC_DEFAULT OFF)
	else()
		set(GGPO_BUILD_SHARED_DEFAULT OFF)
		set(GGPO_BUILD_STATIC_DEFAULT ON)
	endif()
else()
	set(GGPO_BUILD_SHARED_DEFAULT ON)
	set(GGPO_BUILD_STATIC_DEFAULT ON)
endif()
option(GGPO_BUILD_SHARED "Build the shared GGPO library (.dll/.so)" ${GGPO_BUILD_SHARED_DEFAULT})
option(GGPO_BUILD_STATIC "Build the static GGPO library (libGGPO.lib/libGGPO.a)" ${GGPO_BUILD_STATIC_DEFAULT})
if(NOT GGPO_BUILD_SHARED AND NOT GGPO_BUILD_STATIC)
	message(FATAL_ERROR "Enable GGPO_BUILD_SHARED, GGPO_BUILD_STATIC, or both")
endif()

if(WIN32)
	include(platform/windows/Platform.cmake)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
	include(platform/linux/Platform.cmake)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
	include(platform/android/Platform.cmake)
else()
	message(FATAL_ERROR "Unsupported GGPO platform: ${CMAKE_SYSTEM_NAME}")
endif()

# Create one linkage flavor.  Both flavors compile the same sources; on Windows
# only the DLL build defines GGPO_SDK_EXPORT, so the sources are compiled once
# per flavor rather than shared through an object library.
function(ggpo_add_library target type export_name)
	add_library(${target} ${type} ${GGPO_LIB_SRC})
	add_library(GGPO::${export_name} ALIAS ${target})
	add_common_flags(${target})
	target_include_directories(${target}
		PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
		$<INSTALL_INTERFACE:include>
		PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/lib/ggpo
	)
	ggpo_configure_platform(${target})
	set_target_properties(${target} PROPERTIES EXPORT_NAME ${export_name})
	# Android packages libraries by plain name (libGGPO.so); versioned files
	# and symlinks cannot be loaded from an APK.
	if(NOT ANDROID)
		set_target_properties(${target} PROPERTIES VERSION ${PROJECT_VERSION})
	endif()
endfunction()

set(GGPO_LIBRARY_TARGETS)
if(GGPO_BUILD_SHARED)
	ggpo_add_library(GGPO_shared SHARED shared)
	# MSVC: GGPO.dll + GGPO.lib (import library). MinGW: libGGPO.dll +
	# libGGPO.dll.a. Linux: libGGPO.so.
	set_target_properties(GGPO_shared PROPERTIES OUTPUT_NAME GGPO)
	list(APPEND GGPO_LIBRARY_TARGETS GGPO_shared)
endif()
if(GGPO_BUILD_STATIC)
	ggpo_add_library(GGPO_static STATIC static)
	if(MSVC)
		# libGGPO.lib, distinct from the DLL import library GGPO.lib.
		set_target_properties(GGPO_static PROPERTIES
			OUTPUT_NAME libGGPO
			COMPILE_PDB_NAME libGGPO
			COMPILE_PDB_OUTPUT_DIRECTORY_DEBUG ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG}
			COMPILE_PDB_OUTPUT_DIRECTORY_RELEASE ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE}
		)
	else()
		# libGGPO.a on Linux and MinGW.
		set_target_properties(GGPO_static PROPERTIES OUTPUT_NAME GGPO)
	endif()
	list(APPEND GGPO_LIBRARY_TARGETS GGPO_static)
endif()

# In-tree default: shared when built, otherwise static.  Mirrors the installed
# GGPO::GGPO default so tests and samples consume what most applications will.
if(GGPO_BUILD_SHARED)
	add_library(GGPO::GGPO ALIAS GGPO_shared)
else()
	add_library(GGPO::GGPO ALIAS GGPO_static)
endif()
set(GGPO_LIBRARY_TARGETS ${GGPO_LIBRARY_TARGETS} PARENT_SCOPE)

# Install
install(
	DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
	DESTINATION ./
	FILES_MATCHING
	PATTERN "*.h"
	PATTERN "*.hpp"
)

# Each flavor has its own export so an install may contain either or both.
if(GGPO_BUILD_SHARED)
	install(
		TARGETS GGPO_shared
		EXPORT GGPOSharedTargets
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
		INCLUDES DESTINATION include
	)
	install(
		EXPORT GGPOSharedTargets
		FILE GGPOSharedTargets.cmake
		NAMESPACE GGPO::
		DESTINATION lib/cmake/GGPO
	)
endif()
if(GGPO_BUILD_STATIC)
	install(
		TARGETS GGPO_static
		EXPORT GGPOStaticTargets
		ARCHIVE DESTINATION lib
		INCLUDES DESTINATION include
	)
	install(
		EXPORT GGPOStaticTargets
		FILE GGPOStaticTargets.cmake
		NAMESPACE GGPO::
		DESTINATION lib/cmake/GGPO
	)
endif()

# Recorded in GGPOConfig.cmake so a Windows consumer built with the other
# toolchain is rejected instead of failing at link time.
if(ANDROID)
	set(GGPO_TOOLCHAIN android)
elseif(MSVC)
	set(GGPO_TOOLCHAIN msvc)
elseif(MINGW)
	set(GGPO_TOOLCHAIN mingw)
else()
	set(GGPO_TOOLCHAIN gnu)
endif()

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
	FILES
		${CMAKE_CURRENT_BINARY_DIR}/GGPOConfig.cmake
		${CMAKE_CURRENT_BINARY_DIR}/GGPOConfigVersion.cmake
	DESTINATION lib/cmake/GGPO
)

if(MSVC)
	if(GGPO_BUILD_SHARED)
		install(
			FILES $<TARGET_PDB_FILE:GGPO_shared>
			DESTINATION lib
			OPTIONAL
		)
	endif()
	if(GGPO_BUILD_STATIC)
		# CMake cannot install a static library's compiler PDB directly; it is
		# written next to libGGPO.lib by COMPILE_PDB_OUTPUT_DIRECTORY above.
		install(
			FILES "${GGPO_LIBRARY_OUTPUT_DIR}/$<CONFIG>/libGGPO.pdb"
			DESTINATION lib
			OPTIONAL
		)
	endif()
endif()
