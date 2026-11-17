// SPDX-License-Identifier: GPL-3.0-only
#include <cstdlib>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

#include "DicomController.h"
#include "DicomImageProvider.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("DICOM Viewer"));
    QGuiApplication::setOrganizationName(QStringLiteral("DicomViewer"));
    QGuiApplication::setApplicationVersion(QStringLiteral(DICOMVIEWER_VERSION));

    QQuickStyle::setStyle(QStringLiteral("Material"));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    // The controller is the QML singleton; its latest frame is served as
    // image://dicom/<revision> (CPU path) and its packed raw frame as
    // image://dicomraw/<revision> (GPU path), next to the colormap tables of
    // image://dicomlut/<colormap>.
    auto *controller = engine.singletonInstance<DicomController *>("DicomViewer", "DicomController");
    if (controller == nullptr) {
        qCritical("The DicomController singleton is not registered in the DicomViewer module");
        return EXIT_FAILURE;
    }
    engine.addImageProvider(QStringLiteral("dicom"), new DicomImageProvider(controller->frameStore()));
    engine.addImageProvider(QStringLiteral("dicomraw"), new DicomImageProvider(controller->gpuFrameStore()));
    engine.addImageProvider(QStringLiteral("dicomlut"), new ColormapImageProvider);
    engine.loadFromModule("DicomViewer", "Main");

    return QGuiApplication::exec();
}
