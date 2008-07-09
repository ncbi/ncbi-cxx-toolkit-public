#ifndef NETSCHEDULE_WORKER_NODE__HPP
#define NETSCHEDULE_WORKER_NODE__HPP

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
 * Authors:  Victor Joukov
 *
 * File Description:
 *   NetSchedule worker node
 *
 */

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include "ns_types.hpp"

#include <list>
#include <vector>

BEGIN_NCBI_SCOPE


// Old worker node notification structures, to be replaced with CWorkerNode
// Notification and worker node management
// (host, port) key into TWorkerNodes map
typedef pair<unsigned, unsigned short> TWorkerNodeHostPort;
// value for TWorkerNodes map
struct SWorkerNodeInfo
{
    SWorkerNodeInfo() :
        last_visit(0), visit_timeout(0), notify_time(0)
    { } 
    SWorkerNodeInfo(time_t curr, unsigned timeout, const string& auth_) :
        last_visit(curr), visit_timeout(3600),
        auth(auth_), notify_time(curr + timeout)
    { }
    void Set(time_t curr, unsigned timeout, const string& auth_)
    {
        last_visit = curr;
        notify_time = curr + timeout;
        auth = auth_;
    }
    void Set(time_t curr, unsigned timeout)
    {
        last_visit = curr;
        notify_time = curr + timeout;
    }

    // Session management
    time_t last_visit;      ///< Last time client executed worker node command
    unsigned visit_timeout; ///< When last visit should be expired
    string auth;            ///< Authentication string
    // Notification management
    time_t notify_time;     ///< Up to what time client expects notifications
};
typedef map<TWorkerNodeHostPort, SWorkerNodeInfo> TWorkerNodes;




class CWorkerNode
{
public:
private:
    unsigned         m_Host;
    unsigned short   m_Port;
    string           m_Id;
    vector<unsigned> m_Jobs;

    // Session management
    time_t last_visit;      ///< Last time client executed worker node command
    unsigned visit_timeout; ///< When last visit should be expired
    string auth;            ///< Authentication string
    // Notification management
    time_t notify_time;     ///< Up to what time client expects notifications
    
};


class CQueueWorkerNodes
{
public:
    CQueueWorkerNodes();
    // Returns true and adds entries to notify_list if it decides that it's OK to notify.
    bool GetNotifyList(bool unconditional, time_t curr,
                       int notify_timeout, list<TWorkerNodeHostPort>& notify_list);
    void GetNodesInfo(time_t curr, int run_timeout, list<string>& nodes_info) const;
    void RegisterNotificationListener(unsigned        host,
                                      unsigned short  port,
                                      unsigned        timeout,
                                      const string&   auth);
    void UnRegisterNotificationListener(unsigned       host,
                                        unsigned short port);
    void RegisterWorkerNodeVisit(unsigned       host_addr,
                                 unsigned short udp_port,
                                 unsigned       timeout);
private:
    time_t                       m_LastNotifyTime; ///< last notification time
    TWorkerNodes                 m_WorkerNodes;
    mutable CRWLock              m_WNodeLock;   ///< wnodes locker
};

END_NCBI_SCOPE

#endif /* NETSCHEDULE_WORKER_NODE__HPP */
