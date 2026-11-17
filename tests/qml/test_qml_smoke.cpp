// SPDX-License-Identifier: GPL-3.0-only
// Instantiates the real QML interface (Main -> MainView) and drives it the way a user does:
// open a file, toggle the Metadata, Errors and Options panes, switch theme and language.
// Any QML warning or error (binding loop, undefined property, missing import, ...) fails the test.

#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStringList>

#include "AppSettings.h"
#include "AppTestSupport.h"
#include "DicomController.h"
#include "DicomImageProvider.h"
#include "LanguageManager.h"
#include "TestSupport.h"

using namespace dv::test;

namespace {

// What the offscreen platform prints that says nothing about the QML: Qt Quick 3D cannot render
// without an RHI-based scene graph (these two messages only), and the platform plugin's own
// notices (log category qt.qpa.*; and the offscreen plugin's "does not support setting window
// opacity", which the splash fade-out triggers), and the "QFontDatabase: Cannot find font directory"
// notice of a Qt installed without lib/fonts (the Windows CI runner). Every other
// warning, critical or fatal message is a defect.
bool isKnownEnvironmentNotice(const QMessageLogContext &context, const QString &message)
{
    if (context.category != nullptr && qstrncmp(context.category, "qt.qpa.", 7) == 0)
        return true;
    return message == QLatin1String("This plugin does not support setting window opacity")
        || message.startsWith(QLatin1String("QFontDatabase: Cannot find font directory"))
        || message.contains(QLatin1String("QSGRendererInterface::isApiRhiBased"))
        || message.startsWith(QLatin1String("The Qt Quick scene is using a rendering method that is not based on QRhi"));
}

// Collects the warnings, critical and fatal messages printed while it lives.
class MessageCollector
{
public:
    MessageCollector()
    {
        s_instance = this;
        m_previous = qInstallMessageHandler(&MessageCollector::handle);
    }
    ~MessageCollector()
    {
        qInstallMessageHandler(m_previous);
        s_instance = nullptr;
    }

    const QStringList &problems() const { return m_problems; }
    const QStringList &ignored() const { return m_ignored; }

private:
    static void handle(QtMsgType type, const QMessageLogContext &context, const QString &message)
    {
        if (s_instance != nullptr && type != QtDebugMsg && type != QtInfoMsg) {
            const QString where = context.file != nullptr
                ? QStringLiteral(" (%1:%2)").arg(QString::fromUtf8(context.file)).arg(context.line)
                : QString();
            (isKnownEnvironmentNotice(context, message) ? s_instance->m_ignored : s_instance->m_problems) << message + where;
        }
        if (s_instance != nullptr && s_instance->m_previous != nullptr)
            s_instance->m_previous(type, context, message);
    }

    inline static MessageCollector *s_instance = nullptr;
    QtMessageHandler m_previous = nullptr;
    QStringList m_problems;
    QStringList m_ignored;
};

// First item of the tree below `root` whose QML type name starts with `typeName` ("TopBar" matches the
// generated class "TopBar_QMLTYPE_12"); `skip` ones are passed over, to pick the n-th of several.
QQuickItem *findItem(QQuickItem *root, const char *typeName, int skip = 0)
{
    int seen = 0;
    std::function<QQuickItem *(QQuickItem *)> visit = [&](QQuickItem *item) -> QQuickItem * {
        if (qstrncmp(item->metaObject()->className(), typeName, qstrlen(typeName)) == 0
            && item->metaObject()->className()[qstrlen(typeName)] == '_' && seen++ == skip)
            return item;
        for (QQuickItem *child : item->childItems()) {
            if (QQuickItem *found = visit(child))
                return found;
        }
        return nullptr;
    };
    return visit(root);
}

// Runs the event loop until at least one frame has been swapped (or the window cannot render, in
// which case the events alone are processed): bounded, no sleeping.
void settle(QQuickWindow *window)
{
    bool swapped = false;
    const auto connection = QObject::connect(window, &QQuickWindow::frameSwapped, window, [&] { swapped = true; });
    window->requestUpdate();
    pumpUntil([&] { return swapped; }, 2000);
    QObject::disconnect(connection);
    pumpFor(50);
}

} // namespace

