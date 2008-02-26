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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/rwstream.hpp>

#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/grid_client.hpp>

BEGIN_NCBI_SCOPE

//////////////////////////////////////////////////////////////////////////////
//
CGridClient::CGridClient(const CNetScheduleSubmitter& ns_client,
                         IBlobStorage& storage,
                         ECleanUp cleanup,
                         EProgressMsg progress_msg,
                         bool use_embedded_storage)
    : m_NSClient(ns_client), m_NSStorage(storage),
      m_UseEmbeddedStorage(use_embedded_storage)
{
    m_JobSubmitter.reset(new CGridJobSubmitter(*this,
                                             progress_msg == eProgressMsgOn));
    m_JobBatchSubmitter.reset(new CGridJobBatchSubmitter(*this));
    m_JobStatus.reset(new CGridJobStatus(*this,
                                         cleanup == eAutomaticCleanup,
                                         progress_msg == eProgressMsgOn));
}

CGridClient::~CGridClient()
{
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
    if (m_NSStorage.IsKeyValid(data_key))
        m_NSStorage.DeleteBlob(data_key);
}

size_t CGridClient::GetMaxServerInputSize()
{
    return m_UseEmbeddedStorage ? m_NSClient.GetServerParams().max_input_size : 0;
}

//////////////////////////////////////////////////////////////////////////////
//
CGridJobSubmitter::CGridJobSubmitter(CGridClient& grid_client,
                                     bool use_progress)
    : m_GridClient(grid_client), m_UseProgress(use_progress)
{
}

CGridJobSubmitter::~CGridJobSubmitter()
{
}
void CGridJobSubmitter::SetJobInput(const string& input)
{
    m_Job.input = input;
}
void CGridJobSubmitter::SetJobMask(CNetScheduleAPI::TJobMask mask)
{
    m_Job.mask = mask;
}
void CGridJobSubmitter::SetJobTags(const CNetScheduleAPI::TJobTags& tags)
{
    m_Job.tags = tags;
}
void CGridJobSubmitter::SetJobAffinity(const string& affinity)
{
    m_Job.affinity = affinity;
}

CNcbiOstream& CGridJobSubmitter::GetOStream()
{
    IWriter* writer =
        new CStringOrBlobStorageWriter(m_GridClient.GetMaxServerInputSize(),
                                       m_GridClient.GetStorage(),
                                       m_Job.input);

    m_WStream.reset(new CWStream(writer, 0, 0, CRWStreambuf::fOwnWriter
                                             | CRWStreambuf::fLogExceptions ));
    m_WStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    return *m_WStream;
    //    return m_GridClient.GetStorage().CreateOStream(m_Input);
}

string CGridJobSubmitter::Submit(const string& affinity)
{
    m_WStream.reset();
    if (!affinity.empty() && m_Job.affinity.empty())
        m_Job.affinity = affinity;
    if (m_UseProgress)
        m_Job.progress_msg = m_GridClient.GetStorage().CreateEmptyBlob();
    string job_key = m_GridClient.GetNSClient().SubmitJob(m_Job);
    m_Job.Reset();
    return job_key;
}

//////////////////////////////////////////////////////////
CGridJobBatchSubmitter::~CGridJobBatchSubmitter()
{
}

void CGridJobBatchSubmitter::SetJobInput(const string& input)
{
    if (m_HasBeenSubmitted)
        NCBI_THROW(CGridClientException, eBatchHasAlreadyBeenSubmitted,
                   "The Batch has already been submitted. Use Reset() method to start the a one");
    if( m_Jobs.empty() )
        PrepareNextJob();
    m_Jobs[m_JobIndex].input = input;
}

CNcbiOstream& CGridJobBatchSubmitter::GetOStream()
{
    if (m_HasBeenSubmitted)
        NCBI_THROW(CGridClientException, eBatchHasAlreadyBeenSubmitted,
                   "The Batch has already been submitted. Use Reset() method to start the a one");

    if ( m_Jobs.empty() )
        PrepareNextJob();
    IWriter* writer =
        new CStringOrBlobStorageWriter(m_GridClient.GetMaxServerInputSize(),
                                       m_GridClient.GetStorage(),
                                       m_Jobs[m_JobIndex].input);

    m_WStream.reset(new CWStream(writer, 0, 0, CRWStreambuf::fOwnWriter
                                             | CRWStreambuf::fLogExceptions ));
    m_WStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    return *m_WStream;
}

