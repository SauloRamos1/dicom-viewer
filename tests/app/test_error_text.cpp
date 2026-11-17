// SPDX-License-Identifier: GPL-3.0-only
#include <catch2/catch_test_macros.hpp>

#include "DicomError.h"
#include "ErrorText.h"

TEST_CASE("Every error code has a translated message", "[app][errors]")
{
    // Keep in step with dv::ErrorCode: SingleSliceSeries is its last enumerator.
    for (int i = 0; i <= static_cast<int>(dv::ErrorCode::SingleSliceSeries); ++i) {
        const auto code = static_cast<dv::ErrorCode>(i);
        INFO("code " << dv::toString(code));
        CHECK_FALSE(ErrorText::message(code).isEmpty());
        CHECK(ErrorText::message(code) != QStringLiteral("Unknown error"));
        CHECK_FALSE(ErrorText::sampleDetail(code).isEmpty());
    }
}

TEST_CASE("Severity keys", "[app][errors]")
{
    CHECK(ErrorText::severityKey(dv::Severity::Error) == QStringLiteral("error"));
    CHECK(ErrorText::severityKey(dv::Severity::Warning) == QStringLiteral("warning"));
    CHECK(ErrorText::severityKey(dv::Severity::Info) == QStringLiteral("info"));
}
