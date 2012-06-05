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

#include "ns_types.hpp"
#include "job_status.hpp"
#include "ns_command_arguments.hpp"


BEGIN_NCBI_SCOPE


// Forward for CJob/CJobEvent friendship
class CQueue;
class CNetScheduleHandler;
class CNSAffinityRegistry;
class CNSGroupsRegistry;


// Used to specify what to fetch and what to include into a transaction
enum EQueueJobTable {
    eJobTable       = 1,
    eJobInfoTable   = 2,
    eJobEventsTable = 4,
    eAffinityTable  = 8,
    eGroupTable     = 16,
    eAllTables      = eJobTable | eJobInfoTable |
                      eJobEventsTable | eAffinityTable |
                      eGroupTable
};



// Instantiation of a Job on a Worker Node
class CJob;
class CJobEvent
{
public:
    // Events which can trigger state change
    enum EJobEvent {
        eUnknown = -1,  // Used for initialisation

        eSubmit = 0,    // SUBMIT
        eBatchSubmit,   // Batch submit
        eRequest,       // GET, WGET, JXCG
        eDone,          // PUT, JXCG
        eReturn,        // RETURN
        eFail,          // FPUT
        eRead,          // READ
        eReadFail,      // FRED
        eReadDone,      // CFRM
        eReadRollback,  // RDRB
        eClear,         // CLRN

        eCancel,        // CANCEL
        eTimeout,       // exec timeout
        eReadTimeout,   // read timeout
        eSessionChanged // client has changed session
    };

    // Converts event code into its string representation
    static std::string EventToString(EJobEvent  event);

public:
    CJobEvent();
    // setters/getters
    TJobStatus GetStatus() const
    { return m_Status; }
    EJobEvent GetEvent() const
    { return m_Event; }
    unsigned GetTimestamp() const
    { return m_Timestamp; }
    unsigned GetNodeAddr() const
    { return m_NodeAddr; }
    int      GetRetCode() const
    { return m_RetCode; }
    const string& GetClientNode() const
    { return m_ClientNode; }
    const string& GetClientSession() const
    { return m_ClientSession; }
    const string& GetErrorMsg() const
    { return m_ErrorMsg; }
    const string GetQuotedErrorMsg() const
    { return "'" + NStr::PrintableString(m_ErrorMsg) + "'"; }

    void SetStatus(TJobStatus status);
    void SetEvent(EJobEvent  event);
    void SetTimestamp(time_t t);
    void SetNodeAddr(unsigned node_ip);
    void SetRetCode(int retcode);
    void SetClientNode(const string& client_node);
    void SetClientSession(const string& client_session);
    void SetErrorMsg(const string& msg);

private:
    friend class CJob;
    // Service fields
    bool            m_Dirty;

    // SEventDB fields
    // id, event id - implicit
    TJobStatus      m_Status;           ///< Job status after the event
    EJobEvent       m_Event;            ///< Event
    unsigned        m_Timestamp;        ///< event timestamp
    unsigned        m_NodeAddr;         ///< IP of a client (typically, worker node)
    int             m_RetCode;          ///< Return code
    string          m_ClientNode;       ///< Client node id
    string          m_ClientSession;    ///< Client session
    string          m_ErrorMsg;         ///< Error message (exception::what())
};



time_t  GetJobExpirationTime(time_t      last_touch,
                             TJobStatus  status,
                             time_t      job_submit_time,
                             time_t      job_timeout,
                             time_t      job_run_timeout,
                             time_t      queue_timeout,
                             time_t      queue_run_timeout,
                             time_t      queue_pending_timeout,
                             time_t      event_time);


// Internal representation of a Job
// mirrors database tables SQueueDB, SJobInfoDB, and SEventsDB
class CJob
{
public:
    // Parts of objects residing in different DB tables
    enum EPart {
        fJobPart     = 1 << 0, ///< SQueueDB part
        fJobInfoPart = 1 << 1, ///< SJobInfoDB part
        fEventsPart  = 1 << 2  ///< SEventsDB part
    };
    enum EJobFetchResult {
        eJF_Ok       = 0,
        eJF_NotFound = 1,
        eJF_DBErr    = 2
    };
    enum EAuthTokenCompareResult {
        eCompleteMatch = 0,
        ePassportOnlyMatch = 1,
        eNoMatch = 2,

        eInvalidTokenFormat = 3
    };

    CJob();
    CJob(const SNSCommandArguments &  request);

    // Getter/setters
    unsigned       GetId() const
    { return m_Id; }
    unsigned int   GetPassport() const
    { return m_Passport; }
    string         GetAuthToken() const
    { return NStr::IntToString(m_Passport) + "_" +
             NStr::SizetToString(m_Events.size()); }
    TJobStatus     GetStatus() const
    { return m_Status; }
    time_t         GetTimeSubmit() const
    { _ASSERT(!m_Events.empty());
      return m_Events[0].m_Timestamp; }
    time_t         GetTimeout() const
    { return m_Timeout; }
    unsigned       GetRunTimeout() const
    { return m_RunTimeout; }

    unsigned       GetSubmAddr() const
    { _ASSERT(!m_Events.empty());
      return m_Events[0].m_NodeAddr; }
    unsigned short GetSubmNotifPort() const
    { return m_SubmNotifPort; }
    unsigned       GetSubmNotifTimeout() const
    { return m_SubmNotifTimeout; }

    unsigned       GetRunCount() const
    { return m_RunCount; }
    unsigned       GetReadCount() const
    { return m_ReadCount; }
    const string &  GetProgressMsg() const
    { return m_ProgressMsg; }

    unsigned       GetAffinityId() const
    { return m_AffinityId; }

