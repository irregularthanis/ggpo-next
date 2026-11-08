find_package(PythonInterp 3 REQUIRED)

set(GGPO_SMOKE_COMMON
    smoke/smoke_session.cpp
    smoke/smoke_session.h
)

function(ggpo_configure_test target)
    add_common_flags(${target})
    if(MSVC)
        target_compile_options(${target} PRIVATE "/W4")
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/src/lib/ggpo
        ${CMAKE_CURRENT_SOURCE_DIR}/smoke
    )
    if(BUILD_SHARED_LIBS)
        target_compile_definitions(${target} PRIVATE GGPO_SHARED_LIB)
    endif()
endfunction()

# Configure fixtures that compile GGPO sources directly into a static target.
function(ggpo_configure_static_test target)
    add_common_flags(${target})
    if(MSVC)
        target_compile_options(${target} PRIVATE "/W4")
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/src/lib/ggpo
        ${CMAKE_CURRENT_SOURCE_DIR}/smoke
    )
endfunction()

# Compile-time/public-data contracts; neither test opens a socket.
if(WIN32)
    add_executable(ggpo_windows_public_layout
        baseline/windows/public_layout.cpp
    )
    ggpo_configure_test(ggpo_windows_public_layout)
    add_test(NAME windows_public_layout COMMAND ggpo_windows_public_layout)

    add_executable(ggpo_windows_packet_layout
        baseline/windows/packet_layout.cpp
        platform/fake/platform_fake.cpp
        platform/fake/platform_fake.h
    )
    ggpo_configure_test(ggpo_windows_packet_layout)
    target_include_directories(ggpo_windows_packet_layout PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/fake
    )
    add_test(NAME windows_packet_layout COMMAND ggpo_windows_packet_layout)
    set(GGPO_SMOKE_PREFIX windows)
else()
    set(GGPO_SMOKE_PREFIX linux)
endif()

# Runtime contracts: SyncTest isolates rollback, while peer loopback exercises
# the public session API through two processes and real Windows UDP sockets.
set(GGPO_SYNCTEST_TARGET ggpo_${GGPO_SMOKE_PREFIX}_synctest_smoke)
add_executable(${GGPO_SYNCTEST_TARGET}
    smoke/synctest_smoke.cpp
    ${GGPO_SMOKE_COMMON}
)
ggpo_configure_test(${GGPO_SYNCTEST_TARGET})
target_link_libraries(${GGPO_SYNCTEST_TARGET} PRIVATE GGPO::GGPO)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/synctest)
add_test(NAME ${GGPO_SMOKE_PREFIX}_synctest_smoke COMMAND ${GGPO_SYNCTEST_TARGET})
set_tests_properties(${GGPO_SMOKE_PREFIX}_synctest_smoke PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/synctest
    TIMEOUT 60
)

set(GGPO_PEER_TARGET ggpo_${GGPO_SMOKE_PREFIX}_peer_smoke)
add_executable(${GGPO_PEER_TARGET}
    smoke/peer_smoke.cpp
    ${GGPO_SMOKE_COMMON}
)
ggpo_configure_test(${GGPO_PEER_TARGET})
target_link_libraries(${GGPO_PEER_TARGET} PRIVATE GGPO::GGPO)
add_test(NAME ${GGPO_SMOKE_PREFIX}_peer_loopback
    COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/smoke/run_peer_smoke.py
            $<TARGET_FILE:${GGPO_PEER_TARGET}>
            --frames 180
)
set_tests_properties(${GGPO_SMOKE_PREFIX}_peer_loopback PROPERTIES TIMEOUT 90)

# Negative contracts prove the runner rejects lifecycle and state failures.
add_test(NAME ${GGPO_SMOKE_PREFIX}_peer_negative_contracts
    COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/smoke/peer_runner_negative.py
            $<TARGET_FILE:${GGPO_PEER_TARGET}>
)
set_tests_properties(${GGPO_SMOKE_PREFIX}_peer_negative_contracts PROPERTIES
    TIMEOUT 45
)

# Build the production rollback core against a deterministic adapter. This is
# intentionally a separate static library: it proves SyncTest does not acquire
# Win32/Winsock behavior through an accidental transitive dependency.
include(${PROJECT_SOURCE_DIR}/src/CMakeSources.cmake)
set(GGPO_FAKE_CORE_RELATIVE_SOURCES ${GGPO_LIB_SRC})
list(REMOVE_ITEM GGPO_FAKE_CORE_RELATIVE_SOURCES
    "lib/ggpo/platform_windows.cpp"
    "lib/ggpo/platform_linux.cpp"
)
set(GGPO_FAKE_CORE_SOURCES)
foreach(source ${GGPO_FAKE_CORE_RELATIVE_SOURCES})
    list(APPEND GGPO_FAKE_CORE_SOURCES "${PROJECT_SOURCE_DIR}/src/${source}")
endforeach()

add_library(ggpo_fake_core STATIC
    ${GGPO_FAKE_CORE_SOURCES}
    platform/fake/platform_fake.cpp
    platform/fake/platform_fake.h
)
ggpo_configure_static_test(ggpo_fake_core)
if(MSVC)
    target_compile_options(ggpo_fake_core PRIVATE "/WX")
endif()
target_compile_definitions(ggpo_fake_core PRIVATE _CRT_SECURE_NO_WARNINGS)
target_include_directories(ggpo_fake_core PRIVATE
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src/lib/ggpo
)

add_executable(ggpo_fake_core_synctest
    smoke/synctest_smoke.cpp
    ${GGPO_SMOKE_COMMON}
)
ggpo_configure_static_test(ggpo_fake_core_synctest)
target_include_directories(ggpo_fake_core_synctest PRIVATE
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src/lib/ggpo
    ${CMAKE_CURRENT_SOURCE_DIR}/smoke
)
target_link_libraries(ggpo_fake_core_synctest PRIVATE ggpo_fake_core)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fake-synctest)
add_test(NAME fake_platform_synctest COMMAND ggpo_fake_core_synctest)
set_tests_properties(fake_platform_synctest PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fake-synctest
    TIMEOUT 60
)

add_executable(ggpo_fake_platform_contract
    platform/fake/platform_fake_test.cpp
)
ggpo_configure_static_test(ggpo_fake_platform_contract)
target_include_directories(ggpo_fake_platform_contract PRIVATE
    ${PROJECT_SOURCE_DIR}/src/lib/ggpo
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/fake
)
target_link_libraries(ggpo_fake_platform_contract PRIVATE ggpo_fake_core)
add_test(NAME fake_platform_contract COMMAND ggpo_fake_platform_contract)

# Focused portable-core contracts run against production algorithms and the
# deterministic fake adapter; no native clock, socket, or filesystem is used.
function(ggpo_add_portable_contract target source test_name)
    add_executable(${target} ${source})
    ggpo_configure_static_test(${target})
    target_link_libraries(${target} PRIVATE ggpo_fake_core)
    add_test(NAME ${test_name} COMMAND ${target})
    set_tests_properties(${test_name} PROPERTIES LABELS "portable;core")
endfunction()

ggpo_add_portable_contract(ggpo_input_queue_contract
    unit/input_queue_test.cpp input_queue_contract)
ggpo_add_portable_contract(ggpo_sync_contract
    unit/sync_contract_test.cpp sync_contract)
ggpo_add_portable_contract(ggpo_timesync_contract
    unit/timesync_test.cpp timesync_contract)
ggpo_add_portable_contract(ggpo_core_utilities_contract
    unit/core_utilities_test.cpp core_utilities_contract)
