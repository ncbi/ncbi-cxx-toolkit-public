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
#include "ns_precise_time.hpp"


BEGIN_NCBI_SCOPE


// Forward for CJob/CJobEvent friendship
class CQueue;
class CNSAffinityRegistry;
class CNSGroupsRegistry;
struct SJobDumpHeader;


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
        eFinalFail,         // FPUT2 with no_retries=1
        eRead,              // READ
        eReadFail,          // FRED
        eReadFinalFail,     // FRED with no_retries=1
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
        eReturnNoBlacklist, // WN asked to return a job without
                            // adding it to the WN blacklist
        eReschedule,        // RESCHEDULE
        eRedo,              // REDO
        eReread             // REREAD
    };

    // Converts event code into its string representation
    static std::string EventToString(EJobEvent  event);

public:
    CJobEvent();
    CJobEvent(const CJobEvent &) = default;
    CJobEvent &  operator=(const CJobEvent &) = default;
    CJobEvent(CJobEvent&&) = default;
    CJobEvent &  operator=(CJobEvent &&) = default;

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
    { m_Status = status; }
    void SetEvent(EJobEvent  event)
    { m_Event = event; }
    void SetTimestamp(const CNSPreciseTime & t)
    { m_Timestamp = t; }
    void SetNodeAddr(unsigned int  node_ip)
    { m_NodeAddr = node_ip; }
    void SetRetCode(int retcode)
    { m_RetCode = retcode; }
    // The size of the client_node is normalized at the handshake stage
    void SetClientNode(const string &  client_node)
    { m_ClientNode = client_node; }
    // The size of the client_session is normalized at the handshake stage
    void SetClientSession(const string &  cliet_session)
    { m_ClientSession = cliet_session; }
    // The size of the error message is truncated (if needed) at the
    // command parameters processing stage
    void SetErrorMsg(const string &  msg)
    { m_ErrorMsg = msg; }

private:
    friend class CJob;

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
                     const CNSPreciseTime &   job_read_timeout,
                     const CNSPreciseTime &   queue_timeout,
                     const CNSPreciseTime &   queue_run_timeout,
                     const CNSPreciseTime &   queue_read_timeout,
                     const CNSPreciseTime &   queue_pending_timeout,
                     const CNSPreciseTime &   event_time);


// Internal representation of a Job
// mirrors database tables SQueueDB, SJobInfoDB, and SEventsDB
class CJob
{
public:
    enum EAuthTokenCompareResult {
        eCompleteMatch = 0,
        ePassportOnlyMatch = 1,
        eNoMatch = 2,

        eInvalidTokenFormat = 3
    };

    CJob();
    CJob(const SNSCommandArguments &  request);
    CJob(const CJob &) = default;
    CJob &  operator=(const CJob &) = default;
    CJob(CJob &&) = default;
    CJob &  operator=(CJob &&) = default;

    // Getter/setters
    unsigned       GetId() const
    { return m_Id; }
    unsigned int   GetPassport() const
    { return m_Passport; }
    string         GetAuthToken() const
    { return to_string(m_Passport) + "_" + to_string(m_Events.size()); }
    TJobStatus     GetStatus() const
    { return m_Status; }
    CNSPreciseTime GetTimeout() const
    { return m_Timeout; }
    CNSPreciseTime GetRunTimeout() const
    { return m_RunTimeout; }
    CNSPreciseTime GetReadTimeout() const
    { return m_ReadTimeout; }

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
    bool  GetSubmNeedProgressMsgNotif() const
    { return m_NeedSubmProgressMsgNotif; }
    bool  GetLsnrNeedProgressMsgNotif() const
    { return m_NeedLsnrProgressMsgNotif; }
    bool  GetNeedStolenNotif() const
    { return m_NeedStolenNotif; }
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

    void SetId(unsigned id)
    { m_Id = id; }
    void SetPassport(unsigned int  passport)
    { m_Passport = passport; }
    void SetStatus(TJobStatus status)
    { m_Status = status; }
    void SetTimeout(const CNSPreciseTime & t)
    { m_Timeout = t; }
    void SetRunTimeout(const CNSPreciseTime & t)
    { m_RunTimeout = t; }
    void SetReadTimeout(const CNSPreciseTime & t)
    { m_ReadTimeout = t; }

    void SetSubmNotifPort(unsigned short port)
    { m_SubmNotifPort = port; }
    void SetSubmNotifTimeout(const CNSPreciseTime & t)
    { m_SubmNotifTimeout = t; }

    void SetListenerNotifAddr(unsigned int  address)
    { m_ListenerNotifAddress = address; }
    void SetListenerNotifPort(unsigned short  port)
    { m_ListenerNotifPort = port; }
    void SetListenerNotifAbsTime(const CNSPreciseTime & abs_time)
    { m_ListenerNotifAbsTime = abs_time; }

