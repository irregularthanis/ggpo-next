include(CMakeSources.cmake)

add_library(VectorWarCore STATIC
	${GGPO_EXAMPLES_VECTORWAR_SRC}
)

add_common_flags(VectorWarCore)
# VectorWarNet: matchmaking client and path proxy (see matchmade.h).
target_link_libraries(VectorWarCore PUBLIC GGPO::GGPO VectorWarNet)
if(ANDROID)
	# matchmade.cpp reports matchmaking progress to logcat.
	target_link_libraries(VectorWarCore PUBLIC log)
endif()
target_include_directories(VectorWarCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
	target_compile_options(VectorWarCore PRIVATE "/W4" "/WX")
else()
	target_compile_options(VectorWarCore PRIVATE -Wall -Wextra)
endif()

# Matchmaking client and path proxy (hole punching with relay fallback).
set(GGPO_VECTORWAR_NET_ALL ${GGPO_EXAMPLES_VECTORWAR_NET_SRC} ${GGPO_EXAMPLES_VECTORWAR_NET_IO_SRC})
add_library(VectorWarNet STATIC ${GGPO_VECTORWAR_NET_ALL})
add_common_flags(VectorWarNet)
target_include_directories(VectorWarNet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/net)
if(WIN32)
	target_link_libraries(VectorWarNet PUBLIC ws2_32)
else()
	find_package(Threads REQUIRED)
	target_link_libraries(VectorWarNet PUBLIC Threads::Threads)
endif()
if(MSVC)
	target_compile_options(VectorWarNet PRIVATE "/W4")
	target_compile_definitions(VectorWarNet PRIVATE _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
else()
	target_compile_options(VectorWarNet PRIVATE -Wall -Wextra)
endif()

add_executable(VectorWarHeadless
	headless_main.cpp
	headless_host.cpp
	headless_host.h
)
add_common_flags(VectorWarHeadless)
target_link_libraries(VectorWarHeadless PRIVATE VectorWarCore VectorWarNet)
if(MSVC)
	target_compile_options(VectorWarHeadless PRIVATE "/W4" "/WX")
	target_compile_definitions(VectorWarHeadless PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(VectorWarHeadless PRIVATE -Wall -Wextra)
endif()

if(GGPO_BUILD_VECTORWAR_GUI AND WIN32)
	add_executable(VectorWar WIN32
		${GGPO_EXAMPLES_VECTORWAR_WIN32_INC}
		${GGPO_EXAMPLES_VECTORWAR_WIN32_SRC}
		${GGPO_EXAMPLES_VECTORWAR_WIN32RES}
	)

	add_common_flags(VectorWar)
	target_compile_definitions(VectorWar PRIVATE _UNICODE UNICODE)
	target_link_libraries(VectorWar PRIVATE VectorWarCore)
	target_compile_options(VectorWar PRIVATE "/W4" "/WX")
elseif(GGPO_BUILD_VECTORWAR_GUI AND ANDROID)
	# VectorWarHeadless (above) always builds. The interactive APK frontend is
	# the same SDL2 frontend, built as the libmain.so that SDLActivity loads,
	# when the Gradle project supplies an SDL2 source tree.
	set(GGPO_ANDROID_SDL2_SOURCE_DIR "" CACHE PATH "SDL2 source tree for the Vector War APK")
	if(GGPO_ANDROID_SDL2_SOURCE_DIR)
		set(SDL_SHARED ON CACHE BOOL "" FORCE)
		set(SDL_STATIC OFF CACHE BOOL "" FORCE)
		set(SDL_TEST OFF CACHE BOOL "" FORCE)
		set(SDL_TESTS OFF CACHE BOOL "" FORCE)
		# GGPO installs only its SDK; never SDL2 into that prefix.
		set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
		add_subdirectory(${GGPO_ANDROID_SDL2_SOURCE_DIR}
			${CMAKE_CURRENT_BINARY_DIR}/sdl2 EXCLUDE_FROM_ALL)
		add_library(VectorWarAndroid SHARED ${GGPO_EXAMPLES_VECTORWAR_SDL_SRC})
		set_target_properties(VectorWarAndroid PROPERTIES OUTPUT_NAME main)
		add_common_flags(VectorWarAndroid)
		target_link_libraries(VectorWarAndroid PRIVATE VectorWarCore SDL2::SDL2 log android)
		target_compile_options(VectorWarAndroid PRIVATE -Wall -Wextra)
	endif()
elseif(GGPO_BUILD_VECTORWAR_GUI)
	find_package(SDL2 REQUIRED)
	add_executable(VectorWar ${GGPO_EXAMPLES_VECTORWAR_SDL_SRC})
	add_common_flags(VectorWar)
	target_link_libraries(VectorWar PRIVATE VectorWarCore SDL2::SDL2)
	target_compile_options(VectorWar PRIVATE -Wall -Wextra)
	if(BUILD_TESTING)
		add_test(NAME linux_vectorwar_frontend_smoke
			COMMAND VectorWar --frontend-smoke)
		set_tests_properties(linux_vectorwar_frontend_smoke PROPERTIES
			ENVIRONMENT "SDL_VIDEODRIVER=dummy"
			LABELS "native;sample;vectorwar-host")
	endif()
endif()
