#include <catch2/catch_test_macros.hpp>

#include <string>

#include "Codecs.h"

TEST_CASE("registerCodecs is idempotent", "[codecs]")
{
    dv::registerCodecs();
    REQUIRE_NOTHROW(dv::registerCodecs());

    dv::cleanupCodecs();
    REQUIRE_NOTHROW(dv::cleanupCodecs());

    // Registering again after a cleanup must work.
    REQUIRE_NOTHROW(dv::registerCodecs());
    dv::cleanupCodecs();
}

TEST_CASE("dcmtkVersion reports the linked DCMTK", "[codecs]")
{
    const std::string version = dv::dcmtkVersion();
    REQUIRE_FALSE(version.empty());
    REQUIRE(version.starts_with("3.7"));
}
