#include "DicomImageProvider.h"

#include <cstring>

QImage toQImage(const dv::RenderedImage &rendered)
{
    if (rendered.width <= 0 || rendered.height <= 0
        || rendered.data.size() < static_cast<size_t>(rendered.width) * rendered.height * rendered.channels)
        return {};
    const QImage::Format format = rendered.channels == 1 ? QImage::Format_Grayscale8 : QImage::Format_RGB888;
    QImage image(rendered.width, rendered.height, format);
    if (image.isNull())
        return {};
    const size_t rowBytes = static_cast<size_t>(rendered.width) * rendered.channels;
    for (int y = 0; y < rendered.height; ++y)
        std::memcpy(image.scanLine(y), rendered.data.data() + y * rowBytes, rowBytes);
    return image;
}

void FrameStore::set(QImage image)
{
    const std::scoped_lock lock(m_mutex);
    m_image = std::move(image);
}

QImage FrameStore::get() const
{
    const std::scoped_lock lock(m_mutex);
    return m_image;
}

DicomImageProvider::DicomImageProvider(std::shared_ptr<FrameStore> store)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_store(std::move(store))
{
}

QImage DicomImageProvider::requestImage(const QString &, QSize *size, const QSize &)
{
    QImage image = m_store->get();
    if (image.isNull()) {
        image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
    }
    if (size)
        *size = image.size();
    return image;
}

ColormapImageProvider::ColormapImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage ColormapImageProvider::requestImage(const QString &id, QSize *size, const QSize &)
{
    bool ok = false;
    const int index = id.toInt(&ok);
    const auto colormap = ok && index >= 0 && index <= static_cast<int>(dv::Colormap::Jet)
        ? static_cast<dv::Colormap>(index)
        : dv::Colormap::Gray;
    const dv::ColorLut &lut = dv::colormapLut(colormap);
    QImage image(static_cast<int>(lut.size()), 1, QImage::Format_RGBA8888);
    auto *pixels = image.scanLine(0);
    for (size_t i = 0; i < lut.size(); ++i) {
        pixels[i * 4] = lut[i][0];
        pixels[i * 4 + 1] = lut[i][1];
        pixels[i * 4 + 2] = lut[i][2];
        pixels[i * 4 + 3] = 255;
    }
    if (size)
        *size = image.size();
    return image;
}
