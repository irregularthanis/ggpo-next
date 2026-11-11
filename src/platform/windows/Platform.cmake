# Windows owns every native source, definition, and system-library input used by
# the otherwise platform-neutral GGPO target.
function(ggpo_configure_platform target)
	target_sources(${target} PRIVATE
		${PROJECT_SOURCE_DIR}/src/lib/ggpo/platform_windows.cpp
	)
	if(MSVC)
		target_compile_options(${target} PRIVATE "/W4" "/WX")
	else()
		# MinGW-w64.
		target_compile_options(${target} PRIVATE -Wall -Wextra)
	endif()
	target_compile_definitions(${target} PRIVATE
		_CRT_SECURE_NO_WARNINGS
		GGPO_PLATFORM_WINDOWS
	)
	# Bare names resolve for both MSVC (winmm.lib) and MinGW (libwinmm.a).
	target_link_libraries(${target} PUBLIC winmm ws2_32)
	get_target_property(target_type ${target} TYPE)
	if(target_type STREQUAL "SHARED_LIBRARY")
		target_compile_definitions(${target} PUBLIC GGPO_SHARED_LIB)
		target_compile_definitions(${target} PRIVATE GGPO_SDK_EXPORT)
		if(MINGW)
			# Embed libgcc/libstdc++ (and winpthread) so libGGPO.dll needs no
			# MinGW runtime DLLs next to the application.
			target_link_options(${target} PRIVATE -static)
		endif()
	endif()
endfunction()
