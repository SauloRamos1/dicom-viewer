#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// Persisted UI preferences (QSettings). `theme` is "system" on first run, then
// "light" or "dark" once the user toggles it. `lastFolder` is where the Open
// dialog starts: the folder of the last opened file (Documents on first run).
class AppSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QUrl lastFolder READ lastFolder WRITE setLastFolder NOTIFY lastFolderChanged)

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString theme() const;
    void setTheme(const QString &theme);
    QUrl lastFolder() const;
    void setLastFolder(const QUrl &folder);

    // Reads the persisted values (startup step).
    Q_INVOKABLE void load();

signals:
    void themeChanged();
    void lastFolderChanged();

private:
    QString m_theme = QStringLiteral("system");
    QUrl m_lastFolder;
};
