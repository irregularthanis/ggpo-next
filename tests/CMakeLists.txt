find_package(PythonInterp 3 REQUIRED)

# Validate the platform-neutral result schema and its independent state oracle.
add_test(NAME cross_host_result_contract
    COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/e2e/test_result_schema.py
)

# Reject incomplete matrices and unauthenticated or unsafe control requests.
add_test(NAME cross_host_orchestration_contract
    COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/e2e/test_orchestration.py
)

# Vector War matchmaking server: protocol vectors, pairing rules, loopback UDP.
add_test(NAME matchmaking_server_contract
    COMMAND ${PYTHON_EXECUTABLE} -m unittest discover -s tests -t .
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/services/matchmaking
)
set_tests_properties(matchmaking_server_contract PROPERTIES LABELS "portable;matchmaking")

# Prove the run-scoped Deployment record fails closed on stale/duplicate data.
add_test(NAME cross_host_rendezvous_contract
    COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/e2e/test_rendezvous.py
)

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
    # GGPO_SHARED_LIB (Windows dllimport) arrives through GGPO::shared's
    # interface, so tests linked to GGPO::static never receive it.
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
    set_tests_properties(windows_public_layout PROPERTIES LABELS "native;abi")

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
    set_tests_properties(windows_packet_layout PROPERTIES LABELS "native;abi;protocol")
    set(GGPO_SMOKE_PREFIX windows)
else()
    # POSIX platforms (Linux, Android) name native tests by platform.
    if(ANDROID)
        set(GGPO_SMOKE_PREFIX android)
    else()
        set(GGPO_SMOKE_PREFIX linux)
    endif()
    # Linux and Android counterparts reuse the same frozen assertions: the public layout
    # matches on LP64, and the packed wire layout must be byte-identical for
    # Linux<->Windows peers to interoperate.
    add_executable(ggpo_${GGPO_SMOKE_PREFIX}_public_layout
        baseline/windows/public_layout.cpp
    )
    ggpo_configure_test(ggpo_${GGPO_SMOKE_PREFIX}_public_layout)
    add_test(NAME ${GGPO_SMOKE_PREFIX}_public_layout COMMAND ggpo_${GGPO_SMOKE_PREFIX}_public_layout)
    set_tests_properties(${GGPO_SMOKE_PREFIX}_public_layout PROPERTIES LABELS "native;abi")

    add_executable(ggpo_${GGPO_SMOKE_PREFIX}_packet_layout
        baseline/windows/packet_layout.cpp
        platform/fake/platform_fake.cpp
        platform/fake/platform_fake.h
    )
    ggpo_configure_test(ggpo_${GGPO_SMOKE_PREFIX}_packet_layout)
    target_include_directories(ggpo_${GGPO_SMOKE_PREFIX}_packet_layout PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/platform/fake
    )
    add_test(NAME ${GGPO_SMOKE_PREFIX}_packet_layout COMMAND ggpo_${GGPO_SMOKE_PREFIX}_packet_layout)
    set_tests_properties(${GGPO_SMOKE_PREFIX}_packet_layout PROPERTIES LABELS "native;abi;protocol")

    # Shared builds freeze the dynamic ggpo_* export set like the DLL manifest.
    if(TARGET GGPO_shared)
        add_test(NAME ${GGPO_SMOKE_PREFIX}_exports
            COMMAND ${PYTHON_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/baseline/linux/check_exports.py
                    $<TARGET_FILE:GGPO_shared>
                    ${CMAKE_CURRENT_SOURCE_DIR}/baseline/linux/exports.txt
        )
        set_tests_properties(${GGPO_SMOKE_PREFIX}_exports PROPERTIES LABELS "native;abi")
    endif()
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
    LABELS "native;core"
)

set(GGPO_PEER_TARGET ggpo_${GGPO_SMOKE_PREFIX}_peer_smoke)
add_executable(${GGPO_PEER_TARGET}
    smoke/peer_smoke.cpp
    ${GGPO_SMOKE_COMMON}
)
ggpo_configure_test(${GGPO_PEER_TARGET})
target_link_libraries(${GGPO_PEER_TARGET} PRIVATE GGPO::GGPO)
set(GGPO_PEER_PROCESS_TIMEOUT 45)
set(GGPO_PEER_CTEST_TIMEOUT 90)
if(ANDROID)
    # API 21's emulator can take substantially longer to launch two adb-backed
    # peers than current Android releases. Keep the execution bounded while
    # giving the same network contract enough time to complete.
    set(GGPO_PEER_PROCESS_TIMEOUT 120)
    set(GGPO_PEER_CTEST_TIMEOUT 180)
