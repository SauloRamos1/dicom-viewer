// SPDX-License-Identifier: GPL-3.0-only
#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QLocale>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSettings>

#include <memory>

#include "AppTestSupport.h"
#include "LanguageManager.h"

using namespace dv::test;

namespace {

// Source text and its PT-BR translation, both from the View2D overlay.
const QString kSource = QStringLiteral("Frame %1/%2");
const QString kPortuguese = QStringLiteral("Quadro %1/%2");

QString translated()
{
    return QCoreApplication::translate("View2D", kSource.toUtf8().constData());
}

// A QML object whose property is a qsTr() call, like the overlays in View2D.qml
// (the qsTr context is the file name, hence the View2D.qml URL).
std::unique_ptr<QObject> makeQmlLabel(QQmlEngine &engine)
{
    QQmlComponent component(&engine);
    component.setData("import QtQml\nQtObject { property string text: qsTr(\"Frame %1/%2\") }",
                      QUrl(QStringLiteral("file:///View2D.qml")));
    std::unique_ptr<QObject> object(component.create());
    REQUIRE(object != nullptr);
    return object;
}

} // namespace

TEST_CASE("The first run follows the system language, later runs the stored choice", "[app][language]")
{
    const QLocale portugueseBrazil(QLocale::Portuguese, QLocale::Brazil);
    const QLocale portuguesePortugal(QLocale::Portuguese, QLocale::Portugal);
    const QLocale english(QLocale::English, QLocale::UnitedStates);
    const QLocale spanish(QLocale::Spanish, QLocale::Spain);

    CHECK(LanguageManager::initialLanguage({}, portugueseBrazil) == QStringLiteral("pt_BR"));
    CHECK(LanguageManager::initialLanguage({}, portuguesePortugal) == QStringLiteral("pt_BR"));
    CHECK(LanguageManager::initialLanguage({}, english) == QStringLiteral("en"));
    CHECK(LanguageManager::initialLanguage({}, spanish) == QStringLiteral("en"));
    // A stored supported choice wins over the system language ...
    CHECK(LanguageManager::initialLanguage(QStringLiteral("en"), portugueseBrazil) == QStringLiteral("en"));
    CHECK(LanguageManager::initialLanguage(QStringLiteral("pt_BR"), english) == QStringLiteral("pt_BR"));
    // ... and an unsupported one is ignored.
    CHECK(LanguageManager::initialLanguage(QStringLiteral("fr"), portugueseBrazil) == QStringLiteral("pt_BR"));
}

TEST_CASE("Switching language changes a known translated string", "[app][language]")
{
    const IsolatedSettings isolated;
    QQmlEngine engine;
    LanguageManager manager(&engine);

    SECTION("English shows the source text")
    {
        manager.setLanguage(QStringLiteral("en"));
        CHECK(manager.language() == QStringLiteral("en"));
        CHECK(translated() == kSource);
    }
    SECTION("pt_BR loads the right .qm and en goes back to the source text")
    {
        int changes = 0;
        QObject::connect(&manager, &LanguageManager::languageChanged, [&] { ++changes; });

        manager.setLanguage(QStringLiteral("pt_BR"));
        CHECK(manager.language() == QStringLiteral("pt_BR"));
        CHECK(translated() == kPortuguese);
        CHECK(changes == 1);

        manager.setLanguage(QStringLiteral("en"));
        CHECK(manager.language() == QStringLiteral("en"));
        CHECK(translated() == kSource);
        CHECK(changes == 2);

        manager.setLanguage(QStringLiteral("en")); // no change, no signal
        CHECK(changes == 2);
    }
    SECTION("an unsupported language is refused and not stored")
    {
        manager.setLanguage(QStringLiteral("fr"));
        CHECK(manager.language() == QStringLiteral("en"));
        CHECK(translated() == kSource);
        CHECK_FALSE(QSettings().contains(QStringLiteral("language")));
    }
}

TEST_CASE("Switching language retranslates the QML bindings", "[app][language]")
{
    const IsolatedSettings isolated;
    QQmlEngine engine;
    LanguageManager manager(&engine);
    manager.setLanguage(QStringLiteral("en"));
    const auto label = makeQmlLabel(engine);
    REQUIRE(label->property("text").toString() == kSource);

    manager.setLanguage(QStringLiteral("pt_BR"));
    CHECK(label->property("text").toString() == kPortuguese);

    manager.setLanguage(QStringLiteral("en"));
    CHECK(label->property("text").toString() == kSource);
}

TEST_CASE("The chosen language persists in an isolated settings scope", "[app][language]")
{
    const IsolatedSettings isolated;
    CHECK(QSettings().fileName().startsWith(QString::fromStdString(isolated.path().generic_string())));

    {
        QQmlEngine engine;
        LanguageManager manager(&engine);
        manager.setLanguage(QStringLiteral("pt_BR"));
    }
    CHECK(QSettings().value(QStringLiteral("language")).toString() == QStringLiteral("pt_BR"));

    // A new run (new manager, fresh engine) starts in the stored language.
    QQmlEngine engine;
    LanguageManager manager(&engine);
    manager.init();
    CHECK(manager.language() == QStringLiteral("pt_BR"));
    CHECK(translated() == kPortuguese);

    // A stored English choice is kept whatever the system language is.
    manager.setLanguage(QStringLiteral("en"));
    LanguageManager again(&engine);
    again.init();
    CHECK(again.language() == QStringLiteral("en"));
    CHECK(translated() == kSource);
}