TEST_CASE("The QML interface runs without warnings through the main interactions", "[qml][smoke]")
{
    const IsolatedSettings settings;
    MessageCollector messages;

    QQmlApplicationEngine engine;
    auto *controller = engine.singletonInstance<DicomController *>("DicomViewer", "DicomController");
    if (controller == nullptr)
        SKIP("the DicomViewer QML module is not available");
    engine.addImageProvider(QStringLiteral("dicom"), new DicomImageProvider(controller->frameStore()));
    engine.addImageProvider(QStringLiteral("dicomraw"), new DicomImageProvider(controller->gpuFrameStore()));
    engine.addImageProvider(QStringLiteral("dicomlut"), new ColormapImageProvider);
    engine.loadFromModule("DicomViewer", "Main");
    if (engine.rootObjects().isEmpty()) {
        INFO("problems: " << messages.problems().join("\n").toStdString());
        FAIL("Main.qml did not load");
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    REQUIRE(window != nullptr);

    // The splash is a child window of the main one, yet it must be on screen first: a child window
    // waits for its (invisible) transient parent unless it has none.
    const auto splashes = window->findChildren<QQuickWindow *>();
    REQUIRE(splashes.size() == 1);
    REQUIRE(pumpUntil([&] { return splashes.constFirst()->isVisible(); }, 5000));
    CHECK_FALSE(window->isVisible());

    // Main.qml shows the splash for about 3 s, runs its startup steps, then shows the window.
    REQUIRE(pumpUntil([&] { return window->isVisible() && findItem(window->contentItem(), "TopBar") != nullptr; }, 30000));
    auto *topBar = findItem(window->contentItem(), "TopBar");
    REQUIRE(topBar != nullptr);
    settle(window);
    CHECK(messages.problems().isEmpty());

    // Open a file through the controller, as the Open dialog does.
    controller->open(urlOf(dataFile("CT_small.dcm")));
    REQUIRE(pumpUntil([&] { return !controller->busy(); }));
    REQUIRE(controller->hasImage());
    settle(window);

    // Metadata and Errors panels, and the Options pane: toggle each on and off.
    for (const char *property : {"metadataChecked", "errorsChecked", "optionsChecked"}) {
        CAPTURE(property);
        topBar->setProperty(property, true);
        REQUIRE(topBar->property(property).toBool());
        settle(window);
        topBar->setProperty(property, false);
        settle(window);
    }
    // The two side panels follow their buttons (Metadata first, Errors last).
    auto *metadataPanel = findItem(window->contentItem(), "Panel", 0);
    auto *errorsPanel = findItem(window->contentItem(), "Panel", 1);
    REQUIRE(metadataPanel != nullptr);
    REQUIRE(errorsPanel != nullptr);
    CHECK_FALSE(metadataPanel->property("open").toBool());
    topBar->setProperty("metadataChecked", true);
    topBar->setProperty("errorsChecked", true);
    CHECK(metadataPanel->property("open").toBool());
    CHECK(errorsPanel->property("open").toBool());
    topBar->setProperty("errorsChecked", false);
    topBar->setProperty("optionsChecked", true);
    settle(window);

    // Theme, then language, with every panel open.
    auto *appSettings = engine.singletonInstance<AppSettings *>("DicomViewer", "AppSettings");
    REQUIRE(appSettings != nullptr);
    for (const QString &theme : {QStringLiteral("light"), QStringLiteral("dark")}) {
        appSettings->setTheme(theme);
        settle(window);
    }
    auto *languages = engine.singletonInstance<LanguageManager *>("DicomViewer", "LanguageManager");
    REQUIRE(languages != nullptr);
    for (const QString &language : {QStringLiteral("pt_BR"), QStringLiteral("en")}) {
        languages->setLanguage(language);
        REQUIRE(languages->language() == language);
        settle(window);
    }

    // A series folder (slice stepping, 3D view on and off), then the reset to the empty state.
    const TempDir series;
    writeCtSeries(series.path(), 4);
    controller->openSeries(urlOf(series.path()));
    REQUIRE(pumpUntil([&] { return !controller->busy(); }));
    REQUIRE(controller->hasVolume());
    settle(window);
    controller->setFrame(2);
    topBar->setProperty("view3DChecked", true);
    settle(window);
    topBar->setProperty("view3DChecked", false);
    settle(window);
    controller->reset();
    settle(window);

    INFO("ignored environment notices: " << messages.ignored().join("\n").toStdString());
    INFO("QML problems:\n" << messages.problems().join("\n").toStdString());
    CHECK(messages.problems().isEmpty());
}
