#pragma once

#include <string>

namespace dv {

// Registers the DCMTK decoders for JPEG, JPEG-LS and RLE transfer syntaxes.
// Idempotent and thread-safe. JPEG 2000 is intentionally not supported.
void registerCodecs();

// Releases the decoder registrations. Safe to call when nothing is registered.
void cleanupCodecs();

// DCMTK version string, e.g. "3.7.1".
std::string dcmtkVersion();

} // namespace dv
