// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Private to src/core: pulls in DCMTK. Public headers must not include this.

#include <expected>
#include <filesystem>
#include <mutex>
#include <string>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/ofstd/offile.h>

#include "DicomError.h"
#include "DicomFile.h"

namespace dv {

// Owns the parsed DCMTK object. DCMTK getters mutate internal caches (byte
// swapping, lazy value loading, representation changes), so every access goes
// through `mutex`.
struct DicomFileState {
    DcmFileFormat file;
    std::mutex mutex;

    DcmDataset& dataset() { return *file.getDataset(); }
};

// Existence/readability checks, then parse. Any failure is a DicomError.
[[nodiscard]] std::expected<void, DicomError> parseFile(const std::filesystem::path& path,
                                                        DcmFileFormat& file);

// DCMTK file name for `path`. On Windows, with DCMTK built with wide file I/O
// (see cmake/Dependencies.cmake), the UTF-16 name is passed so Unicode paths
// are not narrowed to the ANSI code page; elsewhere the native narrow name is UTF-8.
[[nodiscard]] inline OFFilename dcmtkFilename(const std::filesystem::path& path)
{
#if defined(_WIN32) && defined(WIDE_CHAR_FILE_IO_FUNCTIONS)
    return OFFilename(path.c_str(), OFTrue);
#else
    return OFFilename(path.string().c_str());
#endif
}

// FrameOutOfRange unless 0 <= frame < info.frameCount.
[[nodiscard]] inline std::expected<void, DicomError> checkFrame(const ImageInfo& info, int frame)
{
    if (frame < 0 || frame >= info.frameCount)
        return std::unexpected(makeError(ErrorCode::FrameOutOfRange,
                                         "frame " + std::to_string(frame) + " of " + std::to_string(info.frameCount)));
    return {};
}

// Formats a failed OFCondition as technical detail text.
[[nodiscard]] std::string conditionText(const OFCondition& condition);

} // namespace dv
