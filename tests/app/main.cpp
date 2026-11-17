// Entry point of the Qt glue tests: Catch2 runs inside a QGuiApplication so the
// controller's worker results can be delivered through the event loop.

#include <catch2/catch_session.hpp>

#include <QGuiApplication>

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    // QSettings needs both names; tests that persist redirect its folder.
    QGuiApplication::setOrganizationName(QStringLiteral("DicomViewerTests"));
    QGuiApplication::setApplicationName(QStringLiteral("dicomapp_tests"));
    return Catch::Session().run(argc, argv);
}
