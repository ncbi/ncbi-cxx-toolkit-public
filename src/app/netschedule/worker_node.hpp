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
#include "ns_util.hpp"
#include "job.hpp"

#include <map>
#include <list>
#include <set>

#include <vector>

BEGIN_NCBI_SCOPE


/// Affinity association information
struct SAffinityInfo
{
    // These members are unsynchronized and should only be used through
    // synchronizing accessor - CWorkerNodeAffinityGuard
    TNSBitVector  aff_ids;          ///< List of affinity tokens
    TNSBitVector  candidate_jobs;   ///< List of job candidates for this node
    TNSBitVector  blacklisted_jobs; ///< List of jobs, blacklisted for node
    // Blacklist expiration handling
    /// We should not bother revise the list before this time
    time_t        min_expire_time;
    typedef vector<pair<time_t, unsigned> > TExpirations;
    TExpirations blacklisted_expirations;

    SAffinityInfo()
        : aff_ids(bm::BM_GAP), candidate_jobs(bm::BM_GAP),
        blacklisted_jobs(bm::BM_GAP)
    {}
    const TNSBitVector& GetBlacklistedJobs(time_t t);
    void BlacklistJob(unsigned job_id, time_t exp_time);
};


// Notification and worker node management
typedef pair<unsigned, unsigned short>  TWorkerNodeHostPort;
typedef list<TNSJobId>                  TJobList;


class CWorkerNode;
struct SJobInfo
{
    SJobInfo(TNSJobId id, time_t t, CWorkerNode* node) :
        job_id(id), exp_time(t), assigned_node(node)
    {}

    // Only for making search images
    SJobInfo(TNSJobId id) : job_id(id)
    {}

    TNSJobId                        job_id;
    time_t                          exp_time;
    CWorkerNode*                    assigned_node;
};


struct SCompareJobIds
{
    bool operator()(const SJobInfo* r1, const SJobInfo* r2) const
    {
        return r1->job_id < r2->job_id;
    }
};


typedef set<SJobInfo*, SCompareJobIds> TJobInfoById;

// Worker node methods
enum EWNodeFormat {
    eWNF_Old = 0,
    eWNF_WithNodeId
};

class CQueueWorkerNodeList;
class CQueueWorkerNodeListGuard;
class CWorkerNodeAffinityGuard;


// CWorkerNode keeps information about worker node which is on the other
// end of network connection. There are two styles of worker node: old style
// is found when it issues first worker node-specific command.
// New style node explicitly sets both its id and port with INIT command.
class CWorkerNode : public CObject
{
public:
    // Special constructors for making search images.
    CWorkerNode(const string& node_id) :
        m_Id(node_id)
    {}
    CWorkerNode(unsigned host, unsigned short port) :
        m_Host(host), m_Port(port)
    {}

    // Set authentication token
    void SetAuth(const string& auth);
    void SetNotificationTimeout(unsigned timeout);
    bool ShouldNotify(time_t curr) const;
    time_t ValidityTime() const;
    void UpdateValidityTime();
    string AsString(time_t curr, EWNodeFormat fmt) const;

    const string& GetId() const {return m_Id;}
    unsigned GetHost() const {return m_Host;}
    unsigned short GetPort() const {return m_Port;}

    bool IsRegistered() const {return m_Registered;}
    bool IsIdentified() const {return m_Port > 0;}

protected:
    friend class CQueueWorkerNodeList;
    friend class CWorkerNodeAffinityGuard;
    friend class CQueueWorkerNodeListGuard;

    // Locking order for node is ALWAYS m_WorkerNodeList.m_Lock (Read or Write,
    // depending on purpose) and then m_Lock.
    CQueueWorkerNodeList *  m_WorkerNodeList;
    CFastMutex              m_Lock;

    string                  m_Id;
    string                  m_Auth;
    unsigned                m_Host;
    unsigned short          m_Port;

