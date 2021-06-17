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
* Author:  Aaron Ucko
*
* File Description:
*   Glue for integrating gRPC into the C++ toolkit, to ensure logging
*   and metadata propagation (sid, phid, auth tokens etc) are all
*   handled uniformly and appropriately.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/grpc_integration/grpc_integration.hpp>
#include <corelib/ncbiapp.hpp>
#include <util/static_map.hpp>
#include <serial/error_codes.hpp>
#ifdef HAVE_LIBGRPC
#  include <grpc++/server_context.h>
#  include <grpc/support/alloc.h>
#  include <grpc/support/log.h>
#endif
#if defined(HAVE_LIBCONNEXT)  &&  defined(NCBI_OS_UNIX)
#  include <connect/ext/ncbi_ifconf.h>
#endif

#define NCBI_USE_ERRCODE_X Serial_GRPCIntegration

BEGIN_NCBI_SCOPE

#ifdef HAVE_LIBGRPC

static string s_EncodeMetadataName(const string& name)
{
    static const CTempString kLegalPunct("-_.");
    if (NStr::StartsWith(name, "ncbi_")) {
        if (name == "ncbi_dtab") {
            return "l5d-dtab";
        } else if (name == "ncbi_phid") {
            return "ncbi-phid";
        } else if (name == "ncbi_sid") {
            return "ncbi-sid";
        }
    }
    string result = name;
    NStr::ToLower(result);
    for (char& c : result) {
        if ( !isalnum((unsigned char) c)  &&  kLegalPunct.find(c) == NPOS) {
            c = '_';
        }
    }
    return result;
}

static string s_DecodeMetadataName(const CTempString& name)
{
    if (name == "l5d-dtab") {
        return "ncbi_dtab";
    } else if (name == "ncbi-phid") {
        return "ncbi_phid";
    } else if (name == "ncbi-sid") {
        return "ncbi_sid";
    } else {
        // NStr::ToUpper(name);
        return name;
    }
}

inline
static string s_EncodeMetadataValue(const string& value)
{
    return NStr::CEncode(value, NStr::eNotQuoted);
}

inline
static string s_DecodeMetadataValue(const CTempString& value)
{
    return NStr::CParse(value, NStr::eNotQuoted);
}

class CGRPCInitializer
{
public:
    CGRPCInitializer(void);

private:
    static grpc::Server::GlobalCallbacks* volatile sm_ServerCallbacks;
};

grpc::Server::GlobalCallbacks* volatile CGRPCInitializer::sm_ServerCallbacks;
static CGRPCInitializer s_GRPCInitializer;

extern "C" {
    static void s_NCBI_GPR_Log_Function(gpr_log_func_args *args) {
        static const char* unk_func = g_DiagUnknownFunction();
        CDiagCompileInfo diag_info(args->file, args->line, unk_func, "GRPC");
        EDiagSev sev = eDiag_Error;
        switch (args->severity) {
        case GPR_LOG_SEVERITY_DEBUG:  sev = eDiag_Trace;  break;
        case GPR_LOG_SEVERITY_INFO:   sev = eDiag_Info;   break;
        case GPR_LOG_SEVERITY_ERROR:  sev = eDiag_Error;  break;
        }
        CNcbiDiag(diag_info) << Severity(sev) << args->message << Endm;
    }
}

CGRPCInitializer::CGRPCInitializer(void)
{
    gpr_set_log_function(s_NCBI_GPR_Log_Function);
    sm_ServerCallbacks = new CGRPCServerCallbacks;
    grpc::Server::SetGlobalCallbacks(sm_ServerCallbacks);
    // NB: on the client side, we encourage the use of
    // CGRPCClientContext rather than setting global callbacks, to
    // avoid sending inappropriate metadata to non-NCBI services.
}

#endif


unique_ptr<TGRPCBaseClientContext>
CGRPCClientContext::FromServerContext(const TGRPCServerContext& sctx,
                                      TGRPCPropagationOptions options)
{
#ifdef HAVE_LIBGRPC
    unique_ptr<TGRPCBaseClientContext> result
        (TGRPCBaseClientContext::FromServerContext(sctx, options));
    if (result.get() != NULL) {
        AddStandardNCBIMetadata(*result);
    }
    return result;
#else
    return make_unique<CGRPCClientContext>();
#endif    
}

