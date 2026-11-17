// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdatset.h>

namespace dv::test {

// Path of a committed fixture in tests/data.
std::filesystem::path dataFile(std::string_view name);

// Unique scratch directory removed on destruction, for fixtures built at runtime.
class TempDir {
public:
    TempDir();
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const noexcept { return path_; }
    std::filesystem::path operator/(std::string_view name) const { return path_ / name; }

private:
    std::filesystem::path path_;
};

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path);
void writeBytes(const std::filesystem::path& path, std::span<const std::uint8_t> bytes);

// Loads an uncompressed DICOM file with DCMTK, lets `edit` modify its dataset
// and writes it to `dst` (Little Endian Explicit).
void rewrite(const std::filesystem::path& src, const std::filesystem::path& dst,
             const std::function<void(DcmDataset&)>& edit);

// A synthetic grayscale image. `samples` are the stored sample bit patterns
// (frame by frame, each row-major: rows * columns * frames of them); for signed data pass
// static_cast<std::uint16_t>(negative value).
struct MonoImageSpec {
    int rows{};
    int columns{};
    int frames{1};
    int bitsAllocated{16};
    int bitsStored{16};
    int pixelRepresentation{0};
    double slope{1.0};
    double intercept{0.0};
    const char* photometric{"MONOCHROME2"};
    std::vector<std::uint16_t> samples;
};

// Writes `spec` to `dst`, using MR_small.dcm as the carrier of the other attributes
// (no window in the file, so presets fall back to Min-Max).
void writeMonoImage(const std::filesystem::path& dst, const MonoImageSpec& spec);

// `count` single-frame slices of CT_small.dcm in `dir` (slice_000.dcm, ...), all in series
// `seriesUid`: slice i lies at z = 5 * i mm with Instance Number i + 1 and stored value 900 + 40 * i in
// pixel (0,0), so every slice shows as a different gray level there. Returns the paths in order.
std::vector<std::filesystem::path> writeCtSeries(const std::filesystem::path& dir, int count,
                                                 std::string_view seriesUid = "1.2.3.100");

// One multi-frame CT_small.dcm of `frames` frames (frame i has stored value 900 + 40 * i in pixel (0,0)).
void writeCtMultiFrame(const std::filesystem::path& dst, int frames);

// Offset of the Pixel Data value in a raw Explicit VR Little Endian file, found
// by scanning for the (7FE0,0010) element header; independent of the code under test.
std::size_t pixelDataOffset(std::span<const std::uint8_t> bytes);

} // namespace dv::test