endif()
add_test(NAME ${GGPO_SMOKE_PREFIX}_peer_loopback
    COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/smoke/run_peer_smoke.py
            $<TARGET_FILE:${GGPO_PEER_TARGET}>
            --frames 180
            --timeout ${GGPO_PEER_PROCESS_TIMEOUT}
)
set_tests_properties(${GGPO_SMOKE_PREFIX}_peer_loopback PROPERTIES
    TIMEOUT ${GGPO_PEER_CTEST_TIMEOUT}
    LABELS "native;network"
)

# Exercise the portable result schema and two-peer completion barrier locally.
add_test(NAME ${GGPO_SMOKE_PREFIX}_peer_schema2_loopback
    COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/e2e/run_schema2_loopback.py
            $<TARGET_FILE:${GGPO_PEER_TARGET}>
            --frames 180
)
set_tests_properties(${GGPO_SMOKE_PREFIX}_peer_schema2_loopback PROPERTIES
    TIMEOUT 90
)

# Negative contracts prove the runner rejects lifecycle and state failures.
add_test(NAME ${GGPO_SMOKE_PREFIX}_peer_negative_contracts
    COMMAND ${PYTHON_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/smoke/peer_runner_negative.py
            $<TARGET_FILE:${GGPO_PEER_TARGET}>
)
set_tests_properties(${GGPO_SMOKE_PREFIX}_peer_negative_contracts PROPERTIES
    TIMEOUT 45
    LABELS "native;negative"
)

# When both flavors are built, GGPO::GGPO is the shared library.  Run the
# runtime smokes against the static library too so each lane covers both.
if(TARGET GGPO_shared AND TARGET GGPO_static)
    set(GGPO_STATIC_SYNCTEST_TARGET ggpo_${GGPO_SMOKE_PREFIX}_static_synctest_smoke)
    add_executable(${GGPO_STATIC_SYNCTEST_TARGET}
        smoke/synctest_smoke.cpp
        ${GGPO_SMOKE_COMMON}
    )
    ggpo_configure_test(${GGPO_STATIC_SYNCTEST_TARGET})
    target_link_libraries(${GGPO_STATIC_SYNCTEST_TARGET} PRIVATE GGPO::static)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/static-synctest)
    add_test(NAME ${GGPO_SMOKE_PREFIX}_static_synctest_smoke
        COMMAND ${GGPO_STATIC_SYNCTEST_TARGET})
    set_tests_properties(${GGPO_SMOKE_PREFIX}_static_synctest_smoke PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/static-synctest
        TIMEOUT 60
        LABELS "native;core;static"
    )

    set(GGPO_STATIC_PEER_TARGET ggpo_${GGPO_SMOKE_PREFIX}_static_peer_smoke)
    add_executable(${GGPO_STATIC_PEER_TARGET}
        smoke/peer_smoke.cpp
        ${GGPO_SMOKE_COMMON}
    )
    ggpo_configure_test(${GGPO_STATIC_PEER_TARGET})
    target_link_libraries(${GGPO_STATIC_PEER_TARGET} PRIVATE GGPO::static)
    add_test(NAME ${GGPO_SMOKE_PREFIX}_static_peer_loopback
        COMMAND ${PYTHON_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/smoke/run_peer_smoke.py
                $<TARGET_FILE:${GGPO_STATIC_PEER_TARGET}>
                --frames 180
                --timeout ${GGPO_PEER_PROCESS_TIMEOUT}
    )
    set_tests_properties(${GGPO_SMOKE_PREFIX}_static_peer_loopback PROPERTIES
        TIMEOUT ${GGPO_PEER_CTEST_TIMEOUT}
        LABELS "native;network;static"
    )
    set(GGPO_STATIC_RUNTIME_TEST_TARGETS
        ${GGPO_STATIC_SYNCTEST_TARGET} ${GGPO_STATIC_PEER_TARGET})
endif()

# Build the production rollback core against a deterministic adapter. This is
# intentionally a separate static library: it proves SyncTest does not acquire
# Win32/Winsock behavior through an accidental transitive dependency.
include(${PROJECT_SOURCE_DIR}/src/CMakeSources.cmake)
set(GGPO_FAKE_CORE_RELATIVE_SOURCES ${GGPO_LIB_SRC})
list(REMOVE_ITEM GGPO_FAKE_CORE_RELATIVE_SOURCES
    "lib/ggpo/platform_windows.cpp"
    "lib/ggpo/platform_posix.cpp"
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
    LABELS "portable;core"
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
set_tests_properties(fake_platform_contract PROPERTIES LABELS "portable;platform")

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
ggpo_add_portable_contract(ggpo_udp_send_contract
    unit/udp_send_test.cpp udp_send_contract)
target_include_directories(ggpo_udp_send_contract PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/fake)
# ASSERT exits with status 0, so require the explicit pass line as well.
set_tests_properties(udp_send_contract PROPERTIES
    PASS_REGULAR_EXPRESSION "udp send contract: pass")
ggpo_add_portable_contract(ggpo_protocol_contract
    unit/protocol_test.cpp protocol_contract)
set_property(TEST protocol_contract APPEND PROPERTY LABELS protocol)

# Public wrappers run against production backends and the fake platform so
# lifecycle/error contracts remain deterministic and socket-free.
add_executable(ggpo_public_api_contract
    contract/public_api_contract_test.cpp
)
ggpo_configure_static_test(ggpo_public_api_contract)
target_link_libraries(ggpo_public_api_contract PRIVATE ggpo_fake_core)
add_test(NAME public_api_contract COMMAND ggpo_public_api_contract)
set_tests_properties(public_api_contract PROPERTIES LABELS "portable;api")

# Compile one assertion source against both the fake and selected native
# adapters so platform drift cannot hide behind OS-specific test logic.
add_executable(ggpo_fake_platform_conformance
    platform/conformance/platform_conformance_test.cpp)
ggpo_configure_static_test(ggpo_fake_platform_conformance)
target_link_libraries(ggpo_fake_platform_conformance PRIVATE ggpo_fake_core)
add_test(NAME fake_platform_conformance COMMAND ggpo_fake_platform_conformance)
set_tests_properties(fake_platform_conformance PROPERTIES LABELS "portable;platform")

add_executable(ggpo_native_platform_conformance
    platform/conformance/platform_conformance_test.cpp)
ggpo_configure_test(ggpo_native_platform_conformance)
# Exercise the selected adapter directly.  Platform helpers are internal
# implementation details and therefore are not exported by the Windows DLL.
ggpo_configure_platform(ggpo_native_platform_conformance)
add_test(NAME ${GGPO_SMOKE_PREFIX}_platform_conformance
    COMMAND ggpo_native_platform_conformance)
set_tests_properties(${GGPO_SMOKE_PREFIX}_platform_conformance
    PROPERTIES LABELS "native;platform")

# Exercise the sample's real deterministic simulation without a window. This
# freezes the state path before the interactive host is made portable.
add_executable(ggpo_vectorwar_state_contract
    vectorwar/gamestate_contract_test.cpp
    ${PROJECT_SOURCE_DIR}/src/apps/vectorwar/gamestate.cpp
    ${PROJECT_SOURCE_DIR}/src/apps/vectorwar/state_contract.cpp
)
ggpo_configure_static_test(ggpo_vectorwar_state_contract)
target_include_directories(ggpo_vectorwar_state_contract PRIVATE
    ${PROJECT_SOURCE_DIR}/src/apps/vectorwar
)
target_link_libraries(ggpo_vectorwar_state_contract PRIVATE ggpo_fake_core)
add_test(NAME vectorwar_state_contract COMMAND ggpo_vectorwar_state_contract)
set_tests_properties(vectorwar_state_contract PROPERTIES
    LABELS "portable;sample;vectorwar-gameplay")

# Shared gameplay layer: exact cardinal initialization must be bit-stable on
# every supported compiler before networking or platform services participate.
add_executable(ggpo_vectorwar_gameplay_contract
    vectorwar/gameplay_determinism_test.cpp
    ${PROJECT_SOURCE_DIR}/src/apps/vectorwar/gamestate.cpp
    ${PROJECT_SOURCE_DIR}/src/apps/vectorwar/state_contract.cpp
)
ggpo_configure_static_test(ggpo_vectorwar_gameplay_contract)
target_include_directories(ggpo_vectorwar_gameplay_contract PRIVATE
    ${PROJECT_SOURCE_DIR}/src/apps/vectorwar
)
target_link_libraries(ggpo_vectorwar_gameplay_contract PRIVATE ggpo_fake_core)
add_test(NAME vectorwar_gameplay_contract COMMAND ggpo_vectorwar_gameplay_contract)
set_tests_properties(vectorwar_gameplay_contract PROPERTIES
    LABELS "portable;sample;vectorwar-gameplay")

# Native-host layer: compile and execute each platform's clock/sleep branch and
# freeze the headless adapter's bounds, input, renderer and diagnostics contract.
add_executable(ggpo_vectorwar_native_host_contract
    vectorwar/native_host_contract_test.cpp
    ${PROJECT_SOURCE_DIR}/src/apps/vectorwar/headless_host.cpp
)
ggpo_configure_test(ggpo_vectorwar_native_host_contract)
target_include_directories(ggpo_vectorwar_native_host_contract PRIVATE
    ${PROJECT_SOURCE_DIR}/src/apps/vectorwar
)
target_link_libraries(ggpo_vectorwar_native_host_contract PRIVATE GGPO::GGPO)
add_test(NAME ${GGPO_SMOKE_PREFIX}_vectorwar_native_host_contract
    COMMAND ggpo_vectorwar_native_host_contract)
set_tests_properties(${GGPO_SMOKE_PREFIX}_vectorwar_native_host_contract PROPERTIES
    LABELS "native;platform;sample;vectorwar-host")

if(MSVC)
    # Built from Vector War's Win32/GDI frontend, which MinGW does not build.
    add_executable(ggpo_vectorwar_win32_host_contract
        vectorwar/win32_host_contract_test.cpp
        ${PROJECT_SOURCE_DIR}/src/apps/vectorwar/win32_host.cpp
        ${PROJECT_SOURCE_DIR}/src/apps/vectorwar/gdi_renderer.cpp
        ${PROJECT_SOURCE_DIR}/src/apps/vectorwar/ggpo_perfmon.cpp
    )
    ggpo_configure_test(ggpo_vectorwar_win32_host_contract)
    target_compile_definitions(ggpo_vectorwar_win32_host_contract PRIVATE
        _UNICODE UNICODE)
    target_include_directories(ggpo_vectorwar_win32_host_contract PRIVATE
        ${PROJECT_SOURCE_DIR}/src/apps/vectorwar
    )
    target_link_libraries(ggpo_vectorwar_win32_host_contract PRIVATE GGPO::GGPO)
    add_test(NAME windows_vectorwar_gui_host_contract
        COMMAND ggpo_vectorwar_win32_host_contract)
    set_tests_properties(windows_vectorwar_gui_host_contract PROPERTIES
        LABELS "native;platform;sample;vectorwar-host")
elseif(TARGET VectorWar)
    # find_package targets are directory-scoped; resolve SDL2 here as well.
    find_package(SDL2 REQUIRED)
    add_executable(ggpo_vectorwar_sdl_host_contract
        vectorwar/sdl_host_contract_test.cpp
        ${PROJECT_SOURCE_DIR}/src/apps/vectorwar/sdl_host.cpp
        ${PROJECT_SOURCE_DIR}/src/apps/vectorwar/sdl_renderer.cpp
    )
    ggpo_configure_test(ggpo_vectorwar_sdl_host_contract)
    target_include_directories(ggpo_vectorwar_sdl_host_contract PRIVATE
        ${PROJECT_SOURCE_DIR}/src/apps/vectorwar
    )
    target_link_libraries(ggpo_vectorwar_sdl_host_contract PRIVATE
        GGPO::GGPO SDL2::SDL2)
    add_test(NAME linux_vectorwar_gui_host_contract
        COMMAND ggpo_vectorwar_sdl_host_contract)
    set_tests_properties(linux_vectorwar_gui_host_contract PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy"
        LABELS "native;platform;sample;vectorwar-host")
elseif(TARGET VectorWarAndroid)
    # The APK's SDL2 frontend, built when GGPO_ANDROID_SDL2_SOURCE_DIR supplies
    # SDL2. The test selects the dummy video driver itself; the launcher
    # pushes libSDL2.so from its build directory.
    add_executable(ggpo_vectorwar_sdl_host_contract
        vectorwar/sdl_host_contract_test.cpp
        ${PROJECT_SOURCE_DIR}/src/apps/vectorwar/sdl_host.cpp
        ${PROJECT_SOURCE_DIR}/src/apps/vectorwar/sdl_renderer.cpp
    )
    ggpo_configure_test(ggpo_vectorwar_sdl_host_contract)
    target_include_directories(ggpo_vectorwar_sdl_host_contract PRIVATE
        ${PROJECT_SOURCE_DIR}/src/apps/vectorwar
    )
    target_link_libraries(ggpo_vectorwar_sdl_host_contract PRIVATE
        GGPO::GGPO SDL2::SDL2 log android)
    add_test(NAME android_vectorwar_gui_host_contract
        COMMAND ggpo_vectorwar_sdl_host_contract)
    set_tests_properties(android_vectorwar_gui_host_contract PROPERTIES
        ENVIRONMENT "GGPO_LAUNCHER_LIB_DIRS=$<TARGET_FILE_DIR:SDL2>"
        LABELS "native;platform;sample;vectorwar-host")
endif()

# Launch two instances of the real Vector War application core over native
# loopback sockets and compare their final deterministic game state.
if(TARGET VectorWarHeadless)
    add_test(NAME vectorwar_headless_loopback
        COMMAND ${PYTHON_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/vectorwar/run_headless_loopback.py
                $<TARGET_FILE:VectorWarHeadless>
                --timeout ${GGPO_PEER_PROCESS_TIMEOUT}
    )
    set_tests_properties(vectorwar_headless_loopback PROPERTIES
        TIMEOUT ${GGPO_PEER_CTEST_TIMEOUT}
        LABELS "native;network;sample"
    )
    add_test(NAME vectorwar_headless_negative_contracts
        COMMAND ${PYTHON_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/vectorwar/headless_negative.py
                $<TARGET_FILE:VectorWarHeadless>
    )
    set_tests_properties(vectorwar_headless_negative_contracts PROPERTIES
        TIMEOUT 30
        LABELS "native;negative;sample"
    )
endif()

# Export every portable contract target so the root test-on-build target builds
# the executables before invoking CTest on multi-config generators such as MSVC.
set(GGPO_PORTABLE_TEST_TARGETS
	ggpo_vectorwar_state_contract
    ggpo_vectorwar_gameplay_contract
    ggpo_vectorwar_native_host_contract
    ggpo_input_queue_contract
    ggpo_sync_contract
    ggpo_timesync_contract
    ggpo_core_utilities_contract
    ggpo_protocol_contract
    ggpo_udp_send_contract
    ggpo_public_api_contract
    ggpo_fake_platform_conformance
    ggpo_native_platform_conformance
)
if(TARGET VectorWarHeadless)
    list(APPEND GGPO_PORTABLE_TEST_TARGETS VectorWarHeadless)
endif()
if(TARGET ggpo_vectorwar_win32_host_contract)
    list(APPEND GGPO_PORTABLE_TEST_TARGETS ggpo_vectorwar_win32_host_contract)
endif()
if(GGPO_STATIC_RUNTIME_TEST_TARGETS)
    list(APPEND GGPO_PORTABLE_TEST_TARGETS ${GGPO_STATIC_RUNTIME_TEST_TARGETS})
endif()
if(TARGET ggpo_vectorwar_sdl_host_contract)
    list(APPEND GGPO_PORTABLE_TEST_TARGETS ggpo_vectorwar_sdl_host_contract)
endif()
set(GGPO_PORTABLE_TEST_TARGETS ${GGPO_PORTABLE_TEST_TARGETS} PARENT_SCOPE)
