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
: m_NSClient(ns_client), m_NSStorage(storage)
{
    m_JobSubmitter.reset(new CGridJobSubmitter(*this,
                                             progress_msg == eProgressMsgOn,
                                             use_embedded_storage));
    m_JobBatchSubmitter.reset(new CGridJobBatchSubmitter(*this,
                                                         use_embedded_storage));
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

//////////////////////////////////////////////////////////////////////////////
//
CGridJobSubmitter::CGridJobSubmitter(CGridClient& grid_client, 
                                   bool use_progress,
                                   bool use_embedded_storage)
    : m_GridClient(grid_client), m_UseProgress(use_progress), 
      m_UseEmbeddedStorage(use_embedded_storage)    
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
    size_t max_data_size = m_UseEmbeddedStorage ? kNetScheduleMaxDataSize : 0;
    IWriter* writer = 
        new CStringOrBlobStorageWriter(max_data_size,
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

    if( m_Jobs.empty() )
        PrepareNextJob();
    size_t max_data_size = m_UseEmbeddedStorage ? kNetScheduleMaxDataSize : 0;
    IWriter* writer = 
        new CStringOrBlobStorageWriter(max_data_size,
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

CGridJobBatchSubmitter::CGridJobBatchSubmitter(CGridClient& grid_client, bool use_embedded_storage)
    : m_GridClient(grid_client), m_JobIndex(0),
      m_UseEmbeddedStorage(use_embedded_storage),
      m_HasBeenSubmitted(false)
{
}

//////////////////////////////////////////////////////////////////////////////
//

CGridJobStatus::CGridJobStatus(CGridClient& grid_client, 
                               bool auto_cleanup, 
                               bool use_progress)
    : m_GridClient(grid_client), m_BlobSize(0), 
      m_AutoCleanUp(auto_cleanup), m_UseProgress(use_progress)
{
}

CGridJobStatus::~CGridJobStatus()
{
}

CNetScheduleAPI::EJobStatus CGridJobStatus::GetStatus()
{
    CNetScheduleAPI::EJobStatus status = m_GridClient.GetNSClient(). GetJobDetails(m_Job);
    if ( m_AutoCleanUp && (
              status == CNetScheduleAPI::eDone || 
              status == CNetScheduleAPI::eCanceled) ) {
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
    IReader* reader = new CStringOrBlobStorageReader(m_Job.output,
                                                     m_GridClient.GetStorage(),
                                                     &m_BlobSize, mode);
    m_RStream.reset(new CRStream(reader,0,0,CRWStreambuf::fOwnReader
                                          | CRWStreambuf::fLogExceptions));
    m_RStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    return *m_RStream;
}

string CGridJobStatus::GetProgressMessage()
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
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.15  2006/09/19 14:34:41  didenko
 * Code clean up
 * Catch and log all exceptions in destructors
 *
 * Revision 1.14  2006/07/13 14:27:26  didenko
 * Added access to the job's mask for grid's clients/wnodes
 *
 * Revision 1.13  2006/06/28 16:01:56  didenko
 * Redone job's exlusivity processing
 *
 * Revision 1.12  2006/06/19 19:41:06  didenko
 * Spelling fix
 *
 * Revision 1.11  2006/05/30 16:41:05  didenko
 * Improved error handling
 *
 * Revision 1.10  2006/05/08 15:16:42  didenko
 * Added support for an optional saving of a remote application's stdout
 * and stderr into files on a local file system
 *
 * Revision 1.9  2006/05/03 14:50:08  didenko
 * Added affinity support
 *
 * Revision 1.8  2006/04/12 19:03:49  didenko
 * Renamed parameter "use_embedded_input" to "use_embedded_storage"
 *
 * Revision 1.7  2006/03/15 17:30:12  didenko
 * Added ability to use embedded NetSchedule job's storage as a job's input/output data instead of using it as a NetCache blob key. This reduces network traffic and increases job submittion speed.
 *
 * Revision 1.6  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 1.5  2005/10/26 16:37:44  didenko
 * Added for non-blocking read for netschedule storage
 *
 * Revision 1.4  2005/04/20 19:25:59  didenko
 * Added support for progress messages passing from a worker node to a client
 *
 * Revision 1.3  2005/04/18 18:54:24  didenko
 * Added optional automatic NetScheduler storage cleanup
 *
 * Revision 1.2  2005/03/25 21:36:33  ucko
 * Empty strings with erase() rather than clear() for GCC 2.95 compatibility.
 *
 * Revision 1.1  2005/03/25 16:25:41  didenko
 * Initial version
 *
 * ===========================================================================
 */
 
