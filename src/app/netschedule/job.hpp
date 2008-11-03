#ifndef NETSCHEDULE_JOB__HPP
#define NETSCHEDULE_JOB__HPP

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
 *   NetSchedule job
 *
 */

#include <connect/services/netschedule_api.hpp>

//#include "ns_types.hpp"
#include "job_status.hpp"


BEGIN_NCBI_SCOPE


typedef pair<string, string> TNSTag;
typedef list<TNSTag> TNSTagList;

// Forward for CJob/CJobRun friendship
struct SLockedQueue;

// Instantiation of a Job on a Worker Node
class CJob;
class CJobRun
{
public:
    CJobRun();
    // setters/getters
    TJobStatus GetStatus() const 
    { return m_Status; }
    unsigned GetTimeStart() const
    { return m_TimeStart; }
    unsigned GetTimeDone() const
    { return m_TimeDone; }
    unsigned GetNodeAddr() const
    { return m_NodeAddr; }
    unsigned short GetNodePort() const
    { return m_NodePort; }
    int      GetRetCode() const
    { return m_RetCode; }
    const string& GetNodeId() const
    { return m_NodeId; }
    const string& GetErrorMsg() const
    { return m_ErrorMsg; }

    void SetStatus(TJobStatus status);
    void SetTimeStart(unsigned t);
    void SetTimeDone(unsigned t);
    void SetNodeAddr(unsigned node_ip);
    void SetNodePort(unsigned short port);
    void SetRetCode(int retcode);
    void SetNodeId(const string& node_id);
    void SetErrorMsg(const string& msg);

    // generic access via field name
    static
        int GetFieldIndex(const string& name);
    string GetField(int index) const;

private:
    friend class CJob;
    // Service fields
    bool      m_Dirty;

    // SRunsDB fields
    // id, run - implicit
    TJobStatus m_Status;      ///< final job status for this run
    unsigned   m_TimeStart;   ///< job start time
    unsigned   m_TimeDone;    ///< job result submission time
    unsigned   m_NodeAddr;    ///< IP of a client (typically, worker node)
    unsigned short m_NodePort; ///< Notification port of a client
    int        m_RetCode;     ///< Return code
    string     m_NodeId;      //
    string     m_ErrorMsg;    ///< Error message (exception::what())
};

class CBDB_Transaction;

// Internal representation of a Job
// mirrors database tables SQueueDB, SJobInfoDB, and SRunsDB
class CJob
{
public:
    // Parts of objects residing in different DB tables
    enum EPart {
        fJobPart     = 1 << 0, ///< SQueueDB part
        fJobInfoPart = 1 << 1, ///< SJobInfoDB part
        fRunsPart    = 1 << 2  ///< SRunsDB part
    };
    enum EJobFetchResult {
        eJF_Ok = 0,
        eJF_NotFound = 1,
        eJF_DBErr = 2
    };
    CJob();

    // Getter/setters
    unsigned       GetId() const
    { return m_Id; }
    TJobStatus     GetStatus() const 
    { return m_Status; }
    unsigned       GetTimeSubmit() const
    { return m_TimeSubmit; }
    unsigned       GetTimeout() const
    { return m_Timeout; }
    unsigned       GetRunTimeout() const
    { return m_RunTimeout; }

    unsigned       GetSubmAddr() const
    { return m_SubmAddr; }
    unsigned short GetSubmPort() const
    { return m_SubmPort; }
    unsigned       GetSubmTimeout() const
    { return m_SubmTimeout; }

    unsigned       GetRunCount() const
    { return m_RunCount; }
    unsigned       GetReadGroup() const
    { return m_ReadGroup; }
    const string&  GetProgressMsg() const
    { return m_ProgressMsg; }

    unsigned       GetAffinityId() const
    { return m_AffinityId; }
    bool           HasAffinityToken() const
    { return m_AffinityToken.size(); }
    const string&  GetAffinityToken() const
    { return m_AffinityToken; }

