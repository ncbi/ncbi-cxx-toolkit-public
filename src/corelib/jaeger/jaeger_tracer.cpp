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


#include <ncbi_pch.hpp>


// This is required for htonll to be found in jaegertracing.
#define INCL_EXTRA_HTON_FUNCTIONS

#include <corelib/jaeger/jaeger_tracer.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbi_strings.h>
#include <jaegertracing/Tracer.h>


#define NCBI_USE_ERRCODE_X   Corelib_Diag

BEGIN_NCBI_SCOPE


static void s_InitTracer(const string& service_name,
                         const jaegertracing::Config& config)
{
    static CFastMutex s_InitMutex;
    if (jaegertracing::Tracer::IsGlobalTracerRegistered()) return;
    CFastMutexGuard guard(s_InitMutex);
    if (jaegertracing::Tracer::IsGlobalTracerRegistered()) return;

    auto tracer = service_name.empty() ?
        jaegertracing::Tracer::make(config) :
        jaegertracing::Tracer::make(service_name, config);
    jaegertracing::Tracer::InitGlobal(
        std::static_pointer_cast<jaegertracing::Tracer>(tracer));
}


static jaegertracing::Config s_GetDefaultConfig(void)
{
    jaegertracing::Config config(
        false,
        false,
        jaegertracing::samplers::Config(jaegertracing::kSamplerTypeConst, 1),
        jaegertracing::reporters::Config(
            jaegertracing::reporters::Config::kDefaultQueueSize,
            jaegertracing::reporters::Config::defaultBufferFlushInterval(),
            true));
    config.fromEnv();
    return config;
}


CJaegerTracer::CJaegerTracer(void)
{
    jaegertracing::Config config = s_GetDefaultConfig();
    string service_name = "no-service-name";
    {
        auto guard = CNcbiApplication::InstanceGuard();
        if ( guard ) service_name = guard->GetProgramDisplayName();
    }
    s_InitTracer(service_name, config);
}


CJaegerTracer::CJaegerTracer(const string& service_name)
{
    jaegertracing::Config config = s_GetDefaultConfig();
    s_InitTracer(service_name, config);
}


CJaegerTracer::CJaegerTracer(const string& service_name,
                             const jaegertracing::Config& config)
{
    s_InitTracer(service_name, config);
}


CJaegerTracer::~CJaegerTracer(void)
{
}


void CJaegerTracer::OnRequestStart(CRequestContext& context)
{
    if (!context.IsSetHitID()) context.SetHitID();
    auto span = make_shared<CJaegerTracerSpan>(jaegertracing::Tracer::Global()->StartSpan(context.GetHitID()));
    // Post PHID to the trace.
    span->GetSpan().SetBaggageItem(g_GetNcbiString(eNcbiStrings_PHID), context.GetHitID());
    // Post trace and span IDs to the log.
    // For some reason jaegertracing does not overload SpanContext::ToSpanID()
    // and SpanContext::ToTraceID() so that both return empty strings.
    auto jctx = dynamic_cast<const jaegertracing::SpanContext*>(&span->GetSpan().context());
    if (jctx != nullptr) {
        CNcbiOstrstream trace_str, span_str;
        jctx->traceID().print(trace_str);
        span_str << hex << jctx->spanID();
        GetDiagContext().Extra()
            .Print("jaeger_trace_id", trace_str.str())
            .Print("jaeger_span_id", span_str.str());
    }
    context.SetTracerSpan(span);
}


void CJaegerTracer::OnRequestStop(CRequestContext& context)
{
    CJaegerTracerSpan* span = dynamic_cast<CJaegerTracerSpan*>(context.GetTracerSpan().get());
    if (!span) return;
    span->GetSpan().Finish();
    context.SetTracerSpan(nullptr);
}


END_NCBI_SCOPE
