cmake_minimum_required(VERSION 3.24)

project(DicomViewer VERSION 0.1.0 LANGUAGES C CXX)

option(DICOMVIEWER_BUILD_TESTS "Build the unit and BDD tests" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Third-party dependencies (DCMTK, Catch2) via FetchContent.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(Dependencies)

# Warnings apply to our own targets only (never to fetched dependencies).
add_library(dicomviewer_warnings INTERFACE)
if(MSVC)
    target_compile_options(dicomviewer_warnings INTERFACE /W4)
else()
    target_compile_options(dicomviewer_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src/core)
add_subdirectory(src/app)

if(DICOMVIEWER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
