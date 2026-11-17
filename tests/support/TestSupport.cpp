#include "TestSupport.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcxfer.h>

#ifdef _WIN32
#include <process.h>
#define DV_GETPID _getpid
#else
#include <unistd.h>
#define DV_GETPID getpid
#endif

namespace dv::test {

std::filesystem::path dataFile(std::string_view name)
{
    return std::filesystem::path(DICOM_TEST_DATA_DIR) / name;
}

TempDir::TempDir()
{
    static std::atomic<int> counter{0};
    path_ = std::filesystem::temp_directory_path()
        / ("dicomviewer_test_" + std::to_string(DV_GETPID()) + "_" + std::to_string(counter++));
    std::filesystem::remove_all(path_);
    std::filesystem::create_directories(path_);
}

TempDir::~TempDir()
{
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void writeBytes(const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
{
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void rewrite(const std::filesystem::path& src, const std::filesystem::path& dst,
             const std::function<void(DcmDataset&)>& edit)
{
    DcmFileFormat file;
    if (file.loadFile(src.string().c_str()).bad())
        throw std::runtime_error("cannot load " + src.string());
    edit(*file.getDataset());
    if (file.saveFile(dst.string().c_str(), EXS_LittleEndianExplicit).bad())
        throw std::runtime_error("cannot save " + dst.string());
}

std::size_t pixelDataOffset(std::span<const std::uint8_t> bytes)
{
    static constexpr std::array<std::uint8_t, 4> tag{0xE0, 0x7F, 0x10, 0x00};
    const auto it = std::ranges::search(bytes, tag).begin();
    if (it == bytes.end())
        throw std::runtime_error("no pixel data element");
    return static_cast<std::size_t>(it - bytes.begin()) + 12; // tag + VR + reserved + 32-bit length
}

} // namespace dv::test
