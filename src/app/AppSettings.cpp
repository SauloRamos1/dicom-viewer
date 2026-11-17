#include "AppSettings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {
constexpr auto kThemeKey = "theme";
constexpr auto kLastFolderKey = "lastFolder";

bool isValidTheme(const QString &theme)
{
    return theme == u"system" || theme == u"light" || theme == u"dark";
}
} // namespace

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
    , m_lastFolder(QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)))
{
}

QString AppSettings::theme() const
{
    return m_theme;
}

void AppSettings::setTheme(const QString &theme)
{
    if (!isValidTheme(theme) || theme == m_theme)
        return;
    m_theme = theme;
    QSettings().setValue(kThemeKey, m_theme);
    emit themeChanged();
}

QUrl AppSettings::lastFolder() const
{
    return m_lastFolder;
}

void AppSettings::setLastFolder(const QUrl &folder)
{
    if (!folder.isLocalFile() || folder == m_lastFolder)
        return;
    m_lastFolder = folder;
    QSettings().setValue(kLastFolderKey, m_lastFolder.toLocalFile());
    emit lastFolderChanged();
}

void AppSettings::load()
{
    const QString folder = QSettings().value(kLastFolderKey).toString();
    if (!folder.isEmpty() && QDir(folder).exists()) {
        m_lastFolder = QUrl::fromLocalFile(folder);
        emit lastFolderChanged();
    }

    const QString stored = QSettings().value(kThemeKey).toString();
    if (isValidTheme(stored) && stored != m_theme) {
        m_theme = stored;
        emit themeChanged();
    }
}
