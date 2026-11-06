if(NOT WIN32)
    message(STATUS "M0 baseline tests are Windows-only")
    return()
endif()

find_package(PythonInterp 3 REQUIRED)

set(GGPO_M0_SMOKE_COMMON
    smoke/smoke_session.cpp
    smoke/smoke_session.h
)

function(ggpo_configure_m0_test target)
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

add_executable(ggpo_m0_public_layout
    baseline/windows/public_layout.cpp
)
ggpo_configure_m0_test(ggpo_m0_public_layout)
add_test(NAME m0_public_layout COMMAND ggpo_m0_public_layout)

add_executable(ggpo_m0_packet_layout
    baseline/windows/packet_layout.cpp
)
ggpo_configure_m0_test(ggpo_m0_packet_layout)
add_test(NAME m0_packet_layout COMMAND ggpo_m0_packet_layout)

add_executable(ggpo_m0_synctest_smoke
    smoke/synctest_smoke.cpp
    ${GGPO_M0_SMOKE_COMMON}
)
ggpo_configure_m0_test(ggpo_m0_synctest_smoke)
target_link_libraries(ggpo_m0_synctest_smoke PRIVATE GGPO winmm.lib ws2_32.lib)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/synctest)
add_test(NAME m0_synctest_smoke COMMAND ggpo_m0_synctest_smoke)
set_tests_properties(m0_synctest_smoke PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/synctest
    TIMEOUT 60
)

add_executable(ggpo_m0_peer_smoke
    smoke/peer_smoke.cpp
    ${GGPO_M0_SMOKE_COMMON}
)
ggpo_configure_m0_test(ggpo_m0_peer_smoke)
target_link_libraries(ggpo_m0_peer_smoke PRIVATE GGPO winmm.lib ws2_32.lib)
add_test(NAME m0_peer_smoke
    COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/smoke/run_peer_smoke.py
            $<TARGET_FILE:ggpo_m0_peer_smoke>
            --frames 180
)
set_tests_properties(m0_peer_smoke PROPERTIES TIMEOUT 90)
