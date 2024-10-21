#ifndef PSG_MYNCBI__MYNCBI_FACTORY__HPP
#define PSG_MYNCBI__MYNCBI_FACTORY__HPP

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
 * Authors: Dmitrii Saprykin
 *
 * File Description: MyNCBIAccount request factory
 *
 */

#include <corelib/ncbistl.hpp>

#include <atomic>
#include <chrono>
#include <string>

#include <objtools/pubseq_gateway/impl/myncbi/myncbi_request.hpp>
#include <objtools/pubseq_gateway/impl/myncbi/myncbi_exception.hpp>

#include <uv.h>

BEGIN_NCBI_SCOPE

class CPSG_MyNCBIFactory final
{
public:
    static atomic_bool sVerboseLogging;

    static bool InitGlobal(string& error);
    static void CleanupGlobal();

    void SetRequestTimeout(chrono::milliseconds timeout);
    chrono::milliseconds GetRequestTimeout() const;

    void SetResolveTimeout(chrono::milliseconds timeout);
    chrono::milliseconds GetResolveTimeout() const;

    void SetMyNCBIURL(string url);
    string GetMyNCBIURL() const;

    void SetHttpProxy(string proxy);
    string GetHttpProxy() const;

    void SetVerboseCURL(bool value);
    bool IsVerboseCURL() const;

    // Performs DNS resolution for HttpProxy if configured
    //   otherwise performs DNS resolution for MyNCBIURL hostname
    // @throws CPSG_MyNCBIException
    void ResolveAccessPoint();

    shared_ptr<CPSG_MyNCBIRequest_WhoAmI> CreateWhoAmI(
        uv_loop_t * loop,
        string cookie,
        CPSG_MyNCBIRequest_WhoAmI::TConsumeCallback consume_cb,
        TDataErrorCallback error_cb
    );
    shared_ptr<CPSG_MyNCBIRequest_SignIn> CreateSignIn(
        uv_loop_t * loop,
        string username, string password,
        TDataErrorCallback error_cb
    );

    // Executes WhoAmI request synchronously
    // {loop} needs to be initialized/finalized in the same thread
    //     as ExecuteWhoAmI() will call {uv_run(loop, UV_RUN_DEFAULT)}
    SPSG_MyNCBISyncResponse<CPSG_MyNCBIRequest_WhoAmI::TResponse> ExecuteWhoAmI(uv_loop_t * loop, string cookie);

private:
    chrono::milliseconds m_RequestTimeout{chrono::milliseconds(1000)};
    chrono::milliseconds m_ResolveTimeout{chrono::milliseconds(300)};
    string m_MyNCBIUrl;
    string m_HttpProxy;
    bool m_VerboseCURL{false};
};

END_NCBI_SCOPE

#endif  // PSG_MYNCBI__MYNCBI_FACTORY__HPP
