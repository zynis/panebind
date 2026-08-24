enable_language(RC)

add_library(
    snapweave_windows_observer
    STATIC
    text_encoding.cpp
    windows_observer.cpp
)
target_link_libraries(
    snapweave_windows_observer
    PUBLIC snapweave_core
    PRIVATE dwmapi user32
)
target_compile_definitions(
    snapweave_windows_observer
    PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
snapweave_enable_warnings(snapweave_windows_observer)

add_executable(
    snapweave-observer
    ../../app/observer_main.cpp
    observer.rc
    observer.manifest
)
set_source_files_properties(observer.rc PROPERTIES OBJECT_DEPENDS observer.manifest)
target_link_libraries(snapweave-observer PRIVATE snapweave_windows_observer)
if(MSVC)
    # observer.rc embeds the single audited PerMonitorV2 manifest. Suppress the
    # linker's generated second RT_MANIFEST resource.
    target_link_options(snapweave-observer PRIVATE /MANIFEST:NO)
endif()
target_compile_definitions(
    snapweave-observer
    PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)
snapweave_enable_warnings(snapweave-observer)

if(BUILD_TESTING)
    add_executable(
        snapweave-windows-text-encoding-tests
        text_encoding.cpp
        text_encoding_tests.cpp
    )
    target_link_libraries(
        snapweave-windows-text-encoding-tests
        PRIVATE snapweave_core
    )
    target_compile_definitions(
        snapweave-windows-text-encoding-tests
        PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
    )
    snapweave_enable_warnings(snapweave-windows-text-encoding-tests)
    add_test(NAME windows-text-encoding COMMAND snapweave-windows-text-encoding-tests)
endif()
