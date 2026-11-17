// SPDX-License-Identifier: GPL-3.0-only
#include "AppSettings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

namespace {
constexpr auto kThemeKey = "theme";
constexpr auto kLastFolderKey = "lastFolder";
constexpr auto kSmoothingKey = "smoothing";
constexpr auto kRenderBackendKey = "renderBackend";
constexpr auto kOptionsOpenKey = "optionsOpen";
constexpr auto kOptionsXKey = "optionsX";
constexpr auto kOptionsYKey = "optionsY";
constexpr auto kDepthScaleKey = "depthScale";
constexpr auto kThresholdKey = "threshold";
constexpr auto kOpacityKey = "opacity";
constexpr auto kSampleStepsKey = "sampleSteps";

bool isValidTheme(const QString &theme)
{
    return theme == u"system" || theme == u"light" || theme == u"dark";
}

bool isValidBackend(const QString &backend)
{
    return backend == u"cpu" || backend == u"gpu";
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

void AppSettings::setSmoothing(bool smoothing)
{
    if (smoothing == m_smoothing)
        return;
    m_smoothing = smoothing;
    QSettings().setValue(kSmoothingKey, m_smoothing);
    emit smoothingChanged();
}

void AppSettings::setRenderBackend(const QString &backend)
{
    if (!isValidBackend(backend) || backend == m_renderBackend)
        return;
    m_renderBackend = backend;
    QSettings().setValue(kRenderBackendKey, m_renderBackend);
    emit renderBackendChanged();
}

void AppSettings::setOptionsOpen(bool open)
{
    if (open == m_optionsOpen)
        return;
    m_optionsOpen = open;
    QSettings().setValue(kOptionsOpenKey, m_optionsOpen);
    emit optionsOpenChanged();
}

void AppSettings::setOptionsX(double x)
{
    x = std::clamp(x, 0.0, 1.0);
    if (x == m_optionsX)
        return;
    m_optionsX = x;
    QSettings().setValue(kOptionsXKey, m_optionsX);
    emit optionsPositionChanged();
}

void AppSettings::setOptionsY(double y)
{
    y = std::clamp(y, 0.0, 1.0);
    if (y == m_optionsY)
        return;
    m_optionsY = y;
    QSettings().setValue(kOptionsYKey, m_optionsY);
    emit optionsPositionChanged();
}

void AppSettings::setDepthScale(double scale)
{
    scale = std::clamp(scale, kMinDepthScale, 1.0);
    if (scale == m_depthScale)
        return;
    m_depthScale = scale;
    QSettings().setValue(kDepthScaleKey, m_depthScale);
    emit depthScaleChanged();
}

void AppSettings::setThreshold(double threshold)
{
    threshold = std::clamp(threshold, 0.0, 1.0);
    if (threshold == m_threshold)
        return;
    m_threshold = threshold;
    QSettings().setValue(kThresholdKey, m_threshold);
    emit thresholdChanged();
}

void AppSettings::setOpacity(double opacity)
{
    opacity = std::clamp(opacity, kMinOpacity, 1.0);
    if (opacity == m_opacity)
        return;
    m_opacity = opacity;
    QSettings().setValue(kOpacityKey, m_opacity);
    emit opacityChanged();
}

void AppSettings::setSampleSteps(int steps)
{
    steps = std::clamp(steps, kMinSampleSteps, kMaxSampleSteps);
    if (steps == m_sampleSteps)
        return;
    m_sampleSteps = steps;
    QSettings().setValue(kSampleStepsKey, m_sampleSteps);
    emit sampleStepsChanged();
}

void AppSettings::resetView3D()
{
    setDepthScale(kDefaultDepthScale);
    setThreshold(kDefaultThreshold);
    setOpacity(kDefaultOpacity);
    setSampleSteps(kDefaultSampleSteps);
}

void AppSettings::load()
{
    const QSettings stored;

    const QString folder = stored.value(kLastFolderKey).toString();
    if (!folder.isEmpty() && QDir(folder).exists()) {
        m_lastFolder = QUrl::fromLocalFile(folder);
        emit lastFolderChanged();
    }

    const QString theme = stored.value(kThemeKey).toString();
    if (isValidTheme(theme) && theme != m_theme) {
        m_theme = theme;
        emit themeChanged();
    }

    m_smoothing = stored.value(kSmoothingKey, m_smoothing).toBool();
    const QString backend = stored.value(kRenderBackendKey).toString();
    if (isValidBackend(backend))
        m_renderBackend = backend;
    m_optionsOpen = stored.value(kOptionsOpenKey, m_optionsOpen).toBool();
    m_optionsX = std::clamp(stored.value(kOptionsXKey, m_optionsX).toDouble(), 0.0, 1.0);
    m_optionsY = std::clamp(stored.value(kOptionsYKey, m_optionsY).toDouble(), 0.0, 1.0);
    m_depthScale = std::clamp(stored.value(kDepthScaleKey, m_depthScale).toDouble(), kMinDepthScale, 1.0);
    m_threshold = std::clamp(stored.value(kThresholdKey, m_threshold).toDouble(), 0.0, 1.0);
    m_opacity = std::clamp(stored.value(kOpacityKey, m_opacity).toDouble(), kMinOpacity, 1.0);
    m_sampleSteps = std::clamp(stored.value(kSampleStepsKey, m_sampleSteps).toInt(), kMinSampleSteps, kMaxSampleSteps);
    emit depthScaleChanged();
    emit thresholdChanged();
    emit opacityChanged();
    emit sampleStepsChanged();
    emit smoothingChanged();
    emit renderBackendChanged();
    emit optionsOpenChanged();
    emit optionsPositionChanged();
}
