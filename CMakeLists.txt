cmake_minimum_required(VERSION 3.10)

# TODO: SDK Versioning.
project(GGPO VERSION 0.1.0)

include(CTest)

# Remove RelWithDebInfo and MinSizeRelease build types to match the original Visual Studio Project. 
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

include(src/cmake/properties.cmake)
include(src/cmake/helper_methods.cmake)

if(WIN32)
	## Prevent the CMake trying to install GGPO in Program Files on Windows
	if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
		set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/installed" CACHE PATH "Default install path" FORCE)
	endif()
endif()

# What do we want to build?
option(GGPO_BUILD_SDK "Enable the build of the GGPO SDK" ON)
option(GGPO_BUILD_VECTORWAR "Enable the build of the Vector War example app" ON)
option(BUILD_SHARED_LIBS "Enable the build of shared libraries (.dll/.so) instead of static ones (.lib/.a)" ON)
option(GGPO_RUN_TESTS_ON_BUILD "Run self-contained GGPO tests during the default build" ON)

if(GGPO_BUILD_SDK)
	add_subdirectory(src)
	install(FILES LICENSE README.md DESTINATION share/GGPO)
endif()

if(GGPO_BUILD_VECTORWAR)
	# Vector War is Windows only.
	if(WIN32)
		add_subdirectory(src/apps/vectorwar)
	else()
		message(WARNING "The Vector War app only supports Windows, skipping...")
	endif()
endif()

if(BUILD_TESTING AND GGPO_BUILD_SDK)
	add_subdirectory(tests)
	if(GGPO_RUN_TESTS_ON_BUILD)
		if(WIN32)
			set(GGPO_PLATFORM_TEST_TARGETS
				ggpo_windows_public_layout
				ggpo_windows_packet_layout
				ggpo_windows_synctest_smoke
				ggpo_windows_peer_smoke)
		else()
			set(GGPO_PLATFORM_TEST_TARGETS
				ggpo_linux_synctest_smoke
				ggpo_linux_peer_smoke)
		endif()
		add_custom_target(ggpo_check ALL
			COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C $<CONFIG>
			DEPENDS
				${GGPO_PLATFORM_TEST_TARGETS}
				ggpo_fake_core_synctest
				ggpo_fake_platform_contract
			USES_TERMINAL
		)
	endif()
endif()
