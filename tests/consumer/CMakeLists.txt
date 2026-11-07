cmake_minimum_required(VERSION 3.10)
project(GGPOInstalledConsumers LANGUAGES C CXX)

find_package(GGPO CONFIG REQUIRED)

function(ggpo_add_installed_consumer target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE GGPO::GGPO)
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
