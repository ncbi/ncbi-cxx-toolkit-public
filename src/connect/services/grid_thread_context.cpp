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
 *    NetSchedule Worker Node implementation
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/rwstream.hpp>

#include <connect/services/grid_globals.hpp>
#include <connect/services/grid_debug_context.hpp>
#include <connect/services/grid_globals.hpp>
#include <connect/services/grid_rw_impl.hpp>

#include "grid_thread_context.hpp"


BEGIN_NCBI_SCOPE

/// @internal
CGridThreadContext::CGridThreadContext(CGridWorkerNode& node, long check_status_period)
    : m_JobContext(NULL), m_MsgThrottler(1), 
      m_StatusThrottler(1,CTimeSpan(check_status_period > 1 ? check_status_period : 1, 0))
{
    m_Reporter.reset(node.CreateClient());
    m_Reader.reset(node.CreateStorage());
    m_Writer.reset(node.CreateStorage());
    m_ProgressWriter.reset(node.CreateStorage());
}

void CGridThreadContext::SetJobContext(CWorkerNodeJobContext& job_context,
                                       const string& new_job_key, 
                                       const string& new_job_input,
                                       CNetScheduleClient::TJobMask jmask)
{
    job_context.Reset(new_job_key, new_job_input, 
                      CGridGlobals::GetInstance().GetNewJobNumber(), jmask);
    SetJobContext(job_context);
}

/// @internal
void CGridThreadContext::SetJobContext(CWorkerNodeJobContext& job_context)
{
    _ASSERT(!m_JobContext);
    m_JobContext = &job_context;
    job_context.SetThreadContext(this);
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (debug_context) {
        debug_context->DumpInput(m_JobContext->GetJobInput(), 
                                 m_JobContext->GetJobNumber());
    }
    m_JobContext->GetWorkerNode()
        .x_NotifyJobWatcher(*m_JobContext,
                            IWorkerNodeJobWatcher::eJobStarted);
}
/// @internal
void CGridThreadContext::Reset()
{
    _ASSERT(m_JobContext);
    m_JobContext->GetWorkerNode()
        .x_NotifyJobWatcher(*m_JobContext,
                            IWorkerNodeJobWatcher::eJobStopped);
    if (m_JobContext->IsJobExclusive()) {
        CGridGlobals::GetInstance().SetExclusiveMode(false);
    }
    m_JobContext->SetThreadContext(NULL);
    m_JobContext = NULL;
}
/// @internal
CWorkerNodeJobContext& CGridThreadContext::GetJobContext()
{
    _ASSERT(m_JobContext);
    return *m_JobContext;
}

