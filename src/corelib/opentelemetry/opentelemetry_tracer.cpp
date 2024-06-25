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


#include <ncbi_pch.hpp>

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/opentelemetry/opentelemetry_tracer.hpp>
#include <opentelemetry/sdk/trace/id_generator.h>
#include <opentelemetry/sdk/trace/exporter.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/samplers/always_on.h>
#include <opentelemetry/sdk/trace/processor.h>
#include <opentelemetry/sdk/trace/simple_processor_factory.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_exporter_options.h>
#include <opentelemetry/exporters/ostream/span_exporter_factory.h>


#define NCBI_USE_ERRCODE_X   Corelib_Diag

BEGIN_NCBI_SCOPE

using namespace opentelemetry;


namespace {

using TIdBuf = vector<uint8_t>;

static constexpr int kTraceIdSize = trace::TraceId::kSize;
using TTraceIdBuf = nostd::span<const uint8_t, kTraceIdSize>;

#define GET_BYTE(i64, b) ((i64 >> (56 - b*8)) & 0xff)

TIdBuf trace_id_hash(const string &s)
{
    uint64_t h1 = 5381; // low
    uint64_t h2 = 0;    // high
    for (char c : s) {
        h1 = c + (h1 << 5) + h1; // c + h1*33
        h2 = c + (h2 << 6) + (h2 << 16) - h2; // c + h2*65599
    }
    TIdBuf buf(kTraceIdSize, 0);
    for (size_t i = 0; i < kTraceIdSize / 2; ++i) {
        if (i > 8) break; // use 16 bytes
        buf[i] = GET_BYTE(h2, i);
        buf[kTraceIdSize / 2 + i] = GET_BYTE(h1, i);
    }
    return buf;
}


static constexpr int kSpanIdSize = trace::SpanId::kSize;
using TSpanIdBuf = nostd::span<const uint8_t, kSpanIdSize>;

TIdBuf span_id_hash(const string &s)
{
    uint64_t h = 0;
    for (char c : s) {
        h = c + (h << 6) + (h << 16) - h; // c + h*65599
    }
    TIdBuf buf(kSpanIdSize, 0);
    for (size_t i = 0; i < kSpanIdSize; ++i) {
        if (i > 8) break; // use 8 bytes
        buf[i] = GET_BYTE(h, i);
    }
    return buf;
}

} // namespace


// NOTE: Using an id generator is the only way to set custom span-ids in OT.
class CPhidIdGenerator : public sdk::trace::IdGenerator
{
public:
    using TParent = sdk::trace::IdGenerator;
    
    CPhidIdGenerator() : TParent(false) {}

    trace::SpanId GenerateSpanId() noexcept override
    {
        return s_GenerateSpanId();
    }

    trace::TraceId GenerateTraceId() noexcept override
    {
        return s_GenerateTraceId();
    }

    static string s_GetBaseHitId(void)
    {
        auto phid = CDiagContext::GetRequestContext().GetHitID();
        return phid.substr(0, phid.find_first_of('.'));
    }
    
    static string s_GetParentHitId(void)
    {
        auto phid = CDiagContext::GetRequestContext().GetHitID();
        size_t dot = phid.find_last_of('.');
        if (dot != string::npos && phid.size() > dot) {
            return phid.substr(0, dot);
        }
        return kEmptyStr;
    }

    static trace::SpanId s_GenerateSpanId() noexcept
    {
        auto phid = CDiagContext::GetRequestContext().GetHitID();
        TIdBuf span_id_buf = span_id_hash(phid);
        return trace::SpanId(TSpanIdBuf(span_id_buf.data(), span_id_buf.size()));
    }

    static trace::TraceId s_GenerateTraceId() noexcept
     {
        string base_phid = s_GetBaseHitId();
        TIdBuf trace_id_buf = trace_id_hash(base_phid);
        return trace::TraceId(TTraceIdBuf(trace_id_buf.data(), trace_id_buf.size()));
    }

    static trace::SpanId s_GenerateParentSpanId() noexcept
    {
        auto parent_phid = s_GetParentHitId();
        if (parent_phid.empty()) return trace::SpanId();
        TIdBuf span_id_buf = span_id_hash(parent_phid);
        return trace::SpanId(TSpanIdBuf(span_id_buf.data(), span_id_buf.size()));
    }
};


using TTracer = nostd::shared_ptr<trace::Tracer>;

static TTracer s_GetTracer(void)
{
    string app_name = "ncbi-app";
    string app_ver = "0.0";
    {
        auto guard = CNcbiApplication::InstanceGuard();
        if ( guard ) {
            app_name = guard->GetProgramDisplayName();
            app_ver = guard->GetVersion().Print();
        }
    }
    return trace::Provider::GetTracerProvider()->GetTracer(app_name, app_ver);
}


