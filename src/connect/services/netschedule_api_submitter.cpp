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



//////////////////////////////////////////////////////////////////////////////
static void s_SerializeJob(string& cmd, const CNetScheduleJob& job,
    unsigned short udp_port, unsigned wait_time)
{
    cmd.append("\"");
    string ps_input = NStr::PrintableString(job.input);

    cmd.append(ps_input);
    cmd.append("\"");

    if (!job.progress_msg.empty()) {
        cmd.append(" \"");
        cmd.append(job.progress_msg);
        cmd.append("\"");
    }

    if (udp_port != 0) {
        cmd.append(" ");
        cmd.append(NStr::UIntToString(udp_port));
        cmd.append(" ");
        cmd.append(NStr::UIntToString(wait_time));
    }

    if (!job.affinity.empty()) {
        cmd.append(" aff=\"");
        cmd.append(NStr::PrintableString(job.affinity));
        cmd.append("\"");
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
    unsigned short udp_port, unsigned wait_time)
{
    size_t max_input_size = m_API->GetServerParams().max_input_size;
    s_CheckInputSize(job.input, max_input_size);

    string cmd = "SUBMIT ";

    s_SerializeJob(cmd, job, udp_port, wait_time);

    s_AppendClientIPAndSessionID(cmd);

    job.job_id = m_API->m_Service.FindServerAndExec(cmd).response;

    if (job.job_id.empty()) {
        NCBI_THROW(CNetServiceException, eCommunicationError,
                   "Invalid server response. Empty key.");
    }

    return job.job_id;
}

void CNetScheduleSubmitter::SubmitJobBatch(vector<CNetScheduleJob>& jobs)
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
    CReadCmdExecutor(const string& cmd, string& batch_id,
            vector<string>& job_ids) :
        m_Cmd(cmd), m_BatchId(batch_id), m_JobIds(job_ids)
    {
    }

    virtual bool Consider(CNetServer server);

private:
    string m_Cmd;
    string& m_BatchId;
    vector<string>& m_JobIds;
};

bool CReadCmdExecutor::Consider(CNetServer server)
{
    string response = server.ExecWithRetry(m_Cmd).response;

    if (response.empty() || response == "0 " || response == "0")
        return false;

    string encoded_bitvector;

    NStr::SplitInTwo(response, " ", m_BatchId, encoded_bitvector);

    CBitVectorDecoder bvdec(encoded_bitvector);

    string host = server.GetHost();
    unsigned port = server.GetPort();

    unsigned from, to;

    CNetScheduleKeyGenerator key_gen(host, port);

    while (bvdec.GetNextRange(from, to))
        while (from <= to)
            m_JobIds.push_back(key_gen.GenerateV1(from++));

    return true;
}

bool CNetScheduleSubmitter::Read(string& batch_id,
    vector<string>& job_ids,
    unsigned max_jobs, unsigned timeout)
{
    job_ids.clear();

    string cmd("READ ");

    cmd.append(NStr::UIntToString(max_jobs));

    if (timeout > 0) {
        cmd += ' ';
        cmd += NStr::UIntToString(timeout);
    }

    CReadCmdExecutor read_executor(cmd, batch_id, job_ids);

    return m_Impl->m_API->m_Service.FindServer(&read_executor,
        CNetService::eRandomize);
}

void SNetScheduleSubmitterImpl::ExecReadCommand(const char* cmd_start,
    const char* cmd_name,
    const string& batch_id,
    const vector<string>& job_ids,
    const string& error_message)
{
    if (job_ids.empty()) {
        NCBI_THROW_FMT(CNetScheduleException, eInvalidParameter,
            cmd_name << ": no job keys specified");
    }

    CBitVectorEncoder bvenc;

    vector<string>::const_iterator job_id = job_ids.begin();

    CNetScheduleKey first_key(*job_id);

    bvenc.AppendInteger(first_key.id);

    while (++job_id != job_ids.end()) {
        CNetScheduleKey key(*job_id);

        if (key.host != first_key.host || key.port != first_key.port) {
            NCBI_THROW_FMT(CNetScheduleException, eInvalidParameter,
                cmd_name << ": all jobs must belong to a single NS");
        }

        bvenc.AppendInteger(key.id);
    }

    string cmd = cmd_start + batch_id;

    cmd += " \"";
    cmd += bvenc.Encode();
    cmd += '"';

    if (!error_message.empty()) {
        cmd += " \"";
        cmd += NStr::PrintableString(error_message);
        cmd += '"';
    }

    m_API->m_Service->m_ServerPool->GetServer(
        first_key.host, first_key.port).ExecWithRetry(cmd);
}

void CNetScheduleSubmitter::ReadConfirm(const string& batch_id,
    const vector<string>& job_ids)
{
    m_Impl->ExecReadCommand("CFRM ", "ReadConfirm",
        batch_id, job_ids, kEmptyStr);
}

void CNetScheduleSubmitter::ReadRollback(const string& batch_id,
    const vector<string>& job_ids)
{
    m_Impl->ExecReadCommand("RDRB ", "ReadRollback",
        batch_id, job_ids, kEmptyStr);
}

void CNetScheduleSubmitter::ReadFail(const string& batch_id,
    const vector<string>& job_ids,
    const string& error_message)
{
    m_Impl->ExecReadCommand("FRED ", "ReadFail",
        batch_id, job_ids, error_message);
}

struct SWaitJobPred {
    SWaitJobPred(const string& job_id) : m_JobId(job_id) {}

    bool operator()(const string& buf) const
    {
        static const char prefix[] = "JNTF key=";

        return NStr::StartsWith(buf, prefix) &&
            m_JobId == CTempString(buf.data() + sizeof(prefix) - 1,
                buf.size() - sizeof(prefix) - 1);
    }

    const string& m_JobId;
};


CNetScheduleAPI::EJobStatus
CNetScheduleSubmitter::SubmitJobAndWait(CNetScheduleJob& job,
                                        unsigned       wait_time,
                                        unsigned short udp_port)
{
    _ASSERT(wait_time);
    _ASSERT(udp_port);

    m_Impl->SubmitJobImpl(job, udp_port, wait_time);

    s_WaitNotification(wait_time, udp_port, SWaitJobPred(job.job_id));

    CNetScheduleAPI::EJobStatus status = GetJobStatus(job.job_id);

    if (status == CNetScheduleAPI::eDone || status == CNetScheduleAPI::eFailed)
        m_Impl->m_API.GetJobDetails(job);

    return status;
}

void CNetScheduleSubmitter::CancelJob(const string& job_key)
{
    m_Impl->m_API->x_SendJobCmdWaitResponse("CANCEL", job_key);
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

const CNetScheduleAPI::SServerParams& CNetScheduleSubmitter::GetServerParams()
{
    return m_Impl->m_API->GetServerParams();
}

END_NCBI_SCOPE
