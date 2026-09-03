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

    add_executable(
        panebind-topology-tests
        tests/core/topology_tests.cpp
    )
    target_link_libraries(panebind-topology-tests PRIVATE panebind_core)
    panebind_enable_warnings(panebind-topology-tests)
    add_test(NAME topology COMMAND panebind-topology-tests)

    add_executable(
        panebind-translation-tests
        tests/core/translation_tests.cpp
    )
    target_link_libraries(panebind-translation-tests PRIVATE panebind_core)
    panebind_enable_warnings(panebind-translation-tests)
    add_test(NAME translation COMMAND panebind-translation-tests)

    add_executable(
        panebind-glue-move-coordinator-tests
        tests/core/glue_move_coordinator_tests.cpp
    )
    target_link_libraries(panebind-glue-move-coordinator-tests PRIVATE panebind_core)
    panebind_enable_warnings(panebind-glue-move-coordinator-tests)
    add_test(NAME glue-move-coordinator COMMAND panebind-glue-move-coordinator-tests)
endif()

if(WIN32)
    add_subdirectory(src/platform/windows)
endif()
