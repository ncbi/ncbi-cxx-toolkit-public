#ifndef CONNECT__NCBI_HTTP2_SESSION__HPP
#define CONNECT__NCBI_HTTP2_SESSION__HPP

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
 * Authors: Rafael Sadyrov
 *
 */

#include "ncbi_http_session.hpp"


BEGIN_NCBI_SCOPE


/// @sa CHttpSession_Base
class NCBI_XXCONNECT2_EXPORT CHttp2Session : public CHttpSession_Base
{
public:
    CHttp2Session();

    /// Get an API lock.
    /// Holding this API lock is essential if numerous short-lived session instances are used.
    /// It prevents an internal I/O implementation (TCP connections, HTTP sessions, etc)
    /// from being destroyed (on destroying last remaining session instance)
    /// and then re-created (with new session instance).
    using TApiLock = shared_ptr<void>;
    static TApiLock GetApiLock();

private:
    void x_StartRequest(CHttpSession_Base::EProtocol protocol, CHttpRequest& req, bool use_form_data) override;
    bool x_Downgrade(CHttpResponse& resp, CHttpSession_Base::EProtocol& protocol) const override;

    static void UpdateResponse(CHttpRequest& req, CHttpHeaders::THeaders headers);

    TApiLock m_ApiLock;
};




END_NCBI_SCOPE


#endif
