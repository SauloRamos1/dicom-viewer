// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QSettings>
#include <QString>
#include <QUrl>

#include <filesystem>
#include <functional>

#include "TestSupport.h"

namespace dv::test {

// Runs the event loop until `done()` is true; false on timeout.
inline bool pumpUntil(const std::function<bool()> &done, int timeoutMs = 15000)
{
    const QDeadlineTimer deadline(timeoutMs);
    while (!done()) {
        if (deadline.hasExpired())
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return true;
}

// Keeps the event loop running for `ms`, e.g. to prove nothing else arrives.
inline void pumpFor(int ms)
{
    const QDeadlineTimer deadline(ms);
    while (!deadline.hasExpired())
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

// Points QSettings at a scratch folder so the tests never touch real preferences.
class IsolatedSettings
{
public:
    IsolatedSettings()
    {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           QString::fromStdString(m_dir.path().string()));
    }

    const std::filesystem::path &path() const { return m_dir.path(); }

private:
    TempDir m_dir;
};

inline QUrl urlOf(const std::filesystem::path &path)
{
    return QUrl::fromLocalFile(QString::fromStdString(path.string()));
}

} // namespace dv::test