/// @internal
CNcbiIstream& CGridThreadContext::GetIStream(IBlobStorage::ELockMode mode)
{
    _ASSERT(m_JobContext);
    if (m_Reader.get()) {
        IReader* reader =
               new CStringOrBlobStorageReader(m_JobContext->GetJobInput(),
                                              *m_Reader,
                                              &m_JobContext->SetJobInputBlobSize(),
                                              mode);
        m_RStream.reset(new CRStream(reader, 0,0, 
                                     CRWStreambuf::fOwnReader 
                                   | CRWStreambuf::fLogExceptions));
        m_RStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
        return *m_RStream;
    }
    NCBI_THROW(CBlobStorageException,
               eReader, "Reader is not set.");
}
/// @internal
CNcbiOstream& CGridThreadContext::GetOStream()
{
    _ASSERT(m_JobContext);
    if (m_Writer.get()) {
        size_t max_data_size = 
            m_JobContext->GetWorkerNode().IsEmeddedStorageUsed() ?
                                         kNetScheduleMaxDataSize : 0;
        IWriter* writer = 
            new CStringOrBlobStorageWriter(max_data_size,
                                           *m_Writer,
                                           m_JobContext->SetJobOutput());
        m_WStream.reset(new CWStream(writer, 0,0, CRWStreambuf::fOwnWriter
                                                | CRWStreambuf::fLogExceptions));
        m_WStream->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
        return *m_WStream;
    }
    NCBI_THROW(CBlobStorageException,
               eWriter, "Writer is not set.");
}
/// @internal
void CGridThreadContext::PutProgressMessage(const string& msg, bool send_immediately)
{
    _ASSERT(m_JobContext);
    if ( !send_immediately && !m_MsgThrottler.Approve(CRequestRateControl::eErrCode))
        return;
    try {
        CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
        string& blob_id = m_JobContext->SetJobProgressMsgKey();
        if (!debug_context || 
            debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {
            
            if (blob_id.empty() && m_Reporter.get()) {
                blob_id = m_Reporter->GetProgressMsg(m_JobContext->GetJobKey());
            }
            if (!blob_id.empty() && m_ProgressWriter.get()) {
                CNcbiOstream& os = m_ProgressWriter->CreateOStream(blob_id);
                os << msg;
                m_ProgressWriter->Reset();
            }
            else {
                //ERR_POST("Couldn't send a progress message.");
            }
        }
        if (debug_context) {
            debug_context->DumpProgressMessage(m_JobContext->GetJobKey(),
                                               msg, 
                                               m_JobContext->GetJobNumber());
        }           
    } catch (exception& ex) {
        ERR_POST("Couldn't send a progress message: " << ex.what());
    }
}
/// @internal
void CGridThreadContext::SetJobRunTimeout(unsigned time_to_run)
{
    _ASSERT(m_JobContext);
    if (m_Reporter.get()) {
        try {
            m_Reporter->SetRunTimeout(m_JobContext->GetJobKey(), time_to_run);
        }
        catch(exception& ex) {
            ERR_POST("CWorkerNodeJobContext::SetJobRunTimeout : " 
                     << ex.what());
        } 
    }
    else {
        NCBI_THROW(CBlobStorageException,
                   eWriter, "Reporter is not set.");
    }
}

void CGridThreadContext::JobDelayExpiration(unsigned time_to_run)
{
    _ASSERT(m_JobContext);
    if (m_Reporter.get()) {
        try {
            m_Reporter->JobDelayExpiration(m_JobContext->GetJobKey(), time_to_run);
        }
        catch(exception& ex) {
            ERR_POST("CWorkerNodeJobContext::JobDelayExpiration : " 
                     << ex.what());
        } 
    }
    else {
        NCBI_THROW(CBlobStorageException,
                   eWriter, "Reporter is not set.");
    }
}

/// @internal
bool CGridThreadContext::PutResult(int ret_code, 
                                   string& new_job_key,
                                   string& new_job_input,
                                   CNetScheduleClient::TJobMask& jmask)
{
    _ASSERT(m_JobContext);
    if ( m_JobContext->GetCommitStatus() != CWorkerNodeJobContext::eDone ) {
        PutFailure(m_JobContext->GetErrMsg(), ret_code);
        return false;
    }
    bool more_jobs = false;
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context || 
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        if (m_Reporter.get()) {
            if (CGridGlobals::GetInstance().
                GetShutdownLevel() != CNetScheduleClient::eNoShutdown ||
                CGridGlobals::GetInstance().IsExclusiveMode() ) {
                m_Reporter->PutResult(m_JobContext->GetJobKey(),
                                      ret_code,
                                      m_JobContext->GetJobOutput());
            } else {
                more_jobs = 
                    m_Reporter->PutResultGetJob(m_JobContext->GetJobKey(),
                                                ret_code,
                                                m_JobContext->GetJobOutput(),
                                                &new_job_key, 
                                                &new_job_input,
                                                &jmask);
            }
        }
    }
    m_JobContext->GetWorkerNode()
        .x_NotifyJobWatcher(*m_JobContext,
                            IWorkerNodeJobWatcher::eJobSucceed);
    return more_jobs;
}
/// @internal
void CGridThreadContext::ReturnJob()
{
    _ASSERT(m_JobContext);
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context || 
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        if (m_Reporter.get()) {
            int ret_code;
            string output;
            CNetScheduleClient::EJobStatus status = 
                m_Reporter->GetStatus(m_JobContext->GetJobKey(), 
                                      &ret_code, 
                                      &output);
            IWorkerNodeJobWatcher::EEvent event = IWorkerNodeJobWatcher::eJobReturned;
            switch(status) {
            case CNetScheduleClient::eJobNotFound:
                event = IWorkerNodeJobWatcher::eJobLost;
                break;
            case CNetScheduleClient::eFailed:
                event = IWorkerNodeJobWatcher::eJobFailed;
                break;
            case CNetScheduleClient::eDone:
                event = IWorkerNodeJobWatcher::eJobSucceed;
                break;
            case CNetScheduleClient::eCanceled:
                event = IWorkerNodeJobWatcher::eJobCanceled;
                break;
            case CNetScheduleClient::eRunning:
                m_Reporter->ReturnJob(m_JobContext->GetJobKey());
                break;
                //            case CNetScheduleClient::eReturned:
                //            case CNetScheduleClient::ePending:
            default:
                break;
            }
            m_JobContext->GetWorkerNode()
                .x_NotifyJobWatcher(*m_JobContext, event);
            return;
        }
    }
    m_JobContext->GetWorkerNode()
        .x_NotifyJobWatcher(*m_JobContext,
                            IWorkerNodeJobWatcher::eJobReturned);
}
/// @internal
void CGridThreadContext::PutFailure(const string& msg, int ret_code)
{
    _ASSERT(m_JobContext);
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context || 
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        if (m_Reporter.get()) {
            m_Reporter->PutFailure(m_JobContext->GetJobKey(),
                                   msg,
                                   m_JobContext->GetJobOutput(),
                                   ret_code);
        }
    }
    m_JobContext->GetWorkerNode()
        .x_NotifyJobWatcher(*m_JobContext,
                            IWorkerNodeJobWatcher::eJobFailed);
}
bool CGridThreadContext::IsJobCanceled()
{
    _ASSERT(m_JobContext);
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context || 
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        if (m_Reporter.get() && m_StatusThrottler.Approve(CRequestRateControl::eErrCode)) {
            int ret_code;
            string output;
            CNetScheduleClient::EJobStatus status = 
                m_Reporter->GetStatus(m_JobContext->GetJobKey(), 
                                      &ret_code, 
                                      &output);
            if (status != CNetScheduleClient::eRunning)
                return true;
        }
    }
    return false;
}

