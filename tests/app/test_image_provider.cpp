#include <catch2/catch_test_macros.hpp>

#include <QSize>

#include "DicomImageProvider.h"

TEST_CASE("toQImage converts gray and RGB frames", "[app][provider]")
{
    dv::RenderedImage gray{.width = 3, .height = 2, .channels = 1, .data = {0, 10, 20, 30, 40, 50}};
    const QImage g = toQImage(gray);
    REQUIRE(g.format() == QImage::Format_Grayscale8);
    CHECK(g.size() == QSize(3, 2));
    CHECK(qGray(g.pixel(1, 0)) == 10);
    CHECK(qGray(g.pixel(2, 1)) == 50);

    dv::RenderedImage rgb{.width = 2, .height = 1, .channels = 3, .data = {255, 0, 0, 0, 0, 255}};
    const QImage c = toQImage(rgb);
    REQUIRE(c.format() == QImage::Format_RGB888);
    CHECK(c.pixelColor(0, 0) == QColor(255, 0, 0));
    CHECK(c.pixelColor(1, 0) == QColor(0, 0, 255));

    // The image owns its pixels.
    gray.data.assign(gray.data.size(), 99);
    CHECK(qGray(g.pixel(1, 0)) == 10);
}

TEST_CASE("toQImage rejects inconsistent buffers", "[app][provider]")
{
    CHECK(toQImage({.width = 2, .height = 2, .channels = 1, .data = {1, 2, 3}}).isNull());
    CHECK(toQImage({.width = 0, .height = 2, .channels = 1, .data = {}}).isNull());
}

TEST_CASE("DicomImageProvider serves the latest frame", "[app][provider]")
{
    auto store = std::make_shared<FrameStore>();
    DicomImageProvider provider(store);
    QSize size;

    SECTION("no frame yet gives a 1x1 transparent image")
    {
        const QImage image = provider.requestImage(QStringLiteral("0"), &size, {});
        CHECK(size == QSize(1, 1));
        CHECK(image.size() == QSize(1, 1));
        CHECK(image.pixelColor(0, 0).alpha() == 0);
    }

    SECTION("returns the stored frame whatever the revision in the URL")
    {
        QImage frame(4, 3, QImage::Format_Grayscale8);
        frame.fill(77);
        store->set(frame);

        const QImage image = provider.requestImage(QStringLiteral("17"), &size, {});
        CHECK(size == QSize(4, 3));
        CHECK(image.format() == QImage::Format_Grayscale8);
        CHECK(qGray(image.pixel(2, 1)) == 77);

        // Replacing the stored frame does not touch images already handed out.
        QImage next(2, 2, QImage::Format_RGB888);
        next.fill(Qt::red);
        store->set(next);
        CHECK(size == QSize(4, 3));
        CHECK(qGray(image.pixel(0, 0)) == 77);
        CHECK(provider.requestImage(QStringLiteral("18"), &size, {}).size() == QSize(2, 2));
    }

    SECTION("a null size pointer is accepted")
    {
        CHECK_FALSE(provider.requestImage(QStringLiteral("0"), nullptr, {}).isNull());
    }
}

TEST_CASE("ColormapImageProvider serves each colormap as a 256x1 look-up table", "[app][provider][gpu]")
{
    ColormapImageProvider provider;
    for (int index = 0; index < 4; ++index) {
        QSize size;
        const QImage image = provider.requestImage(QString::number(index), &size, {});
        CAPTURE(index);
        REQUIRE(image.size() == QSize(256, 1));
        CHECK(size == QSize(256, 1));
        CHECK(image.format() == QImage::Format_RGBA8888);
        const auto &lut = dv::colormapLut(static_cast<dv::Colormap>(index));
        for (int level : {0, 1, 100, 200, 255}) {
            const QColor pixel = image.pixelColor(level, 0);
            CHECK(pixel.red() == lut[static_cast<size_t>(level)][0]);
            CHECK(pixel.green() == lut[static_cast<size_t>(level)][1]);
            CHECK(pixel.blue() == lut[static_cast<size_t>(level)][2]);
            CHECK(pixel.alpha() == 255);
        }
    }

    // An unknown id falls back to gray.
    QSize size;
    const QImage fallback = provider.requestImage(QStringLiteral("nope"), &size, {});
    CHECK(fallback.pixelColor(77, 0).red() == 77);
}
