# macOS uses the POSIX adapter shared with Linux and Android; this file owns
# the macOS-specific build settings. The deployment target (macOS 11) is set
# in the root build before project(), so every target shares it.
function(ggpo_configure_platform target)
	target_sources(${target} PRIVATE
		${PROJECT_SOURCE_DIR}/src/lib/ggpo/platform_posix.cpp
	)
	# Calling an API newer than the deployment target without an
	# availability check is an error, so macOS 11-14, which no CI runner
	# offers, cannot break at load time.
	target_compile_options(${target} PRIVATE
		-Wall -Wextra
		-Werror=unguarded-availability-new
	)
	target_compile_definitions(${target} PRIVATE GGPO_PLATFORM_MACOS)
	get_target_property(target_type ${target} TYPE)
	if(target_type STREQUAL "SHARED_LIBRARY")
		set_target_properties(${target} PROPERTIES
			SOVERSION ${PROJECT_VERSION_MAJOR}
			INSTALL_NAME_DIR "@rpath"
		)
	endif()
endfunction()