DEFINE_STATIC_FAST_MUTEX(s_InitTracerMutex);

static TTracer s_InitTracer(unique_ptr<sdk::trace::SpanExporter> exporter)
{
    CFastMutexGuard guard(s_InitTracerMutex);

    auto processor = sdk::trace::SimpleSpanProcessorFactory::Create(std::move(exporter));
    vector<unique_ptr<sdk::trace::SpanProcessor>> processors;
    processors.push_back(std::move(processor));

    opentelemetry::sdk::resource::Resource
        resource =
        sdk::resource::Resource::Create({});
    unique_ptr<sdk::trace::Sampler> sampler(new sdk::trace::AlwaysOnSampler());

    unique_ptr<sdk::trace::IdGenerator> id_generator(new CPhidIdGenerator());

    shared_ptr<trace::TracerProvider> provider =
        sdk::trace::TracerProviderFactory::Create(std::move(processors),
        resource, std::move(sampler), std::move(id_generator));
    trace::Provider::SetTracerProvider(provider);

    return s_GetTracer();
}


void COpentelemetryTracer::CleanupTracer(void)
{
    shared_ptr<trace::TracerProvider> none;
    trace::Provider::SetTracerProvider(none);
}


COpentelemetryTracer::COpentelemetryTracer(ostream& ostr)
{
    x_InitStream(ostr);
}


COpentelemetryTracer::COpentelemetryTracer(void)
{
    x_Init();
}


COpentelemetryTracer::COpentelemetryTracer(const string& endpoint)
{
    x_InitOtlp(endpoint);
}


COpentelemetryTracer::~COpentelemetryTracer(void)
{
}


enum class EOTEL_Exporter {
    e_None,
    e_Otlp,
    e_Console
};

NCBI_PARAM_ENUM_DECL(EOTEL_Exporter, OTEL, TRACES_EXPORTER);
NCBI_PARAM_ENUM_ARRAY(EOTEL_Exporter, OTEL, TRACES_EXPORTER)
{
    {"none", EOTEL_Exporter::e_None},
    {"otlp", EOTEL_Exporter::e_Otlp},
    {"console", EOTEL_Exporter::e_Console}
};
NCBI_PARAM_ENUM_DEF_EX(EOTEL_Exporter, OTEL, TRACES_EXPORTER,
                       EOTEL_Exporter::e_Otlp,
                       eParam_NoThread, OTEL_TRACES_EXPORTER);
typedef NCBI_PARAM_TYPE(OTEL, TRACES_EXPORTER) TParam_Exporter;


enum class EOtlp_Protocol {
    e_HTTP_Protobuf,
    e_HTTP_Json,
    e_gRPC
};

NCBI_PARAM_ENUM_DECL(EOtlp_Protocol, OTEL, EXPORTER_OTLP_PROTOCOL);
NCBI_PARAM_ENUM_ARRAY(EOtlp_Protocol, OTEL, EXPORTER_OTLP_PROTOCOL)
{
    {"http/protobuf", EOtlp_Protocol::e_HTTP_Protobuf},
    {"http/json", EOtlp_Protocol::e_HTTP_Json},
    {"grpc", EOtlp_Protocol::e_gRPC}
};
NCBI_PARAM_ENUM_DEF_EX(EOtlp_Protocol, OTEL, EXPORTER_OTLP_PROTOCOL,
                       EOtlp_Protocol::e_HTTP_Protobuf,
                       eParam_NoThread, OTEL_EXPORTER_OTLP_PROTOCOL);
typedef NCBI_PARAM_TYPE(OTEL, EXPORTER_OTLP_PROTOCOL) TParam_OtlpProtocol;


NCBI_PARAM_DECL(string, OTEL, EXPORTER_OTLP_ENDPOINT);
NCBI_PARAM_DEF_EX(string, OTEL, EXPORTER_OTLP_ENDPOINT, "", eParam_NoThread, OTEL_EXPORTER_OTLP_ENDPOINT);
typedef NCBI_PARAM_TYPE(OTEL, EXPORTER_OTLP_ENDPOINT) TParam_OtlpEndpoint;


void COpentelemetryTracer::x_Init(void)
{
    EOTEL_Exporter exporter = TParam_Exporter::GetDefault();
    switch (exporter) {
    case EOTEL_Exporter::e_None:
        return;
    case EOTEL_Exporter::e_Otlp:
        x_InitOtlp(TParam_OtlpEndpoint::GetDefault());
        break;
    case EOTEL_Exporter::e_Console:
        x_InitStream(cout);
        break;
    default:
        ERR_POST("Unknown OTEL exporter");
        return;
    }
}


