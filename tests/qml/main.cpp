// SPDX-License-Identifier: GPL-3.0-only
// Entry point of the QML smoke test: Catch2 inside a QGuiApplication on the offscreen
// platform, with the Material style the app uses.

#include <catch2/catch_session.hpp>

#include <QGuiApplication>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    // QSettings needs both names; the test redirects its folder.
    QGuiApplication::setOrganizationName(QStringLiteral("DicomViewerTests"));
    QGuiApplication::setApplicationName(QStringLiteral("dicomapp_qml_tests"));
    QQuickStyle::setStyle(QStringLiteral("Material"));
    return Catch::Session().run(argc, argv);
}
