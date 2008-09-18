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

#include <map>
#include <list>
#include <set>

BEGIN_NCBI_SCOPE

struct SWorkerNodeInfo
{
    SWorkerNodeInfo() : host(0), port(0) {}
    string          node_id;
    string          auth;
    unsigned        host;
    unsigned short  port;
};


// Notification and worker node management
// (host, port) key into TWorkerNodes map
typedef pair<unsigned, unsigned short> TWorkerNodeHostPort;
typedef list<TNSJobId> TJobList;


// CWorkerNode keeps information about worker node which is on the other
// end of network connection. There are two styles of worker node: old style
// is found when it issues first worker node-specific command. It leads to call
// to CQueueWorkerNodeList::RegisterNodeVisit and node gets generated node id.
// New style node explicitly sets both its id and port with INIT command.
class CWorkerNode : public CObject
{
public:
	// Constructor for the new style worker nodes, called from INIT command
    CWorkerNode(const string& node_id, time_t curr,
        const string& auth, unsigned host, unsigned short port);
	// Constructor for the old style worker nodes, called at the first WN-specific
	// command. Port is set later when one of GET, WGET, REGC commands mention it.
    // Called from CQueueWorkerNodeList::RegisterNodeVisit
	CWorkerNode(const string& node_id, time_t curr,
        const string& auth, unsigned host);
    // Ditto, but being called from CQueueWorkerNodeList::AddJob have even less
    // info to fill out.
    CWorkerNode(const string& node_id, time_t curr);
    // Set authentication token and host - we're doing it from RegisterNodeVisit
    void SetClientInfo(const string& auth, unsigned host);
    void SetNotificationTimeout(time_t curr, unsigned timeout);
    bool ShouldNotify(time_t curr) const;
	time_t ValidityTime() const;
    void UpdateValidityTime();
	string AsString(time_t curr) const;
private:
    friend class CQueueWorkerNodeList;
    string           m_Id;
    string           m_Auth;
	bool             m_NewStyle;
    unsigned         m_Host;
    unsigned short   m_Port;

    typedef map<TNSJobId, time_t> TWNJobInfoMap;
    TWNJobInfoMap m_Jobs;

    // Copied over from old SWorkerNodeInfo
    // Session management
	// On every update of m_Jobs, maximum of their validity time is recorded into
	// m_JobValidityTime. Then we can figure if node is still valid by comparing
	// maximum of (m_LastVisit+m_VisitTimeout), m_JobValidityTime and m_NotifyTime
	// with current time.
    time_t   m_LastVisit;    ///< Last time client executed worker node command
    unsigned m_VisitTimeout; ///< When last visit should be expired
	time_t   m_JobValidityTime; ///< Max of jobs validity times
    // Notification management
    /// Up to what time client expects notifications, if 0 - do not notify
    time_t   m_NotifyTime;
};


class CQueueWorkerNodeList
{
public:
    CQueueWorkerNodeList();

    // Register node with host, port and unique node id. If there was another
    // node with different id but the same (host, port) - append that node's
    // jobs to job_list, so that they can be safely failed and possibly retried.
    void RegisterNode(const SWorkerNodeInfo& node_info, TJobList& jobs);
    // Unregister node, return its jobs
    void ClearNode(const string& node_id, TJobList& jobs);
    
    // Add job to worker node job list
    void AddJob(SWorkerNodeInfo& node_info, TNSJobId job_id, time_t exp_time);
    // Remove job from worker node job list
    void RemoveJob(const SWorkerNodeInfo& node_info, TNSJobId job_id);

    // Returns true and adds entries to notify_list if it decides that it's OK to notify.
    bool GetNotifyList(bool unconditional, time_t curr,
                       int notify_timeout, list<TWorkerNodeHostPort>& notify_list);
    void GetNodesInfo(time_t curr, list<string>& nodes_info) const;

    void RegisterNotificationListener(const string&  node_id,
                                      unsigned short port,
                                      unsigned       timeout);
	// In the case of old style job, returns true, and node needs its affinity,
	// cleared and jobs which ran on the node (and returned in 'jobs' out
	// parameter) should be failed.
    bool UnRegisterNotificationListener(const string& node_id, TJobList& jobs);

    // Update node liveness time-out. If node is old-fashioned, it did not
    // register with INIT command, so we assign the node server-generated id.
    void RegisterNodeVisit(SWorkerNodeInfo& node_info);
private:
	typedef map<TWorkerNodeHostPort, string> THostPortIdx;
	typedef map<string, CRef<CWorkerNode> >  TWorkerNodeById;

	void x_ClearNode(const string& node_id, TJobList& jobs);
	void x_ClearNode(TWorkerNodeById::iterator& it, TJobList& jobs);
    void x_GenerateNodeId(string& node_id);

    time_t          m_LastNotifyTime;

    THostPortIdx    m_HostPortIdx;
    TWorkerNodeById m_WorkerNodeById;

    CAtomicCounter  m_GeneratedIdCounter;
    string          m_HostName;
    mutable CRWLock m_Lock;
};

END_NCBI_SCOPE

#endif /* NETSCHEDULE_WORKER_NODE__HPP */
