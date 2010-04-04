#ifndef CONNECT___NCBI_CONN_TEST__HPP
#define CONNECT___NCBI_CONN_TEST__HPP

/* $Id$
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
 * Author:  Anton Lavrentiev
 *
 * @file
 * File Description:
 *   NCBI connectivity test suite.
 *
 */

#include <corelib/ncbistd.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <ostream>
#include <string>
#include <vector>

/** @addtogroup Utility
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XCONNECT_EXPORT CConnTest : virtual public CConnIniter
{
public:
    enum EStage {
        eHttp,
        eDispatcher,
        eStatelessService,
        eFirewallConnPoints,
        eFirewallConnections,
        eStatefulService
    };

    CConnTest(const STimeout* timeout = kDefaultTimeout, ostream* out = 0);
    virtual ~CConnTest() { }

    virtual EIO_Status Execute(EStage& stage, string* reason = 0);

protected:
    struct CFWConnPoint {
        unsigned int   host;  ///< Network byte order
        unsigned short port;  ///< Host byte order
        bool           okay;  ///< True if okay

        bool operator < (const CFWConnPoint& p) const
        { return port < p.port; }
    };

    virtual EIO_Status HttpOkay          (string* reason);
    virtual EIO_Status DispatcherOkay    (string* reason);
    virtual EIO_Status ServiceOkay       (string* reason);
    virtual EIO_Status GetFWConnections  (string* reason);
    virtual EIO_Status CheckFWConnections(string* reason);
    virtual EIO_Status StatefulOkay      (string* reason);

    // User-defined callbacks
    virtual void       x_PreCheck (EStage stage, unsigned int substage,
                                   const string& title);

    virtual void       x_PostCheck(EStage stage, unsigned int substage,
                                   EIO_Status status, const string& reason);

    virtual EIO_Status x_ConnStatus(bool failure, CConn_IOStream& io);

    // Extended info of the last step IO
    const string& GetCheckPoint(void) const { return m_CheckPoint; }

    static const STimeout kTimeout;
    const STimeout*       m_Timeout;
    ostream*              m_Out;

    bool                  m_HttpProxy;
    bool                  m_Stateless;
    bool                  m_Firewall;
    bool                  m_FWProxy;
    bool                  m_Forced;

    vector<CFWConnPoint>  m_Fwd;

    bool                  m_End;
private:
    string                m_CheckPoint;
    STimeout              m_TimeoutValue;
};


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___NCBI_CONN_TEST__HPP */