    // Interim, for compatibility only
    SAffinityInfo           m_AffinityInfo;
    TJobInfoById            m_JobInfoById;

    // Copied over from old SWorkerNodeInfo
    // Session management
    // On every update of m_JobInfoById, maximum of their validity time is recorded into
    // m_JobValidityTime. Then we can figure if node is still valid by comparing
    // maximum of (m_LastVisit+m_VisitTimeout), m_JobValidityTime and m_NotifyTime
    // with current time.
    time_t   m_LastVisit;    ///< Last time client executed worker node command
    unsigned m_VisitTimeout; ///< When last visit should be expired
    time_t   m_JobValidityTime; ///< Max of jobs validity times
    // Notification management
    /// Up to what time client expects notifications, if 0 - do not notify
    time_t   m_NotifyTime;

    bool     m_Registered;
    // TODO Must be removed when all worker nodes become "new-style".
    bool     m_NewStyle;
};


class CWorkerNode_Real : public CWorkerNode
{
public:
    // Universal constructor for both new and old nodes
    CWorkerNode_Real(unsigned host);

    // The destructor removes this worker node from the
    // CQueueWorkerNodeList register.
    virtual ~CWorkerNode_Real();
};


struct SCompareNodeAddress
{
    bool operator()(const CWorkerNode* l, const CWorkerNode* r) const
    {
        return l->GetHost() < r->GetHost() ||
            (l->GetHost() == r->GetHost() && l->GetPort() < r->GetPort());
    }
};


struct SCompareNodeId
{
    bool operator()(const CWorkerNode* l, const CWorkerNode* r) const
    {
        return l->GetId() < r->GetId();
    }
};



class IAffinityResolver
{
public:
    virtual ~IAffinityResolver() {}
    virtual void GetJobsWithAffinities(const TNSBitVector& aff_id_set,
                                       TNSBitVector*       jobs) = 0;
};


// Synchronizing accessor to WorkerNode methods. Through using both
// WorkerNodeList and WorkerNode locks it provides reliable access
// with low lock pressure.
class CWorkerNodeAffinityGuard
{
public:
    CWorkerNodeAffinityGuard(CWorkerNode& wn);
    ~CWorkerNodeAffinityGuard();
    bool HasCandidates();
    // Mask worker node affinities to aff_ids and if candidate vector
    // contains jobs, not belonging to the result, flush it.
    void CleanCandidates(const TNSBitVector& aff_ids);
    const TNSBitVector& GetAffinities(time_t t);
    const TNSBitVector& GetBlacklistedJobs(time_t t);
    void AddAffinity(unsigned aff_id, time_t exp_time);
    unsigned GetJobWithAffinities(const TNSBitVector* aff_ids,
                                  CJobStatusTracker& status_tracker,
                                  IAffinityResolver& affinity_resolver);
    void BlacklistJob(unsigned job_id, time_t exp_time);

private:
    CWorkerNode& m_WorkerNode;
};




// According to log system requirements, completion codes should follow
// HTTP convention. 2xx - success, 400 - failure
enum ENSCompletion {
    eNSCDone     = 200,
    eNSCFailed   = 400,
    eNSCReturned = 201,
    eNSCTimeout  = 401
};

typedef CRef<CWorkerNode>                           TWorkerNodeRef;
typedef set<TWorkerNodeRef>                         TWorkerNodeRegister;
typedef set<CWorkerNode*, SCompareNodeAddress>      TWorkerNodeByAddress;
typedef set<CWorkerNode*, SCompareNodeId>           TWorkerNodeById;


class CQueueWorkerNodeList
{
public:
    CQueueWorkerNodeList(const string& queue_name);

