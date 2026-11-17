// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QPointer>
#include <QtQml/qqmlregistration.h>
#include <QtQuick3D/QQuick3DGeometry>

class DicomController;

// The relief mesh of the displayed frame (dv::buildHeightmap, built by the
// controller off the GUI thread) as Quick3D geometry: positions, normals and
// vertex colours, indexed triangles. It follows `controller`'s heightmap and
// is empty while there is none. Heights span +-50 around y = 0 at scale 1, so a
// Model's y scale is the depth scale.
class HeightmapGeometry : public QQuick3DGeometry
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(DicomController *controller READ controller WRITE setController NOTIFY controllerChanged)

public:
    explicit HeightmapGeometry(QQuick3DObject *parent = nullptr);

    DicomController *controller() const { return m_controller; }
    void setController(DicomController *controller);

signals:
    void controllerChanged();

private:
    void rebuild();

    QPointer<DicomController> m_controller;
};
