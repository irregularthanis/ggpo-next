# Windows owns every native source, definition, and system-library input used by
# the otherwise platform-neutral GGPO target.
function(ggpo_configure_platform target)
	target_sources(${target} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/lib/ggpo/platform_windows.cpp
	)
	target_compile_options(${target} PRIVATE "/W4" "/WX")
	target_compile_definitions(${target} PRIVATE
		_CRT_SECURE_NO_WARNINGS
		GGPO_PLATFORM_WINDOWS
	)
	target_link_libraries(${target} PUBLIC winmm.lib ws2_32.lib)
	if(BUILD_SHARED_LIBS)
		target_compile_definitions(${target} PUBLIC GGPO_SHARED_LIB)
		target_compile_definitions(${target} PRIVATE GGPO_SDK_EXPORT)
	endif()
endfunction()
