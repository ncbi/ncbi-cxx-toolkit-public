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
CGridClient::CGridClient(CNetScheduleSubmitter::TInstance ns_client,
                         IBlobStorage& storage,
                         ECleanUp cleanup,
                         EProgressMsg progress_msg) :
    m_NSClient(ns_client),
    m_NetCacheAPI(
        dynamic_cast<CBlobStorage_NetCache&>(storage).GetNetCacheAPI())
{
    Init(cleanup, progress_msg);
}

CGridClient::CGridClient(CNetScheduleSubmitter::TInstance ns_client,
                         CNetCacheAPI::TInstance nc_client,
                         ECleanUp cleanup,
                         EProgressMsg progress_msg) :
    m_NSClient(ns_client),
    m_NetCacheAPI(nc_client)
{
    Init(cleanup, progress_msg);
}

void CGridClient::Init(ECleanUp cleanup, EProgressMsg progress_msg)
{
    m_JobSubmitter.reset(new CGridJobSubmitter(*this));
    m_JobBatchSubmitter.reset(new CGridJobBatchSubmitter(*this));
    m_JobStatus.reset(new CGridJobStatus(*this,
                                         cleanup == eAutomaticCleanup,
                                         progress_msg == eProgressMsgOn));
}

CGridJobSubmitter& CGridClient::GetJobSubmitter()
{
    return *m_JobSubmitter;
}
CGridJobBatchSubmitter& CGridClient::GetJobBatchSubmitter()
{
    m_JobBatchSubmitter->Reset();
    return *m_JobBatchSubmitter;
}
CGridJobStatus& CGridClient::GetJobStatus(const string& job_key)
{
    m_JobStatus->x_SetJobKey(job_key);
    return *m_JobStatus;
}

void CGridClient::CancelJob(const string& job_key)
{
    m_NSClient.CancelJob(job_key);
}
void CGridClient::RemoveDataBlob(const string& data_key)
{
    if (CNetCacheKey::IsValidKey(data_key))
        m_NetCacheAPI.Remove(data_key);
}

size_t CGridClient::GetMaxServerInputSize()
{
    return m_NSClient->m_API->m_UseEmbeddedStorage ?
        m_NSClient.GetServerParams().max_input_size : 0;
}

//////////////////////////////////////////////////////////////////////////////
//
CNcbiOstream& CGridJobSubmitter::GetOStream()
{
    m_Writer.reset(new CStringOrBlobStorageWriter(
        m_GridClient.GetMaxServerInputSize(),
        m_GridClient.GetNetCacheAPI(),
        m_Job.input));

    m_WStream.reset(new CWStream(m_Writer.get(),
        0, 0, CRWStreambuf::fLeakExceptions));

    m_WStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);

    return *m_WStream;
}

void CGridJobSubmitter::CloseStream()
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

string CGridJobSubmitter::Submit(const string& affinity)
{
    CloseStream();

    if (!affinity.empty() && m_Job.affinity.empty())
        m_Job.affinity = affinity;
    string job_key = m_GridClient.GetNSClient().SubmitJob(m_Job);
    m_Job.Reset();
    return job_key;
}

//////////////////////////////////////////////////////////
CGridJobBatchSubmitter::~CGridJobBatchSubmitter()
{
}

void CGridJobBatchSubmitter::CheckIfAlreadySubmitted()
{
    if (m_HasBeenSubmitted)
        NCBI_THROW(CGridClientException, eBatchAlreadySubmitted,
            "The batch has been already submitted. "
            "Use Reset() to start a new one");
}

void CGridJobBatchSubmitter::SetJobInput(const string& input)
{
    CheckIfAlreadySubmitted();
    if (m_Jobs.empty())
        PrepareNextJob();
    m_Jobs[m_JobIndex].input = input;
}

