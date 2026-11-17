// SPDX-License-Identifier: GPL-3.0-only
#include "Codecs.h"

#include <mutex>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcrledrg.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmjpeg/djdecode.h>
#include <dcmtk/dcmjpls/djdecode.h>

namespace dv {

namespace {
std::mutex g_mutex;
bool g_registered = false;
} // namespace

void registerCodecs()
{
    const std::lock_guard lock(g_mutex);
    if (g_registered)
        return;
    DJDecoderRegistration::registerCodecs();
    DJLSDecoderRegistration::registerCodecs();
    DcmRLEDecoderRegistration::registerCodecs();
    g_registered = true;
}

void cleanupCodecs()
{
    const std::lock_guard lock(g_mutex);
    if (!g_registered)
        return;
    DcmRLEDecoderRegistration::cleanup();
    DJLSDecoderRegistration::cleanup();
    DJDecoderRegistration::cleanup();
    g_registered = false;
}

std::string dcmtkVersion()
{
    return OFFIS_DCMTK_VERSION_STRING;
}

} // namespace dv
