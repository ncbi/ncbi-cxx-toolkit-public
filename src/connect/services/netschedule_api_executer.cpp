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

static bool s_ParseGetJobResponse(CNetScheduleJob& job, const string& response)
{
    // Server message format:
    //    JOB_KEY "input" ["affinity" ["client_ip session_id"]] [mask]

    if (response.empty())
        return false;

    job.mask = CNetScheduleAPI::eEmptyMask;
    job.input.erase();
    job.job_id.erase();
    job.affinity.erase();
    job.client_ip.erase();
    job.session_id.erase();

    const char* str = response.c_str();

    while (*str && isspace((unsigned char) (*str)))
        ++str;
    if (*str == 0) {
throw_err:
        NCBI_THROW(CNetScheduleException, eProtocolSyntaxError,
            "Internal error. Cannot parse server output. " +
                job.job_id + "\n" + response);
    }

    for (; *str && !isspace((unsigned char) (*str)); ++str)
        job.job_id += *str;

    if (*str == 0)
        goto throw_err;

    while (*str && isspace((unsigned char) (*str)))
        ++str;

    if (*str && *str == '"') {
        ++str;
        for (; *str && *str; ++str) {
            if (*str == '"' && str[-1] != '\\') {
                ++str;
                break;
            }
            job.input += *str;
        }
    } else {
        goto throw_err;
    }

    job.input = NStr::ParseEscapes(job.input);

    // parse "affinity"
    while (*str && isspace((unsigned char) (*str)))
        ++str;

    if (*str != 0) {
        if (*str == '"') {
            ++str;
            for (; *str && *str; ++str) {
                if (*str == '"' && str[-1] != '\\') {
                    ++str;
                    break;
                }
                job.affinity += *str;
            }
        } else {
            goto throw_err;
        }
        job.affinity = NStr::ParseEscapes(job.affinity);

        // parse "client_ip session_id"
        while (*str && isspace((unsigned char) (*str)))
            ++str;

        if (*str != 0) {
            string client_ip_and_session_id;

            if (*str == '"') {
                ++str;
                for( ;*str && *str; ++str) {
                    if (*str == '"' && str[-1] != '\\') {
                        ++str;
                        break;
                    }
                    client_ip_and_session_id += *str;
                }
            } else {
                goto throw_err;
            }

            NStr::SplitInTwo(NStr::ParseEscapes(client_ip_and_session_id),
                " ", job.client_ip, job.session_id);

            // parse mask
            while (*str && isspace((unsigned char) (*str)))
                ++str;

            if (*str != 0)
                job.mask = atoi(str);
        }
    }

    _ASSERT(!job.job_id.empty());

    return true;
}

////////////////////////////////////////////////////////////////////////
void CNetScheduleExecuter::JobDelayExpiration(const string& job_key,
                                              unsigned      runtime_inc)
{
    m_Impl->m_API->x_SendJobCmdWaitResponse("JDEX" , job_key, runtime_inc);
}


bool CNetScheduleExecuter::GetJob(CNetScheduleJob& job)
{
    string cmd = "GET";

    if (m_Impl->m_ControlPort != 0) {
        cmd += ' ';
        cmd += NStr::IntToString(m_Impl->m_ControlPort);
    }

    return m_Impl->GetJobImpl(cmd, job);
}


struct SWaitQueuePred {
    SWaitQueuePred(const string& queue_name) : m_QueueName(queue_name)
    {
        m_MinLen = queue_name.length() + 9;
    }

    bool operator()(const string& buf)
    {
        static const char* sign = "NCBI_JSQ_";

        if ((buf.size() < m_MinLen) ||
            ((buf[0] ^ sign[0]) | (buf[1] ^ sign[1])))
            return false;

        return m_QueueName == buf.data() + 9;
    }

    string m_QueueName;
    size_t m_MinLen;
};


bool CNetScheduleExecuter::WaitJob(CNetScheduleJob& job,
                                   unsigned   wait_time)
{
    string cmd = "WGET ";

    _ASSERT(m_Impl->m_ControlPort != 0);

    cmd += NStr::UIntToString(m_Impl->m_ControlPort);
    cmd += ' ';
    cmd += NStr::UIntToString(wait_time);

    if (m_Impl->GetJobImpl(cmd, job))
        return true;

    s_WaitNotification(wait_time, m_Impl->m_ControlPort,
        SWaitQueuePred(m_Impl->m_API.GetQueueName()));

    // no matter what WaitResult returned, re-try the request
    // using reliable comm.level and notify server that
    // we no longer on the UDP socket

    return GetJob(job);
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
                                           CNetScheduleJob& new_job)
{
    if (done_job.job_id.empty())
        return GetJob(new_job);

    s_CheckOutputSize(done_job.output,
        m_Impl->m_API->GetServerParams().max_output_size);

    return s_ParseGetJobResponse(new_job,
        m_Impl->m_API->x_SendJobCmdWaitResponse("JXCG",
            done_job.job_id, done_job.ret_code, done_job.output));
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

    return m_API->m_Service.DiscoverServers(CNetService::eIncludePenalized).
        FindServer(&get_executor);
}


void SNetScheduleExecuterImpl::x_RegUnregClient(const string& cmd)
{
    for (CNetServerGroupIterator it = m_API->m_Service.DiscoverServers(
            CNetService::eIncludePenalized).Iterate(); it; ++it) {
        CNetServer server = *it;

        try {
            server.ExecWithRetry(cmd);
        } catch (CNetServiceException& ex) {
            if (ex.GetErrCode() != CNetServiceException::eCommunicationError)
                throw;
            else {
                ERR_POST_X(12, server->m_Address.AsString() <<
                    ": " << ex.what());
            }
        }
    }
}


void CNetScheduleExecuter::RegisterClient()
{
    m_Impl->x_RegUnregClient("REGC " +
        NStr::IntToString(m_Impl->m_ControlPort));
}

const CNetScheduleAPI::SServerParams& CNetScheduleExecuter::GetServerParams()
{
    return m_Impl->m_API->GetServerParams();
}

void CNetScheduleExecuter::UnRegisterClient()
{
    m_Impl->x_RegUnregClient("URGC " +
        NStr::IntToString(m_Impl->m_ControlPort));
    m_Impl->x_RegUnregClient("CLRN " + m_Impl->m_UID);
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
    return m_Impl->m_API->m_Service->m_ServiceName;
}

END_NCBI_SCOPE
