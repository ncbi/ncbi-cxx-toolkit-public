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
#include <misc/grpc_integration/grpc_integration.hpp>
#include <util/static_map.hpp>
#include <grpc++/server_context.h>
#include <grpc/support/alloc.h>
#include <grpc/support/host_port.h>
#include <grpc/support/log.h>

BEGIN_NCBI_SCOPE

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


unique_ptr<grpc::ClientContext>
CGRPCClientContext::FromServerContext(const grpc::ServerContext& sctx,
                                      grpc::PropagationOptions options)
{
    unique_ptr<grpc::ClientContext> result
        (grpc::ClientContext::FromServerContext(sctx, options));
    if (result.get() != NULL) {
        AddStandardNCBIMetadata(*result);
    }
    return result;
}

void CGRPCClientContext::AddStandardNCBIMetadata(grpc::ClientContext& cctx)
{
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
    pass_through.Enumerate([&](const string& name, const string& value) {
                               cctx.AddMetadata(s_EncodeMetadataName(name),
                                                s_EncodeMetadataValue(value));
                               return true;
                           });
}


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

grpc::StatusCode g_AsGRPCStatusCode(CRequestStatus::ECode status_code)
{
    TStatusCodeMap::const_iterator it = sc_ErrorCodeMap.find(status_code);
    if (it != sc_ErrorCodeMap.end()) {
        return it->second;
    } else if (status_code >= CRequestStatus::e100_Continue
               &&  status_code < CRequestStatus::e400_BadRequest) {
        return grpc::OK;
    } else {
        return grpc::UNKNOWN;
    }
}


typedef void (CRequestContext::*FRCSetter)(const string&);
typedef SStaticPair<const char*, FRCSetter> TRCSetterPair;
// Explicitly handle all four standard fields, since some may come in via
// legacy names and AddPassThroughProperty doesn't trigger interpretation
// and may well filter some or all of these fields out altogether.
static const TRCSetterPair sc_RCSetters[] = {
    { "dtab",           &CRequestContext::SetDtab      }, // legacy name
    { "ncbi_client_ip", &CRequestContext::SetClientIP  },
    { "ncbi_dtab",      &CRequestContext::SetDtab      },
    { "ncbi_phid",      &CRequestContext::SetHitID     },
    { "ncbi_sid",       &CRequestContext::SetSessionID },
    { "ncbiphid",       &CRequestContext::SetHitID     }, // legacy name
    { "sessionid",      &CRequestContext::SetSessionID }  // legacy name
};
typedef CStaticArrayMap<const char*, FRCSetter, PCase_CStr> TRCSetterMap;
DEFINE_STATIC_ARRAY_MAP(TRCSetterMap, sc_RCSetterMap, sc_RCSetters);

// TODO - can either of these log any more information, such as the
// service name, the status code, or the number of bytes in and/or out?
// (Byte counts appear to be tracked internally but not exposed. :-/)

void CGRPCServerCallbacks::BeginRequest(grpc::ServerContext* sctx)
{
    CDiagContext&    dctx = GetDiagContext();
    CRequestContext& rctx = dctx.GetRequestContext();
    char*            port = NULL;
    string           client_name, peer_ip;
    rctx.SetRequestID();
    if (sctx != NULL) {
        SIZE_TYPE pos = sctx->peer().find(':');
        if (pos != NPOS) {
            string peer = sctx->peer().substr(pos + 1);
            char *host  = NULL;
            gpr_split_host_port(peer.c_str(), &host, &port);
            if (host != NULL  &&  host[0] != '\0') {
                rctx.SetClientIP(host);
            }
            if (host != NULL) {
                gpr_free(host);
            }
        }
        for (const auto& metadata : sctx->client_metadata()) {
            CTempString nm(metadata.first .data(), metadata.first .size());
            CTempString vl(metadata.second.data(), metadata.second.size());
            string      name  = s_DecodeMetadataName (nm);
            string      value = s_DecodeMetadataValue(vl);
            rctx.AddPassThroughProperty(name, value);

            TRCSetterMap::const_iterator it
                = sc_RCSetterMap.find(name.c_str());
            if (it != sc_RCSetterMap.end()) {
                if (it->second == &CRequestContext::SetClientIP
                    &&  peer_ip.empty()  &&  rctx.GetClientIP() != value) {
                    peer_ip = rctx.GetClientIP();
                }
                (rctx.*(it->second))(value);
            } else if (metadata.first == "client") {
                client_name = value;
            }
        }
    }
    CDiagContext_Extra extra = dctx.PrintRequestStart();
    if ( !client_name.empty() ) {
        extra.Print("client_name", client_name);
    }
    if ( !peer_ip.empty() ) {
        extra.Print("peer_ip", peer_ip);
    }
    if (port != NULL) {
        extra.Print("client_port", port);
        gpr_free(port);
    }
}

void CGRPCServerCallbacks::EndRequest(grpc::ServerContext* context)
{
    GetDiagContext().PrintRequestStop();
}


END_NCBI_SCOPE
