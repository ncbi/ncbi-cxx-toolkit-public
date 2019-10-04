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
 * Author:  Anatoliy Kuznetsov, Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>

#include "netschedule_api_impl.hpp"

#include <connect/services/netschedule_api.hpp>
#include <connect/services/util.hpp>

#include <stdio.h>
#include <cmath>
#include <array>

BEGIN_NCBI_SCOPE

using namespace grid::netschedule;

#define FORCED_SST_INTERVAL_SEC 0
#define MAX_FORCED_SST_INTERVAL_SEC 3
#define FORCED_SST_INTERVAL_NANOSEC 500 * 1000 * 1000

//////////////////////////////////////////////////////////////////////////////
static void s_SerializeJob(string& cmd, const CNetScheduleNewJob& job,
    unsigned short udp_port, unsigned wait_time)
{
    cmd.push_back('"');
    cmd.append(NStr::PrintableString(job.input));
    cmd.push_back('"');

    if (udp_port && wait_time) {
        cmd.append(" port=");
        cmd.append(NStr::UIntToString(udp_port));
        cmd.append(" timeout=");
        cmd.append(NStr::UIntToString(wait_time));
    }

    if (!job.affinity.empty()) {
        limits::Check<limits::SAffinity>(job.affinity);
        cmd.append(" aff=");
        cmd.append(job.affinity);
    }

    if (job.mask != CNetScheduleAPI::eEmptyMask) {
        cmd.append(" msk=");
        cmd.append(NStr::UIntToString(job.mask));
    }
}

inline
void static s_CheckInputSize(const string& input, size_t max_input_size)
{
    if (input.length() >  max_input_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Input data too long.");
    }
}

string CNetScheduleSubmitter::SubmitJob(CNetScheduleNewJob& job)
{
    return m_Impl->SubmitJobImpl(job, 0, 0);
}

void SNetScheduleSubmitterImpl::AppendClientIPSessionIDHitID(string& cmd, const string& job_group)
{
    CRequestContext& req = CDiagContext::GetRequestContext();
    g_AppendClientIPAndSessionID(cmd, req);

    if (!job_group.empty()) {
        limits::Check<limits::SJobGroup>(job_group);
        cmd.append(" group=");
        cmd.append(job_group);
    }

    g_AppendHitID(cmd, req);
}

string SNetScheduleSubmitterImpl::SubmitJobImpl(CNetScheduleNewJob& job,
        unsigned short udp_port, unsigned wait_time, CNetServer* server)
{
    size_t max_input_size = m_API->GetServerParams().max_input_size;
    s_CheckInputSize(job.input, max_input_size);

    string cmd = "SUBMIT ";

    s_SerializeJob(cmd, job, udp_port, wait_time);
    AppendClientIPSessionIDHitID(cmd, job.group);

    CNetServer::SExecResult exec_result(
            m_API->m_Service.FindServerAndExec(cmd, false));

    if ((job.job_id = exec_result.response).empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Invalid server response. Empty key.");
    }

    if (server != NULL)
        *server = exec_result.conn->m_Server;

    return job.job_id;
}

