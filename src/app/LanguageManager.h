#pragma once

#include <QObject>
#include <QString>
#include <QTranslator>
#include <QtQml/qqmlregistration.h>

class QJSEngine;
class QQmlEngine;

// Runtime language switch. `language` is "en" (source strings, no translator)
// or "pt_BR". The first run follows QLocale::system(); later runs use the
// persisted choice.
class LanguageManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)

public:
    explicit LanguageManager(QQmlEngine *engine, QObject *parent = nullptr);

    static LanguageManager *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    QString language() const;
    void setLanguage(const QString &language);

    // Picks the persisted (or system) language and installs its translator
    // (startup step).
    Q_INVOKABLE void init();

signals:
    void languageChanged();

private:
    void apply(const QString &language);

    QQmlEngine *m_engine;
    QTranslator m_translator;
    QString m_language = QStringLiteral("en");
};
