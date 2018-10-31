#ifndef GRPC_INTEGRATION___GRPC_INTEGRATION__HPP
#define GRPC_INTEGRATION___GRPC_INTEGRATION__HPP

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

/// @file grpc_integration.hpp
/// Glue for integrating gRPC into the C++ toolkit, to ensure logging
/// and metadata propagation (sid, phid, auth tokens etc) are all
/// handled uniformly and appropriately.


#include <misc/grpc_integration/impl/grpc_support.hpp>
#include <grpc++/client_context.h>


/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// CGRPCClientContext -- client context for NCBI gRPC services.
///
/// The only difference from plain ClientContext is the automatic
/// inclusion of NCBI metadata via AddStandardNCBIMetadata.  As such,
/// if constructing a CGRPCClientContext is infeasible for any reason,
/// it's also possible to obtain a suitably configured ClientContext
/// by passing a ServerContext to FromServerContext or calling
/// AddStandardNCBIMetadata on an existing ClientContext (bearing in
/// mind that every actual remote procedure call requires a fresh context).
class CGRPCClientContext : public grpc::ClientContext
{
public:
    typedef grpc::ClientContext TParent;
    
    CGRPCClientContext(void)
        { AddStandardNCBIMetadata(*this); }

    static unique_ptr<TParent> FromServerContext(const grpc::ServerContext& sc,
                                                 grpc::PropagationOptions opts
                                                 = grpc::PropagationOptions());

    static void AddStandardNCBIMetadata(TParent& cctx);
};


/// BEGIN_NCBI_GRPC_REQUEST -- Set up AppLog logging for the current
/// (server-side) request handler.
#define BEGIN_NCBI_GRPC_REQUEST(sctx, request, reply) \
    CGRPCRequestLogger request_logger##__LINE__       \
        (sctx, NCBI_CURRENT_FUNCTION, request, reply)


/// NCBI_GRPC_RETURN -- Provide consistent gRPC request handler status
/// information to AppLog and gRPC.
#define NCBI_GRPC_RETURN(sc) do {                                       \
    CRequestStatus::ECode status_code = (sc);                           \
    GetDiagContext().GetRequestContext().SetRequestStatus(status_code); \
    return g_AsGRPCStatus(status_code);                                 \
} while (0);


/// Get "hostport" for the likes of "grpc::CreateChannel(hostport, ...)" trying
/// (in order of priority):
/// - Config file entry "[section] variable"
/// - Environment variables: env_var_name (if not empty/NULL);
///   then "NCBI_CONFIG__<section>__<name>"; then "grpc_proxy"
/// - The hard-coded NCBI default "linkerd:4142"
string g_NCBI_GRPC_GetAddress(const char* section,
                              const char* variable,
                              const char* env_var_name = nullptr);

END_NCBI_SCOPE


/* @} */

#endif  /* GRPC_INTEGRATION___GRPC_INTEGRATION__HPP */
