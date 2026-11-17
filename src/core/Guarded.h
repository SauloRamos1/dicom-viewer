#pragma once

// Private to src/core: boundary helper that turns any exception thrown by
// DCMTK or the standard library into a DicomError, so none escapes the core.

#include <exception>
#include <expected>

#include "DicomError.h"

namespace dv {

template <typename Result, typename Fn>
[[nodiscard]] std::expected<Result, DicomError> guarded(Fn&& fn)
{
    try {
        return fn();
    } catch (const std::exception& e) {
        return std::unexpected(makeError(ErrorCode::DecodeFailed, e.what()));
    } catch (...) {
        return std::unexpected(makeError(ErrorCode::DecodeFailed, "unknown exception"));
    }
}

} // namespace dv