void COpentelemetryTracer::x_InitOtlp(const string& endpoint)
{
    EOtlp_Protocol protocol = TParam_OtlpProtocol::GetDefault();
    if (protocol == EOtlp_Protocol::e_HTTP_Protobuf || protocol == EOtlp_Protocol::e_HTTP_Json) {
        exporter::otlp::OtlpHttpExporterOptions http_opt;
        if (protocol == EOtlp_Protocol::e_HTTP_Protobuf) {
            http_opt.content_type = exporter::otlp::HttpRequestContentType::kBinary;
        }
        else {
            http_opt.content_type = exporter::otlp::HttpRequestContentType::kJson;
        }
        http_opt.url = !endpoint.empty() ? endpoint : "http://localhost:4318/v1/traces";
        m_Tracer = s_InitTracer(exporter::otlp::OtlpHttpExporterFactory::Create(http_opt));
    }
    else if (protocol == EOtlp_Protocol::e_gRPC) {
        exporter::otlp::OtlpGrpcExporterOptions grpc_opt;
        grpc_opt.endpoint = !endpoint.empty() ? endpoint : "http://localhost:4317/v1/traces";
        m_Tracer = s_InitTracer(exporter::otlp::OtlpGrpcExporterFactory::Create(grpc_opt));
    }
    else {
        ERR_POST("Unknown OTLP protocol");
    }
}


void COpentelemetryTracer::x_InitStream(ostream& ostr)
{
    m_Tracer = s_InitTracer(exporter::trace::OStreamSpanExporterFactory::Create(ostr));
}


void COpentelemetryTracer::OnRequestStart(CRequestContext& context)
{
    auto tracer = s_GetTracer();
    if (!tracer) return;

    if (!context.IsSetHitID()) context.SetHitID();
    auto& current_ctx = CDiagContext::GetRequestContext();
    if (&current_ctx != &context) {
        // NOTE: Since ID generator is installed globally, it uses current request context
        // rather than the one passed as an argument to the tracer. Normally they are
        // the same but if they are not issue a warning.
        string phid_cur = current_ctx.IsSetHitID() ? current_ctx.GetHitID() : "<empty>";
        string phid_arg = context.IsSetHitID() ? context.GetHitID() : "<empty>";
        ERR_POST(Warning <<
            "Request context is not current, trace and span ids may be incorrect. Current context phid=" <<
            phid_cur << "; tracer context phid=" << phid_arg);
    }

    trace::SpanContext parent_ctx(
        CPhidIdGenerator::s_GenerateTraceId(),
        CPhidIdGenerator::s_GenerateParentSpanId(),
        trace::TraceFlags(0), false);
    trace::StartSpanOptions options;
    options.parent = parent_ctx;

    auto span = tracer->StartSpan("request-start", {}, options);

    span->SetAttribute("ncbi_phid", context.GetHitID());
    span->SetAttribute("session_id", context.GetSessionID());
    span->SetAttribute("request_id", context.GetRequestID());
    span->SetAttribute("client_ip", context.GetClientIP());
    CDiagContext& diag_context = GetDiagContext();
    char buf[17];
    diag_context.GetStringUID(diag_context.GetUID(), buf, 17);
    span->SetAttribute("guid", buf);
    span->SetAttribute("pid", diag_context.GetPID());
    span->SetAttribute("tid", (Uint8)GetCurrentThreadSystemID());
    span->SetAttribute("host", diag_context.GetHost());

    char trace_id[2*trace::TraceId::kSize + 1];
    trace_id[2*trace::TraceId::kSize] = 0;
    span->GetContext().trace_id().ToLowerBase16(
        nostd::span<char, 2*trace::TraceId::kSize>(trace_id, 2*trace::TraceId::kSize));

    char span_id[2*trace::SpanId::kSize + 1];
    span_id[2*trace::SpanId::kSize] = 0;
    span->GetContext().span_id().ToLowerBase16(
        nostd::span<char, 2*trace::SpanId::kSize>(span_id, 2*trace::SpanId::kSize));

    GetDiagContext().Extra()
        .Print("opentelemetry_trace_id", trace_id)
        .Print("opentelemetry_span_id", span_id);

    shared_ptr<ITracerSpan> ctx_span = make_shared<COpentelemetryTracerSpan>(span);
    context.SetTracerSpan(ctx_span);
}


void COpentelemetryTracer::OnRequestStop(CRequestContext& context)
{
    auto ctx_span = context.GetTracerSpan();
    if (!ctx_span) return;
    auto ot_span = dynamic_pointer_cast<COpentelemetryTracerSpan>(ctx_span);
    if (!ot_span) return;
    ot_span->GetSpan()->SetStatus(trace_api::StatusCode::kOk);
    ot_span->GetSpan()->End();
}


END_NCBI_SCOPE
