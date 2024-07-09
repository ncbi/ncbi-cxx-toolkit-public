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


void id_hash(const string &s, uint64_t &high, uint64_t &low)
{
    uint64_t h1 = 5381;
    uint64_t h2 = 0;
    for (char c : s) {
        h1 = c + (h1 << 5) + h1; // c + h1*33
        h2 = c + (h2 << 6) + (h2 << 16) - h2; // c + h2*65599
    }
    low = h1;
    high = h2;
}


void id_hash(const string &s, uint64_t& h)
{
    h = 0;
    for (char c : s) {
        h = c + (h << 6) + (h << 16) - h; // c + h*65599
    }
}


void CJaegerTracer::OnRequestStart(CRequestContext& context)
{
    if (!context.IsSetHitID()) context.SetHitID();

    uint64_t trace_id_high;
    uint64_t trace_id_low;
    uint64_t span_id;
    uint64_t parent_span_id = 0;

    string phid = context.GetHitID();
    size_t dot = phid.find_first_of('.');
    string base_phid = phid.substr(0, dot);
    // Trace ID is base PHID without sub-hit IDs.
    id_hash(base_phid, trace_id_high, trace_id_low);
    // Self span ID is the whole PHID
    id_hash(phid, span_id);

    // Parent span ID is the whole PHID without the last sub-hit ID or none.
    dot = phid.find_last_of('.');
    if (dot != string::npos && phid.size() > dot) {
        id_hash(phid.substr(0, dot), parent_span_id);
    }

    jaegertracing::TraceID trace_id(trace_id_high, trace_id_low);
    unique_ptr<jaegertracing::SpanContext> span_context(new jaegertracing::SpanContext(
        trace_id,
        span_id,
        parent_span_id,
        (unsigned char)jaegertracing::SpanContext::Flag::kSampled,
        {}));
    auto tracer = dynamic_pointer_cast<const jaegertracing::Tracer>(opentracing::Tracer::Global());
    shared_ptr<jaegertracing::Span> span(new jaegertracing::Span(
        tracer, *span_context, tracer->serviceName()));

    span->SetTag("ncbi_phid", phid);
    span->SetTag("session_id", context.GetSessionID());
    span->SetTag("request_id", context.GetRequestID());
    span->SetTag("client_ip", context.GetClientIP());
    CDiagContext& diag_context = GetDiagContext();
    char buf[17];
    diag_context.GetStringUID(diag_context.GetUID(), buf, 17);
    span->SetTag("guid", buf);
    span->SetTag("pid", diag_context.GetPID());
    span->SetTag("tid", (Uint8)GetCurrentThreadSystemID());
    span->SetTag("host", diag_context.GetHost());

    // Post trace and span IDs to the log.
    // For some reason jaegertracing does not overload SpanContext::ToSpanID()
    // and SpanContext::ToTraceID() so that both return empty strings.
    auto jctx = dynamic_cast<const jaegertracing::SpanContext*>(&span->context());
    if (jctx != nullptr) {
        CNcbiOstrstream trace_str, span_str;
        jctx->traceID().print(trace_str);
        span_str << hex << jctx->spanID();
        GetDiagContext().Extra()
            .Print("jaeger_trace_id", trace_str.str())
            .Print("jaeger_span_id", span_str.str());
    }
    context.SetTracerSpan(make_shared<CJaegerTracerSpan>(span));
}


void CJaegerTracer::OnRequestStop(CRequestContext& context)
{
    CJaegerTracerSpan* span = dynamic_cast<CJaegerTracerSpan*>(context.GetTracerSpan().get());
    if (!span) return;
    span->GetSpan().Finish();
    context.SetTracerSpan(nullptr);
}


void CJaegerTracerSpan::SetAttribute(ESpanAttribute attr, const string& value)
{
    if ( !m_Span ) return;
    switch (attr) {
    case eSessionId:
        m_Span->SetTag("session_id", value);
        break;
    case eClientAddress:
        m_Span->SetTag("client_ip", value);
        break;
    case eClientPort:
        m_Span->SetTag("client_port", value);
        break;
    case eServerAddress:
        m_Span->SetTag("server_ip", value);
        break;
    case eServerPort:
        m_Span->SetTag("server_port", value);
        break;
    case eUrl:
        m_Span->SetTag("url", value);
        break;
    case eRequestMethod:
        m_Span->SetTag("request_method", value);
        break;
    case eStatusCode:
        m_Span->SetTag("status_code", value);
        break;
    case eStatusString:
        m_Span->SetTag("status_string", value);
        break;
    default:
        return;
    }
}


void CJaegerTracerSpan::SetCustomAttribute(const string& attr, const string& value)
{
    if ( !m_Span ) return;
    m_Span->SetTag(attr, value);
}


void CJaegerTracerSpan::SetHttpHeader(EHttpHeaderType header_type, const string& name, const string& value)
{
    if ( !m_Span ) return;
    switch (header_type) {
    case eRequest:
        SetCustomAttribute("http_request_header__" + name, value);
        break;
    case eResponse:
        SetCustomAttribute("http_response_header__" + name, value);
        break;
    default:
        break;
    }
}


void CJaegerTracerSpan::SetSpanStatus(ESpanStatus status)
{
    if ( !m_Span ) return;
    SetCustomAttribute("status", status == eSuccess ? "success" : "error");
}


END_NCBI_SCOPE
