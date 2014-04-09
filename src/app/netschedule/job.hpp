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
#include "ns_db.hpp"
#include "job_status.hpp"
#include "ns_command_arguments.hpp"
#include "ns_precise_time.hpp"


BEGIN_NCBI_SCOPE


// Forward for CJob/CJobEvent friendship
class CQueue;
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
        eUnknown = -1,      // Used for initialisation

        eSubmit = 0,        // SUBMIT
        eBatchSubmit,       // Batch submit
        eRequest,           // GET, WGET, JXCG
        eDone,              // PUT, JXCG
        eReturn,            // RETURN
        eFail,              // FPUT
        eRead,              // READ
        eReadFail,          // FRED
        eReadDone,          // CFRM
        eReadRollback,      // RDRB
        eClear,             // CLRN

        eCancel,            // CANCEL
        eTimeout,           // exec timeout
        eReadTimeout,       // read timeout
        eSessionChanged,    // client has changed session

        eNSSubmitRollback,  // NS cancelled the job because
                            // of a network error when the job
                            // ID is sent to the client
        eNSGetRollback,     // NS returns the job because
                            // of a network error when the job
                            // ID is sent to the client
        eNSReadRollback,    // NS rolls back reading because
                            // of a network error when the job
                            // ID is sent to the client
        eReturnNoBlacklist  // WN asked to return a job without
                            // adding it to the WN blacklist
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
    CNSPreciseTime GetTimestamp() const
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

    void SetStatus(TJobStatus status)
    { m_Dirty = true;
      m_Status = status; }
    void SetEvent(EJobEvent  event)
    { m_Dirty = true;
      m_Event = event; }
    void SetTimestamp(const CNSPreciseTime & t)
    { m_Dirty = true;
      m_Timestamp = t; }
    void SetNodeAddr(unsigned int  node_ip)
    { m_Dirty = true;
      m_NodeAddr = node_ip; }
    void SetRetCode(int retcode)
    { m_Dirty = true;
      m_RetCode = retcode; }
    void SetClientNode(const string& client_node);
    void SetClientSession(const string& client_session);
    void SetErrorMsg(const string& msg);

private:
    friend class CJob;
    // Service fields
    bool            m_Dirty;

    // SEventDB fields
    // id, event id - implicit
    TJobStatus      m_Status;           // Job status after the event
    EJobEvent       m_Event;            // Event
    CNSPreciseTime  m_Timestamp;        // event timestamp
    unsigned        m_NodeAddr;         // IP of a client (typically, worker node)
    int             m_RetCode;          // Return code
    string          m_ClientNode;       // Client node id
    string          m_ClientSession;    // Client session
    string          m_ErrorMsg;         // Error message (exception::what())
};



