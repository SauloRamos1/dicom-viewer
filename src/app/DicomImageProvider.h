#pragma once

#include <QImage>
#include <QQuickImageProvider>

#include <memory>
#include <mutex>

#include "Windowing.h"

// Converts a rendered frame to a QImage (Grayscale8 or RGB888) that owns its
// pixels.
QImage toQImage(const dv::RenderedImage &rendered);

// Latest rendered frame, shared between the controller (writer, GUI thread) and
// the image provider (reader, QML image thread). QImage is reference counted
// and never modified after being stored, so handing out copies is safe.
class FrameStore
{
public:
    void set(QImage image);
    QImage get() const;

private:
    mutable std::mutex m_mutex;
    QImage m_image;
};

// Serves the current frame as image://dicom/<revision>. The revision in the URL
// only busts the QML pixmap cache; the image is always the latest one. When
// there is no frame it returns a 1x1 transparent image (QML hides the Image in
// that case anyway).
class DicomImageProvider : public QQuickImageProvider
{
public:
    explicit DicomImageProvider(std::shared_ptr<FrameStore> store);

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    std::shared_ptr<FrameStore> m_store;
};

// Serves a colormap as image://dicomlut/<colormap index>: a 256x1 RGBA look-up
// table (indexed by gray level) that the GPU shader samples. Unknown ids give
// the gray table. The image is opaque (alpha 255) so Qt Quick uploads it as is.
class ColormapImageProvider : public QQuickImageProvider
{
public:
    ColormapImageProvider();

    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
