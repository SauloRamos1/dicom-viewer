// SPDX-License-Identifier: GPL-3.0-only
#include "Volume.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <string>

#include "CheckedMath.h"
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

// Direction cosines and pixel spacing are compared at these absolute tolerances;
// slice positions at kPositionTolerance (millimetres) or 1% of the median step.
constexpr double kOrientationTolerance = 1e-3;
constexpr double kSpacingTolerance = 1e-3;
constexpr double kPositionTolerance = 0.01;
constexpr double kPositionRelativeTolerance = 0.01;

struct Slice {
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

template <typename Array>
bool sameWithin(const std::optional<Array>& a, const std::optional<Array>& b, double tolerance)
{
    if (a.has_value() != b.has_value())
        return false;
    return !a || std::ranges::equal(*a, *b, [&](double x, double y) { return std::abs(x - y) <= tolerance; });
}

std::unexpected<DicomError> inconsistent(std::string detail)
{
    return std::unexpected(makeError(ErrorCode::VolumeInconsistent, std::move(detail)));
}

std::string nameOf(const Slice& slice)
{
    return slice.file.path().filename().string();
}

// Sorted slice positions along the normal must be distinct and evenly spaced:
// the volume has a single slice spacing, so anything else would be resampled silently.
std::expected<void, DicomError> checkUniformSpacing(const std::vector<Slice>& slices, const std::vector<double>& positions)
{
    std::vector<double> steps;
    for (std::size_t i = 1; i < positions.size(); ++i) {
        const double step = positions[i] - positions[i - 1];
        if (step <= kPositionTolerance)
            return inconsistent(nameOf(slices[i - 1]) + " and " + nameOf(slices[i]) + " are at the same position");
        steps.push_back(step);
    }
    if (steps.empty())
        return {};
    std::ranges::sort(steps);
    const double median = steps.size() % 2 == 1 ? steps[steps.size() / 2]
                                                 : (steps[steps.size() / 2 - 1] + steps[steps.size() / 2]) / 2;
    const double tolerance = std::max(kPositionTolerance, kPositionRelativeTolerance * median);
    for (std::size_t i = 1; i < positions.size(); ++i) {
        const double step = positions[i] - positions[i - 1];
        if (std::abs(step - median) > tolerance) {
            return inconsistent("slice spacing is not uniform: " + std::to_string(step) + " mm between "
                                + nameOf(slices[i - 1]) + " and " + nameOf(slices[i]) + ", expected "
                                + std::to_string(median) + " mm");
        }
    }
    return {};
}

} // namespace

std::expected<void, DicomError> checkVolumeSize(int width, int height, int depth)
{
    const auto bytes = checkedProduct({static_cast<std::size_t>(std::max(width, 0)), static_cast<std::size_t>(std::max(height, 0)),
                                       static_cast<std::size_t>(std::max(depth, 0)), sizeof(float)});
    if (!bytes || *bytes > kMaxVolumeBytes) {
        return std::unexpected(makeError(ErrorCode::VolumeTooLarge,
                                         std::to_string(width) + "x" + std::to_string(height) + "x" + std::to_string(depth)
                                             + " voxels exceed the " + std::to_string(kMaxVolumeBytes >> 20) + " MiB limit"));
    }
    return {};
}

std::expected<Volume, DicomError> Volume::fromMultiFrame(const DicomFile& file)
{
    return guarded<Volume>([&]() -> std::expected<Volume, DicomError> {
        const ImageInfo& info = file.info();
        if (auto fits = checkVolumeSize(info.columns, info.rows, info.frameCount); !fits)
            return std::unexpected(std::move(fits.error()));
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
        std::vector<SkippedFile> skipped;
        for (const auto& path : paths) {
            if (cancelled())
                return cancelledError();
            auto file = DicomFile::load(path);
            if (file) {
                files.push_back(*std::move(file));
                continue;
            }
            if (!firstError)
                firstError = file.error();
            // Files that are not images at all (text, DICOMDIR, reports) are not worth a notice; damaged images are.
            if (file.error().code != ErrorCode::NotDicom && file.error().code != ErrorCode::NoPixelData)
                skipped.push_back({.path = path, .error = std::move(file.error())});
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
            if (reference && !sameWithin(info.imageOrientation, reference->imageOrientation, kOrientationTolerance))
                return inconsistent(name + " has a different Image Orientation (Patient)");
            if (reference && !sameWithin(info.pixelSpacing, reference->pixelSpacing, kSpacingTolerance))
                return inconsistent(name + " has a different Pixel Spacing");
            if (!reference)
                reference = info;
            slices.push_back({.file = file, .position = info.imagePosition, .instance = info.instanceNumber});
        }

        if (auto fits = checkVolumeSize(reference->columns, reference->rows, static_cast<int>(slices.size())); !fits)
            return std::unexpected(std::move(fits.error()));
        const auto projected = sortSlices(slices, sliceNormalOf(*reference));
        if (projected) {
            if (auto uniform = checkUniformSpacing(slices, *projected); !uniform)
                return std::unexpected(std::move(uniform.error()));
        }
        const double sliceSpacing = projected && projected->size() > 1
            ? (projected->back() - projected->front()) / static_cast<double>(projected->size() - 1)
            : nominalSliceSpacing(*reference);

        Series series{.volume = {.width = reference->columns,
                                 .height = reference->rows,
                                 .depth = static_cast<int>(slices.size()),
                                 .spacing = spacingOf(*reference, sliceSpacing)}};
        series.skipped = std::move(skipped);
        series.volume.voxels.reserve(slices.size() * static_cast<std::size_t>(reference->rows)
                                     * static_cast<std::size_t>(reference->columns));
        for (Slice& slice : slices) {
            if (cancelled())
                return cancelledError();
            auto values = slice.file.rawModalityValues(0);
            if (!values)
                return std::unexpected(std::move(values.error()));
            series.volume.voxels.insert(series.volume.voxels.end(), values->begin(), values->end());
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
