// SPDX-License-Identifier: GPL-3.0-only
#include "LanguageManager.h"

#include <QCoreApplication>
#include <QLocale>
#include <QQmlEngine>
#include <QSettings>

namespace {
constexpr auto kLanguageKey = "language";

bool isSupported(const QString &language)
{
    return language == u"en" || language == u"pt_BR";
}
} // namespace

LanguageManager::LanguageManager(QQmlEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

LanguageManager *LanguageManager::create(QQmlEngine *qmlEngine, QJSEngine *)
{
    return new LanguageManager(qmlEngine);
}

QString LanguageManager::language() const
{
    return m_language;
}

void LanguageManager::setLanguage(const QString &language)
{
    if (!isSupported(language) || language == m_language)
        return;
    apply(language);
    QSettings().setValue(kLanguageKey, m_language);
}

void LanguageManager::init()
{
    const QString stored = QSettings().value(kLanguageKey).toString();
    const QString language = isSupported(stored)
        ? stored
        : (QLocale::system().language() == QLocale::Portuguese ? QStringLiteral("pt_BR")
                                                               : QStringLiteral("en"));
    apply(language);
}

void LanguageManager::apply(const QString &language)
{
    QCoreApplication::removeTranslator(&m_translator);
    if (language != u"en" && m_translator.load(QStringLiteral(":/i18n/dicomviewer_%1.qm").arg(language)))
        QCoreApplication::installTranslator(&m_translator);

    const bool changed = language != m_language;
    m_language = language;
    m_engine->retranslate();
    if (changed)
        emit languageChanged();
}
