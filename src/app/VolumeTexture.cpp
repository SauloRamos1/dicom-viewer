#include "VolumeTexture.h"

#include <algorithm>

#include "DicomController.h"
#include "Windowing.h"

VolumeTexture::VolumeTexture(QQuick3DObject *parent)
    : QQuick3DTextureData(parent)
{
    rebuild();
}

void VolumeTexture::setController(DicomController *controller)
{
    if (controller == m_controller)
        return;
    if (m_controller)
        disconnect(m_controller, &DicomController::volumeChanged, this, nullptr);
    m_controller = controller;
    if (m_controller)
        connect(m_controller, &DicomController::volumeChanged, this, &VolumeTexture::rebuild);
    rebuild();
    emit controllerChanged();
}

void VolumeTexture::rebuild()
{
    const auto volume = m_controller ? m_controller->volumeData() : nullptr;
    setFormat(Format::R16);
    setHasTransparency(false);
    if (volume) {
        setSize(QSize(volume->width, volume->height));
        setDepth(volume->depth);
        setTextureData(volume->bytes); // shares the buffer; the controller keeps it alive too
    } else {
        setSize(QSize(1, 1));
        setDepth(1);
        setTextureData(QByteArray(static_cast<qsizetype>(sizeof(quint16)), '\0'));
    }
}

ColormapTexture::ColormapTexture(QQuick3DObject *parent)
    : QQuick3DTextureData(parent)
{
    rebuild();
}

void ColormapTexture::setColormap(int colormap)
{
    colormap = std::clamp(colormap, 0, static_cast<int>(dv::Colormap::Jet));
    if (colormap == m_colormap)
        return;
    m_colormap = colormap;
    rebuild();
    emit colormapChanged();
}

void ColormapTexture::rebuild()
{
    const dv::ColorLut &lut = dv::colormapLut(static_cast<dv::Colormap>(m_colormap));
    QByteArray bytes(static_cast<qsizetype>(lut.size() * 4), Qt::Uninitialized);
    auto *out = reinterpret_cast<quint8 *>(bytes.data());
    for (size_t i = 0; i < lut.size(); ++i) {
        out[i * 4] = lut[i][0];
        out[i * 4 + 1] = lut[i][1];
        out[i * 4 + 2] = lut[i][2];
        out[i * 4 + 3] = 255;
    }
    setFormat(Format::RGBA8);
    setSize(QSize(static_cast<int>(lut.size()), 1));
    setDepth(0);
    setHasTransparency(false);
    setTextureData(bytes);
}
