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

#include <corelib/request_ctx.hpp>

#include <stdio.h>

BEGIN_NCBI_SCOPE

#define FORCED_SST_INTERVAL_SEC 0
#define FORCED_SST_INTERVAL_NANOSEC 500 * 1000 * 1000

//////////////////////////////////////////////////////////////////////////////
static void s_SerializeJob(string& cmd, const CNetScheduleJob& job,
    unsigned short udp_port, unsigned wait_time)
{
    cmd.push_back('"');
    cmd.append(NStr::PrintableString(job.input));
    cmd.push_back('"');

    if (udp_port != 0) {
        cmd.append(" port=");
        cmd.append(NStr::UIntToString(udp_port));
        cmd.append(" timeout=");
        cmd.append(NStr::UIntToString(wait_time));
    }

    if (!job.affinity.empty()) {
        SNetScheduleAPIImpl::VerifyAffinityAlphabet(job.affinity);
        cmd.append(" aff=");
        cmd.append(job.affinity);
    }

    if (job.mask != CNetScheduleAPI::eEmptyMask) {
        cmd.append(" msk=");
        cmd.append(NStr::UIntToString(job.mask));
    }
}

static void s_AppendClientIPAndSessionID(string& cmd)
{
    CRequestContext& req = CDiagContext::GetRequestContext();

    if (req.IsSetClientIP()) {
        cmd += " ip=\"";
        cmd += req.GetClientIP();
        cmd += '"';
    }

    if (req.IsSetSessionID()) {
        cmd += " sid=\"";
        cmd += NStr::PrintableString(req.GetSessionID());
        cmd += '"';
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

string CNetScheduleSubmitter::SubmitJob(CNetScheduleJob& job)
{
    return m_Impl->SubmitJobImpl(job, 0, 0);
}

string SNetScheduleSubmitterImpl::SubmitJobImpl(CNetScheduleJob& job,
        unsigned short udp_port, unsigned wait_time, CNetServer* server)
{
    size_t max_input_size = m_API->GetServerParams().max_input_size;
    s_CheckInputSize(job.input, max_input_size);

    string cmd = "SUBMIT ";

    s_SerializeJob(cmd, job, udp_port, wait_time);

    s_AppendClientIPAndSessionID(cmd);

    if (!job.group.empty()) {
        SNetScheduleAPIImpl::VerifyJobGroupAlphabet(job.group);
        cmd.append(" group=");
        cmd.append(job.group);
    }

    CNetServer::SExecResult exec_result(
            m_API->m_Service.FindServerAndExec(cmd));

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

    s_AppendClientIPAndSessionID(cmd);

    if (!job_group.empty()) {
        SNetScheduleAPIImpl::VerifyJobGroupAlphabet(job_group);
        cmd.append(" group=");
        cmd.append(job_group);
    }

    CNetServer::SExecResult exec_result(
        m_Impl->m_API->m_Service.FindServerAndExec(cmd));

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

        string resp = exec_result.conn.Exec("ENDB");

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

            port = atoi(s);
            if (port == 0) {
                NCBI_THROW(CNetServiceException, eProtocolError,
                        "Invalid server response. Port=0.");
            }
        }

        // assign job ids, protocol guarantees all jobs in batch will
        // receive sequential numbers, so server sends only first job id
        //
        CNetScheduleKeyGenerator key_gen(host, port);
        for (unsigned j = 0; j < batch_size; ++j) {
            key_gen.GenerateV1(&jobs[batch_start].job_id, first_job_id);
            ++first_job_id;
            ++batch_start;
        }

        }}


    } // for

    exec_result.conn.Exec("ENDS");
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
    string response = server.ExecWithRetry(m_Cmd).response;

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

    return true;
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
        SNetScheduleAPIImpl::VerifyJobGroupAlphabet(job_group);
        cmd += " group=";
        cmd += job_group;
    }

    CReadCmdExecutor read_executor(cmd, *job_id, *auth_token, *job_status);

    return m_Impl->m_API->m_Service.FindServer(&read_executor,
        CNetService::eRandomize) != NULL;
}

void SNetScheduleSubmitterImpl::FinalizeRead(const char* cmd_start,
    const char* cmd_name,
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

    m_API->GetServer(job_id).ExecWithRetry(cmd);
}

void CNetScheduleSubmitter::ReadConfirm(const string& job_id,
        const string& auth_token)
{
    m_Impl->FinalizeRead("CFRM job_key=", "ReadConfirm",
            job_id, auth_token, kEmptyStr);
}

