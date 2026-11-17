# SPDX-License-Identifier: GPL-3.0-only
include(Catch)

# Helpers shared by the unit and BDD tests (fixture paths, temp dirs, DCMTK rewrites).
add_library(dicomtest_support STATIC support/TestSupport.cpp support/TestSupport.h)
target_include_directories(dicomtest_support PUBLIC support)
target_link_libraries(dicomtest_support PUBLIC dicomcore PRIVATE dicomviewer_warnings)
target_compile_definitions(dicomtest_support PUBLIC
    DICOM_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

add_executable(dicomcore_tests
    unit/test_codecs.cpp
    unit/test_dicomfile.cpp
    unit/test_error.cpp
    unit/test_metadata.cpp
    unit/test_orientation.cpp
    unit/test_gputexture.cpp
    unit/test_heightmap.cpp
    unit/test_pixeldata.cpp
    unit/test_text.cpp
    unit/test_viewgeometry.cpp
    unit/test_volume.cpp
    unit/test_volume_gpu.cpp
    unit/test_windowing.cpp
    bdd/test_open_file.cpp
)
target_link_libraries(dicomcore_tests
    PRIVATE
        dicomtest_support
        Catch2::Catch2WithMain
        dicomviewer_warnings
)
catch_discover_tests(dicomcore_tests)

# Qt glue (controller, models, image provider): compiles the same sources as the
# app, without the QML module.
find_package(Qt6 6.12 REQUIRED COMPONENTS Quick Quick3D Concurrent LinguistTools)

add_executable(dicomapp_tests
    app/main.cpp
    app/test_app_settings.cpp
    app/test_controller.cpp
    app/test_controller_gpu.cpp
    app/test_controller_pixel.cpp
    app/test_controller_series.cpp
    app/test_error_model.cpp
    app/test_error_text.cpp
    app/test_image_provider.cpp
    app/test_language_manager.cpp
    app/test_metadata_model.cpp
    app/test_quick3d_data.cpp
    bdd/test_open_file_app.cpp
    bdd/test_view_3d.cpp
    ${DICOMAPP_LOGIC_SOURCES}
)
set_target_properties(dicomapp_tests PROPERTIES AUTOMOC ON)
target_include_directories(dicomapp_tests PRIVATE app ${PROJECT_SOURCE_DIR}/src/app)
target_link_libraries(dicomapp_tests
    PRIVATE
        dicomtest_support
        Catch2::Catch2
        Qt6::Quick
        Qt6::Quick3D
        Qt6::Concurrent
        dicomviewer_warnings
)
# The PT-BR catalogue, compiled (lrelease only: lupdate stays the app target's
# job) and embedded where LanguageManager looks for it (:/i18n).
qt_add_lrelease(dicomapp_tests
    TS_FILES ${PROJECT_SOURCE_DIR}/i18n/dicomviewer_pt_BR.ts
    QM_FILES_OUTPUT_VARIABLE dicomapp_tests_qm_files
)
qt_add_resources(dicomapp_tests dicomapp_tests_i18n
    PREFIX /i18n
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${dicomapp_tests_qm_files}
)
catch_discover_tests(dicomapp_tests
    PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)

# QML smoke test: compiles the app's QML module (same sources) into its own
# executable, loads Main and drives the interface offscreen.
find_package(Qt6 6.12 REQUIRED COMPONENTS QuickControls2 Svg)
add_executable(dicomapp_qml_tests
    qml/main.cpp
    qml/test_qml_smoke.cpp
)
set_target_properties(dicomapp_qml_tests PROPERTIES AUTOMOC ON)
dicomviewer_add_qml_module(dicomapp_qml_tests)
target_include_directories(dicomapp_qml_tests PRIVATE app ${PROJECT_SOURCE_DIR}/src/app)
target_compile_definitions(dicomapp_qml_tests PRIVATE DICOMVIEWER_VERSION="${PROJECT_VERSION}")
target_link_libraries(dicomapp_qml_tests
    PRIVATE
        dicomtest_support
        Catch2::Catch2
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Quick3D
        Qt6::Svg
        Qt6::Concurrent
        dicomviewer_warnings
)
# The PT-BR catalogue of the language switch, compiled once by the target above.
qt_add_resources(dicomapp_qml_tests dicomapp_qml_tests_i18n
    PREFIX /i18n
    BASE ${CMAKE_CURRENT_BINARY_DIR}
    FILES ${dicomapp_tests_qm_files}
)
catch_discover_tests(dicomapp_qml_tests
    PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
