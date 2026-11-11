cmake_minimum_required(VERSION 3.10)
project(GGPOInstalledConsumers LANGUAGES C CXX)

# How the consumers select GGPO from an installed SDK:
#   default  GGPO::GGPO with no preference (shared when installed)
#   static   GGPO::GGPO with GGPO_USE_STATIC=ON
#   shared   GGPO::shared, named explicitly
set(GGPO_CONSUMER_LINKAGE "default" CACHE STRING "default, static, or shared")
set_property(CACHE GGPO_CONSUMER_LINKAGE PROPERTY STRINGS default static shared)
# Optional: the flavor GGPO::GGPO must resolve to (shared or static).
set(GGPO_EXPECT_LINKAGE "" CACHE STRING "Expected resolved flavor")

if(GGPO_CONSUMER_LINKAGE STREQUAL "static")
    set(GGPO_USE_STATIC ON)
    find_package(GGPO CONFIG REQUIRED)
    set(GGPO_CONSUMER_TARGET GGPO::GGPO)
elseif(GGPO_CONSUMER_LINKAGE STREQUAL "shared")
    find_package(GGPO CONFIG REQUIRED COMPONENTS shared)
    set(GGPO_CONSUMER_TARGET GGPO::shared)
elseif(GGPO_CONSUMER_LINKAGE STREQUAL "default")
    find_package(GGPO CONFIG REQUIRED)
    set(GGPO_CONSUMER_TARGET GGPO::GGPO)
else()
    message(FATAL_ERROR "Unknown GGPO_CONSUMER_LINKAGE '${GGPO_CONSUMER_LINKAGE}'")
endif()

# Resolve the library type actually linked and fail at configure time if the
# package selected the wrong flavor.
set(_ggpo_resolved ${GGPO_CONSUMER_TARGET})
get_target_property(_ggpo_type ${_ggpo_resolved} TYPE)
if(_ggpo_type STREQUAL "INTERFACE_LIBRARY")
    get_target_property(_ggpo_resolved ${_ggpo_resolved} INTERFACE_LINK_LIBRARIES)
    get_target_property(_ggpo_type ${_ggpo_resolved} TYPE)
endif()
if(_ggpo_type STREQUAL "SHARED_LIBRARY")
    set(GGPO_RESOLVED_LINKAGE shared)
elseif(_ggpo_type STREQUAL "STATIC_LIBRARY")
    set(GGPO_RESOLVED_LINKAGE static)
else()
    message(FATAL_ERROR "${GGPO_CONSUMER_TARGET} resolved to unexpected type ${_ggpo_type}")
endif()
message(STATUS "GGPO consumer linkage: ${GGPO_CONSUMER_LINKAGE} -> ${GGPO_RESOLVED_LINKAGE}")
if(GGPO_EXPECT_LINKAGE AND NOT GGPO_EXPECT_LINKAGE STREQUAL GGPO_RESOLVED_LINKAGE)
    message(FATAL_ERROR
        "Expected ${GGPO_EXPECT_LINKAGE} GGPO, but ${GGPO_CONSUMER_TARGET} resolved to ${GGPO_RESOLVED_LINKAGE}")
endif()

function(ggpo_add_installed_consumer target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ${GGPO_CONSUMER_TARGET})
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