void CNetScheduleSubmitter::SubmitJobBatch(vector<CNetScheduleJob>& jobs,
        const string& job_group)
{
    // verify the input data
    size_t max_input_size = m_Impl->m_API->GetServerParams().max_input_size;

    ITERATE(vector<CNetScheduleJob>, it, jobs) {
        const string& input = it->input;
        s_CheckInputSize(input, max_input_size);
    }

    // Batch submit command.
    string cmd = "BSUB";
    m_Impl->AppendClientIPSessionIDHitID(cmd, job_group);

    CNetServer::SExecResult exec_result(
        m_Impl->m_API->m_Service.FindServerAndExec(cmd, false));

    cmd.reserve(max_input_size * 6);
    string host;
    unsigned short port = 0;
    for (unsigned i = 0; i < jobs.size(); ) {

        // Batch size should be reasonable not to trigger network timeout
        const unsigned kMax_Batch = 10000;

        size_t batch_size = jobs.size() - i;
        if (batch_size > kMax_Batch) {
            batch_size = kMax_Batch;
        }

        cmd.erase();
        cmd = "BTCH ";
        cmd.append(NStr::UIntToString((unsigned) batch_size));

        exec_result.conn->WriteLine(cmd);

        unsigned batch_start = i;
        for (unsigned j = 0; j < batch_size; ++j,++i) {
            cmd.erase();
            s_SerializeJob(cmd, jobs[i], 0, 0);

            exec_result.conn->WriteLine(cmd);
        }

        string resp = exec_result.conn.Exec("ENDB", false);

        if (resp.empty()) {
            NCBI_THROW(CNetServiceException, eProtocolError,
                    "Invalid server response. Empty key.");
        }

        // parse the batch answer
        // FORMAT:
        //  first_job_id host port

        {{
        const char* s = resp.c_str();
        unsigned first_job_id = ::atoi(s);

        if (host.empty()) {
            for (; *s != ' '; ++s) {
                if (*s == 0) {
                    NCBI_THROW(CNetServiceException, eProtocolError,
                            "Invalid server response. Batch answer format.");
                }
            }
            ++s;
            if (*s == 0) {
                NCBI_THROW(CNetServiceException, eProtocolError,
                        "Invalid server response. Batch answer format.");
            }
            for (; *s != ' '; ++s) {
                if (*s == 0) {
                    NCBI_THROW(CNetServiceException, eProtocolError,
                            "Invalid server response. Batch answer format.");
                }
                host.push_back(*s);
            }
            ++s;
            if (*s == 0) {
                NCBI_THROW(CNetServiceException, eProtocolError,
                        "Invalid server response. Batch answer format.");
            }

            port = static_cast<short>(atoi(s));
            if (port == 0) {
                NCBI_THROW(CNetServiceException, eProtocolError,
                        "Invalid server response. Port=0.");
            }
        }

        // assign job ids, protocol guarantees all jobs in batch will
        // receive sequential numbers, so server sends only first job id
        //
        CNetScheduleKeyGenerator key_gen(host, port, m_Impl->m_API->m_Queue);
        for (unsigned j = 0; j < batch_size; ++j) {
            key_gen.Generate(&jobs[batch_start].job_id, first_job_id);
            ++first_job_id;
            ++batch_start;
        }

        }}


    } // for

    exec_result.conn.Exec("ENDS", false);
}

class CReadCmdExecutor : public INetServerFinder
{
public:
    CReadCmdExecutor(const string& cmd,
            string& job_id,
            string& auth_token,
            CNetScheduleAPI::EJobStatus& job_status) :
        m_Cmd(cmd),
        m_JobId(job_id),
        m_AuthToken(auth_token),
        m_JobStatus(job_status)
    {
    }

    virtual bool Consider(CNetServer server);

private:
    string m_Cmd;
    string& m_JobId;
    string& m_AuthToken;
    CNetScheduleAPI::EJobStatus& m_JobStatus;
};

bool CReadCmdExecutor::Consider(CNetServer server)
{
    string response = server.ExecWithRetry(m_Cmd, false).response;

    if (response.empty() || response[0] == '0')
        return false;

    m_JobId.erase();
    m_AuthToken.erase();
    m_JobStatus = CNetScheduleAPI::eDone;

    CUrlArgs read_response_parser(response);

    ITERATE(CUrlArgs::TArgs, it, read_response_parser.GetArgs()) {
        switch (it->name[0]) {
        case 'j':
            if (it->name == "job_key")
                m_JobId = it->value;
            break;
        case 'a':
            if (it->name == "auth_token")
                m_AuthToken = it->value;
            break;
        case 's':
            if (it->name == "status")
                m_JobStatus = CNetScheduleAPI::StringToStatus(it->value);
            break;
        }
    }

    return !m_JobId.empty();
}

