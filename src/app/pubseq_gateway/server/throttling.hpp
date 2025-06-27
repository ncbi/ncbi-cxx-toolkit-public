#ifndef PUBSEQ_GATEWAY_THROTTLING__HPP
#define PUBSEQ_GATEWAY_THROTTLING__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   PSG server throttling support
 *
 */


#include <map>
#include <chrono>
#include <mutex>
using namespace std;



// The structure holds the idling connection properties so it 
struct SPSGS_IdleConnectionProps
{
    // This is:
    // - last request timestamp (if there were any)
    // - opening timestamp (if there were no requests yet)
    chrono::system_clock::time_point    m_LastActivityTimestamp;
    string                              m_PeerIP;
    optional<string>                    m_PeerID;
    optional<string>                    m_UserAgent;

    // Worker thread ID which serves the connection
    unsigned int                        m_WorkerID;

    // Connection ID
    int64_t                             m_ConnID;

    // Some connections could be in a process of closing while they are
    // still in the list
    bool                                m_CloseIssued;

    SPSGS_IdleConnectionProps(const chrono::system_clock::time_point &  ts,
                              const string &  peer_ip,
                              const optional<string> &  peer_id,
                              const optional<string> &  user_agent,
                              unsigned int  worker_id,
                              int64_t  conn_id) :
        m_LastActivityTimestamp(ts), m_PeerIP(peer_ip), m_PeerID(peer_id),
        m_UserAgent(user_agent), m_WorkerID(worker_id), m_ConnID(conn_id),
        m_CloseIssued(false)
    {}
};


struct SThrottlingData
{
    list<SPSGS_IdleConnectionProps>     m_IdleConnProps;

    // Below are the data for 4 criteria:
    // - a map: it is temporary, used during the collection time
    // - a list: formed out of the map; includes only those items which
    //           exceeded the corresponding thresholds

    map<string, size_t>         m_PeerIPCounts;
    list<string>                m_PeerIPOverLimit;

    map<string, size_t>         m_PeerSiteCounts;
    list<string>                m_PeerSiteOverLimit;

    map<string, size_t>         m_PeerIDCounts;
    list<string>                m_PeerIDOverLimit;

    map<string, size_t>         m_UserAgentCounts;
    list<string>                m_UserAgentOverLimit;

    size_t                      m_TotalConns;

    SThrottlingData() :
        m_TotalConns(0)
    {}

    void Clear(void)
    {
        m_IdleConnProps.clear();
        m_PeerIPCounts.clear();
        m_PeerIPOverLimit.clear();
        m_PeerSiteCounts.clear();
        m_PeerSiteOverLimit.clear();
        m_PeerIDCounts.clear();
        m_PeerIDOverLimit.clear();
        m_UserAgentCounts.clear();
        m_UserAgentOverLimit.clear();
        m_TotalConns = 0;
    }
};


#endif /* PUBSEQ_GATEWAY_THROTTLING__HPP */

