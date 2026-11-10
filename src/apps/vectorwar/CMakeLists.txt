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
endif()
