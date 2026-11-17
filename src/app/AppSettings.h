// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// Persisted UI preferences (QSettings). `theme` is "system" on first run, then
// "light" or "dark" once the user toggles it. `lastFolder` is where the Open
// dialog starts: the folder of the last opened file (Documents on first run).
// `smoothing` is the 2D viewport's interpolation. `renderBackend` is the 2D
// render path the user chose, "cpu" (default) or "gpu"; the controller may run
// on the CPU anyway when the GPU is unavailable. The Options pane is remembered
// as `optionsOpen` plus its position as fractions (0..1) of the room it has to
// move in inside the viewer: (1, 0) is the top-right corner. The 3D view's
// `depthScale` (heightmap relief height, fraction of the footprint), `threshold`
// (lowest windowed intensity a volume voxel shows, 0..1), `opacity` (volume
// density) and `sampleSteps` (ray-march samples across the volume) are kept too.
class AppSettings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QUrl lastFolder READ lastFolder WRITE setLastFolder NOTIFY lastFolderChanged)
    Q_PROPERTY(bool smoothing READ smoothing WRITE setSmoothing NOTIFY smoothingChanged)
    Q_PROPERTY(QString renderBackend READ renderBackend WRITE setRenderBackend NOTIFY renderBackendChanged)
    Q_PROPERTY(bool optionsOpen READ optionsOpen WRITE setOptionsOpen NOTIFY optionsOpenChanged)
    Q_PROPERTY(double optionsX READ optionsX WRITE setOptionsX NOTIFY optionsPositionChanged)
    Q_PROPERTY(double optionsY READ optionsY WRITE setOptionsY NOTIFY optionsPositionChanged)
    Q_PROPERTY(double depthScale READ depthScale WRITE setDepthScale NOTIFY depthScaleChanged)
    Q_PROPERTY(double threshold READ threshold WRITE setThreshold NOTIFY thresholdChanged)
    Q_PROPERTY(double opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(int sampleSteps READ sampleSteps WRITE setSampleSteps NOTIFY sampleStepsChanged)
    // Lower bounds (and the steps maximum) of the 3D controls.
    Q_PROPERTY(double minDepthScale READ minDepthScale CONSTANT)
    Q_PROPERTY(double minOpacity READ minOpacity CONSTANT)
    Q_PROPERTY(int minSampleSteps READ minSampleSteps CONSTANT)
    Q_PROPERTY(int maxSampleSteps READ maxSampleSteps CONSTANT)

public:
    static constexpr double kDefaultDepthScale = 0.3;
    static constexpr double kDefaultThreshold = 0.1;
    static constexpr double kDefaultOpacity = 0.4;
    static constexpr int kDefaultSampleSteps = 256;
    // Ranges of the controls (the setters clamp to them).
    static constexpr double kMinDepthScale = 0.05;
    static constexpr double kMinOpacity = 0.02;
    static constexpr int kMinSampleSteps = 32;
    static constexpr int kMaxSampleSteps = 1024;

    explicit AppSettings(QObject *parent = nullptr);

    QString theme() const;
    void setTheme(const QString &theme);
    QUrl lastFolder() const;
    void setLastFolder(const QUrl &folder);
    bool smoothing() const { return m_smoothing; }
    void setSmoothing(bool smoothing);
    QString renderBackend() const { return m_renderBackend; }
    void setRenderBackend(const QString &backend);
    bool optionsOpen() const { return m_optionsOpen; }
    void setOptionsOpen(bool open);
    double optionsX() const { return m_optionsX; }
    void setOptionsX(double x);
    double optionsY() const { return m_optionsY; }
    void setOptionsY(double y);
    double depthScale() const { return m_depthScale; }
    void setDepthScale(double scale);
    double threshold() const { return m_threshold; }
    void setThreshold(double threshold);
    double opacity() const { return m_opacity; }
    void setOpacity(double opacity);
    int sampleSteps() const { return m_sampleSteps; }
    void setSampleSteps(int steps);

    double minDepthScale() const { return kMinDepthScale; }
    double minOpacity() const { return kMinOpacity; }
    int minSampleSteps() const { return kMinSampleSteps; }
    int maxSampleSteps() const { return kMaxSampleSteps; }

    // The four 3D values back to their defaults.
    Q_INVOKABLE void resetView3D();

    // Reads the persisted values (startup step).
    Q_INVOKABLE void load();

signals:
    void themeChanged();
    void lastFolderChanged();
    void smoothingChanged();
    void renderBackendChanged();
    void optionsOpenChanged();
    void optionsPositionChanged();
    void depthScaleChanged();
    void thresholdChanged();
    void opacityChanged();
    void sampleStepsChanged();

private:
    QString m_theme = QStringLiteral("system");
    QUrl m_lastFolder;
    bool m_smoothing = true;
    QString m_renderBackend = QStringLiteral("cpu");
    bool m_optionsOpen = false;
    double m_optionsX = 1.0;
    double m_optionsY = 0.0;
    double m_depthScale = kDefaultDepthScale;
    double m_threshold = kDefaultThreshold;
    double m_opacity = kDefaultOpacity;
    int m_sampleSteps = kDefaultSampleSteps;
};
