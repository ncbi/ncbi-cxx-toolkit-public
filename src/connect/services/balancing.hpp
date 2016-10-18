#ifndef CONNECT_SERVICES___BALANCING__HPP
#define CONNECT_SERVICES___BALANCING__HPP

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
 * File Description:
 *   Contains definitions related to server discovery by the load balancer.
 *
 * Authors:
 *   Dmitry Kazimirov
 *
 */

#include <connect/services/srv_connections.hpp>

BEGIN_NCBI_SCOPE

class CSimpleRebalanceStrategy : public CObject
{
public:
    CSimpleRebalanceStrategy(int rebalance_requests, int rebalance_time) :
        m_RebalanceRequests(rebalance_requests),
        m_RebalanceTime(rebalance_time),
        m_RequestCounter(0),
        m_NextRebalanceTime(CTime::eEmpty)
    {
    }

    bool NeedRebalance();

    void OnResourceRequested() {
        CFastMutexGuard g(m_Mutex);
        ++m_RequestCounter;
    }
    void Reset() {
        CFastMutexGuard g(m_Mutex);
        m_RequestCounter = 0;
        m_NextRebalanceTime.Clear();
    }

private:
    int     m_RebalanceRequests;
    int     m_RebalanceTime;
    int     m_RequestCounter;
    CTime   m_NextRebalanceTime;
    CFastMutex m_Mutex;
};

class CConfig;

NCBI_XCONNECT_EXPORT CRef<CSimpleRebalanceStrategy>
    CreateSimpleRebalanceStrategy(CConfig& config, const string& driver_name);

NCBI_XCONNECT_EXPORT CRef<CSimpleRebalanceStrategy>
    CreateDefaultRebalanceStrategy();

END_NCBI_SCOPE

#endif  /* CONNECT_SERVICES___BALANCING__HPP */
