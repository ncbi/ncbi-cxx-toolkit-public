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

#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/grid_client.hpp>

#include <corelib/rwstream.hpp>

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////
//
CGridClient::CGridClient(CNetScheduleSubmitter::TInstance ns_submitter,
                         IBlobStorage& storage,
                         ECleanUp cleanup,
                         EProgressMsg progress_msg) :
    m_NetScheduleSubmitter(ns_submitter),
    m_NetCacheAPI(
        dynamic_cast<CBlobStorage_NetCache&>(storage).GetNetCacheAPI())
{
    Init(cleanup, progress_msg);
}

CGridClient::CGridClient(CNetScheduleSubmitter::TInstance ns_submitter,
                         CNetCacheAPI::TInstance nc_client,
                         ECleanUp cleanup,
                         EProgressMsg progress_msg) :
    m_NetScheduleSubmitter(ns_submitter),
    m_NetCacheAPI(nc_client)
{
    Init(cleanup, progress_msg);
}

void CGridClient::Init(ECleanUp cleanup, EProgressMsg progress_msg)
{
    m_JobBatchSubmitter.reset(new CGridJobBatchSubmitter(*this));

    m_BlobSize = 0;
    m_AutoCleanUp = cleanup == eAutomaticCleanup;
    m_UseProgress = progress_msg == eProgressMsgOn;
    m_JobDetailsRead = false;
}

CGridJobBatchSubmitter& CGridClient::GetJobBatchSubmitter()
{
    m_JobBatchSubmitter->Reset();
    return *m_JobBatchSubmitter;
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
CNcbiOstream& CGridClient::GetOStream()
{
    m_Writer.reset(new CStringOrBlobStorageWriter(
        GetMaxServerInputSize(),
        GetNetCacheAPI(),
        m_Job.input));

    m_WStream.reset(new CWStream(m_Writer.get(),
        0, 0, CRWStreambuf::fLeakExceptions));

    m_WStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);

    return *m_WStream;
}

void CGridClient::CloseStream()
{
    if (m_Writer.get() != NULL) {
        if (m_WStream.get() != NULL) {
            m_WStream->flush();
            m_WStream.reset();
        }

        m_Writer->Close();
        m_Writer.reset();
    }
}

string CGridClient::Submit(const string& affinity)
{
    CloseStream();

    if (!affinity.empty() && m_Job.affinity.empty())
        m_Job.affinity = affinity;
    string job_key = GetNetScheduleSubmitter().SubmitJob(m_Job);
    m_Job.Reset();
    return job_key;
}

CNetScheduleAPI::EJobStatus CGridClient::SubmitAndWait(unsigned wait_time)
{
    CloseStream();

    return GetNetScheduleSubmitter().SubmitJobAndWait(m_Job, wait_time);
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

    m_Writer.reset(new CStringOrBlobStorageWriter(
        m_GridClient.GetMaxServerInputSize(),
        m_GridClient.GetNetCacheAPI(),
        m_Jobs[m_JobIndex].input));

    m_WStream.reset(new CWStream(m_Writer.get(),
        0, 0, CRWStreambuf::fLeakExceptions));

    m_WStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);

    return *m_WStream;
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
    m_WStream.reset();
    if (m_Writer.get() != NULL) {
        m_Writer->Close();
        m_Writer.reset();
    }
    if (!m_Jobs.empty())
        ++m_JobIndex;
    m_Jobs.push_back(CNetScheduleJob());
}

void CGridJobBatchSubmitter::Submit(const string& job_group)
{
    CheckIfBatchAlreadySubmitted();
    m_WStream.reset();
    if (m_Writer.get() != NULL) {
        m_Writer->Close();
        m_Writer.reset();
    }
    if (!m_Jobs.empty()) {
        m_GridClient.GetNetScheduleSubmitter().SubmitJobBatch(m_Jobs,
                job_group);
        m_HasBeenSubmitted = true;
    }
}

void CGridJobBatchSubmitter::Reset()
{
    m_WStream.reset();
    if (m_Writer.get() != NULL) {
        m_Writer->Close();
        m_Writer.reset();
    }
    m_HasBeenSubmitted = false;
    m_JobIndex = 0;
    m_Jobs.clear();
}

CGridJobBatchSubmitter::CGridJobBatchSubmitter(CGridClient& grid_client)
    : m_GridClient(grid_client), m_JobIndex(0),
      m_HasBeenSubmitted(false)
{
}

//////////////////////////////////////////////////////////////////////////////
//

CNetScheduleAPI::EJobStatus CGridClient::GetStatus()
{
    time_t job_exptime = 0;

    CNetScheduleAPI::EJobStatus status =
        GetNetScheduleSubmitter().GetJobDetails(m_Job, &job_exptime);

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
        x_RenewAllJobBlobs(job_exptime);
    }
    return status;
}

CNcbiIstream& CGridClient::GetIStream()
{
    x_GetJobDetails();
    IReader* reader = new CStringOrBlobStorageReader(
        m_Job.output, GetNetCacheAPI(), &m_BlobSize);
    m_RStream.reset(new CRStream(reader,0,0,CRWStreambuf::fOwnReader
                                          | CRWStreambuf::fLeakExceptions));
    m_RStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    return *m_RStream;
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
    return kEmptyStr;
}

void CGridClient::SetJobKey(const string& job_key)
{
    m_Job.Reset();
    m_Job.job_id = job_key;
    m_RStream.reset();
    m_BlobSize = 0;
    m_JobDetailsRead = false;
}

void CGridClient::x_ProlongBlobLifetime(const string& blob_key, unsigned ttl)
{
    try {
        m_NetCacheAPI.ProlongBlobLifetime(blob_key, ttl);
    }
    catch (CNetServiceException& e) {
        LOG_POST(Warning << "Error while prolonging lifetime for " <<
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

void CGridClient::x_RenewAllJobBlobs(time_t job_exptime)
{
    time_t current_time = time(NULL);
    unsigned ttl = 0;
    if (job_exptime > current_time)
        ttl = unsigned(job_exptime - current_time + 1);
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
    x_RenewAllJobBlobs(job_exptime);
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
