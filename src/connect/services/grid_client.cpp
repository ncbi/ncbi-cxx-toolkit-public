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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko, Dmitry Kazimirov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include "netschedule_api_impl.hpp"
#include "netcache_api_impl.hpp"

#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/grid_client.hpp>

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////
//
CGridClient::CGridClient(CNetScheduleSubmitter::TInstance ns_submitter,
                         IBlobStorage& storage,
                         ECleanUp cleanup,
                         EProgressMsg progress_msg) :
    CGridClient(ns_submitter, dynamic_cast<CBlobStorage_NetCache&>(storage).GetNetCacheAPI(), cleanup, progress_msg)
{
}

CGridClient::CGridClient(CNetScheduleSubmitter::TInstance ns_submitter,
                         CNetCacheAPI::TInstance nc_client,
                         ECleanUp cleanup,
                         EProgressMsg progress_msg) :
    m_NetScheduleSubmitter(ns_submitter),
    m_NetCacheAPI(nc_client),
    m_JobBatchSubmitter(*this),
    m_AutoCleanUp(cleanup == eAutomaticCleanup),
    m_UseProgress(progress_msg == eProgressMsgOn)
{
}

CGridJobBatchSubmitter& CGridClient::GetJobBatchSubmitter()
{
    m_JobBatchSubmitter.Reset();
    return m_JobBatchSubmitter;
}

void CGridClient::CancelJob(const string& job_key)
{
    m_NetScheduleSubmitter.CancelJob(job_key);
}
void CGridClient::RemoveDataBlob(const string& data_key)
{
    if (CNetCacheKey::IsValidKey(data_key, m_NetCacheAPI.GetCompoundIDPool()))
        m_NetCacheAPI.Remove(data_key);
}

size_t CGridClient::GetMaxServerInputSize()
{
    return m_NetScheduleSubmitter->m_API->m_UseEmbeddedStorage ?
        m_NetScheduleSubmitter->m_API->GetServerParams().max_input_size : 0;
}

//////////////////////////////////////////////////////////////////////////////
//
void CGridClient::SetJobInput(const string& input)
{
    m_Job.input = input;
}

CNcbiOstream& CGridClient::GetOStream()
{
    return m_GridWrite(GetNetCacheAPI(), GetMaxServerInputSize(), m_Job.input);
}

void CGridClient::CloseStream()
{
    m_GridWrite.Reset(true);
}

string CGridClient::Submit(const string& affinity)
{
    CloseStream();

    if (!affinity.empty() && m_Job.affinity.empty())
        m_Job.affinity = affinity;
    string job_key = GetNetScheduleSubmitter().SubmitJob(m_Job);
    x_RenewAllJobBlobs(m_NetScheduleSubmitter->m_API->m_JobTtl);
    m_Job.Reset();
    return job_key;
}

CNetScheduleAPI::EJobStatus CGridClient::SubmitAndWait(unsigned wait_time)
{
    CloseStream();
    time_t job_exptime = 0;
    CNetScheduleAPI::EJobStatus status =
        m_NetScheduleSubmitter->SubmitJobAndWait(m_Job, wait_time, &job_exptime);

    return x_CheckAllJobBlobs(status, job_exptime);
}

CNetScheduleAPI::EJobStatus CGridClient::WaitForJob(const string& job_key,
        unsigned wait_time)
{
    CNetScheduleAPI::EJobStatus status =
        m_NetScheduleSubmitter.WaitForJob(job_key, wait_time);

    return x_CheckAllJobBlobs(status, m_NetScheduleSubmitter->m_API->m_JobTtl);
}

//////////////////////////////////////////////////////////
void CGridJobBatchSubmitter::CheckIfBatchAlreadySubmitted()
{
    if (m_HasBeenSubmitted)
        NCBI_THROW(CGridClientException, eBatchAlreadySubmitted,
            "The batch has been already submitted. "
            "Use Reset() to start a new one");
}

void CGridJobBatchSubmitter::CheckIfBatchSubmittedAndPrepareNextJob()
{
    CheckIfBatchAlreadySubmitted();

    if (m_Jobs.empty())
        PrepareNextJob();
}

void CGridJobBatchSubmitter::SetJobInput(const string& input)
{
    CheckIfBatchSubmittedAndPrepareNextJob();

    m_Jobs[m_JobIndex].input = input;
}

CNcbiOstream& CGridJobBatchSubmitter::GetOStream()
{
    CheckIfBatchSubmittedAndPrepareNextJob();
    return m_GridWrite(m_GridClient.GetNetCacheAPI(), m_GridClient.GetMaxServerInputSize(),
            m_Jobs[m_JobIndex].input);
}

void CGridJobBatchSubmitter::SetJobMask(CNetScheduleAPI::TJobMask mask)
{
    CheckIfBatchSubmittedAndPrepareNextJob();

    m_Jobs[m_JobIndex].mask = mask;
}

void CGridJobBatchSubmitter::SetJobAffinity(const string& affinity)
{
    CheckIfBatchSubmittedAndPrepareNextJob();

    m_Jobs[m_JobIndex].affinity = affinity;
}

void CGridJobBatchSubmitter::PrepareNextJob()
{
    CheckIfBatchAlreadySubmitted();
    m_GridWrite.Reset();
    if (!m_Jobs.empty())
        ++m_JobIndex;
    m_Jobs.push_back(CNetScheduleJob());
}

void CGridJobBatchSubmitter::Submit(const string& job_group)
{
    CheckIfBatchAlreadySubmitted();
    m_GridWrite.Reset();
    if (!m_Jobs.empty()) {
        m_GridClient.GetNetScheduleSubmitter().SubmitJobBatch(m_Jobs,
                job_group);
        m_HasBeenSubmitted = true;
    }
}

