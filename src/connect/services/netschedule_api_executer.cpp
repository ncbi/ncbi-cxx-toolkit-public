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

static
void s_ParseGetJobResponse(CNetScheduleJob& job, const std::string& response)
{
    // Server message format:
    //    JOB_KEY "input" ["affinity" ["client_ip session_id"]] [mask]

    _ASSERT(!response.empty());

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

    if (*str == 0)
        return;

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

    if (*str == 0)
        return;

    std::string client_ip_and_session_id;

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
    if (*str == 0)
        return;

    job.mask = atoi(str);
}

///////////////////////////////////////////////////////////////////////////////////////
void CNetScheduleExecuter::SetRunTimeout(const string& job_key,
                                         unsigned      time_to_run) const
{
    m_Impl->m_API->x_SendJobCmdWaitResponse("JRTO" , job_key, time_to_run);
}


void CNetScheduleExecuter::JobDelayExpiration(const string& job_key,
                                              unsigned      runtime_inc) const
{
    m_Impl->m_API->x_SendJobCmdWaitResponse("JDEX" , job_key, runtime_inc);
}


bool CNetScheduleExecuter::GetJob(CNetScheduleJob& job,
                                  unsigned short udp_port) const
{
    string cmd = "GET";

    if (udp_port != 0) {
        cmd += ' ';
        cmd += NStr::IntToString(udp_port);
    }

    return m_Impl->GetJobImpl(cmd, job);
}


bool CNetScheduleExecuter::WaitJob(CNetScheduleJob& job,
                                   unsigned   wait_time,
                                   unsigned short udp_port,
                                   EWaitMode      wait_mode) const
{
    string cmd = "WGET ";

    cmd += NStr::UIntToString(udp_port);
    cmd += ' ';
    cmd += NStr::UIntToString(wait_time);

    if (m_Impl->GetJobImpl(cmd, job))
        return true;

    if (wait_mode != eWaitNotification)
        return false;

    WaitNotification(m_Impl->m_API.GetQueueName(), wait_time, udp_port);

    // no matter is WaitResult we re-try the request
    // using reliable comm.level and notify server that
    // we no longer on the UDP socket

    return GetJob(job, udp_port);
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


/* static */
bool CNetScheduleExecuter::WaitNotification(const string&  queue_name,
                                       unsigned       wait_time,
                                       unsigned short udp_port)
{
    return s_WaitNotification(wait_time, udp_port, SWaitQueuePred(queue_name));
}



inline
void static s_ChechOutputSize(const string& output, size_t max_output_size)
{
    if (output.length() > max_output_size) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Output data too long.");
    }
}


void CNetScheduleExecuter::PutResult(const CNetScheduleJob& job) const
{
    s_ChechOutputSize(job.output,
        m_Impl->m_API->GetServerParams().max_output_size);

    m_Impl->m_API->x_SendJobCmdWaitResponse("PUT",
        job.job_id, job.ret_code, job.output);
}


bool CNetScheduleExecuter::PutResultGetJob(const CNetScheduleJob& done_job,
                                           CNetScheduleJob& new_job) const
{
    if (done_job.job_id.empty())
        return GetJob(new_job, 0);

    s_ChechOutputSize(done_job.output,
        m_Impl->m_API->GetServerParams().max_output_size);

    string resp = m_Impl->m_API->x_SendJobCmdWaitResponse("JXCG",
        done_job.job_id, done_job.ret_code, done_job.output);

    if (!resp.empty()) {
        new_job.mask = CNetScheduleAPI::eEmptyMask;
        s_ParseGetJobResponse(new_job, resp);
        _ASSERT(!new_job.job_id.empty());
        return true;
    }
    return false;
}


void CNetScheduleExecuter::PutProgressMsg(const CNetScheduleJob& job) const
{
    if (job.progress_msg.length() >= kNetScheduleMaxDataSize) {
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "Progress message too long");
    }
    m_Impl->m_API->x_SendJobCmdWaitResponse("MPUT" , job.job_id, job.progress_msg);
}

void CNetScheduleExecuter::GetProgressMsg(CNetScheduleJob& job)
{
    m_Impl->m_API.GetProgressMsg(job);
}

void CNetScheduleExecuter::PutFailure(const CNetScheduleJob& job) const
{
    s_ChechOutputSize(job.output,
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

void CNetScheduleExecuter::ReturnJob(const string& job_key) const
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
    string resp = server.Connect().Exec(m_Cmd);

    if (resp.empty())
        return false;

    m_Job.mask = CNetScheduleAPI::eEmptyMask;
    s_ParseGetJobResponse(m_Job, resp);

    return true;
}

bool SNetScheduleExecuterImpl::GetJobImpl(
    const string& cmd, CNetScheduleJob& job) const
{
    return m_API->m_Service->FindServer(new CGetJobCmdExecutor(cmd, job));
}


void SNetScheduleExecuterImpl::x_RegUnregClient(
    const string& cmd, unsigned short udp_port) const
{
    string tmp = cmd + ' ' + NStr::IntToString(udp_port);

    m_API->m_Service->ForEach(SNSSendCmd(m_API, tmp,
        SNSSendCmd::eLogExceptions | SNSSendCmd::eIgnoreCommunicationError));
}


void CNetScheduleExecuter::RegisterClient(unsigned short udp_port) const
{
    m_Impl->x_RegUnregClient("REGC", udp_port);
}

const CNetScheduleAPI::SServerParams& CNetScheduleExecuter::GetServerParams() const
{
    return m_Impl->m_API->GetServerParams();
}

void CNetScheduleExecuter::UnRegisterClient(unsigned short udp_port) const
{
    m_Impl->x_RegUnregClient("URGC", udp_port);
}

const string& CNetScheduleExecuter::GetQueueName() const
{
    return m_Impl->m_API.GetQueueName();
}

const string& CNetScheduleExecuter::GetClientName() const
{
    return m_Impl->m_API->m_Service.GetClientName();
}

const string& CNetScheduleExecuter::GetServiceName() const
{
    return m_Impl->m_API->m_Service.GetServiceName();
}

END_NCBI_SCOPE
