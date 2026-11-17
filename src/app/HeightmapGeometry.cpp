// SPDX-License-Identifier: GPL-3.0-only
#include "HeightmapGeometry.h"

#include "DicomController.h"

namespace {
constexpr int kFloatSize = static_cast<int>(sizeof(float));
constexpr int kStride = dv::kHeightmapVertexFloats * kFloatSize;
} // namespace

HeightmapGeometry::HeightmapGeometry(QQuick3DObject *parent)
    : QQuick3DGeometry(parent)
{
}

void HeightmapGeometry::setController(DicomController *controller)
{
    if (controller == m_controller)
        return;
    if (m_controller)
        disconnect(m_controller, &DicomController::heightmapChanged, this, nullptr);
    m_controller = controller;
    if (m_controller)
        connect(m_controller, &DicomController::heightmapChanged, this, &HeightmapGeometry::rebuild);
    rebuild();
    emit controllerChanged();
}

void HeightmapGeometry::rebuild()
{
    clear();
    const auto mesh = m_controller ? m_controller->heightmap() : nullptr;
    if (mesh && !mesh->vertices.empty()) {
        setPrimitiveType(PrimitiveType::Triangles);
        setStride(kStride);
        addAttribute(Attribute::PositionSemantic, 0, Attribute::F32Type);
        addAttribute(Attribute::NormalSemantic, 3 * kFloatSize, Attribute::F32Type);
        addAttribute(Attribute::ColorSemantic, 6 * kFloatSize, Attribute::F32Type);
        addAttribute(Attribute::IndexSemantic, 0, Attribute::U32Type);
        setBounds({mesh->boundsMin[0], mesh->boundsMin[1], mesh->boundsMin[2]},
                  {mesh->boundsMax[0], mesh->boundsMax[1], mesh->boundsMax[2]});
        // The geometry keeps its own copy: the mesh may be replaced while the
        // render thread still uploads this one.
        setVertexData(QByteArray(reinterpret_cast<const char *>(mesh->vertices.data()),
                                 static_cast<qsizetype>(mesh->vertices.size() * sizeof(float))));
        setIndexData(QByteArray(reinterpret_cast<const char *>(mesh->indices.data()),
                                static_cast<qsizetype>(mesh->indices.size() * sizeof(std::uint32_t))));
    }
    update();
}
