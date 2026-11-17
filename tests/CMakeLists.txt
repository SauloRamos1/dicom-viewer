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
    unit/test_pixeldata.cpp
    unit/test_volume.cpp
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
find_package(Qt6 6.12 REQUIRED COMPONENTS Quick Concurrent)

add_executable(dicomapp_tests
    app/main.cpp
    app/test_controller.cpp
    app/test_error_model.cpp
    app/test_error_text.cpp
    app/test_image_provider.cpp
    app/test_metadata_model.cpp
    bdd/test_open_file_app.cpp
    ${DICOMAPP_LOGIC_SOURCES}
)
set_target_properties(dicomapp_tests PROPERTIES AUTOMOC ON)
target_include_directories(dicomapp_tests PRIVATE app ${PROJECT_SOURCE_DIR}/src/app)
target_link_libraries(dicomapp_tests
    PRIVATE
        dicomtest_support
        Catch2::Catch2
        Qt6::Quick
        Qt6::Concurrent
        dicomviewer_warnings
)
catch_discover_tests(dicomapp_tests
    PROPERTIES ENVIRONMENT QT_QPA_PLATFORM=offscreen)
