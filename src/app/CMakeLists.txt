# SPDX-License-Identifier: GPL-3.0-only
find_package(Qt6 6.12 REQUIRED COMPONENTS Quick QuickControls2 Quick3D Concurrent LinguistTools ShaderTools Svg)

qt_standard_project_setup(REQUIRES 6.12)

qt_add_executable(appDicomViewer
    main.cpp
)

# Qt glue without any QML file, also compiled by tests/ (see DICOMAPP_LOGIC_SOURCES).
set(DICOMAPP_LOGIC_SOURCES
    AppSettings.cpp AppSettings.h
    DicomController.cpp DicomController.h
    DicomImageProvider.cpp DicomImageProvider.h
    ErrorModel.cpp ErrorModel.h
    ErrorText.cpp ErrorText.h
    HeightmapGeometry.cpp HeightmapGeometry.h
    LanguageManager.cpp LanguageManager.h
    MetadataModel.cpp MetadataModel.h
    VolumeTexture.cpp VolumeTexture.h
)
list(TRANSFORM DICOMAPP_LOGIC_SOURCES PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/)
set(DICOMAPP_LOGIC_SOURCES ${DICOMAPP_LOGIC_SOURCES} PARENT_SCOPE)

# The DicomViewer QML module (QML files, fonts, icons, shaders and the Qt glue
# sources) of `target`. Called for the app and for the QML smoke test
# (tests/CMakeLists.txt), which compiles its own copy of the module.
function(dicomviewer_add_qml_module target)
    set(QML_DIR ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/qml)
    set(RES_DIR ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/resources)
    set(SHADER_DIR ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shaders)

    set(QML_SOURCES
        Main.qml MainView.qml Splash.qml
        # Singletons: theme palette and icon font
        Style/Theme.qml Style/FontAwesome.qml
        # Reusable building blocks, no application logic
        Components/AppToolTip.qml Components/Icon.qml Components/IconButton.qml Components/Panel.qml
        Components/FloatingPane.qml Components/SegmentedControl.qml Components/SliderField.qml
        Components/SwitchRow.qml Components/LabeledCombo.qml
        # Top bar, side panels and status bar
        Panels/TopBar.qml Panels/MetadataPanel.qml Panels/ErrorsPanel.qml Panels/StatusBar.qml
        # Image viewport: 2D, GPU, 3D
        Viewer/ViewerPane.qml Viewer/View2D.qml Viewer/GpuImage.qml Viewer/Viewport3D.qml
        Viewer/HeightmapModel.qml Viewer/VolumeModel.qml
        # Floating Options pane and its sections
        Options/OptionsPane.qml Options/OptionsSection.qml Options/WindowLevelSection.qml
        Options/DisplaySection.qml Options/RenderSection.qml Options/TransformSection.qml
        Options/ThreeDSection.qml
    )
    list(TRANSFORM QML_SOURCES PREPEND ${QML_DIR}/)
    set(RESOURCE_FILES ${RES_DIR}/fonts/fa-solid-900.otf ${RES_DIR}/icons/app.svg)
    # Quick3D custom material snippets: not complete shaders, so Qt Quick 3D
    # completes and compiles them at run time (unlike windowing.frag below).
    set(MATERIAL_SNIPPETS ${SHADER_DIR}/volume.vert ${SHADER_DIR}/volume.frag)

    # QML files and shader snippets live in subdirectories: publish each under its
    # bare module path; fonts and icons under their path relative to resources/.
    foreach(f IN LISTS QML_SOURCES MATERIAL_SNIPPETS)
        cmake_path(GET f FILENAME alias)
        set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS ${alias})
    endforeach()
    foreach(f IN LISTS RESOURCE_FILES)
        cmake_path(RELATIVE_PATH f BASE_DIRECTORY ${RES_DIR} OUTPUT_VARIABLE alias)
        set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS ${alias})
    endforeach()
    set_source_files_properties(${QML_DIR}/Style/FontAwesome.qml ${QML_DIR}/Style/Theme.qml
        PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

    qt_add_qml_module(${target}
        URI DicomViewer
        VERSION 1.0
        SOURCES
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/AppInfo.cpp
            ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/AppInfo.h
            ${DICOMAPP_LOGIC_SOURCES}
        QML_FILES ${QML_SOURCES}
        RESOURCES ${RESOURCE_FILES} ${MATERIAL_SNIPPETS}
    )

    # GPU render path: compiled by qsb for every backend (Metal, D3D, Vulkan, GL) and
    # published next to the QML files, so GpuImage.qml loads "windowing.frag.qsb".
    qt_add_shaders(${target} "dicomviewer_shaders"
        PREFIX "/qt/qml/DicomViewer"
        BASE ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shaders
        FILES ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shaders/windowing.frag
    )
endfunction()

dicomviewer_add_qml_module(appDicomViewer)

qt_add_translations(appDicomViewer
    TS_FILES ${PROJECT_SOURCE_DIR}/i18n/dicomviewer_pt_BR.ts
    RESOURCE_PREFIX /i18n
)

set_target_properties(appDicomViewer PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.SauloRamos1.DicomViewer
    MACOSX_BUNDLE_BUNDLE_NAME "DICOM Viewer"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2026 DICOM Viewer contributors; GPL-3.0-only"
    MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/packaging/Info.plist.in"
    MACOSX_BUNDLE_ICON_FILE app.icns
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)
if(APPLE)
    target_sources(appDicomViewer PRIVATE ${PROJECT_SOURCE_DIR}/packaging/icons/app.icns)
    set_source_files_properties(${PROJECT_SOURCE_DIR}/packaging/icons/app.icns
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
elseif(WIN32)
    enable_language(RC)
    configure_file(${PROJECT_SOURCE_DIR}/packaging/windows/app.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
    target_sources(appDicomViewer PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
endif()

target_compile_definitions(appDicomViewer PRIVATE
    DICOMVIEWER_VERSION="${PROJECT_VERSION}"
    $<$<CONFIG:Debug>:QT_QML_DEBUG>  # required by qmlpreview / QmlPreviewMcpServer
)

target_link_libraries(appDicomViewer
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Svg # Makes deploy tools include SVG image decoding for app.svg.
        Qt6::Concurrent
        Qt6::Quick3D
        dicomcore
        dicomviewer_warnings
)

# Use Qt's recommended bin/lib/qml layout outside the macOS bundle.
install(TARGETS appDicomViewer
    BUNDLE DESTINATION . COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
)
qt_generate_deploy_qml_app_script(
    TARGET appDicomViewer OUTPUT_SCRIPT deploy_script
)

if(APPLE)
    # macdeployqt deploys Cocoa by default. Keep offscreen for package smoke tests.
    install(FILES $<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>
        DESTINATION appDicomViewer.app/Contents/PlugIns/platforms COMPONENT Runtime)
    set(license_dir "appDicomViewer.app/Contents/Resources/licenses")
else()
    set(license_dir "bin/licenses")
endif()
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE
    ${PROJECT_SOURCE_DIR}/packaging/THIRD_PARTY_NOTICES.md
    ${PROJECT_SOURCE_DIR}/src/app/resources/fonts/LICENSE.txt
    DESTINATION ${license_dir} COMPONENT Runtime)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/docs/sbom/
    DESTINATION ${license_dir}/sbom COMPONENT Runtime FILES_MATCHING PATTERN "*.json")
if(UNIX AND NOT APPLE)
    install(FILES ${PROJECT_SOURCE_DIR}/packaging/linux/io.github.SauloRamos1.DicomViewer.desktop
        DESTINATION share/applications COMPONENT Runtime)
    install(FILES ${PROJECT_SOURCE_DIR}/packaging/linux/io.github.SauloRamos1.DicomViewer.metainfo.xml
        DESTINATION share/metainfo COMPONENT Runtime)
    install(FILES ${PROJECT_SOURCE_DIR}/packaging/icons/app-256.png
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME io.github.SauloRamos1.DicomViewer.png COMPONENT Runtime)
    install(FILES ${PROJECT_SOURCE_DIR}/packaging/icons/app-512.png
        DESTINATION share/icons/hicolor/512x512/apps
        RENAME io.github.SauloRamos1.DicomViewer.png COMPONENT Runtime)
endif()

# Deploy/sign after all bundle resources (including notices) are installed.
install(SCRIPT ${deploy_script} COMPONENT Runtime)
