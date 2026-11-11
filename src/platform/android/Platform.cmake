# Android shares the POSIX adapter with Linux; only build settings differ.
function(ggpo_configure_platform target)
	target_sources(${target} PRIVATE
		${PROJECT_SOURCE_DIR}/src/lib/ggpo/platform_posix.cpp
	)
	target_compile_options(${target} PRIVATE -Wall -Wextra)
	target_compile_definitions(${target} PRIVATE GGPO_PLATFORM_ANDROID)
	# -ffp-contract=off is applied project-wide for Android in the root build.
endfunction()
