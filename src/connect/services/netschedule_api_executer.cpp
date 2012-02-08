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

#define NCBI_USE_ERRCODE_X   ConnServ_NetSchedule

BEGIN_NCBI_SCOPE

#define SKIP_SPACE(ptr) \
    while (isspace((unsigned char) *(ptr))) \
        ++ptr;

static bool s_DoParseGetJobResponse(
    CNetScheduleJob& job, const string& response)
{
    // Server message format:
    //    JOB_KEY "input" ["affinity" ["client_ip session_id"]] [mask]

    job.mask = CNetScheduleAPI::eEmptyMask;
    job.input.erase();
    job.job_id.erase();
    job.affinity.erase();
    job.client_ip.erase();
    job.session_id.erase();

    // 1. Extract job ID.
    const char* response_begin = response.c_str();
    const char* response_end = response_begin + response.length();

    SKIP_SPACE(response_begin);

    if (*response_begin == 0)
        return false;

    const char* ptr = response_begin;

    do
        if (*++ptr == 0)
            return false;
    while (!isspace((unsigned char) (*ptr)));

    job.job_id.assign(response_begin, ptr - response_begin);

    while (isspace((unsigned char) (*++ptr)))
        ;

    try {
        size_t field_len;

        // 2. Extract job input
        job.input = NStr::ParseQuoted(CTempString(ptr,
            response_end - ptr), &field_len);

        ptr += field_len;
        SKIP_SPACE(ptr);

        // 3. Extract optional job affinity.
        if (*ptr != 0) {
            job.affinity = NStr::ParseQuoted(CTempString(ptr,
                response_end - ptr), &field_len);

            ptr += field_len;
            SKIP_SPACE(ptr);

            // 4. Extract optional "client_ip session_id".
            if (*ptr != 0) {
                string client_ip_and_session_id(NStr::ParseQuoted(
                    CTempString(ptr, response_end - ptr), &field_len));

                NStr::SplitInTwo(client_ip_and_session_id, " ",
                    job.client_ip, job.session_id);

                ptr += field_len;
                SKIP_SPACE(ptr);

                // 5. Parse job mask
                if (*ptr != 0)
                    job.mask = atoi(ptr);
            }
        }
    }
    catch (CStringException& e) {
        ERR_POST("Error while parsing GET/WGET response " << e);
        return false;
    }

    return true;
}

static bool s_ParseGetJobResponse(CNetScheduleJob& job, const string& response)
{
    if (response.empty())
        return false;

    if (s_DoParseGetJobResponse(job, response))
        return true;

    NCBI_THROW(CNetScheduleException, eProtocolSyntaxError,
        "Cannot parse server output for " +
            job.job_id + ":\n" + response);
}

////////////////////////////////////////////////////////////////////////
void CNetScheduleExecuter::JobDelayExpiration(const string& job_key,
                                              unsigned      runtime_inc)
{
    m_Impl->m_API->x_SendJobCmdWaitResponse("JDEX" , job_key, runtime_inc);
}


bool CNetScheduleExecuter::GetJob(CNetScheduleJob& job, const string& affinity)
{
    string cmd = "GET";

    if (!affinity.empty()) {
        cmd += " aff=\"";
        cmd += NStr::PrintableString(affinity);
        cmd += '"';
    }

    return m_Impl->GetJobImpl(cmd, job);
}


const char s_WGETNotification[] = "NCBI_JSQ_";

struct SWaitQueuePred {
    SWaitQueuePred(const string& queue_name) : m_QueueName(queue_name)
    {
    }

    bool operator()(const string& buf)
    {
        return buf.size() >= sizeof(s_WGETNotification) - 1 +
                m_QueueName.length() &&
            buf[0] == s_WGETNotification[0] &&
            buf[1] == s_WGETNotification[1] &&
            m_QueueName == buf.data() + sizeof(s_WGETNotification) - 1;
    }

    string m_QueueName;
};

bool CNetScheduleExecuter::WaitJob(CNetScheduleJob& job,
                                   unsigned short listening_port,
                                   unsigned wait_time,
                                   const string& affinity)
{
    string cmd = "WGET ";

    cmd += NStr::UIntToString(listening_port);
    cmd += ' ';
    cmd += NStr::UIntToString(wait_time);

    if (!affinity.empty()) {
        cmd += ' ';
        cmd += NStr::PrintableString(affinity);
    }

    if (m_Impl->GetJobImpl(cmd, job))
        return true;

    s_WaitNotification(wait_time, listening_port,
        SWaitQueuePred(m_Impl->m_API.GetQueueName()));

    // Regardless of what s_WaitNotification returned,
    // retry the request using TCP and notify the NetSchedule
    // servers that we no longer on the UDP socket.

    return GetJob(job, affinity);
}


inline
void static s_CheckOutputSize(const string& output, size_t max_output_size)
{
    if (output.length() > max_output_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Output data too long.");
    }
}


