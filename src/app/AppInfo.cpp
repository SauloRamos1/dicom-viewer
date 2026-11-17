// SPDX-License-Identifier: GPL-3.0-only
#include "AppInfo.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <algorithm>

#include "Codecs.h"

AppInfo::AppInfo(QObject *parent)
    : QObject(parent)
{
}

QString AppInfo::appName() const
{
    return QCoreApplication::applicationName();
}

QString AppInfo::version() const
{
    return QStringLiteral(DICOMVIEWER_VERSION);
}

QString AppInfo::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}

QString AppInfo::dcmtkVersion() const
{
    return QString::fromStdString(dv::dcmtkVersion());
}

QUrl AppInfo::startupFile() const
{
    const QStringList args = QCoreApplication::arguments().mid(1);
    const auto positional = std::ranges::find_if(args, [](const QString &arg) { return !arg.startsWith(u'-'); });
    return positional == args.end() ? QUrl() : QUrl::fromLocalFile(QFileInfo(*positional).absoluteFilePath());
}

void AppInfo::registerCodecs() const
{
    dv::registerCodecs();
}
