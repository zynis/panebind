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
endif()
