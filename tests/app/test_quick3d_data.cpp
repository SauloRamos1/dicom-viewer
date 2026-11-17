// The Quick3D data providers (heightmap geometry, volume and colormap textures)
// fed by the controller. They need no render backend: only their data is checked.

#include <catch2/catch_test_macros.hpp>

#include "AppTestSupport.h"
#include "DicomController.h"
#include "HeightmapGeometry.h"
#include "TestSupport.h"
#include "VolumeTexture.h"
#include "Windowing.h"

using namespace dv::test;

TEST_CASE("The heightmap geometry mirrors the controller's mesh", "[app][3d][quick3d]")
{
    DicomController controller;
    HeightmapGeometry geometry;
    geometry.setController(&controller);
    CHECK(geometry.vertexData().isEmpty());

    controller.open(urlOf(dataFile("CT_small.dcm")));
    REQUIRE(pumpUntil([&] { return !controller.busy(); }));
    controller.setView3D(true);
    REQUIRE(pumpUntil([&] { return !geometry.vertexData().isEmpty(); }));

    CHECK(geometry.stride() == 40);
    CHECK(geometry.primitiveType() == QQuick3DGeometry::PrimitiveType::Triangles);
    CHECK(geometry.vertexData().size() == 128 * 128 * 40);
    CHECK(geometry.indexData().size() == 127 * 127 * 6 * 4);
    REQUIRE(geometry.attributeCount() == 4);
    CHECK(geometry.attribute(0).semantic == QQuick3DGeometry::Attribute::PositionSemantic);
    CHECK(geometry.attribute(1).semantic == QQuick3DGeometry::Attribute::NormalSemantic);
    CHECK(geometry.attribute(1).offset == 12);
    CHECK(geometry.attribute(2).semantic == QQuick3DGeometry::Attribute::ColorSemantic);
    CHECK(geometry.attribute(2).offset == 24);
    CHECK(geometry.attribute(3).semantic == QQuick3DGeometry::Attribute::IndexSemantic);
    CHECK(geometry.attribute(3).componentType == QQuick3DGeometry::Attribute::U32Type);
    CHECK(geometry.boundsMin().y() == -dv::kHeightmapExtent / 2);
    CHECK(geometry.boundsMax().y() == dv::kHeightmapExtent / 2);

    controller.setView3D(false); // the mesh is dropped, so is the geometry
    CHECK(geometry.vertexData().isEmpty());
    CHECK(geometry.indexData().isEmpty());
}

TEST_CASE("The volume texture is a 1x1x1 placeholder, then the packed volume", "[app][3d][quick3d]")
{
    DicomController controller;
    VolumeTexture texture;
    texture.setController(&controller);
    CHECK(texture.format() == QQuick3DTextureData::R16);
    CHECK(texture.size() == QSize(1, 1));
    CHECK(texture.depth() == 1);
    CHECK(texture.textureData().size() == 2);

    const TempDir dir;
    writeCtSeries(dir.path(), 5);
    controller.openSeries(urlOf(dir.path()));
    REQUIRE(pumpUntil([&] { return !controller.busy(); }));

    CHECK(texture.format() == QQuick3DTextureData::R16);
    CHECK(texture.size() == QSize(128, 128));
    CHECK(texture.depth() == 5);
    CHECK(texture.textureData() == controller.volumeData()->bytes);

    controller.reset();
    CHECK(texture.size() == QSize(1, 1));
    CHECK(texture.depth() == 1);
}

TEST_CASE("The colormap texture is the 256 entry table of the colormap", "[app][3d][quick3d]")
{
    ColormapTexture texture;
    CHECK(texture.format() == QQuick3DTextureData::RGBA8);
    CHECK(texture.size() == QSize(256, 1));
    CHECK(texture.depth() == 0);

    for (const auto colormap : {dv::Colormap::Gray, dv::Colormap::Bone, dv::Colormap::Hot, dv::Colormap::Jet}) {
        texture.setColormap(static_cast<int>(colormap));
        const auto &lut = dv::colormapLut(colormap);
        const QByteArray data = texture.textureData();
        REQUIRE(data.size() == 256 * 4);
        for (const int i : {0, 1, 64, 128, 200, 255}) {
            CHECK(static_cast<quint8>(data[i * 4]) == lut[static_cast<size_t>(i)][0]);
            CHECK(static_cast<quint8>(data[i * 4 + 1]) == lut[static_cast<size_t>(i)][1]);
            CHECK(static_cast<quint8>(data[i * 4 + 2]) == lut[static_cast<size_t>(i)][2]);
            CHECK(static_cast<quint8>(data[i * 4 + 3]) == 255);
        }
    }
    texture.setColormap(99); // clamped to the last colormap
    CHECK(texture.colormap() == static_cast<int>(dv::Colormap::Jet));
}
