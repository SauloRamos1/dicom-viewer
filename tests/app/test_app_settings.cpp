// SPDX-License-Identifier: GPL-3.0-only
#include <catch2/catch_test_macros.hpp>

#include "AppSettings.h"
#include "AppTestSupport.h"

using namespace dv::test;

TEST_CASE("Options settings start with sensible defaults", "[app][settings]")
{
    const IsolatedSettings isolated;
    AppSettings settings;
    settings.load();
    CHECK(settings.smoothing());
    CHECK_FALSE(settings.optionsOpen());
    CHECK(settings.optionsX() == 1.0);
    CHECK(settings.optionsY() == 0.0);
}

TEST_CASE("Options settings persist across instances", "[app][settings]")
{
    const IsolatedSettings isolated;
    {
        AppSettings settings;
        settings.setSmoothing(false);
        settings.setOptionsOpen(true);
        settings.setOptionsX(0.25);
        settings.setOptionsY(0.75);
    }
    AppSettings reloaded;
    reloaded.load();
    CHECK_FALSE(reloaded.smoothing());
    CHECK(reloaded.optionsOpen());
    CHECK(reloaded.optionsX() == 0.25);
    CHECK(reloaded.optionsY() == 0.75);
}

TEST_CASE("The options position is a fraction clamped to 0..1", "[app][settings]")
{
    const IsolatedSettings isolated;
    AppSettings settings;
    settings.setOptionsX(1.7);
    settings.setOptionsY(-3);
    CHECK(settings.optionsX() == 1.0);
    CHECK(settings.optionsY() == 0.0);
}

TEST_CASE("Options settings notify only on change", "[app][settings]")
{
    const IsolatedSettings isolated;
    AppSettings settings;
    int smoothing = 0;
    int position = 0;
    QObject::connect(&settings, &AppSettings::smoothingChanged, [&] { ++smoothing; });
    QObject::connect(&settings, &AppSettings::optionsPositionChanged, [&] { ++position; });
    settings.setSmoothing(true); // already the default
    settings.setSmoothing(false);
    settings.setOptionsX(0.5);
    settings.setOptionsX(0.5);
    CHECK(smoothing == 1);
    CHECK(position == 1);
}

TEST_CASE("The render backend defaults to cpu and persists", "[app][settings][gpu]")
{
    const IsolatedSettings isolated;
    {
        AppSettings settings;
        settings.load();
        CHECK(settings.renderBackend() == QStringLiteral("cpu"));
        int changes = 0;
        QObject::connect(&settings, &AppSettings::renderBackendChanged, [&] { ++changes; });
        settings.setRenderBackend(QStringLiteral("gpu"));
        settings.setRenderBackend(QStringLiteral("gpu"));
        settings.setRenderBackend(QStringLiteral("vulkan")); // not a backend: ignored
        CHECK(settings.renderBackend() == QStringLiteral("gpu"));
        CHECK(changes == 1);
    }
    AppSettings reloaded;
    reloaded.load();
    CHECK(reloaded.renderBackend() == QStringLiteral("gpu"));
}

TEST_CASE("The 3D settings have defaults, are clamped and persist", "[app][settings][3d]")
{
    const IsolatedSettings isolated;
    {
        AppSettings settings;
        settings.load();
        CHECK(settings.depthScale() == AppSettings::kDefaultDepthScale);
        CHECK(settings.threshold() == AppSettings::kDefaultThreshold);
        CHECK(settings.opacity() == AppSettings::kDefaultOpacity);
        CHECK(settings.sampleSteps() == AppSettings::kDefaultSampleSteps);

        settings.setDepthScale(0.8);
        settings.setThreshold(0.4);
        settings.setOpacity(0.9);
        settings.setSampleSteps(512);
        settings.setDepthScale(50); // clamped
        CHECK(settings.depthScale() == 1.0);
        settings.setThreshold(-1);
        CHECK(settings.threshold() == 0.0);
        settings.setOpacity(0);
        CHECK(settings.opacity() > 0.0); // never fully transparent
        settings.setSampleSteps(100000);
        CHECK(settings.sampleSteps() == 1024);
        settings.setSampleSteps(1);
        CHECK(settings.sampleSteps() == 32);

        settings.setDepthScale(0.8);
        settings.setThreshold(0.4);
        settings.setOpacity(0.9);
        settings.setSampleSteps(512);
    }
    AppSettings reloaded;
    reloaded.load();
    CHECK(reloaded.depthScale() == 0.8);
    CHECK(reloaded.threshold() == 0.4);
    CHECK(reloaded.opacity() == 0.9);
    CHECK(reloaded.sampleSteps() == 512);
}

TEST_CASE("Reset 3D restores the defaults and notifies once per value", "[app][settings][3d]")
{
    const IsolatedSettings isolated;
    AppSettings settings;
    settings.setDepthScale(0.9);
    settings.setThreshold(0.7);
    settings.setOpacity(0.2);
    settings.setSampleSteps(64);

    int changes = 0;
    QObject::connect(&settings, &AppSettings::depthScaleChanged, [&] { ++changes; });
    QObject::connect(&settings, &AppSettings::thresholdChanged, [&] { ++changes; });
    QObject::connect(&settings, &AppSettings::opacityChanged, [&] { ++changes; });
    QObject::connect(&settings, &AppSettings::sampleStepsChanged, [&] { ++changes; });
    settings.resetView3D();

    CHECK(settings.depthScale() == AppSettings::kDefaultDepthScale);
    CHECK(settings.threshold() == AppSettings::kDefaultThreshold);
    CHECK(settings.opacity() == AppSettings::kDefaultOpacity);
    CHECK(settings.sampleSteps() == AppSettings::kDefaultSampleSteps);
    CHECK(changes == 4);
    settings.resetView3D(); // already default
    CHECK(changes == 4);
}
