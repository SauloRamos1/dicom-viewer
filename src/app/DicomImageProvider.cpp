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