    void SetRunCount(unsigned count)
    { m_RunCount = count; }
    void SetReadCount(unsigned count)
    { m_ReadCount = count; }
    void SetProgressMsg(const string& msg)
    { m_ProgressMsg = msg; }
    void SetAffinityId(unsigned aff_id)
    { m_AffinityId = aff_id; }
    void SetMask(unsigned mask)
    { m_Mask = mask; }
    void SetGroupId(unsigned id)
    { m_GroupId = id; }
    void SetLastTouch(const CNSPreciseTime &  t)
    { m_LastTouch = t; }

    void SetClientIP(const string& client_ip)
    { m_ClientIP = client_ip; }
    void SetClientSID(const string& client_sid)
    { m_ClientSID = client_sid; }
    void SetNCBIPHID(const string& ncbi_phid)
    { m_NCBIPHID = ncbi_phid; }
    void SetNeedSubmProgressMsgNotif(bool  need)
    { m_NeedSubmProgressMsgNotif = need; }
    void SetNeedLsnrProgressMsgNotif(bool  need)
    { m_NeedLsnrProgressMsgNotif = need; }
    void SetNeedStolenNotif(bool  need)
    { m_NeedStolenNotif = need; }

    void SetEvents(const vector<CJobEvent>& events)
    { m_Events = events; }

    void SetInput(const string& input);
    void SetOutput(const string& output);

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
                                const CNSPreciseTime &  queue_read_timeout,
                                const CNSPreciseTime &  queue_pending_timeout,
                                const CNSPreciseTime &  event_time) const
    {
       return GetJobExpirationTime(m_LastTouch, m_Status,
                                   GetSubmitTime(),
                                   m_Timeout, m_RunTimeout, m_ReadTimeout,
                                   queue_timeout, queue_run_timeout,
                                   queue_read_timeout,
                                   queue_pending_timeout,
                                   event_time);
    }

    EAuthTokenCompareResult  CompareAuthToken(const string &  auth_token) const;
    bool ShouldNotifySubmitter(const CNSPreciseTime & current_time) const;
    bool ShouldNotifyListener(const CNSPreciseTime &  current_time) const;

    string Print(TDumpFields                  dump_fields,
                 const CQueue &               queue,
                 const CNSAffinityRegistry &  aff_registry,
                 const CNSGroupsRegistry &    group_registry) const;

    TJobStatus GetStatusBeforeReading(void) const;
    void Dump(FILE *  jobs_file) const;
    bool LoadFromDump(FILE *  jobs_file,
                      char *  input_buf, char * output_buf,
                      const SJobDumpHeader &  header);

private:
    void x_AppendId(string & dump) const;
    void x_AppendKey(const CQueue & queue, string & dump) const;
    void x_AppendStatus(string & dump) const;
    void x_AppendLastTouch(string & dump) const;
    void x_AppendEraseTime(const CNSPreciseTime & timeout,
                           const CNSPreciseTime & pending_timeout,
                           const CNSPreciseTime & exp_time,
                           string & dump) const;
    void x_AppendRunExpiration(const CNSPreciseTime & run_timeout,
                               const CNSPreciseTime & exp_time,
                               string & dump) const;
    void x_AppendReadExpiration(const CNSPreciseTime & read_timeout,
                                const CNSPreciseTime & exp_time,
                                string & dump) const;
    void x_AppendSubmitNotifPort(string & dump) const;
    void x_AppendSubmitNotifExpiration(string & dump) const;
    void x_AppendListenerNotif(string & dump) const;
    void x_AppendListenerNotifExpiration(string & dump) const;
    void x_AppendEvents(string & dump) const;
    void x_AppendRunCounter(string & dump) const;
    void x_AppendReadCounter(string & dump) const;
    void x_AppendAffinity(const CNSAffinityRegistry & aff_registry,
                          string & dump) const;
    void x_AppendGroup(const CNSGroupsRegistry & group_registry,
                       string & dump) const;
    void x_AppendMask(string & dump) const;
    void x_AppendInput(string & dump) const;
    void x_AppendOutput(string & dump) const;
    void x_AppendProgressMsg(string & dump) const;
    void x_AppendRemoteClientSID(string & dump) const;
    void x_AppendRemoteClientIP(string & dump) const;
    void x_AppendNcbiPhid(string & dump) const;
    void x_AppendNeedSubmitProgressMsgNotif(string & dump) const;
    void x_AppendNeedListenerProgressMsgNotif(string & dump) const;
    void x_AppendNeedStolenNotif(string & dump) const;

private:
    unsigned            m_Id;
    unsigned int        m_Passport;
    TJobStatus          m_Status;
    CNSPreciseTime      m_Timeout;       // Individual timeout
    CNSPreciseTime      m_RunTimeout;    // Job run timeout
    CNSPreciseTime      m_ReadTimeout;   // Job read timeout

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

    vector<CJobEvent>   m_Events;

    string              m_Input;
    string              m_Output;

    bool                m_NeedSubmProgressMsgNotif;
    bool                m_NeedLsnrProgressMsgNotif;
    bool                m_NeedStolenNotif;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_JOB__HPP */