void CNetScheduleExecuter::PutResult(const CNetScheduleJob& job)
{
    s_CheckOutputSize(job.output,
        m_Impl->m_API->GetServerParams().max_output_size);

    m_Impl->m_API->x_SendJobCmdWaitResponse("PUT",
        job.job_id, job.ret_code, job.output);
}


bool CNetScheduleExecuter::PutResultGetJob(const CNetScheduleJob& done_job,
                                           CNetScheduleJob& new_job,
                                           const string& affinity)
{
    if (done_job.job_id.empty())
        return GetJob(new_job);

    s_CheckOutputSize(done_job.output,
        m_Impl->m_API->GetServerParams().max_output_size);

    string response(affinity.empty() ?
        m_Impl->m_API->x_SendJobCmdWaitResponse("JXCG",
            done_job.job_id, done_job.ret_code, done_job.output) :
        m_Impl->m_API->x_SendJobCmdWaitResponse("JXCG",
            done_job.job_id, done_job.ret_code,
            done_job.output, affinity));

    return s_ParseGetJobResponse(new_job, response);
}


void CNetScheduleExecuter::PutProgressMsg(const CNetScheduleJob& job)
{
    if (job.progress_msg.length() >= kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Progress message too long");
    }
    m_Impl->m_API->x_SendJobCmdWaitResponse("MPUT",
        job.job_id, job.progress_msg);
}

void CNetScheduleExecuter::GetProgressMsg(CNetScheduleJob& job)
{
    m_Impl->m_API.GetProgressMsg(job);
}

void CNetScheduleExecuter::PutFailure(const CNetScheduleJob& job)
{
    s_CheckOutputSize(job.output,
        m_Impl->m_API->GetServerParams().max_output_size);

    if (job.error_msg.length() >= kNetScheduleMaxErrSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Error message too long");
    }

    m_Impl->m_API->x_SendJobCmdWaitResponse("FPUT",
        job.job_id, job.error_msg, job.output, job.ret_code);
}

CNetScheduleAPI::EJobStatus
    CNetScheduleExecuter::GetJobStatus(const string& job_key)
{
    return m_Impl->m_API->x_GetJobStatus(job_key, false);
}

void CNetScheduleExecuter::ReturnJob(const string& job_key)
{
    m_Impl->m_API->x_SendJobCmdWaitResponse("RETURN" , job_key);
}

class CGetJobCmdExecutor : public INetServerFinder
{
public:
    CGetJobCmdExecutor(const string& cmd, CNetScheduleJob& job) :
        m_Cmd(cmd), m_Job(job)
    {
    }

    virtual bool Consider(CNetServer server);

private:
    const string& m_Cmd;
    CNetScheduleJob& m_Job;
};

bool CGetJobCmdExecutor::Consider(CNetServer server)
{
    return s_ParseGetJobResponse(m_Job, server.ExecWithRetry(m_Cmd).response);
}

bool SNetScheduleExecuterImpl::GetJobImpl(
    const string& cmd, CNetScheduleJob& job)
{
    CGetJobCmdExecutor get_executor(cmd, job);

    return m_API->m_Service.FindServer(&get_executor,
        CNetService::eIncludePenalized);
}

const CNetScheduleAPI::SServerParams& CNetScheduleExecuter::GetServerParams()
{
    return m_Impl->m_API->GetServerParams();
}

void CNetScheduleExecuter::UnRegisterClient()
{
    string cmd("CLRN");

    for (CNetServiceIterator it = m_Impl->m_API->m_Service.
            Iterate(CNetService::eIncludePenalized); it; ++it) {
        CNetServer server = *it;

        try {
            server.ExecWithRetry(cmd);
        } catch (CNetServiceException& ex) {
            if (ex.GetErrCode() != CNetServiceException::eCommunicationError)
                throw;
            else {
                ERR_POST_X(12,
                    server->m_Address.AsString() << ": " << ex.what());
            }
        }
    }
}

static void s_AppendAffinityTokens(string& cmd,
        const char* sep, const vector<string>& affs)
{
    if (!affs.empty()) {
        ITERATE(vector<string>, aff, affs) {
            cmd.append(sep);
            cmd.append(NStr::PrintableString(*aff));
            sep = ",";
        }
        cmd.push_back('"');
    }
}

void CNetScheduleExecuter::ChangePreferredAffinities(CNetServer server,
    const vector<string>& affs_to_add, const vector<string>& affs_to_delete)
{
    string cmd("CHAFF");

    s_AppendAffinityTokens(cmd, " add=\"", affs_to_add);
    s_AppendAffinityTokens(cmd, " del=\"", affs_to_delete);

    server.ExecWithRetry(cmd);
}

const string& CNetScheduleExecuter::GetQueueName()
{
    return m_Impl->m_API.GetQueueName();
}

const string& CNetScheduleExecuter::GetClientName()
{
    return m_Impl->m_API->m_Service.GetClientName();
}

const string& CNetScheduleExecuter::GetServiceName()
{
    return m_Impl->m_API->m_Service.GetServiceName();
}

END_NCBI_SCOPE
