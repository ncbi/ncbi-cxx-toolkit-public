#ifndef GRPC_INTEGRATION_IMPL___GRPC_SUPPORT__HPP
#define GRPC_INTEGRATION_IMPL___GRPC_SUPPORT__HPP

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
#include <google/protobuf/message.h>
#include <grpc++/server.h>


/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE

grpc::Status     g_AsGRPCStatus    (CRequestStatus::ECode status_code);
grpc::StatusCode g_AsGRPCStatusCode(CRequestStatus::ECode status_code);


class CGRPCRequestLogger
{
public:
    typedef google::protobuf::Message TMessage;
    CGRPCRequestLogger(grpc::ServerContext* sctx, CTempString method_name,
                       const TMessage& request, const TMessage& reply);
    ~CGRPCRequestLogger();

private:
    CDiagContext&    m_DiagContext;
    CRequestContext& m_RequestContext;
    const TMessage&  m_Reply;
    bool             m_ManagingRequest;
};


class CGRPCServerCallbacks : public grpc::Server::GlobalCallbacks
{
public:
    void PreSynchronousRequest (grpc::ServerContext* sctx) override
        { BeginRequest(sctx); }
    void PostSynchronousRequest(grpc::ServerContext* sctx) override
        { EndRequest(sctx);   }

    // Static methods for use by any asynchronous service implementations,
    // which have no obvious provision for global hooks.
    static void BeginRequest(grpc::ServerContext* sctx);
    static void EndRequest  (grpc::ServerContext* sctx);
};


//////////////////////////////////////////////////////////////////////


inline
grpc::Status g_AsGRPCStatus(CRequestStatus::ECode status_code)
{
    if (status_code >= CRequestStatus::e100_Continue
        &&  status_code < CRequestStatus::e400_BadRequest) {
        return grpc::Status::OK;
    } else {
        return grpc::Status(g_AsGRPCStatusCode(status_code),
                            CRequestStatus::GetStdStatusMessage(status_code));
    }
}


inline
CGRPCRequestLogger::CGRPCRequestLogger(grpc::ServerContext* sctx,
                                       CTempString method_name,
                                       const TMessage& request,
                                       const TMessage& reply)
    : m_DiagContext(GetDiagContext()),
      m_RequestContext(m_DiagContext.GetRequestContext()),
      m_Reply(reply), m_ManagingRequest(false)
{
    if (m_DiagContext.GetAppState() < eDiagAppState_RequestBegin) {
        m_ManagingRequest = true;
        CGRPCServerCallbacks::BeginRequest(sctx);
    }
    m_DiagContext.Extra().Print("method_name", method_name);
    m_RequestContext.SetBytesRd(request.ByteSizeLong());
}

inline
CGRPCRequestLogger::~CGRPCRequestLogger()
{
    m_RequestContext.SetBytesWr(m_Reply.ByteSizeLong());
    if (m_ManagingRequest) {
        m_DiagContext.PrintRequestStop();
    }
}

END_NCBI_SCOPE


/* @} */

#endif  /* GRPC_INTEGRATION_IMPL___GRPC_SUPPORT__HPP */
