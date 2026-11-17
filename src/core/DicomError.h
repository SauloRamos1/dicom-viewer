// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace dv {

enum class ErrorCode {
    FileNotFound,
    FileUnreadable,
    NotDicom,
    NoPixelData,
    UnsupportedTransferSyntax,
    UnsupportedPhotometric,
    CorruptPixelData,
    DecodeFailed,
    VolumeInconsistent,
    FrameOutOfRange,
    GpuUnavailable, // app notice (Info severity): the shader path cannot run, the CPU path is used
    NoDicomFiles,   // a series folder holds nothing that opens as a DICOM image
    SingleSliceSeries, // app notice (Info severity): the series has one slice, shown as a plain image
    UnsupportedPixelFormat, // BitsAllocated/BitsStored this viewer cannot decode (e.g. 1-bit or 64-bit)
    VolumeTooLarge,  // the series would need more memory than the loader allows
    SkippedFiles,    // app notice (Warning severity): damaged files of a folder were left out of the series
};

enum class Severity { Info, Warning, Error };

// Error value carried by std::expected<T, DicomError>. `message` is the
// short, user-facing text (translated in the UI by code); `detail` is the
// technical text (e.g. the DCMTK OFCondition); `tag` is an optional DICOM tag
// such as "(7FE0,0010)".
struct DicomError {
    ErrorCode code{};
    Severity severity{Severity::Error};
    std::string message;
    std::string detail;
    std::optional<std::string> tag;
};

constexpr std::string_view toString(ErrorCode code) noexcept
{
    switch (code) {
    case ErrorCode::FileNotFound: return "FileNotFound";
    case ErrorCode::FileUnreadable: return "FileUnreadable";
    case ErrorCode::NotDicom: return "NotDicom";
    case ErrorCode::NoPixelData: return "NoPixelData";
    case ErrorCode::UnsupportedTransferSyntax: return "UnsupportedTransferSyntax";
    case ErrorCode::UnsupportedPhotometric: return "UnsupportedPhotometric";
    case ErrorCode::CorruptPixelData: return "CorruptPixelData";
    case ErrorCode::DecodeFailed: return "DecodeFailed";
    case ErrorCode::VolumeInconsistent: return "VolumeInconsistent";
    case ErrorCode::FrameOutOfRange: return "FrameOutOfRange";
    case ErrorCode::GpuUnavailable: return "GpuUnavailable";
    case ErrorCode::NoDicomFiles: return "NoDicomFiles";
    case ErrorCode::SingleSliceSeries: return "SingleSliceSeries";
    case ErrorCode::UnsupportedPixelFormat: return "UnsupportedPixelFormat";
    case ErrorCode::VolumeTooLarge: return "VolumeTooLarge";
    case ErrorCode::SkippedFiles: return "SkippedFiles";
    }
    return "Unknown";
}

// Short English message for each code; the UI translates by code.
constexpr std::string_view defaultMessage(ErrorCode code) noexcept
{
    switch (code) {
    case ErrorCode::FileNotFound: return "File not found";
    case ErrorCode::FileUnreadable: return "File cannot be read";
    case ErrorCode::NotDicom: return "Not a valid DICOM file";
    case ErrorCode::NoPixelData: return "File has no pixel data";
    case ErrorCode::UnsupportedTransferSyntax: return "Unsupported transfer syntax";
    case ErrorCode::UnsupportedPhotometric: return "Unsupported photometric interpretation";
    case ErrorCode::CorruptPixelData: return "Pixel data is corrupt or incomplete";
    case ErrorCode::DecodeFailed: return "Image could not be decoded";
    case ErrorCode::VolumeInconsistent: return "Slices do not form a consistent volume";
    case ErrorCode::FrameOutOfRange: return "Frame index out of range";
    case ErrorCode::GpuUnavailable: return "GPU rendering unavailable, using CPU";
    case ErrorCode::NoDicomFiles: return "No DICOM files found";
    case ErrorCode::SingleSliceSeries: return "Series has a single slice";
    case ErrorCode::UnsupportedPixelFormat: return "Unsupported pixel format";
    case ErrorCode::VolumeTooLarge: return "Volume is too large to load";
    case ErrorCode::SkippedFiles: return "Some files were skipped";
    }
    return "Unknown error";
}

// Builds an Error-severity DicomError with the default message for `code`.
inline DicomError makeError(ErrorCode code, std::string detail = {},
                            std::optional<std::string> tag = std::nullopt)
{
    return DicomError{.code = code,
                      .message = std::string(defaultMessage(code)),
                      .detail = std::move(detail),
                      .tag = std::move(tag)};
}

} // namespace dv
