#include "ErrorText.h"

QString ErrorText::message(dv::ErrorCode code)
{
    switch (code) {
    case dv::ErrorCode::FileNotFound: return tr("File not found");
    case dv::ErrorCode::FileUnreadable: return tr("The file cannot be read");
    case dv::ErrorCode::NotDicom: return tr("Not a valid DICOM file");
    case dv::ErrorCode::NoPixelData: return tr("The file has no pixel data");
    case dv::ErrorCode::UnsupportedTransferSyntax: return tr("Unsupported transfer syntax");
    case dv::ErrorCode::UnsupportedPhotometric: return tr("Unsupported photometric interpretation");
    case dv::ErrorCode::CorruptPixelData: return tr("The pixel data is corrupt or incomplete");
    case dv::ErrorCode::DecodeFailed: return tr("The image could not be decoded");
    case dv::ErrorCode::VolumeInconsistent: return tr("The slices do not form a consistent volume");
    case dv::ErrorCode::FrameOutOfRange: return tr("The frame index is out of range");
    case dv::ErrorCode::GpuUnavailable: return tr("GPU rendering unavailable, using CPU");
    case dv::ErrorCode::NoDicomFiles: return tr("No DICOM files were found in the folder");
    case dv::ErrorCode::SingleSliceSeries: return tr("The series has a single slice, shown as a plain image");
    }
    return tr("Unknown error");
}

QString ErrorText::sampleDetail(dv::ErrorCode code)
{
    switch (code) {
    case dv::ErrorCode::NotDicom: return tr("The DICM preamble is missing at offset 128.");
    case dv::ErrorCode::UnsupportedTransferSyntax:
        return tr("JPEG 2000 (1.2.840.10008.1.2.4.90) is not supported by this build.");
    case dv::ErrorCode::NoPixelData: return tr("The dataset contains no Pixel Data element.");
    default: return message(code);
    }
}

QString ErrorText::severityKey(dv::Severity severity)
{
    switch (severity) {
    case dv::Severity::Info: return QStringLiteral("info");
    case dv::Severity::Warning: return QStringLiteral("warning");
    case dv::Severity::Error: break;
    }
    return QStringLiteral("error");
}