CNSPreciseTime
GetJobExpirationTime(const CNSPreciseTime &   last_touch,
                     TJobStatus               status,
                     const CNSPreciseTime &   job_submit_time,
                     const CNSPreciseTime &   job_timeout,
                     const CNSPreciseTime &   job_run_timeout,
                     const CNSPreciseTime &   queue_timeout,
                     const CNSPreciseTime &   queue_run_timeout,
                     const CNSPreciseTime &   queue_pending_timeout,
                     const CNSPreciseTime &   event_time);


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
    { return NStr::NumericToString(m_Passport) + "_" +
             NStr::NumericToString(m_Events.size()); }
    TJobStatus     GetStatus() const
    { return m_Status; }
    CNSPreciseTime GetTimeout() const
    { return m_Timeout; }
    CNSPreciseTime GetRunTimeout() const
    { return m_RunTimeout; }

    unsigned       GetSubmAddr() const
    { return m_Events[0].m_NodeAddr; }
    unsigned short GetSubmNotifPort() const
    { return m_SubmNotifPort; }
    CNSPreciseTime GetSubmNotifTimeout() const
    { return m_SubmNotifTimeout; }

    unsigned int   GetListenerNotifAddr() const
    { return m_ListenerNotifAddress; }
    unsigned short GetListenerNotifPort() const
    { return m_ListenerNotifPort; }

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
    CNSPreciseTime GetLastTouch() const
    { return m_LastTouch; }
    const string&  GetClientIP() const
    { return m_ClientIP; }
    const string&  GetClientSID() const
    { return m_ClientSID; }
    const string&  GetNCBIPHID() const
    { return m_NCBIPHID; }

    const vector<CJobEvent>& GetEvents() const
    { return m_Events; }
    size_t  GetLastEventIndex(void) const
    { return m_Events.size() - 1; }
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
    void           SetTimeout(const CNSPreciseTime & t)
    { m_Timeout = t;
      m_Dirty |= fJobPart; }
    void           SetRunTimeout(const CNSPreciseTime & t)
    { m_RunTimeout = t;
      m_Dirty |= fJobPart; }

    void           SetSubmNotifPort(unsigned short port)
    { m_SubmNotifPort = port;
      m_Dirty |= fJobPart; }
    void           SetSubmNotifTimeout(const CNSPreciseTime & t)
    { m_SubmNotifTimeout = t;
      m_Dirty |= fJobPart; }

    void           SetListenerNotifAddr(unsigned int  address)
    { m_ListenerNotifAddress = address;
      m_Dirty |= fJobPart; }
    void           SetListenerNotifPort(unsigned short  port)
    { m_ListenerNotifPort = port;
      m_Dirty |= fJobPart; }
    void           SetListenerNotifAbsTime(const CNSPreciseTime & abs_time)
    { m_ListenerNotifAbsTime = abs_time;
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
    void           SetLastTouch(const CNSPreciseTime &  t)
    { m_LastTouch = t;
      m_Dirty |= fJobPart; }

    void           SetClientIP(const string& client_ip)
    { if (client_ip.size() < kMaxClientIpSize) m_ClientIP = client_ip;
      else m_ClientIP = client_ip.substr(0, kMaxClientIpSize);
      m_Dirty |= fJobPart; }
    void           SetClientSID(const string& client_sid)
    { if (client_sid.size() < kMaxSessionIdSize) m_ClientSID = client_sid;
      else m_ClientSID = "SID_TOO_LONG_FOR_NS_STORAGE";
      m_Dirty |= fJobPart; }
    void           SetNCBIPHID(const string& ncbi_phid)
    { if (ncbi_phid.size() < kMaxHitIdSize) m_NCBIPHID = ncbi_phid;
      else {
        if (ncbi_phid.find('.') == string::npos)
            m_NCBIPHID = "PHID_TOO_LONG_FOR_NS_STORAGE";
        else
            m_NCBIPHID = ncbi_phid.substr(0, kMaxHitIdSize);
      }
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

    CNSPreciseTime      GetSubmitTime(void) const
    { return m_Events[0].m_Timestamp; }
    CNSPreciseTime      GetLastEventTime(void) const
    { return m_Events[GetLastEventIndex()].m_Timestamp; }

    CNSPreciseTime      GetExpirationTime(
                                const CNSPreciseTime &  queue_timeout,
                                const CNSPreciseTime &  queue_run_timeout,
                                const CNSPreciseTime &  queue_pending_timeout,
                                const CNSPreciseTime &  event_time) const
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

    // Fetch object by its numeric id
    EJobFetchResult Fetch(CQueue* queue, unsigned id);

    // Cursor like functionality - not here yet. May be we need
    // to create separate CJobIterator.
    // EJobFetchResult FetchNext(CQueue* queue);
    bool Flush(CQueue* queue);

    bool ShouldNotifySubmitter(const CNSPreciseTime & current_time) const;
    bool ShouldNotifyListener(const CNSPreciseTime &  current_time,
                              TNSBitVector &          jobs_to_notify) const;

    string Print(const CQueue &               queue,
                 const CNSAffinityRegistry &  aff_registry,
                 const CNSGroupsRegistry &    group_registry) const;

private:
    EJobFetchResult x_Fetch(CQueue* queue);

private:
    // Service flags
    bool                m_New;     // Object should be inserted, not updated
    bool                m_Deleted; // Object with this id should be deleted
    unsigned            m_Dirty;

    // Reflection of database structures

    // Reside in SJobDB table
    unsigned            m_Id;
    unsigned int        m_Passport;
    TJobStatus          m_Status;
    CNSPreciseTime      m_Timeout;       // Individual timeout
    CNSPreciseTime      m_RunTimeout;    // Job run timeout

    unsigned short      m_SubmNotifPort;    // Submit notification port
    CNSPreciseTime      m_SubmNotifTimeout; // Submit notification timeout

    unsigned int        m_ListenerNotifAddress;
    unsigned short      m_ListenerNotifPort;
    CNSPreciseTime      m_ListenerNotifAbsTime;

    unsigned            m_RunCount;
    unsigned            m_ReadCount;
    string              m_ProgressMsg;
    unsigned            m_AffinityId;
    unsigned            m_Mask;
    unsigned            m_GroupId;
    CNSPreciseTime      m_LastTouch;

    string              m_ClientIP;
    string              m_ClientSID;
    string              m_NCBIPHID;

    // Resides in SEventsDB table
    vector<CJobEvent>   m_Events;

    // Reside in SJobInfoDB table (input and output - if over limit)
    string              m_Input;
    string              m_Output;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_JOB__HPP */

