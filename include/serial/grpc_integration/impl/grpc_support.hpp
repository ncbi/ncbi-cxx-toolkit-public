#ifndef SERIAL_GRPC_INTEGRATION_IMPL___GRPC_SUPPORT__HPP
#define SERIAL_GRPC_INTEGRATION_IMPL___GRPC_SUPPORT__HPP

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
 */

/// @file grpc_support.hpp
/// Classes and functions supporting grpc_integration.[ch]pp that
/// nothing else is expected to use directly.


#include <corelib/ncbimtx.hpp>
#include <corelib/request_ctx.hpp>
#include <corelib/request_status.hpp>
#ifdef HAVE_LIBGRPC // HAVE_LIBPROTOBUF
#  include <google/protobuf/message.h>
#  if GOOGLE_PROTOBUF_VERSION >= 3002000
#    define NCBI_GRPC_GET_BYTE_SIZE(msg) ((msg).ByteSizeLong())
#  else
#    define NCBI_GRPC_GET_BYTE_SIZE(msg) ((msg).ByteSize())
#  endif
#else
namespace google {
    namespace protobuf {
        class Message {};
    }
}
#  define NCBI_GRPC_GET_BYTE_SIZE(msg) 0L
#endif
#ifdef HAVE_LIBGRPC
#  include <grpc++/server.h>
#  include <grpc++/server_context.h>
#else
namespace grpc {
    typedef int Status;
    typedef int StatusCode;
    class ServerContext {};
    class Server
    {
    public:
        class GlobalCallbacks
        {
            virtual void PreSynchronousRequest (grpc::ServerContext* sctx) = 0;
            virtual void PostSynchronousRequest(grpc::ServerContext* sctx) = 0;
        };
    };
}
#endif

namespace grpc_impl {
    namespace internal {
        class ServerStreamingInterface;
    }
}

/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE

#ifdef GRPCPP_IMPL_CODEGEN_SERVER_CONTEXT_IMPL_H
typedef grpc_impl::ServerContext TGRPCServerContext;
#else
typedef grpc::ServerContext TGRPCServerContext;
#endif

grpc::Status     g_AsGRPCStatus    (CRequestStatus::ECode status_code);
grpc::StatusCode g_AsGRPCStatusCode(CRequestStatus::ECode status_code);


class CGRPCRequestLogger
{
public:
    typedef google::protobuf::Message TMessage;
    CGRPCRequestLogger(TGRPCServerContext* sctx, CTempString method_name,
                       const TMessage& request, const TMessage& reply);
    CGRPCRequestLogger(TGRPCServerContext* sctx, CTempString method_name,
                       const TMessage& request,
                       const grpc_impl::internal::ServerStreamingInterface&);
    ~CGRPCRequestLogger();

private:
    void x_Init(TGRPCServerContext* sctx, CTempString method_name,
                const TMessage& request);
    
    CDiagContext&    m_DiagContext;
    CRequestContext& m_RequestContext;
#ifdef HAVE_LIBGRPC // HAVE_LIBPROTOBUF
    const TMessage*  m_Reply;
#endif
    bool             m_ManagingRequest;
};


class CGRPCServerCallbacks : public grpc::Server::GlobalCallbacks
{
public:
    /// How was BeginRequest or EndRequest called?
    enum EInvocationType {
        eImplicit, ///< From {Pre,Post}SynchronousRequest.
        eExplicit, ///< Via CGRPCRequestLogger or the like.
    };
    
    void PreSynchronousRequest (TGRPCServerContext* sctx) override
        { BeginRequest(sctx, eImplicit); }
    void PostSynchronousRequest(TGRPCServerContext* sctx) override
        { EndRequest(sctx, eImplicit);   }

    // Static methods for use by any asynchronous service implementations,
    // which have no obvious provision for global hooks.
    static void BeginRequest(TGRPCServerContext* sctx,
                             EInvocationType invocation_type = eExplicit);
    static void EndRequest  (TGRPCServerContext* sctx,
                             EInvocationType invocation_type = eExplicit);

private:
    static bool x_IsRealRequest(const TGRPCServerContext* sctx);
};


//////////////////////////////////////////////////////////////////////


inline
grpc::Status g_AsGRPCStatus(CRequestStatus::ECode status_code,
                            const CTempString& msg = nullptr)
{
#ifdef HAVE_LIBGRPC   
    if (status_code >= CRequestStatus::e100_Continue
        &&  status_code < CRequestStatus::e400_BadRequest) {
        return grpc::Status::OK;
    } else {
        grpc::StatusCode grpc_code = g_AsGRPCStatusCode(status_code);
        if (msg.empty()) {
            return grpc::Status
                (grpc_code, CRequestStatus::GetStdStatusMessage(status_code));
        } else {
            return grpc::Status(grpc_code, msg);
        }
    }
#else
    return status_code;
#endif
}


inline
CGRPCRequestLogger::CGRPCRequestLogger(TGRPCServerContext* sctx,
                                       CTempString method_name,
                                       const TMessage& request,
                                       const TMessage& reply)
    : m_DiagContext(GetDiagContext()),
      m_RequestContext(m_DiagContext.GetRequestContext()),
#ifdef HAVE_LIBGRPC // HAVE_LIBPROTOBUF
      m_Reply(&reply),
#endif
      m_ManagingRequest(false)
{
    x_Init(sctx, method_name, request);
}

inline
CGRPCRequestLogger::CGRPCRequestLogger(
    TGRPCServerContext* sctx, CTempString method_name, const TMessage& request,
    const grpc_impl::internal::ServerStreamingInterface&)
    : m_DiagContext(GetDiagContext()),
      m_RequestContext(m_DiagContext.GetRequestContext()),
#ifdef HAVE_LIBGRPC // HAVE_LIBPROTOBUF
      m_Reply(nullptr),
#endif
      m_ManagingRequest(false)
{
    x_Init(sctx, method_name, request);
}

inline
void CGRPCRequestLogger::x_Init(TGRPCServerContext* sctx,
                                CTempString method_name,
                                const TMessage& request)
{
    if (m_DiagContext.GetAppState() < eDiagAppState_RequestBegin) {
        m_ManagingRequest = true;
        CGRPCServerCallbacks::BeginRequest(sctx,
                                           CGRPCServerCallbacks::eExplicit);
    }
    m_DiagContext.Extra().Print("method_name", method_name);
    m_RequestContext.SetBytesRd(NCBI_GRPC_GET_BYTE_SIZE(request));
}

inline
CGRPCRequestLogger::~CGRPCRequestLogger()
{
    if ( !m_RequestContext.IsSetRequestStatus() ) {
        m_RequestContext.SetRequestStatus
            (CRequestStatus::e500_InternalServerError);
    }
#ifdef HAVE_LIBGRPC // HAVE_LIBPROTOBUF
    if (m_Reply != nullptr) {
        m_RequestContext.SetBytesWr(NCBI_GRPC_GET_BYTE_SIZE(*m_Reply));
    }
#endif
    if (m_ManagingRequest) {
        m_DiagContext.PrintRequestStop();
    }
}

END_NCBI_SCOPE


/* @} */

#endif  /* SERIAL_GRPC_INTEGRATION_IMPL___GRPC_SUPPORT__HPP */
