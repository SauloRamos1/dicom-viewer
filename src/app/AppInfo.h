// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// Read-only version information and startup hooks exposed to QML.
class AppInfo : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString appName READ appName CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString qtVersion READ qtVersion CONSTANT)
    Q_PROPERTY(QString dcmtkVersion READ dcmtkVersion CONSTANT)
    // First positional command line argument as a file URL (empty if none).
    Q_PROPERTY(QUrl startupFile READ startupFile CONSTANT)

public:
    explicit AppInfo(QObject *parent = nullptr);

    QString appName() const;
    QString version() const;
    QString qtVersion() const;
    QString dcmtkVersion() const;
    QUrl startupFile() const;

    // Startup step: registers the DICOM decoders (see dv::registerCodecs).
    Q_INVOKABLE void registerCodecs() const;
};