CNcbiOstream& CGridJobBatchSubmitter::GetOStream()
{
    CheckIfAlreadySubmitted();
    if (m_Jobs.empty())
        PrepareNextJob();
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
    CheckIfAlreadySubmitted();
    if (m_Jobs.empty())
        PrepareNextJob();
    m_Jobs[m_JobIndex].mask = mask;
}

void CGridJobBatchSubmitter::SetJobAffinity(const string& affinity)
{
    CheckIfAlreadySubmitted();
    if (m_Jobs.empty())
        PrepareNextJob();
    m_Jobs[m_JobIndex].affinity = affinity;
}

void CGridJobBatchSubmitter::PrepareNextJob()
{
    CheckIfAlreadySubmitted();
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
    CheckIfAlreadySubmitted();
    m_WStream.reset();
    if (m_Writer.get() != NULL) {
        m_Writer->Close();
        m_Writer.reset();
    }
    if (!m_Jobs.empty()) {
        m_GridClient.GetNSClient().SubmitJobBatch(m_Jobs, job_group);
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

CGridJobStatus::CGridJobStatus(CGridClient& grid_client,
                               bool auto_cleanup,
                               bool use_progress)
    : m_GridClient(grid_client), m_BlobSize(0),
      m_AutoCleanUp(auto_cleanup), m_UseProgress(use_progress),
      m_JobDetailsRead(false)
{
}

CGridJobStatus::~CGridJobStatus()
{
}

CNetScheduleAPI::EJobStatus CGridJobStatus::GetStatus()
{
    CNetScheduleAPI::EJobStatus status =
        m_GridClient.GetNSClient().GetJobStatus(m_Job.job_id);
    if (m_AutoCleanUp && (
              status == CNetScheduleAPI::eDone ||
              status == CNetScheduleAPI::eCanceled)) {
        x_GetJobDetails();
        const char* job_input = m_Job.input.c_str();
        if (job_input[0] == 'K' && job_input[1] == ' ')
            m_GridClient.RemoveDataBlob(job_input + 2);
        if (m_UseProgress) {
            m_GridClient.GetNSClient().GetProgressMsg(m_Job);
            if (!m_Job.progress_msg.empty())
                m_GridClient.RemoveDataBlob(m_Job.progress_msg);
        }
    }
    return status;
}

CNcbiIstream& CGridJobStatus::GetIStream()
{
    x_GetJobDetails();
    IReader* reader = new CStringOrBlobStorageReader(
        m_Job.output, m_GridClient.GetNetCacheAPI(), &m_BlobSize);
    m_RStream.reset(new CRStream(reader,0,0,CRWStreambuf::fOwnReader
                                          | CRWStreambuf::fLeakExceptions));
    m_RStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    return *m_RStream;
}

string CGridJobStatus::GetProgressMessage()
{
    if (m_UseProgress) {
        m_GridClient.GetNSClient().GetProgressMsg(m_Job);
        if (!m_Job.progress_msg.empty()) {
            string buffer;
            m_GridClient.GetNetCacheAPI().ReadData(m_Job.progress_msg, buffer);
            return buffer;
        }
    }
    return kEmptyStr;
}

void CGridJobStatus::x_SetJobKey(const string& job_key)
{
    m_Job.Reset();
    m_Job.job_id = job_key;
    m_RStream.reset();
    m_BlobSize = 0;
    m_JobDetailsRead = false;
}

void CGridJobStatus::x_GetJobDetails()
{
    if (m_JobDetailsRead)
        return;
    m_GridClient.GetNSClient().GetJobDetails(m_Job);
    m_JobDetailsRead = true;
}

const string& CGridJobStatus::GetJobOutput()
{
    x_GetJobDetails();
    return m_Job.output;
}

const string& CGridJobStatus::GetJobInput()
{
    x_GetJobDetails();
    return m_Job.input;
}
int CGridJobStatus::GetReturnCode()
{
    x_GetJobDetails();
    return m_Job.ret_code;
}

const string& CGridJobStatus::GetErrorMessage()
{
    x_GetJobDetails();
    return m_Job.error_msg;
}


END_NCBI_SCOPE