    unsigned       GetMask() const
    { return m_Mask; }
    unsigned       GetGroupId() const
    { return m_GroupId; }
    time_t         GetLastTouch() const
    { return m_LastTouch; }
    const string&  GetClientIP() const
    { return m_ClientIP; }
    const string&  GetClientSID() const
    { return m_ClientSID; }

    const vector<CJobEvent>& GetEvents() const
    { return m_Events; }
    const string&  GetInput() const
    { return m_Input; }
    const string GetQuotedInput() const
    { return "'" + NStr::PrintableString(m_Input) + "'"; }
    const string&  GetOutput() const
    { return m_Output; }
    const string GetQuotedOutput() const
    { return "'" + NStr::PrintableString(m_Output) + "'"; }

    string GetErrorMsg() const;
    int    GetRetCode() const;

    void           SetId(unsigned id)
    { m_Id = id;
      m_Dirty |= fJobPart; }
    void           SetPassport(unsigned int  passport)
    { m_Passport = passport;
      m_Dirty |= fJobPart; }
    void           SetStatus(TJobStatus status)
    { m_Status = status;
      m_Dirty |= fJobPart; }
    void           SetTimeout(time_t t)
    { m_Timeout = (unsigned) t;
      m_Dirty |= fJobPart; }
    void           SetRunTimeout(time_t t)
    { m_RunTimeout = (unsigned) t;
      m_Dirty |= fJobPart; }

    void           SetSubmNotifPort(unsigned short port)
    { m_SubmNotifPort = port;
      m_Dirty |= fJobPart; }
    void           SetSubmNotifTimeout(unsigned t)
    { m_SubmNotifTimeout = t;
      m_Dirty |= fJobPart; }

    void           SetRunCount(unsigned count)
    { m_RunCount = count;
      m_Dirty |= fJobPart; }
    void           SetReadCount(unsigned count)
    { m_ReadCount = count;
      m_Dirty |= fJobPart;
    }
    void           SetProgressMsg(const string& msg)
    { m_ProgressMsg = msg;
      m_Dirty |= fJobPart; }
    void           SetAffinityId(unsigned aff_id)
    { m_AffinityId = aff_id;
      m_Dirty |= fJobPart; }
    void           SetMask(unsigned mask)
    { m_Mask = mask;
      m_Dirty |= fJobPart; }
    void           SetGroupId(unsigned id)
    { m_GroupId = id;
      m_Dirty |= fJobPart; }
    void           SetLastTouch(time_t  t)
    { m_LastTouch = t;
      m_Dirty |= fJobPart; }

    void           SetClientIP(const string& client_ip)
    { m_ClientIP = client_ip;
      m_Dirty |= fJobPart; }
    void           SetClientSID(const string& client_sid)
    { m_ClientSID = client_sid;
      m_Dirty |= fJobPart; }

    void           SetEvents(const vector<CJobEvent>& events)
    { m_Events = events;
      m_Dirty |= fEventsPart; }

    void           SetInput(const string& input);
    void           SetOutput(const string& output);


    // manipulators
    CJobEvent &         AppendEvent();
    const CJobEvent *   GetLastEvent() const;
    CJobEvent *         GetLastEvent();

    time_t          GetSubmitTime(void) const
    { return m_Events[0].m_Timestamp; }

    time_t          GetExpirationTime(time_t  queue_timeout,
                                      time_t  queue_run_timeout,
                                      time_t  queue_pending_timeout,
                                      time_t  event_time) const
    {
       return GetJobExpirationTime(m_LastTouch, m_Status,
                                   GetSubmitTime(),
                                   m_Timeout, m_RunTimeout,
                                   queue_timeout, queue_run_timeout,
                                   queue_pending_timeout,
                                   event_time);
    }

    EAuthTokenCompareResult  CompareAuthToken(const string &  auth_token) const;

    // Mark job for deletion
    void Delete();

    // Fetch/flush
    // Fetch current object
    EJobFetchResult Fetch(CQueue* queue);
    // Fetch object by its numeric id
    EJobFetchResult Fetch(CQueue* queue, unsigned id);

    // Cursor like functionality - not here yet. May be we need
    // to create separate CJobIterator.
    // EJobFetchResult FetchNext(CQueue* queue);
    bool Flush(CQueue* queue);

    // Should we notify submitter in the moment of time 'curr'
    bool ShouldNotify(time_t curr);

    void Print(CNetScheduleHandler &        handler,
               const CQueue &               queue,
               const CNSAffinityRegistry &  aff_registry,
               const CNSGroupsRegistry &    group_registry) const;

private:
    // Service flags
    bool                m_New;     ///< Object should be inserted, not updated
    bool                m_Deleted; ///< Object with this id should be deleted
    unsigned            m_Dirty;

    // Reflection of database structures

    // Reside in SJobDB table
    unsigned            m_Id;
    unsigned int        m_Passport;
    TJobStatus          m_Status;
    unsigned            m_Timeout;       ///< Individual timeout
    unsigned            m_RunTimeout;    ///< Job run timeout

    unsigned short      m_SubmNotifPort;    ///< Submit notification port
    unsigned            m_SubmNotifTimeout; ///< Submit notification timeout

    unsigned            m_RunCount;
    unsigned            m_ReadCount;
    string              m_ProgressMsg;
    unsigned            m_AffinityId;
    unsigned            m_Mask;
    unsigned            m_GroupId;
    time_t              m_LastTouch;

    string              m_ClientIP;
    string              m_ClientSID;

    // Resides in SEventsDB table
    vector<CJobEvent>   m_Events;

    // Reside in SJobInfoDB table (input and output - if over limit)
    string              m_Input;
    string              m_Output;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_JOB__HPP */

