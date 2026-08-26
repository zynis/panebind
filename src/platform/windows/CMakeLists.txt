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
    explorer/explorer_shell_inventory.cpp
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

if(BUILD_TESTING)
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
endif()