bool CNetScheduleSubmitter::Read(string* job_id, string* auth_token,
        CNetScheduleAPI::EJobStatus* job_status, unsigned timeout,
        const string& job_group)
{
    string cmd("READ ");

    if (timeout > 0) {
        cmd += " timeout=";
        cmd += NStr::UIntToString(timeout);
    }
    if (!job_group.empty()) {
        limits::Check<limits::SJobGroup>(job_group);
        cmd += " group=";
        cmd += job_group;
    }

    g_AppendClientIPSessionIDHitID(cmd);

    CReadCmdExecutor read_executor(cmd, *job_id, *auth_token, *job_status);

    return m_Impl->m_API->m_Service.FindServer(&read_executor,
        CNetService::eRandomize) != NULL;
}

void SNetScheduleSubmitterImpl::FinalizeRead(const char* cmd_start,
    const string& job_id,
    const string& auth_token,
    const string& error_message)
{
    string cmd = cmd_start + job_id;

    cmd += " auth_token=";
    cmd += auth_token;

    if (!error_message.empty()) {
        cmd += " err_msg=\"";
        cmd += NStr::PrintableString(error_message);
        cmd += '"';
    }

    g_AppendClientIPSessionIDHitID(cmd);
    m_API->ExecOnJobServer(job_id, cmd, eOn);
}

void CNetScheduleSubmitter::ReadConfirm(const string& job_id,
        const string& auth_token)
{
    m_Impl->FinalizeRead("CFRM job_key=",
            job_id, auth_token, kEmptyStr);
}

void CNetScheduleSubmitter::ReadRollback(const string& job_id,
        const string& auth_token)
{
    m_Impl->FinalizeRead("RDRB job_key=",
            job_id, auth_token, kEmptyStr);
}

void CNetScheduleSubmitter::ReadFail(const string& job_id,
        const string& auth_token, const string& error_message)
{
    m_Impl->FinalizeRead("FRED job_key=",
            job_id, auth_token, error_message);
}

void CNetScheduleNotificationHandler::SubmitJob(
        CNetScheduleSubmitter::TInstance submitter,
        CNetScheduleJob& job,
        unsigned wait_time,
        CNetServer* server)
{
    submitter->SubmitJobImpl(job, GetPort(), wait_time, server);
}

bool CNetScheduleNotificationHandler::CheckJobStatusNotification(
        const string& job_id, CNetScheduleAPI::EJobStatus* job_status,
        int* last_event_index /*= NULL*/)
{
    _ASSERT(job_status);

    SNetScheduleOutputParser parser(m_Receiver.message);

    if (parser("job_key") != job_id) return false;

    *job_status = CNetScheduleAPI::StringToStatus(parser("job_status"));

    if (last_event_index) *last_event_index = NStr::StringToInt(parser("last_event_index"), NStr::fConvErr_NoThrow);

    return *job_status != CNetScheduleAPI::eJobNotFound;
}

bool CNetScheduleNotificationHandler::GetJobDetailsIfCompleted(CNetScheduleAPI ns_api, CNetScheduleJob& job,
        time_t* job_exptime, CNetScheduleAPI::EJobStatus& job_status)
{
    SNetScheduleOutputParser parser(m_Receiver.message);

    if (parser("job_key") != job.job_id) return false;

    job_status = CNetScheduleAPI::StringToStatus(parser("job_status"));
    if (job_status <= CNetScheduleAPI::eRunning) return false;

    job_status = ns_api.GetJobDetails(job, job_exptime);
    return job_status > CNetScheduleAPI::eRunning;
}

CNetScheduleAPI::EJobStatus
CNetScheduleSubmitter::SubmitJobAndWait(CNetScheduleJob& job,
                                        unsigned       wait_time)
{
    return m_Impl->SubmitJobAndWait(job, wait_time);
}

