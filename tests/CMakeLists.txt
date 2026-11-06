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
