// SPDX-License-Identifier: GPL-3.0-only
#include "DicomFile.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcxfer.h>

#include "Codecs.h"
#include "DicomFileState.h"
#include "Guarded.h"

namespace dv {

std::string conditionText(const OFCondition& condition)
{
    return std::string(condition.text());
}

std::expected<void, DicomError> parseFile(const std::filesystem::path& path, DcmFileFormat& file)
{
    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
        return std::unexpected(makeError(ErrorCode::FileNotFound, path.string()));
    if (!std::filesystem::is_regular_file(path, ec) || !std::ifstream(path, std::ios::binary).good())
        return std::unexpected(makeError(ErrorCode::FileUnreadable, path.string()));

    const OFCondition status = file.loadFile(path.string().c_str());
    if (status == EC_UnsupportedEncoding) // deflated dataset: DCMTK is built without zlib
        return std::unexpected(makeError(ErrorCode::UnsupportedTransferSyntax, "Deflated Explicit VR Little Endian"));
    if (status.bad())
        return std::unexpected(makeError(ErrorCode::NotDicom, conditionText(status)));
    return {};
}

namespace {

// Transfer syntaxes this build can decode (see registerCodecs). Deflate is
// not listed: DCMTK is built without zlib.
bool isSupportedSyntax(E_TransferSyntax xfer)
{
    switch (xfer) {
    case EXS_LittleEndianImplicit:
    case EXS_LittleEndianExplicit:
    case EXS_BigEndianExplicit:
    case EXS_RLELossless:
    case EXS_JPEGProcess1:
    case EXS_JPEGProcess2_4:
    case EXS_JPEGProcess14:
    case EXS_JPEGProcess14SV1:
    case EXS_JPEGLSLossless:
    case EXS_JPEGLSLossy:
        return true;
    default:
        return false;
    }
}

Photometric parsePhotometric(const std::string& text)
{
    if (text == "MONOCHROME1") return Photometric::Monochrome1;
    if (text == "MONOCHROME2") return Photometric::Monochrome2;
    if (text == "RGB") return Photometric::Rgb;
    if (text == "YBR_FULL") return Photometric::YbrFull;
    if (text == "YBR_FULL_422") return Photometric::YbrFull422;
    if (text == "PALETTE COLOR") return Photometric::PaletteColor;
    return Photometric::Other;
}

std::string getString(DcmDataset& ds, const DcmTagKey& key)
{
    OFString value;
    return ds.findAndGetOFStringArray(key, value).good() ? std::string(value.c_str()) : std::string();
}

std::optional<double> getDouble(DcmDataset& ds, const DcmTagKey& key, unsigned long pos = 0)
{
    Float64 value = 0;
    if (ds.findAndGetFloat64(key, value, pos).good())
        return value;
    return std::nullopt;
}

template <std::size_t N>
std::optional<std::array<double, N>> getDoubles(DcmDataset& ds, const DcmTagKey& key)
{
    std::array<double, N> values{};
    for (std::size_t i = 0; i < N; ++i) {
        const auto v = getDouble(ds, key, i);
        if (!v)
            return std::nullopt;
        values[i] = *v;
    }
    return values;
}

int getUShort(DcmDataset& ds, const DcmTagKey& key, int fallback)
{
    Uint16 value = 0;
    return ds.findAndGetUint16(key, value).good() ? static_cast<int>(value) : fallback;
}

// For IS (integer string) elements such as Number of Frames.
int getIntString(DcmDataset& ds, const DcmTagKey& key, int fallback)
{
    Sint32 value = 0;
    return ds.findAndGetSint32(key, value).good() ? static_cast<int>(value) : fallback;
}

ImageInfo readInfo(DcmDataset& ds, E_TransferSyntax originalXfer)
{
    ImageInfo info;
    info.rows = getUShort(ds, DCM_Rows, 0);
    info.columns = getUShort(ds, DCM_Columns, 0);
    info.samplesPerPixel = getUShort(ds, DCM_SamplesPerPixel, 1);
    info.bitsAllocated = getUShort(ds, DCM_BitsAllocated, 0);
    info.bitsStored = getUShort(ds, DCM_BitsStored, info.bitsAllocated);
    info.pixelRepresentation = getUShort(ds, DCM_PixelRepresentation, 0);
    info.photometric = parsePhotometric(getString(ds, DCM_PhotometricInterpretation));
    info.frameCount = std::max(1, getIntString(ds, DCM_NumberOfFrames, 1));
    if (auto spacing = getDoubles<2>(ds, DCM_PixelSpacing))
        info.pixelSpacing = spacing;
    info.sliceThickness = getDouble(ds, DCM_SliceThickness);
    info.spacingBetweenSlices = getDouble(ds, DCM_SpacingBetweenSlices);
    info.imageOrientation = getDoubles<6>(ds, DCM_ImageOrientationPatient);
    info.imagePosition = getDoubles<3>(ds, DCM_ImagePositionPatient);
    if (Sint32 n = 0; ds.findAndGetSint32(DCM_InstanceNumber, n).good())
        info.instanceNumber = static_cast<int>(n);
    info.rescaleSlope = getDouble(ds, DCM_RescaleSlope).value_or(1.0);
    info.rescaleIntercept = getDouble(ds, DCM_RescaleIntercept).value_or(0.0);
    const auto center = getDouble(ds, DCM_WindowCenter);
    const auto width = getDouble(ds, DCM_WindowWidth);
    if (center && width)
        info.defaultVoi = Voi{*center, *width};

    const DcmXfer xfer(originalXfer);
    info.transferSyntaxUid = xfer.getXferID();
    info.transferSyntaxName = xfer.getXferName();
    info.modality = getString(ds, DCM_Modality);
    info.patientName = getString(ds, DCM_PatientName);
    info.patientId = getString(ds, DCM_PatientID);
    info.studyDescription = getString(ds, DCM_StudyDescription);
    info.seriesDescription = getString(ds, DCM_SeriesDescription);
    info.studyDate = getString(ds, DCM_StudyDate);
    info.studyInstanceUid = getString(ds, DCM_StudyInstanceUID);
    info.seriesInstanceUid = getString(ds, DCM_SeriesInstanceUID);
    return info;
}

// Number of bytes an uncompressed (or decoded) pixel data element must hold.
std::size_t expectedPixelBytes(const ImageInfo& info)
{
    // YBR_FULL_422 stores chroma subsampled: two samples per pixel.
    const int samplesPerPixel = info.photometric == Photometric::YbrFull422 ? 2 : info.samplesPerPixel;
    const std::size_t samples = static_cast<std::size_t>(info.rows) * static_cast<std::size_t>(info.columns)
        * static_cast<std::size_t>(samplesPerPixel) * static_cast<std::size_t>(info.frameCount);
    return samples * static_cast<std::size_t>((info.bitsAllocated + 7) / 8);
}

std::expected<void, DicomError> prepare(const std::filesystem::path& path, DicomFileState& state,
                                        ImageInfo& info)
{
    if (auto parsed = parseFile(path, state.file); !parsed)
        return parsed;
    DcmDataset& ds = state.dataset();

    const E_TransferSyntax originalXfer = ds.getOriginalXfer();
    if (!isSupportedSyntax(originalXfer)) {
        const DcmXfer xfer(originalXfer);
        return std::unexpected(makeError(ErrorCode::UnsupportedTransferSyntax,
                                         std::string(xfer.getXferName()) + " (" + xfer.getXferID() + ")"));
    }
    if (!ds.tagExists(DCM_PixelData))
        return std::unexpected(makeError(ErrorCode::NoPixelData, {}, "(7FE0,0010)"));

    // Decode once so the dataset is plain uncompressed data from here on.
    if (DcmXfer(originalXfer).isPixelDataCompressed()) {
        if (const OFCondition status = ds.chooseRepresentation(EXS_LittleEndianExplicit, nullptr); status.bad())
            return std::unexpected(makeError(ErrorCode::DecodeFailed, conditionText(status), "(7FE0,0010)"));
    }

    info = readInfo(ds, originalXfer);
    if (info.rows <= 0 || info.columns <= 0 || info.bitsAllocated <= 0 || info.bitsStored <= 0
        || info.bitsStored > info.bitsAllocated) {
        return std::unexpected(
            makeError(ErrorCode::CorruptPixelData, "Missing or invalid Rows, Columns, BitsAllocated or BitsStored"));
    }

    DcmElement* pixelData = nullptr;
    if (ds.findAndGetElement(DCM_PixelData, pixelData).bad() || pixelData == nullptr
        || pixelData->getLength(ds.getCurrentXfer()) < expectedPixelBytes(info)) {
        return std::unexpected(
            makeError(ErrorCode::CorruptPixelData, "Pixel data is shorter than the image size", "(7FE0,0010)"));
    }
    return {};
}

} // namespace

std::expected<DicomFile, DicomError> DicomFile::load(const std::filesystem::path& path)
{
    return guarded<DicomFile>([&]() -> std::expected<DicomFile, DicomError> {
        registerCodecs();
        DicomFile result;
        result.path_ = path;
        result.state_ = std::make_shared<DicomFileState>();
        if (auto ready = prepare(path, *result.state_, result.info_); !ready)
            return std::unexpected(std::move(ready.error()));
        return result;
    });
}

} // namespace dv
