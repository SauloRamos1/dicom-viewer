find_package(Qt6 6.12 REQUIRED COMPONENTS Quick QuickControls2 Quick3D Concurrent LinguistTools ShaderTools)

qt_standard_project_setup(REQUIRES 6.12)

qt_add_executable(appDicomViewer
    main.cpp
)

# Qt glue without any QML file, also compiled by tests/ (see DICOMAPP_LOGIC_SOURCES).
set(DICOMAPP_LOGIC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/AppSettings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/AppSettings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DicomController.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DicomController.h
    ${CMAKE_CURRENT_SOURCE_DIR}/DicomImageProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DicomImageProvider.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ErrorModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ErrorModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ErrorText.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ErrorText.h
    ${CMAKE_CURRENT_SOURCE_DIR}/HeightmapGeometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/HeightmapGeometry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/MetadataModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MetadataModel.h
    ${CMAKE_CURRENT_SOURCE_DIR}/VolumeTexture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/VolumeTexture.h
)
set(DICOMAPP_LOGIC_SOURCES ${DICOMAPP_LOGIC_SOURCES} PARENT_SCOPE)

set(QML_DIR ${CMAKE_CURRENT_SOURCE_DIR}/qml)
set(RES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/resources)

set(QML_SOURCES
    ${QML_DIR}/Main.qml
    ${QML_DIR}/MainView.qml
    ${QML_DIR}/Splash.qml
    ${QML_DIR}/TopBar.qml
    ${QML_DIR}/AppToolTip.qml
    ${QML_DIR}/Icon.qml
    ${QML_DIR}/IconButton.qml
    ${QML_DIR}/Panel.qml
    ${QML_DIR}/View2D.qml
    ${QML_DIR}/GpuImage.qml
    ${QML_DIR}/ViewerPane.qml
    ${QML_DIR}/Viewport3D.qml
    ${QML_DIR}/HeightmapModel.qml
    ${QML_DIR}/VolumeModel.qml
    ${QML_DIR}/ThreeDSection.qml
    ${QML_DIR}/MetadataPanel.qml
    ${QML_DIR}/ErrorsPanel.qml
    ${QML_DIR}/StatusBar.qml
    ${QML_DIR}/FloatingPane.qml
    ${QML_DIR}/OptionsPane.qml
    ${QML_DIR}/OptionsSection.qml
    ${QML_DIR}/WindowLevelSection.qml
    ${QML_DIR}/DisplaySection.qml
    ${QML_DIR}/RenderSection.qml
    ${QML_DIR}/SegmentedControl.qml
    ${QML_DIR}/TransformSection.qml
    ${QML_DIR}/SliderField.qml
    ${QML_DIR}/SwitchRow.qml
    ${QML_DIR}/LabeledCombo.qml
    ${QML_DIR}/Theme.qml
    ${QML_DIR}/FontAwesome.qml
)
set(RESOURCE_FILES
    ${RES_DIR}/fonts/fa-solid-900.otf
    ${RES_DIR}/icons/app.svg
)
# Quick3D custom material snippets: not complete shaders, so Qt Quick 3D
# completes and compiles them at run time (unlike windowing.frag below).
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(MATERIAL_SNIPPETS
    ${SHADER_DIR}/volume.vert
    ${SHADER_DIR}/volume.frag
)

# QML and resource files live in subdirectories: publish each under its
# bare module path (QML) or its path relative to resources/ (fonts, icons).
foreach(f IN LISTS QML_SOURCES)
    get_filename_component(name ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()
foreach(f IN LISTS RESOURCE_FILES)
    file(RELATIVE_PATH alias ${RES_DIR} ${f})
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS ${alias})
endforeach()
foreach(f IN LISTS MATERIAL_SNIPPETS)
    get_filename_component(name ${f} NAME)
    set_source_files_properties(${f} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()
set_source_files_properties(${QML_DIR}/FontAwesome.qml ${QML_DIR}/Theme.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appDicomViewer
    URI DicomViewer
    VERSION 1.0
    SOURCES
        AppInfo.cpp
        AppInfo.h
        LanguageManager.cpp
        LanguageManager.h
        ${DICOMAPP_LOGIC_SOURCES}
    QML_FILES ${QML_SOURCES}
    RESOURCES ${RESOURCE_FILES} ${MATERIAL_SNIPPETS}
)

# GPU render path: compiled by qsb for every backend (Metal, D3D, Vulkan, GL) and
# published next to the QML files, so GpuImage.qml loads "windowing.frag.qsb".
qt_add_shaders(appDicomViewer "dicomviewer_shaders"
    PREFIX "/qt/qml/DicomViewer"
    BASE ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/windowing.frag
)

qt_add_translations(appDicomViewer
    TS_FILES ${PROJECT_SOURCE_DIR}/i18n/dicomviewer_pt_BR.ts
    RESOURCE_PREFIX /i18n
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appDicomViewer PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appDicomViewer
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_compile_definitions(appDicomViewer PRIVATE
    DICOMVIEWER_VERSION="${PROJECT_VERSION}"
    $<$<CONFIG:Debug>:QT_QML_DEBUG>  # required by qmlpreview / QmlPreviewMcpServer
)

target_link_libraries(appDicomViewer
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::Concurrent
        Qt6::Quick3D
        dicomcore
        dicomviewer_warnings
)

install(TARGETS appDicomViewer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