void CGRPCClientContext::AddStandardNCBIMetadata(TGRPCBaseClientContext& cctx)
{
#ifdef HAVE_LIBGRPC
    CDiagContext&    dctx = GetDiagContext();
    CRequestContext& rctx = dctx.GetRequestContext();
    cctx.set_initial_metadata_corked(true);
#if 1
    // Respectively redundant with ncbi_sid, ncbi_phid, and ncbi_dtab,
    // but kept for now for the sake of servers that only recognize
    // these legacy names.
    if (rctx.IsSetSessionID()) {
        cctx.AddMetadata("sessionid", rctx.GetSessionID());
    }
    cctx.AddMetadata("ncbiphid", rctx.GetNextSubHitID());
    cctx.AddMetadata("dtab",     rctx.GetDtab());
#endif
    cctx.AddMetadata("client",   dctx.GetAppName());
    
    CRequestContext_PassThrough pass_through;
    bool need_filter = pass_through.IsSet("PATH");
    if (need_filter) {
        ERR_POST_X_ONCE(1, Warning
                        << "No NCBI_CONTEXT_FIELDS or [Context] Fields"
                        " setting; performing ad-hoc filtering.");
    }
    pass_through.Enumerate
        ([&](const string& name, const string& value) {
             if ( !need_filter  ||  name == "http_proxy"
                 ||  NStr::StartsWith(name, "ncbi", NStr::eNocase)
                 ||  NStr::StartsWith(name, "l5d", NStr::eNocase)) {
                 cctx.AddMetadata(s_EncodeMetadataName(name),
                                  s_EncodeMetadataValue(value));
             }
             return true;
         });
#  if defined(HAVE_LIBCONNEXT)  &&  defined(NCBI_OS_UNIX)
    if ( !rctx.IsSetClientIP() ) {
        char buf[64];
        if (NcbiGetHostIP(buf, sizeof(buf)) != nullptr) {
            cctx.AddMetadata("ncbi_client_ip", buf);
        }
    }
#  endif
#endif
}

bool CGRPCClientContext::IsImplemented(void)
{
#ifdef HAVE_LIBGRPC
    return true;
#else
    return false;
#endif
}


#ifdef HAVE_LIBGRPC

typedef SStaticPair<CRequestStatus::ECode, grpc::StatusCode> TStatusCodePair;
#define TSCP(x, y) { CRequestStatus::x, grpc::y }
static const TStatusCodePair sc_ErrorCodes[] = {
    TSCP(e400_BadRequest,            INVALID_ARGUMENT),
    TSCP(e401_Unauthorized,          PERMISSION_DENIED), // UNAUTHENTICATED?
    TSCP(e402_PaymentRequired,       PERMISSION_DENIED),
    TSCP(e403_Forbidden,             PERMISSION_DENIED),
    TSCP(e404_NotFound,              NOT_FOUND),
    TSCP(e405_MethodNotAllowed,      UNIMPLEMENTED), // INVALID_ARGUMENT?
    TSCP(e406_NotAcceptable,         UNIMPLEMENTED), // INVALID_ARGUMENT?
    TSCP(e407_ProxyAuthRequired,     PERMISSION_DENIED), // UNAUTHENTICATED?
    TSCP(e408_RequestTimeout,        DEADLINE_EXCEEDED),
    TSCP(e409_Conflict,              ABORTED),
    TSCP(e410_Gone,                  NOT_FOUND),
    TSCP(e411_LengthRequired,        UNIMPLEMENTED), // INVALID_ARGUMENT?
    TSCP(e412_PreconditionFailed,    FAILED_PRECONDITION),
    TSCP(e413_RequestEntityTooLarge, RESOURCE_EXHAUSTED), // ?
    TSCP(e414_RequestURITooLong,     RESOURCE_EXHAUSTED), // ?
    TSCP(e415_UnsupportedMediaType,  UNIMPLEMENTED), // INVALID_ARGUMENT?
    TSCP(e416_RangeNotSatisfiable,   OUT_OF_RANGE),
    TSCP(e417_ExpectationFailed,     FAILED_PRECONDITION),
    TSCP(e422_UnprocessableEntity,   UNIMPLEMENTED), // INVALID_ARGUMENT?
    TSCP(e499_BrokenConnection,      CANCELLED),
    TSCP(e500_InternalServerError,   INTERNAL),
    TSCP(e501_NotImplemented,        UNIMPLEMENTED),
    TSCP(e502_BadGateway,            INTERNAL),
    TSCP(e503_ServiceUnavailable,    UNAVAILABLE),
    TSCP(e504_GatewayTimeout,        DEADLINE_EXCEEDED),
    TSCP(e505_HTTPVerNotSupported,   UNIMPLEMENTED) // INVALID_ARGUMENT?
};
typedef CStaticArrayMap<CRequestStatus::ECode, grpc::StatusCode> TStatusCodeMap;
DEFINE_STATIC_ARRAY_MAP(TStatusCodeMap, sc_ErrorCodeMap, sc_ErrorCodes);

