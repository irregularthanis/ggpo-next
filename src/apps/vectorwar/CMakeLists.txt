include(CMakeSources.cmake)

add_library(VectorWarCore STATIC
	${GGPO_EXAMPLES_VECTORWAR_SRC}
)

add_common_flags(VectorWarCore)
target_link_libraries(VectorWarCore PUBLIC GGPO::GGPO)
target_include_directories(VectorWarCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
	target_compile_options(VectorWarCore PRIVATE "/W4" "/WX")
else()
	target_compile_options(VectorWarCore PRIVATE -Wall -Wextra)
endif()

add_executable(VectorWarHeadless
	headless_main.cpp
	headless_host.cpp
	headless_host.h
)
add_common_flags(VectorWarHeadless)
target_link_libraries(VectorWarHeadless PRIVATE VectorWarCore)
if(MSVC)
	target_compile_options(VectorWarHeadless PRIVATE "/W4" "/WX")
	target_compile_definitions(VectorWarHeadless PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	target_compile_options(VectorWarHeadless PRIVATE -Wall -Wextra)
endif()

if(WIN32)
	add_executable(VectorWar WIN32
		${GGPO_EXAMPLES_VECTORWAR_WIN32_INC}
		${GGPO_EXAMPLES_VECTORWAR_WIN32_SRC}
		${GGPO_EXAMPLES_VECTORWAR_WIN32RES}
	)

	add_common_flags(VectorWar)
	target_compile_definitions(VectorWar PRIVATE _UNICODE UNICODE)
	target_link_libraries(VectorWar PRIVATE VectorWarCore)
	target_compile_options(VectorWar PRIVATE "/W4" "/WX")
else()
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
