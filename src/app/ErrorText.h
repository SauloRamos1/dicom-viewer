// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QCoreApplication>
#include <QString>

#include "DicomError.h"

// Single place that maps a dv::ErrorCode to its translated user message and to
// the sample text shown in the Errors panel preview. The technical detail of a
// real error is the DCMTK text carried by dv::DicomError::detail.
class ErrorText
{
    Q_DECLARE_TR_FUNCTIONS(ErrorText)

public:
    // Translated, user-facing title for `code` (never empty).
    static QString message(dv::ErrorCode code);

    // Translated example of the technical detail for `code`, used by preview samples.
    static QString sampleDetail(dv::ErrorCode code);

    // "error", "warning" or "info": the severity key used by the Errors panel.
    static QString severityKey(dv::Severity severity);
};
