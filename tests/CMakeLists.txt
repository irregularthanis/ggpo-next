if(NOT WIN32)
    message(STATUS "Windows compatibility baseline tests are Windows-only")
    return()
endif()

find_package(PythonInterp 3 REQUIRED)

set(GGPO_WINDOWS_BASELINE_COMMON
    smoke/smoke_session.cpp
    smoke/smoke_session.h
)

function(ggpo_configure_windows_baseline_test target)
    add_common_flags(${target})
    target_compile_options(${target} PRIVATE "/W4")
    target_include_directories(${target} PRIVATE
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/src/lib/ggpo
        ${CMAKE_CURRENT_SOURCE_DIR}/smoke
    )
    if(BUILD_SHARED_LIBS)
        target_compile_definitions(${target} PRIVATE GGPO_SHARED_LIB)
    endif()
endfunction()

# Compile-time/public-data contracts; neither test opens a socket.
add_executable(ggpo_windows_public_layout
    baseline/windows/public_layout.cpp
)
ggpo_configure_windows_baseline_test(ggpo_windows_public_layout)
add_test(NAME windows_public_layout COMMAND ggpo_windows_public_layout)

add_executable(ggpo_windows_packet_layout
    baseline/windows/packet_layout.cpp
)
ggpo_configure_windows_baseline_test(ggpo_windows_packet_layout)
target_link_libraries(ggpo_windows_packet_layout PRIVATE GGPO winmm.lib ws2_32.lib)
add_test(NAME windows_packet_layout COMMAND ggpo_windows_packet_layout)

# Runtime contracts: SyncTest isolates rollback, while peer loopback exercises
# the public session API through two processes and real Windows UDP sockets.
add_executable(ggpo_windows_synctest_smoke
    smoke/synctest_smoke.cpp
    ${GGPO_WINDOWS_BASELINE_COMMON}
)
ggpo_configure_windows_baseline_test(ggpo_windows_synctest_smoke)
target_link_libraries(ggpo_windows_synctest_smoke PRIVATE GGPO winmm.lib ws2_32.lib)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/synctest)
add_test(NAME windows_synctest_smoke COMMAND ggpo_windows_synctest_smoke)
set_tests_properties(windows_synctest_smoke PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/synctest
    TIMEOUT 60
)

add_executable(ggpo_windows_peer_smoke
    smoke/peer_smoke.cpp
    ${GGPO_WINDOWS_BASELINE_COMMON}
)
ggpo_configure_windows_baseline_test(ggpo_windows_peer_smoke)
target_link_libraries(ggpo_windows_peer_smoke PRIVATE GGPO winmm.lib ws2_32.lib)
add_test(NAME windows_peer_loopback
    COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/smoke/run_peer_smoke.py
            $<TARGET_FILE:ggpo_windows_peer_smoke>
            --frames 180
)
set_tests_properties(windows_peer_loopback PROPERTIES TIMEOUT 90)

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
add_common_flags(ggpo_fake_core)
target_compile_options(ggpo_fake_core PRIVATE "/W4" "/WX")
target_compile_definitions(ggpo_fake_core PRIVATE _CRT_SECURE_NO_WARNINGS)
target_include_directories(ggpo_fake_core PRIVATE
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src/lib/ggpo
)

add_executable(ggpo_fake_core_synctest
    smoke/synctest_smoke.cpp
    ${GGPO_WINDOWS_BASELINE_COMMON}
)
add_common_flags(ggpo_fake_core_synctest)
target_compile_options(ggpo_fake_core_synctest PRIVATE "/W4")
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
add_common_flags(ggpo_fake_platform_contract)
target_compile_options(ggpo_fake_platform_contract PRIVATE "/W4")
target_include_directories(ggpo_fake_platform_contract PRIVATE
    ${PROJECT_SOURCE_DIR}/src/lib/ggpo
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/fake
)
target_link_libraries(ggpo_fake_platform_contract PRIVATE ggpo_fake_core)
add_test(NAME fake_platform_contract COMMAND ggpo_fake_platform_contract)
