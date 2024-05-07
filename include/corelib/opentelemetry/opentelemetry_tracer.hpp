#ifndef CORELIB___OPENTELEMETRY_TRACER__HPP
#define CORELIB___OPENTELEMETRY_TRACER__HPP

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   IRequestTracer implementation for OpenTelemetry.
 *
 */

/// @file opentelemetry_tracer.hpp
///
///   Defines COpentelemetryTracer class for NCBI C++ diagnostic API.
///


/** @addtogroup Diagnostics
 *
 * @{
 */

#include <corelib/request_ctx.hpp>
#include <opentelemetry/trace/span.h>
#include <opentelemetry/trace/tracer.h>


BEGIN_NCBI_SCOPE


class COpentelemetryTracer;

class NCBI_OPENTELEMETRY_TRACER_EXPORT COpentelemetryTracerSpan : public ITracerSpan
{
public:
    using TSpan = opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>;

    COpentelemetryTracerSpan(TSpan& span) : m_Span(span) {}

    TSpan& GetSpan(void) { return m_Span; }

private:
    TSpan m_Span;
};


/// IRequestTracer implementation for OpenTelemetry tracing.
class NCBI_OPENTELEMETRY_TRACER_EXPORT COpentelemetryTracer : public IRequestTracer
{
public:
    /// NOTE: The tracers are global; creating a new tracer replaces any
    /// previously installed one.

    /// Export to the output stream. The stream must exist at least until the tracer is deleted.
    COpentelemetryTracer(ostream& ostr);

    /// Export to OTLP/HTTP collector using default endpoint "http://localhost:4318/v1/traces".
    COpentelemetryTracer(void);

    /// Export to OTLP/HTTP collector using the selected endpoint.
    COpentelemetryTracer(const string& endpoint);
    
    ~COpentelemetryTracer(void);

    void OnRequestStart(CRequestContext& context) override;
    void OnRequestStop(CRequestContext& context) override;

    /// Uninstall current tracer, if any. This call is optional but may be
    /// useful, e.g. to remove current tracer before the output stream it
    /// uses is destroyed.
    static void CleanupTracer(void);

private:
    void x_InitOtlp(const string& endpoint);

    using TTracer = opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>;

    TTracer m_Tracer;
};

END_NCBI_SCOPE

#endif  /* CORELIB___OPENTELEMETRY_TRACER__HPP */
