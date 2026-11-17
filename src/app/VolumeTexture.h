#pragma once

#include <QPointer>
#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/QQuick3DTextureData>

class DicomController;

// The volume of the open series or multi-frame file as one R16 3D texture
// (depth > 0 makes Quick3D bind it as a sampler3D). The controller packed and
// downsampled it off the GUI thread; this uploads that buffer once per volume.
// While there is no volume it is a 1x1x1 texture, so the material stays valid.
class VolumeTexture : public QQuick3DTextureData
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(DicomController *controller READ controller WRITE setController NOTIFY controllerChanged)

public:
    explicit VolumeTexture(QQuick3DObject *parent = nullptr);

    DicomController *controller() const { return m_controller; }
    void setController(DicomController *controller);

signals:
    void controllerChanged();

private:
    void rebuild();

    QPointer<DicomController> m_controller;
};

// The colormap of the controller as a 256x1 RGBA8 table (index = gray level),
// for the ray-march shader; Quick3D textures cannot use image providers.
class ColormapTexture : public QQuick3DTextureData
{
    Q_OBJECT
    QML_ELEMENT
    // Same values as DicomController::Colormap.
    Q_PROPERTY(int colormap READ colormap WRITE setColormap NOTIFY colormapChanged)

public:
    explicit ColormapTexture(QQuick3DObject *parent = nullptr);

    int colormap() const { return m_colormap; }
    void setColormap(int colormap);

signals:
    void colormapChanged();

private:
    void rebuild();

    int m_colormap = 0;
};
