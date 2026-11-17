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
    // The controller is the QML singleton; its latest frame is served as image://dicom/<revision>.
    auto *controller = engine.singletonInstance<DicomController *>("DicomViewer", "DicomController");
    engine.addImageProvider(QStringLiteral("dicom"), new DicomImageProvider(controller->frameStore()));
    engine.loadFromModule("DicomViewer", "Main");

    return QGuiApplication::exec();
}
