enable_language(RC)

add_library(
    panebind_windows_observer
    STATIC
    text_encoding.cpp
    windows_observer.cpp
)
target_link_libraries(
    panebind_windows_observer
    PUBLIC panebind_core
    PRIVATE dwmapi user32
)
target_compile_definitions(
    panebind_windows_observer
    PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
panebind_enable_warnings(panebind_windows_observer)

add_executable(
    panebind-observer
    ../../app/observer_main.cpp
    observer.rc
    observer.manifest
)
set_source_files_properties(observer.rc PROPERTIES OBJECT_DEPENDS observer.manifest)
target_link_libraries(panebind-observer PRIVATE panebind_windows_observer)
if(MSVC)
    # observer.rc embeds the single audited PerMonitorV2 manifest. Suppress the
    # linker's generated second RT_MANIFEST resource.
    target_link_options(panebind-observer PRIVATE /MANIFEST:NO)
endif()
target_compile_definitions(
    panebind-observer
    PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
panebind_enable_warnings(panebind-observer)

add_library(
    panebind_windows_operation_geometry
    STATIC
    operations/window_translation.cpp
)
target_link_libraries(
    panebind_windows_operation_geometry
    PUBLIC panebind_core
)
panebind_enable_warnings(panebind_windows_operation_geometry)

# Shared one-gesture source writer. The owned probe supplies an exact owned
# capability today; a future Explorer adapter must supply its own authority.
add_library(panebind_windows_live_move_writer STATIC
    operations/live_move_writer.cpp
    operations/move_frame_continuity.cpp)
target_link_libraries(panebind_windows_live_move_writer PUBLIC panebind_core)
panebind_enable_warnings(panebind_windows_live_move_writer)

add_library(
    panebind_windows_owned_operations
    STATIC
    operations/owned_window_operations.cpp
)
target_link_libraries(
    panebind_windows_owned_operations
    PUBLIC panebind_core
    PRIVATE panebind_windows_operation_geometry dwmapi user32
)
target_compile_definitions(
    panebind_windows_owned_operations
    PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
panebind_enable_warnings(panebind_windows_owned_operations)

add_executable(
    panebind-owned-window-harness
    ../../app/owned_window_harness_main.cpp
    observer.rc
    observer.manifest
)
target_link_libraries(
    panebind-owned-window-harness
    PRIVATE panebind_windows_owned_operations dwmapi user32
)
if(MSVC)
    # Reuse the single audited PerMonitorV2 manifest resource already used by
    # the observer rather than maintaining a second drifting DPI manifest.
    target_link_options(panebind-owned-window-harness PRIVATE /MANIFEST:NO)
endif()
target_compile_definitions(
    panebind-owned-window-harness
    PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
panebind_enable_warnings(panebind-owned-window-harness)

add_library(
    panebind_windows_companion
    STATIC
    companion/companion_session.cpp
)
target_link_libraries(
    panebind_windows_companion
    PUBLIC panebind_core
    PRIVATE
    panebind_windows_operation_geometry
    advapi32
    bcrypt
    dwmapi
    user32
)
target_compile_definitions(
    panebind_windows_companion
    PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
panebind_enable_warnings(panebind_windows_companion)

add_executable(
    panebind-companion-target
    ../../app/companion_target_main.cpp
    observer.rc
    observer.manifest
)
target_link_libraries(panebind-companion-target PRIVATE panebind_core user32)
if(MSVC)
    target_link_options(panebind-companion-target PRIVATE /MANIFEST:NO)
endif()
target_compile_definitions(
    panebind-companion-target
    PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
panebind_enable_warnings(panebind-companion-target)

add_executable(
    panebind-companion-harness
    ../../app/companion_harness_main.cpp
    observer.rc
    observer.manifest
)
target_link_libraries(
    panebind-companion-harness
    PRIVATE panebind_windows_companion
)
if(MSVC)
    target_link_options(panebind-companion-harness PRIVATE /MANIFEST:NO)
endif()
target_compile_definitions(
    panebind-companion-harness
    PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
panebind_enable_warnings(panebind-companion-harness)

add_library(
    panebind_windows_explorer
    STATIC
    explorer/explorer_glue_event_source.cpp
    explorer/explorer_group_event_source.cpp
    explorer/explorer_group_session.cpp
    explorer/explorer_live_magnet_session.cpp
    explorer/explorer_glue_session.cpp
    explorer/explorer_shell_inventory.cpp
    explorer/explorer_shell_events.cpp
    explorer/explorer_session.cpp
)
target_link_libraries(
    panebind_windows_explorer
    PUBLIC panebind_core
    PRIVATE
    panebind_windows_operation_geometry
    advapi32
    bcrypt
    dwmapi
    ole32
    oleaut32
    propsys
    shell32
    shlwapi
    user32
)
target_compile_definitions(
    panebind_windows_explorer
    PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
panebind_enable_warnings(panebind_windows_explorer)

execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    OUTPUT_VARIABLE panebind_build_sha OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT panebind_build_sha)
    set(panebind_build_sha "unknown")
endif()
execute_process(COMMAND git rev-parse --git-path HEAD WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    OUTPUT_VARIABLE panebind_git_head OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git symbolic-ref -q HEAD WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    OUTPUT_VARIABLE panebind_git_ref OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
execute_process(COMMAND git rev-parse --git-path "${panebind_git_ref}" WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    OUTPUT_VARIABLE panebind_git_ref_path OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
foreach(ref_path IN ITEMS "${panebind_git_head}" "${panebind_git_ref_path}")
    if(ref_path)
        get_filename_component(ref_absolute "${ref_path}" ABSOLUTE BASE_DIR "${PROJECT_SOURCE_DIR}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ref_absolute}")
    endif()
endforeach()
add_executable(panebind-explorer-live-magnet-harness ../../app/explorer_live_magnet_harness_main.cpp text_encoding.cpp observer.rc observer.manifest)
target_link_libraries(panebind-explorer-live-magnet-harness PRIVATE panebind_windows_explorer panebind_windows_sta_console)
target_compile_definitions(panebind-explorer-live-magnet-harness PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE PANEBIND_BUILD_SHA="${panebind_build_sha}")
if(MSVC)
    target_link_options(panebind-explorer-live-magnet-harness PRIVATE /MANIFEST:NO)
endif()
panebind_enable_warnings(panebind-explorer-live-magnet-harness)

if(BUILD_TESTING)
    # Explicit read-only MVP1 input-attribution diagnostic. It never cancels
    # a native drag, creates an overlay, or places a third-party window.
    add_executable(panebind-explorer-anchor-diagnostic ../../app/explorer_anchor_diagnostic_main.cpp text_encoding.cpp observer.rc observer.manifest)
    target_link_libraries(panebind-explorer-anchor-diagnostic PRIVATE panebind_windows_explorer panebind_windows_sta_console ole32 dwmapi)
    target_compile_definitions(panebind-explorer-anchor-diagnostic PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00 PANEBIND_BUILD_SHA="${panebind_build_sha}")
    if(MSVC)
        target_link_options(panebind-explorer-anchor-diagnostic PRIVATE /MANIFEST:NO)
    endif()
    panebind_enable_warnings(panebind-explorer-anchor-diagnostic)
    # Explicit Pivot 1 interaction research; never part of default CTest.
    # Explicit read-only environment CLI, never automatic/default CTest.
    add_executable(panebind-test-input-environment operations/test_input_environment_probe.cpp observer.rc observer.manifest)
    target_link_libraries(panebind-test-input-environment PRIVATE panebind_core user32 wtsapi32 advapi32)
    target_compile_definitions(panebind-test-input-environment PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    if(MSVC)
        target_link_options(panebind-test-input-environment PRIVATE /MANIFEST:NO)
    endif()
    panebind_enable_warnings(panebind-test-input-environment)
    add_executable(panebind-magnet-takeover-probe operations/magnet_takeover_probe.cpp observer.rc observer.manifest)
    target_link_libraries(panebind-magnet-takeover-probe PRIVATE panebind_core dwmapi user32 wtsapi32)
    target_compile_definitions(panebind-magnet-takeover-probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    if(MSVC)
        target_link_options(panebind-magnet-takeover-probe PRIVATE /MANIFEST:NO)
    endif()
    panebind_enable_warnings(panebind-magnet-takeover-probe)
    # Explicit owned-only MVP1 overlay experiment; never registered in CTest.
    add_executable(panebind-owned-overlay-probe operations/owned_gesture_overlay_probe.cpp observer.rc observer.manifest)
    target_link_libraries(panebind-owned-overlay-probe PRIVATE panebind_windows_live_move_writer user32 gdi32 dwmapi wtsapi32 advapi32)
    target_compile_definitions(panebind-owned-overlay-probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    if(MSVC)
        target_link_options(panebind-owned-overlay-probe PRIVATE /MANIFEST:NO)
    endif()
    panebind_enable_warnings(panebind-owned-overlay-probe)
    add_executable(panebind-live-move-writer-tests operations/live_move_writer_tests.cpp)
    target_link_libraries(panebind-live-move-writer-tests PRIVATE panebind_windows_live_move_writer)
    panebind_enable_warnings(panebind-live-move-writer-tests)
    add_test(NAME windows-live-move-writer COMMAND panebind-live-move-writer-tests)
    add_executable(panebind-move-frame-continuity-tests operations/move_frame_continuity_tests.cpp)
    target_link_libraries(panebind-move-frame-continuity-tests PRIVATE panebind_windows_live_move_writer)
    panebind_enable_warnings(panebind-move-frame-continuity-tests)
    add_test(NAME windows-move-frame-continuity COMMAND panebind-move-frame-continuity-tests)
    add_executable(panebind-test-foreground-model-tests operations/test_foreground_bootstrap_model_tests.cpp)
    target_link_libraries(panebind-test-foreground-model-tests PRIVATE panebind_core)
    panebind_enable_warnings(panebind-test-foreground-model-tests)
    add_test(NAME test-foreground-bootstrap-model COMMAND panebind-test-foreground-model-tests)
    add_executable(panebind-magnet-auto-modal-probe operations/magnet_auto_modal_probe.cpp observer.rc observer.manifest)
    target_link_libraries(panebind-magnet-auto-modal-probe PRIVATE panebind_core dwmapi user32 wtsapi32)
    target_compile_definitions(panebind-magnet-auto-modal-probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    if(MSVC)
        target_link_options(panebind-magnet-auto-modal-probe PRIVATE /MANIFEST:NO)
    endif()
    panebind_enable_warnings(panebind-magnet-auto-modal-probe)
    # Never register SendInput integration with CTest.
    add_executable(panebind-magnet-postverify-tests operations/magnet_postverify_diagnostic_tests.cpp)
    target_link_libraries(panebind-magnet-postverify-tests PRIVATE panebind_core)
    panebind_enable_warnings(panebind-magnet-postverify-tests)
    add_test(NAME windows-magnet-postverify COMMAND panebind-magnet-postverify-tests)
    add_executable(panebind-handoff-preflight-tests operations/magnet_handoff_preflight_diagnostic_tests.cpp)
    target_link_libraries(panebind-handoff-preflight-tests PRIVATE panebind_core)
    panebind_enable_warnings(panebind-handoff-preflight-tests)
    add_test(NAME test-handoff-preflight-diagnostic COMMAND panebind-handoff-preflight-tests)
    add_executable(panebind-input-isolation-tests operations/magnet_input_isolation_diagnostic_tests.cpp)
    target_link_libraries(panebind-input-isolation-tests PRIVATE panebind_core)
    panebind_enable_warnings(panebind-input-isolation-tests)
    add_test(NAME test-product-input-isolation COMMAND panebind-input-isolation-tests)
    add_executable(panebind-owned-abort-diagnostic-tests operations/magnet_owned_abort_diagnostic_tests.cpp)
    target_link_libraries(panebind-owned-abort-diagnostic-tests PRIVATE panebind_core)
    panebind_enable_warnings(panebind-owned-abort-diagnostic-tests)
    add_test(NAME test-owned-native-abort-diagnostic COMMAND panebind-owned-abort-diagnostic-tests)
    set_tests_properties(test-owned-native-abort-diagnostic PROPERTIES LABELS "offline")
    # Explicit interaction probe, deliberately NOT a CTest/default test.
    add_executable(panebind-magnet-modal-loop-probe operations/magnet_modal_loop_probe.cpp observer.rc observer.manifest)
    target_link_libraries(panebind-magnet-modal-loop-probe PRIVATE panebind_core dwmapi user32)
    target_compile_definitions(panebind-magnet-modal-loop-probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(MSVC)
        target_link_options(panebind-magnet-modal-loop-probe PRIVATE /MANIFEST:NO)
    endif()
    panebind_enable_warnings(panebind-magnet-modal-loop-probe)
    add_executable(panebind-rect-adjustment-tests operations/window_rect_adjustment_tests.cpp)
    target_link_libraries(panebind-rect-adjustment-tests PRIVATE panebind_core)
    panebind_enable_warnings(panebind-rect-adjustment-tests)
    add_test(NAME windows-rect-adjustment COMMAND panebind-rect-adjustment-tests)
    add_executable(panebind-magnet-native-probe operations/magnet_native_probe.cpp observer.rc observer.manifest)
    target_link_libraries(panebind-magnet-native-probe PRIVATE panebind_core dwmapi user32)
    target_compile_definitions(panebind-magnet-native-probe PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(MSVC)
        target_link_options(panebind-magnet-native-probe PRIVATE /MANIFEST:NO)
    endif()
    panebind_enable_warnings(panebind-magnet-native-probe)
    add_test(NAME windows-magnet-owned-probe COMMAND panebind-magnet-native-probe)
    add_executable(panebind-browser-sink-tests
        explorer/explorer_browser_sink_tests.cpp explorer/explorer_shell_inventory.cpp)
    target_link_libraries(panebind-browser-sink-tests PRIVATE panebind_windows_explorer)
    target_compile_definitions(panebind-browser-sink-tests PRIVATE
        PANEBIND_BROWSER_SINK_TESTS NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
        WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    panebind_enable_warnings(panebind-browser-sink-tests)
    add_test(NAME explorer-browser-sink COMMAND panebind-browser-sink-tests)
    add_executable(panebind-group-capture-tests explorer/explorer_group_capture_tests.cpp)
    target_link_libraries(panebind-group-capture-tests PRIVATE panebind_windows_explorer)
    target_compile_definitions(panebind-group-capture-tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    panebind_enable_warnings(panebind-group-capture-tests)
    add_test(NAME explorer-group-mutable-capture COMMAND panebind-group-capture-tests)
endif()

add_executable(
    panebind-explorer-harness
    ../../app/explorer_harness_main.cpp
    observer.rc
    observer.manifest
)
set_source_files_properties(observer.rc PROPERTIES OBJECT_DEPENDS observer.manifest)
target_link_libraries(
    panebind-explorer-harness
    PRIVATE panebind_windows_explorer ole32
)
if(MSVC)
    # Reuse the audited PerMonitorV2 manifest and suppress the linker's second
    # RT_MANIFEST resource.
    target_link_options(panebind-explorer-harness PRIVATE /MANIFEST:NO)
endif()
target_compile_definitions(
    panebind-explorer-harness
    PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
panebind_enable_warnings(panebind-explorer-harness)

add_executable(
    panebind-explorer-glue-harness
    ../../app/explorer_glue_harness_main.cpp
    observer.rc
    observer.manifest
)
set_source_files_properties(observer.rc PROPERTIES OBJECT_DEPENDS observer.manifest)
target_link_libraries(
    panebind-explorer-glue-harness
    PRIVATE panebind_windows_explorer ole32
)
if(MSVC)
    target_link_options(panebind-explorer-glue-harness PRIVATE /MANIFEST:NO)
endif()
target_compile_definitions(
    panebind-explorer-glue-harness
    PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
panebind_enable_warnings(panebind-explorer-glue-harness)

add_executable(panebind-explorer-group-harness ../../app/explorer_group_harness_main.cpp text_encoding.cpp observer.rc observer.manifest)
add_library(panebind_windows_sta_console STATIC console/sta_console_line_reader.cpp)
target_link_libraries(panebind_windows_sta_console PUBLIC panebind_core PRIVATE user32)
panebind_enable_warnings(panebind_windows_sta_console)
target_link_libraries(panebind-explorer-group-harness PRIVATE panebind_windows_explorer panebind_windows_sta_console ole32)
target_compile_definitions(panebind-explorer-group-harness PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
if(MSVC)
    target_link_options(panebind-explorer-group-harness PRIVATE /MANIFEST:NO)
endif()
panebind_enable_warnings(panebind-explorer-group-harness)

# A fixed build profile reuses target provisioning and evidence serialization;
# the existing executable keeps its Console-confirmed R1-C2B behavior.
add_executable(
    panebind-explorer-ctrl-glue-harness
    ../../app/explorer_glue_harness_main.cpp
    observer.rc
    observer.manifest
)
target_link_libraries(panebind-explorer-ctrl-glue-harness
    PRIVATE panebind_windows_explorer ole32)
if(MSVC)
    target_link_options(panebind-explorer-ctrl-glue-harness PRIVATE /MANIFEST:NO)
endif()
target_compile_definitions(panebind-explorer-ctrl-glue-harness PRIVATE
    PANEBIND_EXPLORER_CTRL_GLUE_HARNESS
    NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
    WINVER=0x0A00 _WIN32_WINNT=0x0A00)
panebind_enable_warnings(panebind-explorer-ctrl-glue-harness)

add_executable(panebind-explorer-profile-glue-harness
    ../../app/explorer_glue_harness_main.cpp observer.rc observer.manifest)
target_link_libraries(panebind-explorer-profile-glue-harness
    PRIVATE panebind_windows_explorer ole32)
if(MSVC)
    target_link_options(panebind-explorer-profile-glue-harness PRIVATE /MANIFEST:NO)
endif()
target_compile_definitions(panebind-explorer-profile-glue-harness PRIVATE
    PANEBIND_EXPLORER_CTRL_GLUE_HARNESS PANEBIND_EXPLORER_PROFILE_GLUE_HARNESS
    NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
panebind_enable_warnings(panebind-explorer-profile-glue-harness)

add_executable(panebind-explorer-optimized-glue-harness
    ../../app/explorer_glue_harness_main.cpp observer.rc observer.manifest)
target_link_libraries(panebind-explorer-optimized-glue-harness PRIVATE panebind_windows_explorer ole32)
if(MSVC)
    target_link_options(panebind-explorer-optimized-glue-harness PRIVATE /MANIFEST:NO)
endif()
target_compile_definitions(panebind-explorer-optimized-glue-harness PRIVATE
    PANEBIND_EXPLORER_CTRL_GLUE_HARNESS PANEBIND_EXPLORER_PROFILE_GLUE_HARNESS
    PANEBIND_EXPLORER_CONSENT_BOUND_GLUE_HARNESS
    NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
panebind_enable_warnings(panebind-explorer-optimized-glue-harness)

add_executable(panebind-explorer-vdm-glue-harness
    ../../app/explorer_glue_harness_main.cpp observer.rc observer.manifest)
target_link_libraries(panebind-explorer-vdm-glue-harness PRIVATE panebind_windows_explorer ole32)
if(MSVC)
    target_link_options(panebind-explorer-vdm-glue-harness PRIVATE /MANIFEST:NO)
endif()
target_compile_definitions(panebind-explorer-vdm-glue-harness PRIVATE
    PANEBIND_EXPLORER_CTRL_GLUE_HARNESS PANEBIND_EXPLORER_PROFILE_GLUE_HARNESS
    PANEBIND_EXPLORER_CONSENT_BOUND_GLUE_HARNESS PANEBIND_EXPLORER_VDM_GLUE_HARNESS
    NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
panebind_enable_warnings(panebind-explorer-vdm-glue-harness)

if(BUILD_TESTING)
    add_executable(panebind-windows-explorer-glue-activation-tests
        explorer/explorer_glue_activation_tests.cpp)
    target_link_libraries(panebind-windows-explorer-glue-activation-tests
        PRIVATE panebind_core user32)
    target_compile_definitions(panebind-windows-explorer-glue-activation-tests PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
        WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    panebind_enable_warnings(panebind-windows-explorer-glue-activation-tests)
    add_test(NAME windows-explorer-glue-activation-unit
        COMMAND panebind-windows-explorer-glue-activation-tests)
    add_executable(panebind-windows-explorer-glue-profile-tests
        explorer/explorer_glue_profile_tests.cpp)
    target_link_libraries(panebind-windows-explorer-glue-profile-tests
        PRIVATE panebind_core user32)
    target_compile_definitions(panebind-windows-explorer-glue-profile-tests PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
        WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    panebind_enable_warnings(panebind-windows-explorer-glue-profile-tests)
    add_test(NAME windows-explorer-glue-profile-unit
        COMMAND panebind-windows-explorer-glue-profile-tests)
    add_executable(panebind-windows-explorer-vdm-tests explorer/explorer_virtual_desktop_manager_tests.cpp)
    target_link_libraries(panebind-windows-explorer-vdm-tests PRIVATE panebind_core ole32 user32 uuid)
    target_compile_definitions(panebind-windows-explorer-vdm-tests PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    panebind_enable_warnings(panebind-windows-explorer-vdm-tests)
    add_test(NAME windows-explorer-vdm-unit COMMAND panebind-windows-explorer-vdm-tests)
    add_executable(
        panebind-windows-text-encoding-tests
        text_encoding.cpp
        text_encoding_tests.cpp
    )
    target_link_libraries(
        panebind-windows-text-encoding-tests
        PRIVATE panebind_core
    )
    target_compile_definitions(
        panebind-windows-text-encoding-tests
        PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    panebind_enable_warnings(panebind-windows-text-encoding-tests)
    add_test(NAME windows-text-encoding COMMAND panebind-windows-text-encoding-tests)

    add_executable(
        panebind-windows-owned-operations-tests
        operations/owned_window_operations_tests.cpp
    )
    target_link_libraries(
        panebind-windows-owned-operations-tests
        PRIVATE panebind_windows_owned_operations
    )
    target_compile_definitions(
        panebind-windows-owned-operations-tests
        PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    panebind_enable_warnings(panebind-windows-owned-operations-tests)
    add_test(NAME windows-owned-operations-unit
             COMMAND panebind-windows-owned-operations-tests)

    add_executable(
        panebind-windows-companion-tests
        companion/companion_session_tests.cpp
    )
    target_link_libraries(
        panebind-windows-companion-tests
        PRIVATE panebind_windows_companion
    )
    target_compile_definitions(
        panebind-windows-companion-tests
        PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    panebind_enable_warnings(panebind-windows-companion-tests)
    add_test(NAME windows-companion-unit
             COMMAND panebind-windows-companion-tests)

    add_executable(
        panebind-windows-explorer-tests
        explorer/explorer_session_tests.cpp
    )
    target_link_libraries(
        panebind-windows-explorer-tests
        PRIVATE panebind_windows_explorer
    )
    target_compile_definitions(
        panebind-windows-explorer-tests
        PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    panebind_enable_warnings(panebind-windows-explorer-tests)
    add_test(NAME windows-explorer-unit
             COMMAND panebind-windows-explorer-tests)

    add_executable(
        panebind-windows-explorer-glue-event-source-tests
        explorer/explorer_glue_event_source.cpp
        explorer/explorer_glue_event_source_tests.cpp
    )
    target_link_libraries(
        panebind-windows-explorer-glue-event-source-tests
        PRIVATE panebind_core user32
    )
    target_compile_definitions(
        panebind-windows-explorer-glue-event-source-tests
        PRIVATE
        PANEBIND_EXPLORER_GLUE_EVENT_SOURCE_TESTING
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    panebind_enable_warnings(
        panebind-windows-explorer-glue-event-source-tests)
    add_test(NAME windows-explorer-glue-event-source-unit
             COMMAND panebind-windows-explorer-glue-event-source-tests)

    add_executable(
        panebind-windows-explorer-glue-session-tests
        explorer/explorer_glue_session_tests.cpp
    )
    target_link_libraries(
        panebind-windows-explorer-glue-session-tests
        PRIVATE panebind_windows_explorer
    )
    target_compile_definitions(
        panebind-windows-explorer-glue-session-tests
        PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    panebind_enable_warnings(
        panebind-windows-explorer-glue-session-tests)
    add_test(NAME windows-explorer-glue-session-unit
             COMMAND panebind-windows-explorer-glue-session-tests)
endif()
if(BUILD_TESTING)
    add_executable(panebind-explorer-group-batch-tests explorer/explorer_group_batch_tests.cpp)
    target_link_libraries(panebind-explorer-group-batch-tests PRIVATE panebind_core user32)
    target_compile_definitions(panebind-explorer-group-batch-tests PRIVATE PANEBIND_EXPLORER_GROUP_TESTING)
    panebind_enable_warnings(panebind-explorer-group-batch-tests)
    add_test(NAME explorer-group-batch COMMAND panebind-explorer-group-batch-tests)
    add_executable(panebind-explorer-group-event-tests explorer/explorer_group_event_tests.cpp explorer/explorer_group_event_source.cpp)
    target_link_libraries(panebind-explorer-group-event-tests PRIVATE panebind_core user32)
    target_compile_definitions(panebind-explorer-group-event-tests PRIVATE PANEBIND_EXPLORER_GROUP_TESTING)
    panebind_enable_warnings(panebind-explorer-group-event-tests)
    add_test(NAME explorer-group-event COMMAND panebind-explorer-group-event-tests)
    add_executable(panebind-explorer-group-layout-tests explorer/explorer_group_layout_tests.cpp)
    target_link_libraries(panebind-explorer-group-layout-tests PRIVATE panebind_windows_explorer)
    panebind_enable_warnings(panebind-explorer-group-layout-tests)
    add_test(NAME explorer-group-layout COMMAND panebind-explorer-group-layout-tests)
    add_executable(panebind-explorer-group-readiness-tests explorer/explorer_group_readiness_tests.cpp)
    target_link_libraries(panebind-explorer-group-readiness-tests PRIVATE panebind_windows_explorer)
    panebind_enable_warnings(panebind-explorer-group-readiness-tests)
    add_test(NAME explorer-group-readiness COMMAND panebind-explorer-group-readiness-tests)
    add_executable(panebind-sta-console-tests console/sta_console_line_reader_tests.cpp)
    target_link_libraries(panebind-sta-console-tests PRIVATE panebind_windows_sta_console ole32 user32)
    panebind_enable_warnings(panebind-sta-console-tests)
    add_test(NAME sta-console-pump COMMAND panebind-sta-console-tests)
    set_tests_properties(sta-console-pump PROPERTIES TIMEOUT 20)
    # Fix F classification is based on the actual command/call path, not "unit"
    # naming. Fake COM objects, ledgers, snapshots and CPU-only clocks are safe;
    # even hidden/message-only HWNDs and private-console GUI children are not.
    set_tests_properties(
        test-foreground-bootstrap-model windows-magnet-postverify
        test-handoff-preflight-diagnostic test-product-input-isolation
        windows-rect-adjustment windows-live-move-writer windows-move-frame-continuity explorer-browser-sink
        explorer-group-mutable-capture windows-explorer-glue-profile-unit
        windows-text-encoding windows-owned-operations-unit
        windows-companion-unit windows-explorer-unit
        windows-explorer-glue-session-unit explorer-group-layout
        explorer-group-readiness
        PROPERTIES LABELS "offline")
    set_tests_properties(
        windows-magnet-owned-probe windows-explorer-glue-activation-unit
        windows-explorer-vdm-unit windows-explorer-glue-event-source-unit
        explorer-group-batch explorer-group-event sta-console-pump
        PROPERTIES LABELS "interactive")
endif()
