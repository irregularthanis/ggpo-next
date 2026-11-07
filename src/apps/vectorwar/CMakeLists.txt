include(CMakeSources.cmake)

add_executable(VectorWar WIN32 
	${GGPO_EXAMPLES_VECTORWAR_SRC}
)

add_common_flags(VectorWar)
# Change the character set to unicode.
add_definitions(-D_UNICODE -DUNICODE)

# Platform link requirements are carried by the SDK target.
target_link_libraries(VectorWar LINK_PUBLIC GGPO::GGPO)
target_compile_options(VectorWar PRIVATE "/W4" "/WX")
