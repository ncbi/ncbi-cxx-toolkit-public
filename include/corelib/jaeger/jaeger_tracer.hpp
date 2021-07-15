#ifndef CORELIB___JAEGER_TRACER__HPP
#define CORELIB___JAEGER_TRACER__HPP

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
 *   IRequestTracer implementation for Jaeger.
 *
 */

/// @file jaeger_tracer.hpp
///
///   Defines CJaegerTracer class for NCBI C++ diagnostic API.
///


/** @addtogroup Diagnostics
 *
 * @{
 */


#include <corelib/request_ctx.hpp>
#include <jaegertracing/Tracer.h>

BEGIN_NCBI_SCOPE


class NCBI_JAEGER_TRACER_EXPORT CJaegerTracerSpan : public ITracerSpan
{
public:
    CJaegerTracerSpan(unique_ptr<opentracing::Span> span) : m_Span(move(span)) {}
    opentracing::Span& GetSpan(void) { return *m_Span; }
private:
    unique_ptr<opentracing::Span> m_Span;
};


class NCBI_JAEGER_TRACER_EXPORT CJaegerTracer : public IRequestTracer
{
public:
    CJaegerTracer(void);
    CJaegerTracer(const string& service_name);
    CJaegerTracer(const string& service_name,
                  const jaegertracing::Config& config);
    ~CJaegerTracer(void);

    void OnRequestStart(CRequestContext& context) override;
    void OnRequestStop(CRequestContext& context) override;
private:
    static void s_InitTracer(const string& service_name,
                             const jaegertracing::Config& config);
};


END_NCBI_SCOPE


#endif  /* CORELIB___JAEGER_TRACER__HPP */
