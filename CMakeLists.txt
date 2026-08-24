cmake_minimum_required(VERSION 3.25)

project(
    SnapWeave
    VERSION 0.0.0
    DESCRIPTION "A portable-first desktop window enhancement project"
    LANGUAGES CXX
)

include(CTest)

function(snapweave_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endfunction()

add_library(snapweave_core INTERFACE)
target_include_directories(snapweave_core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(snapweave_core INTERFACE cxx_std_20)

if(BUILD_TESTING)
    add_executable(
        snapweave-geometry-tests
        tests/core/geometry_tests.cpp
    )
    target_link_libraries(snapweave-geometry-tests PRIVATE snapweave_core)
    snapweave_enable_warnings(snapweave-geometry-tests)
    add_test(NAME geometry COMMAND snapweave-geometry-tests)

    add_executable(
        snapweave-core-model-tests
        tests/core/model_tests.cpp
    )
    target_link_libraries(snapweave-core-model-tests PRIVATE snapweave_core)
    snapweave_enable_warnings(snapweave-core-model-tests)
    add_test(NAME core-model COMMAND snapweave-core-model-tests)
endif()

if(WIN32)
    add_subdirectory(src/platform/windows)
endif()