void CNetScheduleSubmitter::ReadRollback(const string& job_id,
        const string& auth_token)
{
    m_Impl->FinalizeRead("RDRB job_key=", "ReadRollback",
            job_id, auth_token, kEmptyStr);
}

void CNetScheduleSubmitter::ReadFail(const string& job_id,
        const string& auth_token, const string& error_message)
{
    m_Impl->FinalizeRead("FRED job_key=", "ReadFail",
            job_id, auth_token, error_message);
}

void CNetScheduleNotificationHandler::SubmitJob(
        CNetScheduleSubmitter::TInstance submitter,
        CNetScheduleJob& job,
        CAbsTimeout& abs_timeout,
        CNetServer* server)
{
    submitter->SubmitJobImpl(job, GetPort(),
            s_GetRemainingSeconds(abs_timeout), server);
}

#define SUBM_NOTIF_ATTR_COUNT 2

bool CNetScheduleNotificationHandler::CheckSubmitJobNotification(
        CNetScheduleJob& job, CNetScheduleAPI::EJobStatus* status)
{
    static const string attr_names[SUBM_NOTIF_ATTR_COUNT] =
            {"job_key", "job_status"};

    string attr_values[SUBM_NOTIF_ATTR_COUNT];

    return ParseNotification(attr_names, attr_values, SUBM_NOTIF_ATTR_COUNT) ==
                    SUBM_NOTIF_ATTR_COUNT &&
            attr_values[0] == job.job_id &&
            (*status = CNetScheduleAPI::StringToStatus(attr_values[1])) !=
                    CNetScheduleAPI::eJobNotFound;
}

CNetScheduleAPI::EJobStatus
CNetScheduleSubmitter::SubmitJobAndWait(CNetScheduleJob& job,
                                        unsigned       wait_time)
{
    CAbsTimeout abs_timeout(wait_time, 0);

    CNetScheduleNotificationHandler submit_job_handler;

    submit_job_handler.SubmitJob(m_Impl, job, abs_timeout);

    return submit_job_handler.WaitForJobCompletion(job,
            abs_timeout, m_Impl->m_API);
}

CNetScheduleAPI::EJobStatus
CNetScheduleNotificationHandler::WaitForJobCompletion(
        CNetScheduleJob& job,
        CAbsTimeout& abs_timeout,
        CNetScheduleAPI ns_api)
{
    CNetScheduleAPI::EJobStatus status;

    unsigned wait_sec = FORCED_SST_INTERVAL_SEC;

    bool last_timeout = false;
    for (;;) {
        CNanoTimeout abs_timeout_remaining(abs_timeout.GetRemainingTime());

        if (abs_timeout_remaining.IsZero()) {
            status = ns_api->x_GetJobStatus(job.job_id, true);
            break;
        }

        CAbsTimeout timeout(wait_sec++, FORCED_SST_INTERVAL_NANOSEC);

        if (timeout.GetRemainingTime() >= abs_timeout_remaining) {
            timeout = abs_timeout;
            last_timeout = true;
        }

        if (WaitForNotification(timeout)) {
            if (CheckSubmitJobNotification(job, &status))
                break;
        } else {
            status = ns_api->x_GetJobStatus(job.job_id, true);
            if ((status != CNetScheduleAPI::eRunning &&
                    status != CNetScheduleAPI::ePending) || last_timeout)
                break;
        }
    }

    if (status == CNetScheduleAPI::eDone)
        ns_api.GetJobDetails(job);

    return status;
}

void CNetScheduleSubmitter::CancelJob(const string& job_key)
{
    m_Impl->m_API->x_SendJobCmdWaitResponse("CANCEL", job_key);
}

void CNetScheduleSubmitter::CancelJobGroup(const string& job_group)
{
    SNetScheduleAPIImpl::VerifyJobGroupAlphabet(job_group);
    m_Impl->m_API->m_Service.ExecOnAllServers("CANCEL group=" + job_group);
}

CNetScheduleAPI::EJobStatus
    CNetScheduleSubmitter::GetJobStatus(const string& job_key)
{
    return m_Impl->m_API->x_GetJobStatus(job_key, true);
}

CNetScheduleAPI::EJobStatus
    CNetScheduleSubmitter::GetJobDetails(CNetScheduleJob& job)
{
    return m_Impl->m_API.GetJobDetails(job);
}

void CNetScheduleSubmitter::GetProgressMsg(CNetScheduleJob& job)
{
    m_Impl->m_API.GetProgressMsg(job);
}

END_NCBI_SCOPE
