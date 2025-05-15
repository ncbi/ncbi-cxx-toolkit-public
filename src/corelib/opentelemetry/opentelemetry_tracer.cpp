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
#include <opentelemetry/trace/semantic_conventions.h>
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

static string s_DefaultSpanNamePrefix;

static TTracer s_GetTracer(void)
{
    string app_name = "ncbi-app";
    string app_ver = "0.0";
    {
        auto guard = CNcbiApplication::InstanceGuard();
        if ( guard ) {
            app_name = guard->GetProgramDisplayName();
            app_ver = guard->GetVersion().Print();
            s_DefaultSpanNamePrefix = app_name;
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


TIdBuf string_to_id(const string& id)
{
    TIdBuf buf(id.size() / 2, 0);
    for (size_t b = 0; b < id.size() / 2; ++b) {
        buf[b] = NStr::StringToNumeric<uint8_t>(id.substr(b*2, 2), 0, 16);
    }
    return buf;
}


static void s_ParseIds(CRequestContext& ctx, trace::TraceId& trace_id, trace::SpanId& parent_id, int& flags)
{
    constexpr size_t kVersion_Digits = 2;
    constexpr size_t kTraceId_Digits = 32;
    constexpr size_t kParentId_Digits = 16;
    constexpr size_t kFlags_Digits = 2;
    constexpr size_t kSeparator1_Pos = kVersion_Digits;
    constexpr size_t kSeparator2_Pos = kSeparator1_Pos + 1 + kTraceId_Digits;
    constexpr size_t kSeparator3_Pos = kSeparator2_Pos + 1 + kParentId_Digits;
    constexpr size_t kTraceParent_Size = kVersion_Digits + 1 + kTraceId_Digits + 1 + kParentId_Digits + 1 + kFlags_Digits;
    _ASSERT(kTraceIdSize*2 == kTraceId_Digits);
    _ASSERT(kSpanIdSize*2 == kParentId_Digits);

    const string& traceparent = ctx.GetSrcTraceParent();
    // Make sure the incoming value has the correct format, ignore otherwise.
    if (traceparent.size() >= kTraceParent_Size &&
        traceparent[kSeparator1_Pos] == '-' &&
        traceparent[kSeparator2_Pos] == '-' &&
        traceparent[kSeparator3_Pos] == '-') {
        try {
            string_to_id(traceparent.substr(0, kVersion_Digits)); // version
            TIdBuf tid_buf = string_to_id(traceparent.substr(kSeparator1_Pos + 1, kTraceId_Digits));
            TIdBuf pid_buf = string_to_id(traceparent.substr(kSeparator2_Pos + 1, kParentId_Digits));
            TIdBuf flags_buf = string_to_id(traceparent.substr(kSeparator3_Pos + 1, kFlags_Digits));
            trace_id = trace::TraceId(TTraceIdBuf(tid_buf.data(), tid_buf.size()));
            parent_id = trace::SpanId(TSpanIdBuf(pid_buf.data(), pid_buf.size()));
            flags = (flags_buf[0] << 8) | flags_buf[1];
            return;
        }
        catch (CStringException&) {
        }
    }

    // Failed to parse the incoming ids, generate new ones from PHID.
    trace_id = CPhidIdGenerator::s_GenerateTraceId();
    parent_id = CPhidIdGenerator::s_GenerateParentSpanId();
}


static string s_UpdateTraceState(CRequestContext& ctx, const string& traceparent)
{
    string ts = ctx.GetSrcTraceState();
    size_t ncbi_pos = ts.find("ncbi");
    if (ncbi_pos == NPOS) return ts;

    vector<string> states;
    NStr::Split(ts, ",", states);
    string new_ts;
    for (const auto& state : states) {
        if (state.find("ncbi") != NPOS) {
            string key, val;
            NStr::SplitInTwo(state, "=", key, val);
            NStr::TruncateSpaces(key);
            NStr::TruncateSpaces(val);
            if (key == "ncbi") {
                val = traceparent;
                if (!new_ts.empty()) val += ",";
                new_ts = key + "=" + val + new_ts; // the updated 'ncbi' state goes first
                continue;
            }
        }
        if (!new_ts.empty()) new_ts += ",";
        new_ts += state;
    }
    return new_ts;
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

    trace::TraceId context_tid;
    trace::SpanId context_pid;
    int flags = 0;
    s_ParseIds(current_ctx, context_tid, context_pid, flags);

    trace::SpanContext parent_ctx(context_tid, context_pid, trace::TraceFlags(0), false);
    trace::StartSpanOptions options;
    options.parent = parent_ctx;

    switch (context.GetTracerSpanKind()) {
    case ITracerSpan::eKind_Internal:
        options.kind = trace::SpanKind::kInternal;
        break;
    case ITracerSpan::eKind_Server:
        options.kind = trace::SpanKind::kServer;
        break;
    case ITracerSpan::eKind_Client:
        options.kind = trace::SpanKind::kClient;
        break;
    default:
        options.kind = m_SpanKind;
    }

    auto span = tracer->StartSpan(s_DefaultSpanNamePrefix + "/request-start", {}, options);

    CDiagContext& diag_context = GetDiagContext();
    char buf[17];
    diag_context.GetStringUID(diag_context.GetUID(), buf, 17);

    span->SetStatus(trace_api::StatusCode::kOk);

    // Some values (e.g. SemanticConventions::kClientAddress) can be set
    // by CGIs or other external code. Opentelemetry span's SetAttribute()
    // creates multiple attributes with the same name instead of replacing
    // the value (despite what the documentation says). To avoid having
    // duplicate names the values from request context are added as 'ncbi.*'
    // rather than using SemanticConventions::* names.
    span->SetAttribute("ncbi.guid", buf);
    span->SetAttribute("ncbi.pid", diag_context.GetPID());
    span->SetAttribute("ncbi.tid", (Uint8)GetCurrentThreadSystemID());
    span->SetAttribute("ncbi.host", diag_context.GetHost());
    span->SetAttribute("ncbi.request_id", context.GetRequestID());
    span->SetAttribute("ncbi.phid", context.GetHitID());
    string s = context.GetClientIP();
    if ( !s.empty() ) span->SetAttribute("ncbi.client_ip", s);
    s = context.GetSessionID();
    if ( !s.empty() ) span->SetAttribute("ncbi.session_id", s);

    char trace_id_buf[2*trace::TraceId::kSize + 1];
    trace_id_buf[2*trace::TraceId::kSize] = 0;
    span->GetContext().trace_id().ToLowerBase16(
        nostd::span<char, 2*trace::TraceId::kSize>(trace_id_buf, 2*trace::TraceId::kSize));
    string trace_id(trace_id_buf);

    char span_id_buf[2*trace::SpanId::kSize + 1];
    span_id_buf[2*trace::SpanId::kSize] = 0;
    span->GetContext().span_id().ToLowerBase16(
        nostd::span<char, 2*trace::SpanId::kSize>(span_id_buf, 2*trace::SpanId::kSize));
    string span_id(span_id_buf);

    GetDiagContext().Extra()
        .Print("opentelemetry_trace_id", trace_id)
        .Print("opentelemetry_span_id", span_id);

    shared_ptr<COpentelemetryTracerSpan> ctx_span = make_shared<COpentelemetryTracerSpan>(span);
    // NOTE: Version must be set to the latest known to the application. Since the app is
    // sampling the span, flags indlude 'sampled' bit regardless of what the parent did.
    ctx_span->m_TraceParent = "00-" + string(trace_id) + "-" + span_id + "-01";
    ctx_span->m_TraceState = s_UpdateTraceState(current_ctx, ctx_span->m_TraceParent);
    context.SetTracerSpan(ctx_span);
}


void COpentelemetryTracer::OnRequestStop(CRequestContext& context)
{
    auto ctx_span = context.GetTracerSpan();
    if (!ctx_span) return;
    auto ot_span = dynamic_pointer_cast<COpentelemetryTracerSpan>(ctx_span);
    if (!ot_span) return;
    ot_span->GetSpan()->End();
}


COpentelemetryTracerSpan::~COpentelemetryTracerSpan(void)
{
    // NOTE: multiple calls to End() should be ignored by OT, so it's safe
    // to always call it here.
    EndSpan();
}


void COpentelemetryTracerSpan::SetName(const string& name)
{
    if (!m_Span) return;
    m_Span->UpdateName(name);
}


void COpentelemetryTracerSpan::SetAttribute(ESpanAttribute attr, const string& value)
{
    if ( !m_Span ) return;
    switch (attr) {
    case eSessionId:
        m_Span->SetAttribute(trace::SemanticConventions::kSessionId, value);
        break;
    case eClientAddress:
        m_Span->SetAttribute(trace::SemanticConventions::kClientAddress, value);
        break;
    case eClientPort:
        m_Span->SetAttribute(trace::SemanticConventions::kClientPort, value);
        break;
    case eServerAddress:
        m_Span->SetAttribute(trace::SemanticConventions::kServerAddress, value);
        break;
    case eServerPort:
        m_Span->SetAttribute(trace::SemanticConventions::kServerPort, value);
        break;
    case eUrl:
        m_Span->SetAttribute(trace::SemanticConventions::kUrlFull, value);
        break;
    case eRequestMethod:
        m_Span->SetAttribute(trace::SemanticConventions::kHttpRequestMethod, value);
        break;
    case eUrlScheme:
        m_Span->SetAttribute(trace::SemanticConventions::kUrlScheme, value);
        break;
    case eHttpScheme:
        m_Span->SetAttribute(trace::SemanticConventions::kHttpScheme, value);
        break;
    case eStatusCode:
        m_Span->SetAttribute(trace::SemanticConventions::kHttpResponseStatusCode, value);
        break;
    case eStatusString:
        m_Span->SetAttribute(trace::SemanticConventions::kErrorType, value);
        break;
    default:
        break;
    }
}


void COpentelemetryTracerSpan::SetCustomAttribute(const string& attr, const string& value)
{
    if ( !m_Span ) return;
    m_Span->SetAttribute(attr, value);
}


void COpentelemetryTracerSpan::SetHttpHeader(EHttpHeaderType header_type, const string& name, const string& value)
{
    if ( !m_Span ) return;
    switch (header_type) {
    case eRequest:
        SetCustomAttribute("http.request.header." + name, value);
        break;
    case eResponse:
        SetCustomAttribute("http.response.header." + name, value);
        break;
    default:
        break;
    }
}


void COpentelemetryTracerSpan::SetSpanStatus(ESpanStatus status)
{
    if ( !m_Span ) return;
    m_Span->SetStatus(status == eSuccess ? trace::StatusCode::kOk : trace::StatusCode::kError);
}


const string& COpentelemetryTracerSpan::GetTraceState(void) const
{
    return m_TraceState;
}


const string& COpentelemetryTracerSpan::GetTraceParent(void) const
{
    return m_TraceParent;
}


void COpentelemetryTracerSpan::PostEvent(const SDiagMessage& message)
{
    if ( !m_Span ) return;
    // Ignore applog events.
    if (message.m_Flags & eDPF_AppLog) return;
    string msg = message.m_BufferLen ? string(message.m_Buffer, message.m_BufferLen) : "<no message>";
    map<string, string> attr;
    attr["severity"] = CNcbiDiag::SeverityName(message.m_Severity);
    if (message.m_File && *message.m_File) attr["file"] = message.m_File;
    if (message.m_Module && *message.m_Module) attr["module"] = message.m_Module;
    if (message.m_Class && *message.m_Class) attr["class"] = message.m_Class;
    if (message.m_Function && *message.m_Function) attr["function"] = message.m_Function;
    if (message.m_Line) attr["line"] = NStr::NumericToString(message.m_Line);
    if (message.m_ErrCode) attr["err_code"] = NStr::NumericToString(message.m_ErrCode);
    if (message.m_ErrSubCode) attr["err_sub_code"] = NStr::NumericToString(message.m_ErrSubCode);
    if (message.m_ErrText && *message.m_ErrText) attr["err_text"] = message.m_ErrText;
    if (message.m_Prefix && *message.m_Prefix) attr["prefix"] = message.m_Prefix;
    attr["tid"] = NStr::NumericToString(message.m_TID);
    attr["proc_post"] = NStr::NumericToString(message.m_ProcPost);
    attr["thread_post"] = NStr::NumericToString(message.m_ThrPost);
    m_Span->AddEvent(msg, attr);
}


void COpentelemetryTracerSpan::EndSpan(void)
{
    if ( m_Span ) m_Span->End();
}


END_NCBI_SCOPE
