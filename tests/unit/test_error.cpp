// SPDX-License-Identifier: GPL-3.0-only
#include <catch2/catch_test_macros.hpp>

#include <expected>

#include "DicomError.h"

TEST_CASE("DicomError defaults to error severity without a tag", "[error]")
{
    const dv::DicomError error{.code = dv::ErrorCode::NotDicom, .message = "Not a DICOM file"};

    REQUIRE(error.severity == dv::Severity::Error);
    REQUIRE_FALSE(error.tag.has_value());
    REQUIRE(dv::toString(error.code) == "NotDicom");
}

TEST_CASE("DicomError travels through std::expected", "[error]")
{
    const std::expected<int, dv::DicomError> result = std::unexpected(
        dv::DicomError{.code = dv::ErrorCode::NoPixelData, .tag = "(7FE0,0010)"});

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == dv::ErrorCode::NoPixelData);
    REQUIRE(result.error().tag == "(7FE0,0010)");
}