CNetScheduleAPI::EJobStatus
SNetScheduleSubmitterImpl::SubmitJobAndWait(CNetScheduleJob& job, unsigned wait_time, time_t* job_exptime)
{
    CDeadline deadline(wait_time, 0);

    CNetScheduleNotificationHandler submit_job_handler;

    SubmitJobImpl(job, submit_job_handler.GetPort(), wait_time);
    if (!wait_time) return CNetScheduleAPI::ePending;

    return submit_job_handler.WaitForJobCompletion(job, deadline, m_API, job_exptime);
}

CNetScheduleAPI::EJobStatus
CNetScheduleSubmitter::WaitForJob(const string& job_id, unsigned wait_time)
{
    const CNetScheduleNotificationHandler::TJobStatusMask wait_for_statuses =
        CNetScheduleNotificationHandler::fJSM_Canceled |
        CNetScheduleNotificationHandler::fJSM_Failed |
        CNetScheduleNotificationHandler::fJSM_Done;

    CDeadline deadline(wait_time);
    return CNetScheduleNotificationHandler().WaitForJobEvent(job_id,
            deadline, m_Impl->m_API, wait_for_statuses);
}

CNetScheduleAPI::EJobStatus
CNetScheduleNotificationHandler::WaitForJobCompletion(
        CNetScheduleJob& job,
        CDeadline& deadline,
        CNetScheduleAPI ns_api,
        time_t* job_exptime)
{
    CNetScheduleAPI::EJobStatus status = CNetScheduleAPI::ePending;

    unsigned wait_sec = FORCED_SST_INTERVAL_SEC;

    for (;;) {
        CDeadline timeout(wait_sec, FORCED_SST_INTERVAL_NANOSEC);

        if (deadline < timeout) {
            timeout = deadline;
        }

        if (WaitForNotification(timeout)) {
            if (GetJobDetailsIfCompleted(ns_api, job, job_exptime, status)) return status;
        } else {
            // The wait has timed out - query the server directly.

            string err_msg;
            try {
                status = ns_api.GetJobDetails(job, job_exptime);
                if ((status != CNetScheduleAPI::eRunning &&
                        status != CNetScheduleAPI::ePending) || deadline.IsExpired())
                    return status;
                // The job is still running - next time, wait one second
                // longer before querying the server again.
                if (wait_sec < MAX_FORCED_SST_INTERVAL_SEC)
                    ++wait_sec;
                // Go to the next cycle (jump over the error
                // processing code below the catch blocks).
                continue;
            }
            catch (CNetScheduleException& e) {
                if (deadline.IsExpired())
                    throw;
                switch (e.GetErrCode()) {
                case CNetScheduleException::eJobNotFound:
                case CNetScheduleException::eUnknownQueue:
                    throw;
                }
                err_msg = e.GetMsg();
            }
            catch (CException& e) {
                if (deadline.IsExpired())
                    throw;
                err_msg = e.GetMsg();
            }
            // An exception has occurred, but there's still some
            // time left before the deadline.

            CNetServer ns_server(ns_api->GetServer(job.job_id));
            ns_api->GetListener()->OnWarning(err_msg, ns_server);

            // The notification mechanism may have been disrupted
            // by the error - query the server more often.
            wait_sec = FORCED_SST_INTERVAL_SEC;
        }
    }
}

bool CNetScheduleNotificationHandler::RequestJobWatching(
        CNetScheduleAPI::TInstance ns_api,
        const string& job_id,
        const CDeadline& deadline,
        CNetScheduleAPI::EJobStatus* job_status,
        int* last_event_index)
{
    _ASSERT(job_status);
    _ASSERT(last_event_index);

    tie(*job_status, *last_event_index, ignore) = RequestJobWatching(ns_api, job_id, deadline);

    return *job_status != CNetScheduleAPI::eJobNotFound;
}