void CGridJobBatchSubmitter::Reset()
{
    m_GridWrite.Reset();
    m_HasBeenSubmitted = false;
    m_JobIndex = 0;
    m_Jobs.clear();
}

CGridJobBatchSubmitter::CGridJobBatchSubmitter(CGridClient& grid_client)
    : m_GridClient(grid_client), m_JobIndex(0),
      m_HasBeenSubmitted(false)
{
}

static unsigned s_TimeToTtl(time_t time);

//////////////////////////////////////////////////////////////////////////////
//

CNetScheduleAPI::EJobStatus CGridClient::x_CheckAllJobBlobs(
        CNetScheduleAPI::EJobStatus status, time_t job_exptime)
{
    if (m_AutoCleanUp && (
              status == CNetScheduleAPI::eDone ||
              status == CNetScheduleAPI::eCanceled)) {
        if (m_Job.input.length() > 1 &&
                m_Job.input[0] == 'K' && m_Job.input[1] == ' ')
            RemoveDataBlob(m_Job.input.c_str() + 2);
        if (m_UseProgress) {
            GetNetScheduleSubmitter().GetProgressMsg(m_Job);
            if (m_Job.progress_msg.length() > 1) {
                if (m_Job.progress_msg[1] != ' ')
                    RemoveDataBlob(m_Job.progress_msg);
                else if (m_Job.progress_msg[0] == 'K')
                    RemoveDataBlob(m_Job.progress_msg.c_str() + 2);
            }
        }
    } else {
        x_RenewAllJobBlobs(s_TimeToTtl(job_exptime));
    }

    m_JobDetailsRead = true;
    return status;
}
CNetScheduleAPI::EJobStatus CGridClient::GetStatus()
{
    time_t job_exptime = 0;

    CNetScheduleAPI::EJobStatus status =
        GetNetScheduleSubmitter().GetJobDetails(m_Job, &job_exptime);

    return x_CheckAllJobBlobs(status, job_exptime);
}

CNcbiIstream& CGridClient::GetIStream()
{
    x_GetJobDetails();
    return m_GridRead(GetNetCacheAPI(), m_Job.output, &m_BlobSize);
}

string CGridClient::GetProgressMessage()
{
    if (m_UseProgress) {
        GetNetScheduleSubmitter().GetProgressMsg(m_Job);

        if (m_Job.progress_msg.length() <= 1)
            return m_Job.progress_msg;
        else {
            string progress_message_key;
            if (m_Job.progress_msg[1] != ' ')
                if (CNetCacheKey::IsValidKey(m_Job.progress_msg,
                        m_NetCacheAPI.GetCompoundIDPool()))
                    progress_message_key = m_Job.progress_msg;
                else
                    return m_Job.progress_msg;
            else if (m_Job.progress_msg[0] == 'K')
                progress_message_key = m_Job.progress_msg.c_str() + 2;
            else if (m_Job.progress_msg[0] == 'D')
                return m_Job.progress_msg.c_str() + 2;
            else
                return m_Job.progress_msg;

            string buffer;
            GetNetCacheAPI().ReadData(progress_message_key, buffer);
            return buffer;
        }
    }
    return string();
}

void CGridClient::SetJobKey(const string& job_key)
{
    m_Job.Reset();
    m_Job.job_id = job_key;
    m_GridRead.Reset();
    m_BlobSize = 0;
    m_JobDetailsRead = false;
}

void CGridClient::x_ProlongBlobLifetime(const string& blob_key, unsigned ttl)
{
    try {
        m_NetCacheAPI.ProlongBlobLifetime(blob_key, ttl);
    }
    catch (CNetServiceException& e) {
        ERR_POST(Warning << "Error while prolonging lifetime for " <<
            blob_key << ": " << e.GetMsg());
    }
}

bool CGridClient::x_ProlongJobFieldLifetime(
        const string& job_field, unsigned ttl)
{
    if (!NStr::StartsWith(job_field, "K "))
        return false;

    x_ProlongBlobLifetime(string(job_field, 2), ttl);

    return true;
}

static unsigned s_TimeToTtl(time_t exptime)
{
    time_t current_time = time(NULL);
    return exptime > current_time ? unsigned(exptime - current_time + 1) : 0;
}

void CGridClient::x_RenewAllJobBlobs(unsigned ttl)
{
    x_ProlongJobFieldLifetime(m_Job.input, ttl);
    x_ProlongJobFieldLifetime(m_Job.output, ttl);
    if (!m_Job.progress_msg.empty() &&
            !x_ProlongJobFieldLifetime(m_Job.progress_msg, ttl) &&
            CNetCacheKey::ParseBlobKey(m_Job.progress_msg.data(),
                    m_Job.progress_msg.length(), NULL,
                    m_NetCacheAPI.GetCompoundIDPool()))
        x_ProlongBlobLifetime(m_Job.progress_msg, ttl);
}

void CGridClient::x_GetJobDetails()
{
    if (m_JobDetailsRead)
        return;
    time_t job_exptime = 0;
    GetNetScheduleSubmitter().GetJobDetails(m_Job, &job_exptime);
    x_RenewAllJobBlobs(s_TimeToTtl(job_exptime));
    m_JobDetailsRead = true;
}

const string& CGridClient::GetJobOutput()
{
    x_GetJobDetails();
    return m_Job.output;
}

const string& CGridClient::GetJobInput()
{
    x_GetJobDetails();
    return m_Job.input;
}
int CGridClient::GetReturnCode()
{
    x_GetJobDetails();
    return m_Job.ret_code;
}

const string& CGridClient::GetErrorMessage()
{
    x_GetJobDetails();
    return m_Job.error_msg;
}


END_NCBI_SCOPE
