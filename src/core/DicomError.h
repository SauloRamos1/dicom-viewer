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
