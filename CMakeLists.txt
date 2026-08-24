cmake_minimum_required(VERSION 3.25)

project(
    PaneBind
    VERSION 0.0.0
    DESCRIPTION "A portable-first desktop window enhancement project"
    LANGUAGES CXX
)

include(CTest)

function(panebind_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
endfunction()

add_library(panebind_core INTERFACE)
target_include_directories(panebind_core INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(panebind_core INTERFACE cxx_std_20)

if(BUILD_TESTING)
    add_executable(
        panebind-geometry-tests
        tests/core/geometry_tests.cpp
    )
    target_link_libraries(panebind-geometry-tests PRIVATE panebind_core)
    panebind_enable_warnings(panebind-geometry-tests)
    add_test(NAME geometry COMMAND panebind-geometry-tests)

    add_executable(
        panebind-core-model-tests
        tests/core/model_tests.cpp
    )
    target_link_libraries(panebind-core-model-tests PRIVATE panebind_core)
    panebind_enable_warnings(panebind-core-model-tests)
    add_test(NAME core-model COMMAND panebind-core-model-tests)
endif()

if(WIN32)
    add_subdirectory(src/platform/windows)
endif()
