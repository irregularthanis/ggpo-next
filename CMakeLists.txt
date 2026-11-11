cmake_minimum_required(VERSION 3.10)

# TODO: SDK Versioning.
project(GGPO VERSION 0.5.0)

include(CTest)

# Remove RelWithDebInfo and MinSizeRelease build types to match the original Visual Studio Project. 
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

include(src/cmake/properties.cmake)
include(src/cmake/helper_methods.cmake)

# 32-bit GCC defaults to x87 floating point (80-bit intermediates), which rounds
# differently from SSE2 used by MSVC x86, every x64 compiler, and Linux. Match
# them so deterministic simulations agree across peers. MSVC x86 already
# requires SSE2 by default.
if(MINGW AND CMAKE_SIZEOF_VOID_P EQUAL 4)
	add_compile_options(-msse2 -mfpmath=sse)
endif()

# Android (ARM in particular) contracts a*b+c into fused multiply-add, which
# rounds differently from x86 and broke Vector War's frozen checksum on arm64.
# Apply to every target: library, tests, and samples.
if(ANDROID)
	add_compile_options(-ffp-contract=off)
endif()

if(WIN32)
	## Prevent the CMake trying to install GGPO in Program Files on Windows
	if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
		set (CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/installed" CACHE PATH "Default install path" FORCE)
	endif()
endif()

# What do we want to build?
option(GGPO_BUILD_SDK "Enable the build of the GGPO SDK" ON)
# Vector War's Windows frontend is MSVC-only; MinGW builds the SDK alone.
if(MINGW)
	set(GGPO_BUILD_VECTORWAR_DEFAULT OFF)
else()
	set(GGPO_BUILD_VECTORWAR_DEFAULT ON)
endif()
option(GGPO_BUILD_VECTORWAR "Enable the build of the Vector War example app" ${GGPO_BUILD_VECTORWAR_DEFAULT})
if(MINGW AND GGPO_BUILD_VECTORWAR)
	message(FATAL_ERROR "Vector War is not supported with MinGW; configure with -DGGPO_BUILD_VECTORWAR=OFF")
endif()
# Linkage: leave BUILD_SHARED_LIBS unset to build both shared and static GGPO,
# or pass -DBUILD_SHARED_LIBS=ON/OFF for one flavor (see src/CMakeLists.txt).
# Cross-compiled tests (Android) cannot run on the build host.
if(CMAKE_CROSSCOMPILING)
	set(GGPO_RUN_TESTS_ON_BUILD_DEFAULT OFF)
else()
	set(GGPO_RUN_TESTS_ON_BUILD_DEFAULT ON)
endif()
option(GGPO_RUN_TESTS_ON_BUILD "Run self-contained GGPO tests during the default build" ${GGPO_RUN_TESTS_ON_BUILD_DEFAULT})

if(GGPO_BUILD_SDK)
	add_subdirectory(src)
	install(FILES LICENSE README.md DESTINATION share/GGPO)
endif()

if(GGPO_BUILD_VECTORWAR)
	# The application/session core is portable; platform frontends are selected
	# inside the sample directory.
	add_subdirectory(src/apps/vectorwar)
endif()

if(BUILD_TESTING AND GGPO_BUILD_SDK)
	add_subdirectory(tests)
	if(GGPO_RUN_TESTS_ON_BUILD)
		if(ANDROID)
			set(GGPO_POSIX_TEST_PREFIX android)
		else()
			set(GGPO_POSIX_TEST_PREFIX linux)
		endif()
		if(WIN32)
			set(GGPO_PLATFORM_TEST_TARGETS
				ggpo_windows_public_layout
				ggpo_windows_packet_layout
				ggpo_windows_synctest_smoke
				ggpo_windows_peer_smoke)
		else()
			set(GGPO_PLATFORM_TEST_TARGETS
				ggpo_${GGPO_POSIX_TEST_PREFIX}_public_layout
				ggpo_${GGPO_POSIX_TEST_PREFIX}_packet_layout
				ggpo_${GGPO_POSIX_TEST_PREFIX}_synctest_smoke
				ggpo_${GGPO_POSIX_TEST_PREFIX}_peer_smoke)
		endif()
		# The registered frontend lifecycle test must not race its executable link.
		if(TARGET VectorWar)
			list(APPEND GGPO_PLATFORM_TEST_TARGETS VectorWar)
		endif()
		get_property(GGPO_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
		if(GGPO_MULTI_CONFIG)
			set(GGPO_CTEST_CONFIG_ARGS -C $<CONFIG>)
		else()
			set(GGPO_CTEST_CONFIG_ARGS)
		endif()
		add_custom_target(ggpo_check ALL
			COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure ${GGPO_CTEST_CONFIG_ARGS}
			DEPENDS
				${GGPO_PLATFORM_TEST_TARGETS}
				${GGPO_PORTABLE_TEST_TARGETS}
				ggpo_fake_core_synctest
				ggpo_fake_platform_contract
			USES_TERMINAL
		)
	endif()
endif()

# Generate native Linux SDK packages from the same install rules as consumers.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND GGPO_BUILD_SDK)
	include(src/cmake/linux_packaging.cmake)
endif()
