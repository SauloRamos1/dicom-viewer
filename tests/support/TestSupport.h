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

// Offset of the Pixel Data value in a raw Explicit VR Little Endian file, found
// by scanning for the (7FE0,0010) element header; independent of the code under test.
std::size_t pixelDataOffset(std::span<const std::uint8_t> bytes);

} // namespace dv::test