    unsigned       GetMask() const
    { return m_Mask; }
    const string&  GetClientIP() const
    { return m_ClientIP; }
    const string&  GetClientSID() const
    { return m_ClientSID; }

    const vector<CJobRun>& GetRuns() const
    { return m_Runs; }
    const TNSTagList& GetTags() const
    { return m_Tags; }
    const string&  GetInput() const
    { return m_Input; }
    const string&  GetOutput() const
    { return m_Output; }

    void           SetId(unsigned id);
    void           SetStatus(TJobStatus status);
    void           SetTimeSubmit(unsigned t);
    void           SetTimeout(unsigned t);
    void           SetRunTimeout(unsigned t);

    void           SetSubmAddr(unsigned addr);
    void           SetSubmPort(unsigned short port);
    void           SetSubmTimeout(unsigned t);

    void           SetRunCount(unsigned count);
    void           SetReadGroup(unsigned group);
    void           SetProgressMsg(const string& msg);
    void           SetAffinityId(unsigned aff_id);
    void           SetAffinityToken(const string& aff_token);
    void           SetMask(unsigned mask);

    void           SetClientIP(const string& client_ip);
    void           SetClientSID(const string& client_sid);

    void           SetRuns(const vector<CJobRun>& runs);
    void           SetTags(const TNSTagList& tags);
    void           SetTags(const string& strtags);
    void           SetInput(const string& input);
    void           SetOutput(const string& output);

    // generic access via field name
    static int     GetFieldIndex(const string& name);
    string         GetField(int index) const;

    // manipulators
    CJobRun&       AppendRun();
    const CJobRun* GetLastRun() const;
    CJobRun*       GetLastRun();

    // preparatory
    // Lookup affinity token and fill out affinity id
    void CheckAffinityToken(SLockedQueue*     queue,
        CBDB_Transaction* trans);
    // Using affinity id fill out affinity token
    void FetchAffinityToken(SLockedQueue*     queue);

    // Mark job for deletion
    void Delete();

    // Fetch/flush
    // Fetch current object
    EJobFetchResult Fetch(SLockedQueue* queue);
    // Fetch object by its numeric id
    EJobFetchResult Fetch(SLockedQueue* queue, unsigned id);
    // Cursor like functionality - not here yet. May be we need
    // to create separate CJobIterator.
    // EJobFetchResult FetchNext(SLockedQueue* queue);
    bool Flush(SLockedQueue* queue);

    // Should we notify submitter in the moment of time 'curr'
    bool ShouldNotify(time_t curr);
private:
    void x_ParseTags(const string& strtags, TNSTagList& tags);
    // Service flags
    bool     m_New;     ///< Object should be inserted, not updated
    bool     m_Deleted; ///< Object with this id should be deleted
    unsigned m_Dirty;

    // Reflection of database structures

    // Reside in SQueueDB table
    unsigned        m_Id;
    TJobStatus      m_Status;
    unsigned        m_TimeSubmit;    ///< Job submit time
    unsigned        m_Timeout;       ///<     individual timeout
    unsigned        m_RunTimeout;    ///<     job run timeout

    unsigned        m_SubmAddr;      ///< netw BO (for notification)
    unsigned short  m_SubmPort;      ///< notification port
    unsigned        m_SubmTimeout;   ///< notification timeout

    unsigned        m_RunCount;      ///< since last reschedule
    unsigned        m_ReadGroup;
    string          m_ProgressMsg;

    unsigned        m_AffinityId;
    string          m_AffinityToken;

    unsigned        m_Mask;

    string          m_ClientIP;
    string          m_ClientSID;

    // Resides in SRunsDB table
    vector<CJobRun> m_Runs;

    // Reside in SJobInfoDB table (input and output - if over limit)
    TNSTagList      m_Tags;
    string          m_Input;
    string          m_Output;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_JOB__HPP */
