// SPDX-License-Identifier: GPL-3.0-only
#include "Volume.h"

#include <algorithm>
#include <map>
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
    DicomFile file;
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

std::expected<Series, DicomError> loadSeries(std::span<const std::filesystem::path> paths, const std::atomic<bool>* cancel)
{
    return guarded<Series>([&]() -> std::expected<Series, DicomError> {
        const auto cancelled = [&] { return cancel != nullptr && cancel->load(); };
        const auto cancelledError = [] { return std::unexpected(makeError(ErrorCode::DecodeFailed, "cancelled")); };

        std::optional<DicomError> firstError;
        std::vector<DicomFile> files;
        for (const auto& path : paths) {
            if (cancelled())
                return cancelledError();
            auto file = DicomFile::load(path);
            if (file)
                files.push_back(*std::move(file));
            else if (!firstError)
                firstError = std::move(file.error());
        }

        if (files.empty()) {
            // A folder of other file types is "no DICOM files", not a NotDicom error of its first file.
            if (firstError && firstError->code != ErrorCode::NotDicom)
                return std::unexpected(std::move(*firstError));
            return std::unexpected(makeError(ErrorCode::NoDicomFiles));
        }

        // The series with the most images; the first one met wins a tie.
        std::map<std::string, int> counts;
        std::string seriesUid = files.front().info().seriesInstanceUid;
        for (const DicomFile& file : files)
            ++counts[file.info().seriesInstanceUid];
        for (const DicomFile& file : files) {
            if (counts[file.info().seriesInstanceUid] > counts[seriesUid])
                seriesUid = file.info().seriesInstanceUid;
        }

        std::optional<ImageInfo> reference; // first slice of the series: size and spacing
        std::vector<Slice> slices;
        for (const DicomFile& file : files) {
            if (cancelled())
                return cancelledError();
            const ImageInfo& info = file.info();
            if (info.seriesInstanceUid != seriesUid)
                continue;
            const std::string name = file.path().filename().string();
            if (info.frameCount != 1)
                return std::unexpected(makeError(ErrorCode::VolumeInconsistent, name + " is a multi-frame file"));
            if (reference && (info.rows != reference->rows || info.columns != reference->columns)) {
                return std::unexpected(makeError(
                    ErrorCode::VolumeInconsistent,
                    name + " is " + std::to_string(info.columns) + "x" + std::to_string(info.rows) + ", expected "
                        + std::to_string(reference->columns) + "x" + std::to_string(reference->rows)));
            }
            auto values = file.rawModalityValues(0);
            if (!values)
                return std::unexpected(std::move(values.error()));
            if (!reference)
                reference = info;
            slices.push_back({.values = std::move(*values), .file = file, .position = info.imagePosition, .instance = info.instanceNumber});
        }

        const auto projected = sortSlices(slices, sliceNormalOf(*reference));
        const double sliceSpacing = projected && projected->size() > 1
            ? (projected->back() - projected->front()) / static_cast<double>(projected->size() - 1)
            : nominalSliceSpacing(*reference);

        Series series{.volume = {.width = reference->columns,
                                 .height = reference->rows,
                                 .depth = static_cast<int>(slices.size()),
                                 .spacing = spacingOf(*reference, sliceSpacing)}};
        series.volume.voxels.reserve(slices.size() * slices.front().values.size());
        for (Slice& slice : slices) {
            series.volume.voxels.insert(series.volume.voxels.end(), slice.values.begin(), slice.values.end());
            series.slices.push_back(std::move(slice.file));
        }
        return series;
    });
}

std::expected<Volume, DicomError> Volume::fromSeries(std::span<const std::filesystem::path> paths)
{
    auto series = loadSeries(paths);
    if (!series)
        return std::unexpected(std::move(series.error()));
    return std::move(series->volume);
}

} // namespace dv
