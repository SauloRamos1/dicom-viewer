#include "Volume.h"

#include <algorithm>
#include <optional>
#include <string>

#include "Guarded.h"

namespace dv {

namespace {

using Vec3 = std::array<double, 3>;

Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

double dot(const Vec3& a, const Vec3& b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// Millimetres between slices when positions do not tell: DICOM's own spacing
// attributes, then 1.
double nominalSliceSpacing(const ImageInfo& info)
{
    return info.spacingBetweenSlices.value_or(info.sliceThickness.value_or(1.0));
}

// {x (column), y (row), z}: Pixel Spacing is stored as {row, column}.
std::array<double, 3> spacingOf(const ImageInfo& info, double z)
{
    const auto& ps = info.pixelSpacing;
    return {ps ? (*ps)[1] : 1.0, ps ? (*ps)[0] : 1.0, z};
}

struct Slice {
    std::vector<float> values;
    std::optional<Vec3> position;
    std::optional<int> instance;
};

std::optional<Vec3> sliceNormalOf(const ImageInfo& info)
{
    if (!info.imageOrientation)
        return std::nullopt;
    const auto& iop = *info.imageOrientation;
    return cross({iop[0], iop[1], iop[2]}, {iop[3], iop[4], iop[5]});
}

// Orders slices by projection of Image Position (Patient) on the slice normal,
// else by Instance Number, else keeps the input order. Returns the projected
// positions when that ordering was used.
std::optional<std::vector<double>> sortSlices(std::vector<Slice>& slices, const std::optional<Vec3>& normal)
{
    const bool havePositions = normal.has_value()
        && std::ranges::all_of(slices, [](const Slice& s) { return s.position.has_value(); });
    if (havePositions) {
        const auto project = [&](const Slice& s) { return dot(*s.position, *normal); };
        std::ranges::stable_sort(slices, {}, project);
        std::vector<double> projected;
        for (const Slice& s : slices)
            projected.push_back(project(s));
        return projected;
    }
    if (std::ranges::all_of(slices, [](const Slice& s) { return s.instance.has_value(); }))
        std::ranges::stable_sort(slices, {}, [](const Slice& s) { return *s.instance; });
    return std::nullopt;
}

} // namespace

std::expected<Volume, DicomError> Volume::fromMultiFrame(const DicomFile& file)
{
    return guarded<Volume>([&]() -> std::expected<Volume, DicomError> {
        const ImageInfo& info = file.info();
        Volume volume{.width = info.columns,
                      .height = info.rows,
                      .depth = info.frameCount,
                      .spacing = spacingOf(info, nominalSliceSpacing(info))};
        for (int frame = 0; frame < info.frameCount; ++frame) {
            auto values = file.rawModalityValues(frame);
            if (!values)
                return std::unexpected(std::move(values.error()));
            volume.voxels.insert(volume.voxels.end(), values->begin(), values->end());
        }
        return volume;
    });
}

std::expected<Volume, DicomError> Volume::fromSeries(std::span<const std::filesystem::path> paths)
{
    return guarded<Volume>([&]() -> std::expected<Volume, DicomError> {
        std::optional<DicomError> firstError;
        std::optional<ImageInfo> reference; // first valid slice defines series, size and spacing
        std::vector<Slice> slices;

        for (const auto& path : paths) {
            auto file = DicomFile::load(path);
            if (!file) {
                if (!firstError)
                    firstError = std::move(file.error());
                continue;
            }
            const ImageInfo& info = file->info();
            if (reference && info.seriesInstanceUid != reference->seriesInstanceUid)
                continue;
            if (info.frameCount != 1) {
                return std::unexpected(makeError(ErrorCode::VolumeInconsistent,
                                                 path.filename().string() + " is a multi-frame file"));
            }
            if (reference && (info.rows != reference->rows || info.columns != reference->columns)) {
                return std::unexpected(makeError(
                    ErrorCode::VolumeInconsistent,
                    path.filename().string() + " is " + std::to_string(info.columns) + "x"
                        + std::to_string(info.rows) + ", expected " + std::to_string(reference->columns) + "x"
                        + std::to_string(reference->rows)));
            }
            auto values = file->rawModalityValues(0);
            if (!values)
                return std::unexpected(std::move(values.error()));
            if (!reference)
                reference = info;
            slices.push_back({.values = std::move(*values), .position = info.imagePosition, .instance = info.instanceNumber});
        }

        if (slices.empty()) {
            if (firstError)
                return std::unexpected(std::move(*firstError));
            return std::unexpected(makeError(ErrorCode::VolumeInconsistent, "no slices"));
        }

        const auto projected = sortSlices(slices, sliceNormalOf(*reference));
        const double sliceSpacing = projected && projected->size() > 1
            ? (projected->back() - projected->front()) / static_cast<double>(projected->size() - 1)
            : nominalSliceSpacing(*reference);

        Volume volume{.width = reference->columns,
                      .height = reference->rows,
                      .depth = static_cast<int>(slices.size()),
                      .spacing = spacingOf(*reference, sliceSpacing)};
        volume.voxels.reserve(slices.size() * slices.front().values.size());
        for (const Slice& slice : slices)
            volume.voxels.insert(volume.voxels.end(), slice.values.begin(), slice.values.end());
        return volume;
    });
}

} // namespace dv
