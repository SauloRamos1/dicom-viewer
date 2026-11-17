# SPDX-License-Identifier: GPL-3.0-only
# Third-party dependencies, fetched at configure time and pinned by tag.
# FIND_PACKAGE_ARGS makes CMake use an installed copy first when one is found.
include(FetchContent)

# --- DCMTK -------------------------------------------------------------------
# Static build, lean: no apps/docs and no optional external libraries. The
# built-in JPEG (dcmjpeg) and JPEG-LS (dcmjpls) codecs are kept. Only the
# modules we need are built (DCMTK_MODULES).
set(DCMTK_MODULES ofstd oflog oficonv dcmdata dcmimgle dcmimage dcmjpeg dcmjpls
    CACHE STRING "DCMTK modules to build" FORCE)
set(BUILD_APPS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(DCMTK_WITH_DOXYGEN OFF CACHE BOOL "" FORCE)
foreach(opt TIFF PNG XML ZLIB OPENSSL ICONV WRAP OPENJPEG SNDFILE)
    set(DCMTK_WITH_${opt} OFF CACHE BOOL "" FORCE)
endforeach()
# Unicode file names on Windows: open files through their UTF-16 names.
if(WIN32)
    set(DCMTK_WIDE_CHAR_FILE_IO_FUNCTIONS ON CACHE BOOL "" FORCE)
endif()
# Compile the data dictionary in: no external dictionary file at runtime.
set(DCMTK_DEFAULT_DICT builtin CACHE STRING "" FORCE)

FetchContent_Declare(dcmtk
    GIT_REPOSITORY https://github.com/DCMTK/dcmtk.git
    GIT_TAG        DCMTK-3.7.1
    GIT_SHALLOW    TRUE
    FIND_PACKAGE_ARGS 3.7 CONFIG
)

# DCMTK only knows C++ <= 20: build it at C++20, then restore ours.
set(_dv_cxx_standard "${CMAKE_CXX_STANDARD}")
set(CMAKE_CXX_STANDARD 20)
FetchContent_MakeAvailable(dcmtk)
set(CMAKE_CXX_STANDARD "${_dv_cxx_standard}")

# One INTERFACE target hides the in-tree (DCMTK) vs installed (DCMTK::DCMTK)
# difference from the rest of the build.
add_library(dcmtk_deps INTERFACE)
if(TARGET DCMTK::DCMTK)
    target_link_libraries(dcmtk_deps INTERFACE DCMTK::DCMTK)
else()
    target_link_libraries(dcmtk_deps INTERFACE DCMTK)
endif()

# --- Catch2 ------------------------------------------------------------------
if(DICOMVIEWER_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.16.1
        GIT_SHALLOW    TRUE
        FIND_PACKAGE_ARGS 3 CONFIG
    )
    FetchContent_MakeAvailable(Catch2)
    if(NOT TARGET Catch2::Catch2WithMain)
        message(FATAL_ERROR "Catch2::Catch2WithMain not available")
    endif()
    if(catch2_SOURCE_DIR) # in-tree build: Catch.cmake lives in the sources
        list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    endif()
endif()
