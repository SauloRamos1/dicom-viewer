# SPDX-License-Identifier: GPL-3.0-only
add_library(dicomcore STATIC
    Codecs.cpp
    Codecs.h
    Colormap.cpp
    DicomError.h
    DicomFile.cpp
    DicomFile.h
    DicomFileState.h
    GpuTexture.h
    Guarded.h
    Heightmap.cpp
    Heightmap.h
    Metadata.cpp
    Metadata.h
    Orientation.cpp
    Orientation.h
    PixelData.cpp
    Voi.h
    Volume.cpp
    Volume.h
    VolumeGpu.cpp
    VolumeGpu.h
    Windowing.cpp
    Windowing.h
)
target_include_directories(dicomcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dicomcore
    PUBLIC dcmtk_deps
    PRIVATE dicomviewer_warnings
)
target_compile_features(dicomcore PUBLIC cxx_std_23)