/// @internal
bool CGridThreadContext::IsJobCommitted() const
{
    _ASSERT(m_JobContext);
    return m_JobContext->IsJobCommitted();
}
/// @internal
IWorkerNodeJob* CGridThreadContext::GetJob()
{
    _ASSERT(m_JobContext);
    IWorkerNodeJob* ret = m_Job.GetPointer();
    if (!ret) {
        try {
            if (!CGridGlobals::GetInstance().ReuseJobObject())
                ret = m_JobContext->GetWorkerNode().CreateJob();
            else {
                m_Job.Reset(m_JobContext->GetWorkerNode().CreateJob());
                ret = m_Job.GetPointer(); 
            }
        } catch (...) {
            ERR_POST( "Could not create an instance of a job class." );
            CGridGlobals::GetInstance().
                RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
            throw;
        }
    }
    if (!ret) {
        CGridGlobals::GetInstance().
            RequestShutdown(CNetScheduleClient::eShutdownImmidiate);
        NCBI_THROW(CException, eInvalid, 
                   "Could not create an instance of a job class.");
    }
    return ret;
}

void CGridThreadContext::CloseStreams()
{
    _ASSERT(m_JobContext);
    m_RStream.reset(); // Destructor of IReader resets m_Reader also
    m_WStream.reset(); // Destructor of IWriter resets m_Writer also

    m_ProgressWriter->Reset();
    m_MsgThrottler.Reset(1);

    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (debug_context) {
        debug_context->DumpOutput(m_JobContext->GetJobKey(),
                                  m_JobContext->GetJobOutput(), 
                                  m_JobContext->GetJobNumber());
    }
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.27  2006/09/19 14:34:41  didenko
 * Code clean up
 * Catch and log all exceptions in destructors
 *
 * Revision 6.26  2006/06/28 16:01:56  didenko
 * Redone job's exlusivity processing
 *
 * Revision 6.25  2006/06/26 14:04:06  didenko
 * Fixed checking job cancelation algorithm
 *
 * Revision 6.24  2006/06/22 13:52:36  didenko
 * Returned back a temporary fix for CStdPoolOfThreads
 * Added check_status_period configuration paramter
 * Fixed exclusive mode checking
 *
 * Revision 6.23  2006/05/30 16:41:05  didenko
 * Improved error handling
 *
 * Revision 6.22  2006/05/22 18:11:43  didenko
 * Added an option to fail a job if a remote app returns non zore code
 *
 * Revision 6.21  2006/05/15 15:26:53  didenko
 * Added support for running exclusive jobs
 *
 * Revision 6.20  2006/05/10 19:54:21  didenko
 * Added JobDelayExpiration method to CWorkerNodeContext class
 * Added keep_alive_period and max_job_run_time parmerter to the config
 * file of remote_app
 *
 * Revision 6.19  2006/05/08 15:16:42  didenko
 * Added support for an optional saving of a remote application's stdout
 * and stderr into files on a local file system
 *
 * Revision 6.18  2006/04/12 19:03:49  didenko
 * Renamed parameter "use_embedded_input" to "use_embedded_storage"
 *
 * Revision 6.17  2006/03/15 17:30:12  didenko
 * Added ability to use embedded NetSchedule job's storage as a job's input/output data instead of using it as a NetCache blob key. This reduces network traffic and increases job submittion speed.
 *
 * Revision 6.16  2006/02/16 15:39:10  didenko
 * If an instance of a job's class could not be create then the worker node
 * should shutdown itself.
 *
 * Revision 6.15  2006/02/15 19:48:34  didenko
 * Added new optional config parameter "reuse_job_object" which allows reusing
 * IWorkerNodeJob objects in the jobs' threads instead of creating
 * a new object for each job.
 *
 * Revision 6.14  2006/02/15 15:19:03  didenko
 * Implemented an optional possibility for a worker node to have a permanent connection
 * to a NetSchedule server.
 * To expedite the job exchange between a worker node and a NetSchedule server,
 * a call to CNetScheduleClient::PutResult method is replaced to a
 * call to CNetScheduleClient::PutResultGetJob method.
 *
 * Revision 6.13  2006/02/01 16:39:01  didenko
 * Added Idle Task facility to the Grid Worker Node Framework
 *
 * Revision 6.12  2006/01/18 17:47:42  didenko
 * Added JobWatchers mechanism
 * Reimplement worker node statistics as a JobWatcher
 * Added JobWatcher for diag stream
 * Fixed a problem with PutProgressMessage method of CWorkerNodeThreadContext class
 *
 * Revision 6.11  2005/12/20 17:26:22  didenko
 * Reorganized netschedule storage facility.
 * renamed INetScheduleStorage to IBlobStorage and moved it to corelib
 * renamed INetScheduleStorageFactory to IBlobStorageFactory and moved it to corelib
 * renamed CNetScheduleNSStorage_NetCache to CBlobStorage_NetCache and moved it
 * to separate files
 * Moved CNetScheduleClientFactory to separate files
 *
 * Revision 6.10  2005/10/26 16:37:44  didenko
 * Added for non-blocking read for netschedule storage
 *
 * Revision 6.9  2005/09/30 14:58:56  didenko
 * Added optional parameter to PutProgressMessage methods which allows
 * sending progress messages regardless of the rate control.
 *
 * Revision 6.8  2005/05/27 14:46:06  didenko
 * Fixed a worker node statistics
 *
 * Revision 6.7  2005/05/23 17:58:57  didenko
 * Changed "grid_debug_context.hpp" to <connect/services/grid_debug_context.hpp>
 *
 * Revision 6.6  2005/05/11 18:57:39  didenko
 * Added worker node statictics
 *
 * Revision 6.5  2005/05/10 14:13:10  didenko
 * Added CloseStreams method
 *
 * Revision 6.4  2005/05/06 14:43:14  didenko
 * Minor fix
 *
 * Revision 6.3  2005/05/06 13:08:06  didenko
 * Added check for a job cancelation in the GetShoutdownLevel method
 *
 * Revision 6.2  2005/05/05 15:57:35  didenko
 * Minor fixes
 *
 * Revision 6.1  2005/05/05 15:18:51  didenko
 * Added debugging facility to worker nodes
 *
 * ===========================================================================
 */
 