    // Register a new worker node.
    void RegisterNode(CWorkerNode* worker_node);
    // Remove the specified node from the register.
    void UnregisterNode(CWorkerNode* worker_node);
    // Return jobs assigned to the worker node and clear the assignment.
    void ClearNode(CWorkerNode* worker_node, TJobList& jobs);
    // Do the same but also return the worker node object with the
    // specified node_id, if it exists.
    TWorkerNodeRef ClearNode(const string& node_id, TJobList& jobs);

    void SetId(CWorkerNode* worker_node, const string& node_id);
    void SetPort(CWorkerNode* worker_node, unsigned short port);

    // Add job to worker node job list
    void AddJob(CWorkerNode* worker_node, const CJob& job, time_t exp_time);
    // Update job expiration time
    void UpdateJob(CQueue *  q, TNSJobId  job_id, time_t  exp_time);
    // Remove job from worker node job list
    void RemoveJob(CQueue *  q, const CJob &  job,
                   ENSCompletion reason);

    SJobInfo* FindJobById(TNSJobId job_id);

    CWorkerNode* FindWorkerNodeByAddress(unsigned host, unsigned short port);
    CWorkerNode* FindWorkerNodeById(const string& node_id);

    // Returns true and adds entries to notify_list if it decides that it's OK to notify.
    bool GetNotifyList(bool unconditional, time_t curr,
                       int notify_timeout, list<TWorkerNodeHostPort>& notify_list);
    // Get all active nodes
    void GetNodes(time_t t, list<TWorkerNodeRef>& nodes) const;

    void RegisterNotificationListener(CWorkerNode*           worker_node,
                                      unsigned               timeout);
    // In the case of old style job, returns true, and node needs its affinity,
    // cleared and jobs which ran on the node should be failed.
    bool UnRegisterNotificationListener(CWorkerNode* worker_node);

    // Replaces temporary_wn with a pointer to an "identified" node
    // if it exists. Otherwise, makes this temporary_wn identified
    // by assigning the port number to it and updates the indices.
    void IdentifyWorkerNodeByAddress(
        TWorkerNodeRef& temporary_wn,
        unsigned short port);

    // This method can leave the temporary worker node "unidentified"
    // if job_id belongs to another (or the same) temporary worker node.
    void IdentifyWorkerNodeByJobId(
        TWorkerNodeRef& temporary_wn,
        TNSJobId job_id);

    ~CQueueWorkerNodeList();

private:
    friend class CWorkerNodeAffinityGuard;
    friend class CQueueWorkerNodeListGuard;
    // Move all jobs assigned to the specified worker node to the 'jobs' list.
    void DisplaceWorkerNodeJobs(CWorkerNode* worker_node, TJobList& jobs);
    void MergeWorkerNodes(TWorkerNodeRef& temporary, CWorkerNode* identified);

    //void x_GenerateNodeId(string& node_id);

    string                  m_QueueName;
    time_t                  m_LastNotifyTime;

    TWorkerNodeRegister     m_WorkerNodeRegister;
    TWorkerNodeByAddress    m_WorkerNodeByAddress;
    TWorkerNodeById         m_WorkerNodeById;

    TJobInfoById            m_JobInfoById;

    CAtomicCounter          m_GeneratedIdCounter;
    string                  m_HostName;
    time_t                  m_StartTime;
    mutable CRWLock         m_Lock;
};


class CQueueWorkerNodeListGuard
{
public:
    CQueueWorkerNodeListGuard(CQueueWorkerNodeList& wnl);
    ~CQueueWorkerNodeListGuard();
    void GetNodes(time_t t, list<TWorkerNodeRef>& nodes) const;
    // Get affinities for the node 'wn'
    const TNSBitVector& GetNodeAffinities(time_t t, CWorkerNode* wn);
    // Get all assigned affinities for the list ORed into 'aff_ids'
    void GetAffinities(time_t t, TNSBitVector& aff_ids);

private:
    CQueueWorkerNodeList& m_WorkerNodeList;
};

END_NCBI_SCOPE

#endif /* NETSCHEDULE_WORKER_NODE__HPP */

