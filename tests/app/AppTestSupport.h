#pragma once

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QUrl>

#include <filesystem>
#include <functional>

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

inline QUrl urlOf(const std::filesystem::path &path)
{
    return QUrl::fromLocalFile(QString::fromStdString(path.string()));
}

} // namespace dv::test
