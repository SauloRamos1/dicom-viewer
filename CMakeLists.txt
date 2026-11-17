# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.24)

project(DicomViewer VERSION 0.1.0 LANGUAGES C CXX)

option(DICOMVIEWER_BUILD_TESTS "Build the unit and BDD tests" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Apply Qt's macOS baseline to every target, including static dependencies.
if(APPLE)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${QT_SUPPORTED_MIN_MACOS_VERSION}")
    endif()
endif()

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

# Package only our runtime component, excluding fetched libraries' dev installs.
set(CPACK_PACKAGE_NAME "dicom-viewer")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
string(TOLOWER "${CMAKE_SYSTEM_NAME}" package_os)
set(CPACK_PACKAGE_FILE_NAME "dicom-viewer-${PROJECT_VERSION}-${package_os}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_COMPONENTS_ALL Runtime)
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)
if(APPLE)
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK OFF)
    set(CPACK_DMG_VOLUME_NAME "DICOM Viewer ${PROJECT_VERSION}")
elseif(WIN32)
    set(CPACK_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
endif()
include(CPack)