CNetScheduleNotificationHandler::TJobInfo CNetScheduleNotificationHandler::RequestJobWatching(
        CNetScheduleAPI::TInstance ns_api,
        const string& job_id,
        const CDeadline& deadline)
{
    double remaining_seconds = ceil(deadline.GetRemainingTime().GetAsDouble());

    string cmd("LISTEN job_key=" + job_id);

    cmd += " port=";
    cmd += NStr::NumericToString(GetPort());
    cmd += " timeout=";
    cmd += NStr::NumericToString((unsigned)remaining_seconds);

    g_AppendClientIPSessionIDHitID(cmd);

    cmd += " need_progress_msg=1";
    m_Receiver.message = ns_api->ExecOnJobServer(job_id, cmd, eOn);

    SNetScheduleOutputParser parser(m_Receiver.message);
    const auto job_status = CNetScheduleAPI::StringToStatus(parser("job_status"));
    const auto last_event_index = NStr::StringToInt(parser("last_event_index"), NStr::fConvErr_NoThrow);
    const auto progress_message = parser("msg");

    return make_tuple(job_status, last_event_index, progress_message);
}

CNetScheduleAPI::EJobStatus
CNetScheduleNotificationHandler::WaitForJobEvent(
        const string& job_key,
        CDeadline& deadline,
        CNetScheduleAPI ns_api,
        TJobStatusMask status_mask,
        int last_event_index,
        int *new_event_index)
{
    int index = -1;

    CNetScheduleAPI::EJobStatus job_status;

    unsigned wait_sec = FORCED_SST_INTERVAL_SEC;

    do {
        CDeadline timeout(wait_sec, FORCED_SST_INTERVAL_NANOSEC);

        if (deadline < timeout) {
            timeout = deadline;
        }

        tie(job_status, index, ignore) = RequestJobWatching(ns_api, job_key, timeout);

        if ((job_status != CNetScheduleAPI::eJobNotFound) &&
                ((status_mask & (1 << job_status)) != 0 ||
                index > last_event_index))
            break;

        if (deadline.IsExpired())
            break;

        if (!WaitForNotification(timeout))
            ++wait_sec;
        else if (CheckJobStatusNotification(job_key, &job_status, &index) &&
                ((status_mask & (1 << job_status)) != 0 ||
                index > last_event_index))
            break;
    } while (!deadline.IsExpired());

    if (new_event_index) {
        *new_event_index = index;
    }

    return job_status;
}

void CNetScheduleSubmitter::CancelJob(const string& job_key)
{
    string cmd("CANCEL " + job_key);
    g_AppendClientIPSessionIDHitID(cmd);
    m_Impl->m_API->ExecOnJobServer(job_key, cmd);
}

void CNetScheduleSubmitter::CancelJobGroup(const string& job_group,
        const string& job_statuses)
{
    limits::Check<limits::SJobGroup>(job_group);
    string cmd("CANCEL group=" + job_group);
    if (!job_statuses.empty()) {
        cmd.append(" status=");
        cmd.append(job_statuses);
    }
    g_AppendClientIPSessionIDHitID(cmd);
    m_Impl->m_API->m_Service.ExecOnAllServers(cmd);
}

CNetScheduleAPI::EJobStatus CNetScheduleSubmitter::GetJobStatus(
        const string& job_key, time_t* job_exptime,
        ENetScheduleQueuePauseMode* pause_mode)
{
    CNetScheduleJob job;
    job.job_id = job_key;
    return m_Impl->m_API->GetJobStatus("SST2", job, job_exptime, pause_mode);
}

CNetScheduleAPI::EJobStatus CNetScheduleSubmitter::GetJobDetails(
        CNetScheduleJob& job, time_t* job_exptime,
        ENetScheduleQueuePauseMode* pause_mode)
{
    return m_Impl->m_API.GetJobDetails(job, job_exptime, pause_mode);
}

void CNetScheduleSubmitter::GetProgressMsg(CNetScheduleJob& job)
{
    m_Impl->m_API.GetProgressMsg(job);
}

END_NCBI_SCOPE