void CGridJobBatchSubmitter::SetJobMask(CNetScheduleAPI::TJobMask mask)
{
    if (m_HasBeenSubmitted)
        NCBI_THROW(CGridClientException, eBatchHasAlreadyBeenSubmitted,
                   "The Batch has already been submitted. Use Reset() method to start the a one");
    if( m_Jobs.empty() )
        PrepareNextJob();
    m_Jobs[m_JobIndex].mask = mask;
}

void CGridJobBatchSubmitter::SetJobTags(const CNetScheduleAPI::TJobTags& tags)
{
    if (m_HasBeenSubmitted)
        NCBI_THROW(CGridClientException, eBatchHasAlreadyBeenSubmitted,
                   "The Batch has already been submitted. Use Reset() method to start the a one");
    if( m_Jobs.empty() )
        PrepareNextJob();
    m_Jobs[m_JobIndex].tags = tags;
}

void CGridJobBatchSubmitter::SetJobAffinity(const string& affinity)
{
    if (m_HasBeenSubmitted)
        NCBI_THROW(CGridClientException, eBatchHasAlreadyBeenSubmitted,
                   "The Batch has already been submitted. Use Reset() method to start the a one");
    if( m_Jobs.empty() )
        PrepareNextJob();
    m_Jobs[m_JobIndex].affinity = affinity;
}

void CGridJobBatchSubmitter::PrepareNextJob()
{
    if (m_HasBeenSubmitted)
        NCBI_THROW(CGridClientException, eBatchHasAlreadyBeenSubmitted,
                   "The Batch has already been submitted. Use Reset() method to start the a one");
    m_WStream.reset();
    if( !m_Jobs.empty() )
        ++m_JobIndex;
    m_Jobs.push_back(CNetScheduleJob());
}

void CGridJobBatchSubmitter::Submit()
{
    if (m_HasBeenSubmitted)
        NCBI_THROW(CGridClientException, eBatchHasAlreadyBeenSubmitted,
                   "The Batch has already been submitted. Use Reset() method to start the a one");
    m_WStream.reset();
    if( !m_Jobs.empty() ) {
        m_GridClient.GetNSClient().SubmitJobBatch(m_Jobs);
        m_HasBeenSubmitted = true;
    }

}

void CGridJobBatchSubmitter::Reset()
{
    m_WStream.reset();
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
    if ( m_AutoCleanUp && (
              status == CNetScheduleAPI::eDone ||
              status == CNetScheduleAPI::eCanceled) ) {
        x_GetJobDetails();
        m_GridClient.RemoveDataBlob(m_Job.input);
        if (m_UseProgress) {
            m_GridClient.GetNSClient().GetProgressMsg(m_Job);
            if (!m_Job.progress_msg.empty())
                m_GridClient.RemoveDataBlob(m_Job.progress_msg);
        }
    }
    return status;
}

CNcbiIstream& CGridJobStatus::GetIStream(IBlobStorage::ELockMode mode)
{
    x_GetJobDetails();
    IReader* reader = new CStringOrBlobStorageReader(m_Job.output,
                                                     m_GridClient.GetStorage(),
                                                     &m_BlobSize, mode);
    m_RStream.reset(new CRStream(reader,0,0,CRWStreambuf::fOwnReader
                                          | CRWStreambuf::fLogExceptions));
    m_RStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    return *m_RStream;
}

string CGridJobStatus::GetProgressMessage() const
{
    if (m_UseProgress) {
        m_GridClient.GetNSClient().GetProgressMsg(m_Job);
        if (!m_Job.progress_msg.empty())
            return m_GridClient.GetStorage().GetBlobAsString(m_Job.progress_msg);
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

void CGridJobStatus::x_GetJobDetails() const
{
    if (m_JobDetailsRead)
        return;
    m_GridClient.GetNSClient().GetJobDetails(m_Job);
    m_JobDetailsRead = true;
}

const string& CGridJobStatus::GetJobOutput() const
{
    x_GetJobDetails();
    return m_Job.output;
}

const string& CGridJobStatus::GetJobInput() const
{
    x_GetJobDetails();
    return m_Job.input;
}
int CGridJobStatus::GetReturnCode() const
{
    x_GetJobDetails();
    return m_Job.ret_code;
}

const string& CGridJobStatus::GetErrorMessage() const
{
    x_GetJobDetails();
    return m_Job.error_msg;
}


END_NCBI_SCOPE
