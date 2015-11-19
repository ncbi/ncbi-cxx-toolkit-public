#ifndef CONNECT_SERVICES_IMPL__NETSCHEDULE_API_INT__HPP
#define CONNECT_SERVICES_IMPL__NETSCHEDULE_API_INT__HPP

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
 * Authors: Dmitry Kazimirov, Rafael Sadyrov
 *
 * File Description:
 *   Internal declarations for NetSchedule client API.
 *
 */


BEGIN_NCBI_SCOPE


const unsigned int kNetScheduleMaxDBDataSize = 2048;

const unsigned int kNetScheduleMaxDBErrSize = 4096;


NCBI_DECLARE_INTERFACE_VERSION(SNetScheduleAPIImpl, "xnetschedule_api", 1,0, 0);

extern NCBI_XCONNECT_EXPORT const char* const kNetScheduleAPIDriverName;

extern NCBI_XCONNECT_EXPORT
void g_AppendClientIPAndSessionID(string& cmd,
        const string* default_session = NULL);

extern NCBI_XCONNECT_EXPORT
void g_AppendHitID(string& cmd);

extern NCBI_XCONNECT_EXPORT
void g_AppendClientIPSessionIDHitID(string& cmd,
        const string* default_session = NULL);

extern NCBI_XCONNECT_EXPORT
int g_ParseNSOutput(const string& attr_string, const char* const* attr_names,
        string* attr_values, int attr_count);

void NCBI_XCONNECT_EXPORT NCBI_EntryPoint_xnetscheduleapi(
     CPluginManager<SNetScheduleAPIImpl>::TDriverInfoList&   info_list,
     CPluginManager<SNetScheduleAPIImpl>::EEntryPointRequest method);

class NCBI_XCONNECT_EXPORT CNetScheduleNotificationHandler
{
public:
    CNetScheduleNotificationHandler();

    bool ReceiveNotification(string* server_host = NULL);

    bool WaitForNotification(const CDeadline& deadline,
                             string*          server_host = NULL);

    unsigned short GetPort() const {return m_UDPPort;}

    const string& GetMessage() const {return m_Message;}

    void PrintPortNumber();

// Submitter methods.
public:
    void SubmitJob(CNetScheduleSubmitter::TInstance submitter,
            CNetScheduleJob& job,
            unsigned wait_time,
            CNetServer* server = NULL);

    bool CheckJobStatusNotification(const string& job_id,
            CNetScheduleAPI::EJobStatus* job_status,
            int* last_event_index = NULL);

    // This method requires calling SubmitJob prior with wait_time set
    CNetScheduleAPI::EJobStatus WaitForJobCompletion(CNetScheduleJob& job,
            CDeadline& deadline, CNetScheduleAPI ns_api,
            time_t* job_exptime = NULL);

    bool RequestJobWatching(CNetScheduleAPI::TInstance ns_api,
            const string& job_id,
            const CDeadline& deadline,
            CNetScheduleAPI::EJobStatus* job_status,
            int* last_event_index);

    enum EJobStatusMask {
        fJSM_Pending        = 1 << CNetScheduleAPI::ePending,
        fJSM_Running        = 1 << CNetScheduleAPI::eRunning,
        fJSM_Canceled       = 1 << CNetScheduleAPI::eCanceled,
        fJSM_Failed         = 1 << CNetScheduleAPI::eFailed,
        fJSM_Done           = 1 << CNetScheduleAPI::eDone,
        fJSM_Reading        = 1 << CNetScheduleAPI::eReading,
        fJSM_Confirmed      = 1 << CNetScheduleAPI::eConfirmed,
        fJSM_ReadFailed     = 1 << CNetScheduleAPI::eReadFailed,
        fJSM_Deleted        = 1 << CNetScheduleAPI::eDeleted
    };
    typedef int TJobStatusMask;

    CNetScheduleAPI::EJobStatus WaitForJobEvent(
            const string& job_key,
            CDeadline& deadline,
            CNetScheduleAPI ns_api,
            TJobStatusMask status_mask,
            int last_event_index = kMax_Int,
            int *new_event_index = NULL);

// Worker node methods.
public:
    static string MkBaseGETCmd(
        CNetScheduleExecutor::EJobAffinityPreference affinity_preference,
        const string& affinity_list);
    void CmdAppendTimeoutGroupAndClientInfo(string& cmd,
            const CDeadline* deadline, const string& job_group);
    bool RequestJob(CNetScheduleExecutor::TInstance executor,
                    CNetScheduleJob& job,
                    const string& cmd);
    bool CheckRequestJobNotification(CNetScheduleExecutor::TInstance executor,
                                     CNetServer* server);

protected:
    CDatagramSocket m_UDPSocket;
    unsigned short m_UDPPort;

    char m_Buffer[1024];
    string m_Message;
};

struct NCBI_XCONNECT_EXPORT CNetScheduleAPIExt : CNetScheduleAPI
{
    CNetScheduleAPIExt(CNetScheduleAPI::TInstance api) : CNetScheduleAPI(api) {}
    void AddToClientNode(const string& data);

    // Create workernode-compatible API
    static TInstance CreateWnCompat(const string&, const string&);

    // Create API with no auto config loading (from server)
    static TInstance CreateNoCfgLoad(const string&, const string&, const string&);
};


END_NCBI_SCOPE


#endif
