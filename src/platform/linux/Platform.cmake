# Linux uses the POSIX adapter shared with Android; this file owns the
# Linux-specific build settings for the otherwise platform-neutral target.
function(ggpo_configure_platform target)
	target_sources(${target} PRIVATE
		${PROJECT_SOURCE_DIR}/src/lib/ggpo/platform_posix.cpp
	)
	target_compile_options(${target} PRIVATE -Wall -Wextra)
	target_compile_definitions(${target} PRIVATE GGPO_PLATFORM_LINUX)
	get_target_property(target_type ${target} TYPE)
	if(target_type STREQUAL "SHARED_LIBRARY")
		set_target_properties(${target} PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
	endif()
endfunction()