#endif

grpc::StatusCode g_AsGRPCStatusCode(CRequestStatus::ECode status_code)
{
#ifdef HAVE_LIBGRPC
    TStatusCodeMap::const_iterator it = sc_ErrorCodeMap.find(status_code);
    if (it != sc_ErrorCodeMap.end()) {
        return it->second;
    } else if (status_code >= CRequestStatus::e100_Continue
               &&  status_code < CRequestStatus::e400_BadRequest) {
        return grpc::OK;
    } else {
        return grpc::UNKNOWN;
    }
#else
    return status_code;
#endif
}

#ifdef HAVE_LIBGRPC

// Work around inability to compare pointers to members.
static void s_SetDtab(CRequestContext& r, const string& s)
{
    r.SetDtab(s);
}
static void s_SetClientIP(CRequestContext& r, const string& s)
{
    r.SetClientIP(s);
}
static void s_SetHitID(CRequestContext& r, const string& s)
{
    r.SetHitID(s);
}
static void s_SetSessionID(CRequestContext& r, const string& s)
{
    r.SetSessionID(s);
}

typedef void (*FRCSetter)(CRequestContext&, const string&);
typedef SStaticPair<const char*, FRCSetter> TRCSetterPair;
// Explicitly handle all four standard fields, since some may come in via
// legacy names and AddPassThroughProperty doesn't trigger interpretation
// and may well filter some or all of these fields out altogether.
static const TRCSetterPair sc_RCSetters[] = {
    { "dtab",           &s_SetDtab      }, // legacy name
    { "ncbi_client_ip", &s_SetClientIP  },
    { "ncbi_dtab",      &s_SetDtab      },
    { "ncbi_phid",      &s_SetHitID     },
    { "ncbi_sid",       &s_SetSessionID },
    { "ncbiphid",       &s_SetHitID     }, // legacy name
    { "sessionid",      &s_SetSessionID }  // legacy name
};
typedef CStaticArrayMap<const char*, FRCSetter, PCase_CStr> TRCSetterMap;
DEFINE_STATIC_ARRAY_MAP(TRCSetterMap, sc_RCSetterMap, sc_RCSetters);

#endif

// TODO - can either of these log any more information, such as the
// service name, the status code, or the number of bytes in and/or out?
// (Byte counts appear to be tracked internally but not exposed. :-/)

