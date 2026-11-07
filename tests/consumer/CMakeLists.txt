cmake_minimum_required(VERSION 3.2)
project(GGPOInstalledConsumers LANGUAGES C CXX)

if(NOT DEFINED GGPO_INSTALL_PREFIX)
    message(FATAL_ERROR "GGPO_INSTALL_PREFIX must identify an installed GGPO tree")
endif()

find_path(GGPO_INSTALLED_INCLUDE_DIR ggponet.h
    PATHS "${GGPO_INSTALL_PREFIX}/include"
    NO_DEFAULT_PATH
)
find_library(GGPO_INSTALLED_LIBRARY GGPO
    PATHS "${GGPO_INSTALL_PREFIX}/lib"
    NO_DEFAULT_PATH
)
if(NOT GGPO_INSTALLED_INCLUDE_DIR OR NOT GGPO_INSTALLED_LIBRARY)
    message(FATAL_ERROR "Installed GGPO header or library was not found under GGPO_INSTALL_PREFIX")
endif()

function(ggpo_add_installed_consumer target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE "${GGPO_INSTALLED_INCLUDE_DIR}")
    target_link_libraries(${target} PRIVATE "${GGPO_INSTALLED_LIBRARY}")
    if(WIN32)
        target_link_libraries(${target} PRIVATE winmm.lib ws2_32.lib)
    endif()
    if(GGPO_SHARED_LIB)
        target_compile_definitions(${target} PRIVATE GGPO_SHARED_LIB)
    endif()
endfunction()

# These targets deliberately consume only installed files. They catch public
# header or link-interface regressions that an in-tree GGPO target can conceal.
ggpo_add_installed_consumer(ggpo_installed_c11 consumer.c)
set_property(TARGET ggpo_installed_c11 PROPERTY C_STANDARD 11)
set_property(TARGET ggpo_installed_c11 PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET ggpo_installed_c11 PROPERTY C_EXTENSIONS OFF)

ggpo_add_installed_consumer(ggpo_installed_cpp98 consumer.cpp)
set_property(TARGET ggpo_installed_cpp98 PROPERTY CXX_STANDARD 98)
set_property(TARGET ggpo_installed_cpp98 PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ggpo_installed_cpp98 PROPERTY CXX_EXTENSIONS OFF)
