// SPDX-License-Identifier: GPL-3.0-only
#include "TestSupport.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcsequen.h>
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

void writeMonoImage(const std::filesystem::path& dst, const MonoImageSpec& spec)
{
    rewrite(dataFile("MR_small.dcm"), dst, [&](DcmDataset& ds) {
        const auto must = [](const OFCondition& condition) {
            if (condition.bad())
                throw std::runtime_error(condition.text());
        };
        must(ds.putAndInsertString(DCM_Modality, "OT")); // MR/CT would make DCMTK warn about the rescale
        must(ds.putAndInsertString(DCM_NumberOfFrames, std::to_string(spec.frames).c_str()));
        must(ds.putAndInsertUint16(DCM_Rows, static_cast<Uint16>(spec.rows)));
        must(ds.putAndInsertUint16(DCM_Columns, static_cast<Uint16>(spec.columns)));
        must(ds.putAndInsertUint16(DCM_BitsAllocated, static_cast<Uint16>(spec.bitsAllocated)));
        must(ds.putAndInsertUint16(DCM_BitsStored, static_cast<Uint16>(spec.bitsStored)));
        must(ds.putAndInsertUint16(DCM_HighBit, static_cast<Uint16>(spec.bitsStored - 1)));
        must(ds.putAndInsertUint16(DCM_PixelRepresentation, static_cast<Uint16>(spec.pixelRepresentation)));
        must(ds.putAndInsertString(DCM_PhotometricInterpretation, spec.photometric));
        must(ds.putAndInsertString(DCM_RescaleSlope, std::to_string(spec.slope).c_str()));
        must(ds.putAndInsertString(DCM_RescaleIntercept, std::to_string(spec.intercept).c_str()));
        ds.findAndDeleteElement(DCM_WindowCenter);
        ds.findAndDeleteElement(DCM_WindowWidth);
        if (spec.bitsAllocated == 8) {
            std::vector<Uint8> bytes(spec.samples.begin(), spec.samples.end());
            must(ds.putAndInsertUint8Array(DCM_PixelData, bytes.data(), bytes.size()));
        } else {
            must(ds.putAndInsertUint16Array(DCM_PixelData, spec.samples.data(), spec.samples.size()));
        }
    });
}

void writeModalityLutImage(const std::filesystem::path& dst, const std::vector<std::uint16_t>& samples,
                           const std::vector<std::uint16_t>& lut, int firstMapped, int pixelRepresentation)
{
    writeMonoImage(dst, {.rows = 1,
                         .columns = static_cast<int>(samples.size()),
                         .pixelRepresentation = pixelRepresentation,
                         .samples = samples});
    rewrite(dst, dst, [&](DcmDataset& ds) {
        DcmItem* item = nullptr;
        const std::array<Uint16, 3> descriptor{static_cast<Uint16>(lut.size()), static_cast<Uint16>(firstMapped), 16};
        if (ds.findOrCreateSequenceItem(DCM_ModalityLUTSequence, item, 0).bad()
            || item->putAndInsertUint16Array(DCM_LUTDescriptor, descriptor.data(), 3).bad()
            || item->putAndInsertUint16Array(DCM_LUTData, lut.data(), lut.size()).bad())
            throw std::runtime_error("cannot write the Modality LUT");
    });
}

void write32BitImage(const std::filesystem::path& dst, const std::vector<std::uint32_t>& samples)
{
    rewrite(dataFile("MR_small.dcm"), dst, [&](DcmDataset& ds) {
        std::vector<Uint16> words; // little endian: low word first
        for (const std::uint32_t sample : samples) {
            words.push_back(static_cast<Uint16>(sample & 0xFFFFU));
            words.push_back(static_cast<Uint16>(sample >> 16));
        }
        ds.putAndInsertUint16(DCM_Rows, 1);
        ds.putAndInsertUint16(DCM_Columns, static_cast<Uint16>(samples.size()));
        ds.putAndInsertString(DCM_NumberOfFrames, "1");
        ds.putAndInsertUint16(DCM_BitsAllocated, 32);
        ds.putAndInsertUint16(DCM_BitsStored, 32);
        ds.putAndInsertUint16(DCM_HighBit, 31);
        ds.putAndInsertUint16(DCM_PixelRepresentation, 0);
        ds.putAndInsertString(DCM_RescaleSlope, "1");
        ds.putAndInsertString(DCM_RescaleIntercept, "0");
        ds.findAndDeleteElement(DCM_WindowCenter);
        ds.findAndDeleteElement(DCM_WindowWidth);
        ds.putAndInsertUint16Array(DCM_PixelData, words.data(), words.size());
    });
}

namespace {

std::vector<Uint16> ctPixels(const DcmDataset& ds)
{
    const Uint16* pixels = nullptr;
    unsigned long count = 0;
    if (const_cast<DcmDataset&>(ds).findAndGetUint16Array(DCM_PixelData, pixels, &count).bad())
        throw std::runtime_error("CT_small.dcm has no 16-bit pixel data");
    return {pixels, pixels + count};
}

} // namespace

std::vector<std::filesystem::path> writeCtSeries(const std::filesystem::path& dir, int count, std::string_view seriesUid)
{
    std::vector<std::filesystem::path> paths;
    for (int i = 0; i < count; ++i) {
        std::string name = std::to_string(i);
        name = "slice_" + std::string(3 - std::min<std::size_t>(3, name.size()), '0') + name + ".dcm";
        rewrite(dataFile("CT_small.dcm"), dir / name, [&](DcmDataset& ds) {
            ds.putAndInsertString(DCM_ImageOrientationPatient, "1\\0\\0\\0\\1\\0");
            ds.putAndInsertString(DCM_ImagePositionPatient, ("0\\0\\" + std::to_string(5 * i)).c_str());
            ds.putAndInsertString(DCM_InstanceNumber, std::to_string(i + 1).c_str());
            ds.putAndInsertString(DCM_SeriesInstanceUID, std::string(seriesUid).c_str());
            std::vector<Uint16> pixels = ctPixels(ds);
            pixels[0] = static_cast<Uint16>(900 + 40 * i);
            ds.putAndInsertUint16Array(DCM_PixelData, pixels.data(), pixels.size());
        });
        paths.push_back(dir / name);
    }
    return paths;
}

void writeCtMultiFrame(const std::filesystem::path& dst, int frames)
{
    rewrite(dataFile("CT_small.dcm"), dst, [&](DcmDataset& ds) {
        const std::vector<Uint16> frame = ctPixels(ds);
        std::vector<Uint16> all;
        for (int i = 0; i < frames; ++i) {
            all.insert(all.end(), frame.begin(), frame.end());
            all[all.size() - frame.size()] = static_cast<Uint16>(900 + 40 * i);
        }
        ds.putAndInsertUint16Array(DCM_PixelData, all.data(), all.size());
        ds.putAndInsertString(DCM_NumberOfFrames, std::to_string(frames).c_str());
    });
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