void CGRPCServerCallbacks::BeginRequest(TGRPCServerContext* sctx,
                                        EInvocationType invocation_type)
{
    if (invocation_type == eImplicit  &&  !x_IsRealRequest(sctx)) {
        return;
    }
    
    CDiagContext&    dctx = GetDiagContext();
    CRequestContext& rctx = dctx.GetRequestContext();
    string           client_name, peer_ip, port;
    map<string, string> grpc_fields, env_fields;
    rctx.SetRequestID();
#ifdef HAVE_LIBGRPC
    if (sctx != NULL) {
        SIZE_TYPE pos = sctx->peer().find(':');
        if (pos != NPOS) {
            string peer = sctx->peer().substr(pos + 1);
            CTempString host;
            pos = peer.rfind(':');
            if (pos == NPOS  ||  peer[peer.size() - 1] == ']') {
                host = peer;
            } else {
                port = peer.substr(pos + 1);
                host.assign(peer, 0, pos);
                if (host[0] == '['  &&  host[pos - 1] == ']') {
                    host = host.substr(1, pos - 2);
                }
            }
            if ( !host.empty() ) {
                rctx.SetClientIP(host);
            }
        }
        map<FRCSetter, map<string, string>> rcsettings;
        for (const auto& metadata : sctx->client_metadata()) {
            CTempString nm(metadata.first .data(), metadata.first .size());
            CTempString vl(metadata.second.data(), metadata.second.size());
            string      name  = s_DecodeMetadataName (nm);
            string      value = s_DecodeMetadataValue(vl);
            rctx.AddPassThroughProperty(name, value);

            string name2 = name;
            NStr::ToLower(name2);
            NStr::ReplaceInPlace(name2, "_", "-");
            grpc_fields[name2] = value;

            TRCSetterMap::const_iterator it
                = sc_RCSetterMap.find(name.c_str());
            if (it != sc_RCSetterMap.end()) {
                if (it->second == &s_SetClientIP
                    &&  peer_ip.empty()  &&  rctx.GetClientIP() != value) {
                    peer_ip = rctx.GetClientIP();
                }
                rcsettings[it->second][name] = value;
            } else if (metadata.first == "client") {
                client_name = value;
            }
        }
        for (const auto& it : rcsettings) {
            const string* value = nullptr;
            if (it.second.size() == 1) {
                value = &it.second.begin()->second;
            } else {
                _ASSERT(it.second.size() == 2);
                const string *old_name = nullptr, *new_name = nullptr;
                for (const auto& it2 : it.second) {
                    if (NStr::StartsWith(it2.first, "ncbi_")) {
                        new_name = &it2.first;
                        value    = &it2.second;
                    } else {
                        old_name = &it2.first;
                    }
                }
                _ASSERT(old_name != nullptr);
                _ASSERT(new_name != nullptr  &&  value != nullptr);
                ERR_POST_X_ONCE(2, Info
                                << "Ignoring deprecated metadata field "
                                << *old_name << " in favor of " << *new_name);
            }
            (*(it.first))(rctx, *value);
        }
    }
#endif
    CDiagContext_Extra extra = dctx.PrintRequestStart();
    if ( !client_name.empty() ) {
        extra.Print("client_name", client_name);
    }
    if ( !peer_ip.empty() ) {
        extra.Print("peer_ip", peer_ip);
    }
    if ( !port.empty() ) {
        extra.Print("client_port", port);
    }
    extra.Flush();

    CNcbiApplication* app = CNcbiApplication::Instance();
    AutoPtr<CNcbiEnvironment> env;
    if (app == nullptr) {
        env.reset(new CNcbiEnvironment);
    } else {
        env.reset(const_cast<CNcbiEnvironment*>(&app->GetEnvironment()),
                  eNoOwnership);
    }
    list<string> env_names;
    env->Enumerate(env_names);
    for (const auto & it : env_names) {
        string name = it;
        NStr::ToLower(name);
        NStr::ReplaceInPlace(name, "_", "-");
        env_fields[name] = env->Get(it);
    }
    CNcbiLogFields("env").LogFields(env_fields);
    CNcbiLogFields("grpc").LogFields(grpc_fields);
}


void CGRPCServerCallbacks::EndRequest(TGRPCServerContext* sctx,
                                      EInvocationType invocation_type)
{
    if (invocation_type == eImplicit  &&  !x_IsRealRequest(sctx)) {
        return;
    }
    GetDiagContext().PrintRequestStop();
}


bool CGRPCServerCallbacks::x_IsRealRequest(const TGRPCServerContext* sctx)
{
#ifdef HAVE_LIBGRPC
    auto peer = sctx->peer();
    if ( !NStr::StartsWith(peer, "ipv4:127.")
        &&  !NStr::StartsWith(peer, "ipv6:[::1]") ) {
        return true;
    }
    for (const auto& it : sctx->client_metadata()) {
        CTempString name(it.first.data(), it.first.size());
        if (NStr::StartsWith(name, "ncbi")  ||  name == "sessionid") {
            // l5d-*? dtab?
            return true;
        }
    }
#endif
    return false;
}


/// Get "hostport" for the likes of "grpc::CreateChannel(hostport, ...)" trying
/// (in order of priority):
/// - Config file entry "[section] variable"
/// - Environment variables: env_var_name (if not empty/NULL);
///   then "NCBI_CONFIG__<section>__<name>"; then "GRPC_PROXY"
/// - The hard-coded NCBI default "linkerd:4142"
string g_NCBI_GRPC_GetAddress(const char* section,
                              const char* variable,
                              const char* env_var_name,
                              CParamBase::EParamSource* value_source)
{
    auto addr = g_GetConfigString(section, variable, env_var_name, nullptr, value_source);
    if ( addr.empty() ) {
        addr = g_GetConfigString(nullptr, nullptr, "GRPC_PROXY", "linkerd:4142", value_source);
    }
    return addr;
}


END_NCBI_SCOPE
