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
#include <connect/services/error_codes.hpp>

#include "grid_thread_context.hpp"


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode


BEGIN_NCBI_SCOPE

/// @internal
CGridThreadContext::CGridThreadContext(CGridWorkerNode& node, long check_status_period)
    : m_JobContext(NULL), m_MsgThrottler(1), 
      m_CheckStatusPeriod(check_status_period > 1 ? check_status_period : 1),
      m_StatusThrottler(1,CTimeSpan(m_CheckStatusPeriod, 0))
{
    m_Reporter.reset(node.CreateClient());
    m_Reader.reset(node.CreateStorage());
    m_Writer.reset(node.CreateStorage());
    m_ProgressWriter.reset(node.CreateStorage());
}

void CGridThreadContext::SetJobContext(CWorkerNodeJobContext& job_context,
                                       const CNetScheduleJob& new_job)
{
    job_context.Reset(new_job, CGridGlobals::GetInstance().GetNewJobNumber());
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
            m_JobContext->GetWorkerNode().GetServerOutputSize();
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
        if (!debug_context || 
            debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {
            
            if (m_JobContext->m_Job.progress_msg.empty() && m_Reporter.get()) {
                m_Reporter->GetProgressMsg(m_JobContext->m_Job);
            }
            if (!m_JobContext->m_Job.progress_msg.empty() && m_ProgressWriter.get()) {
                CNcbiOstream& os = m_ProgressWriter->CreateOStream(m_JobContext->m_Job.progress_msg);
                os << msg;
                m_ProgressWriter->Reset();
            }
            else {
                //ERR_POST_X(5, "Couldn't send a progress message.");
            }
        }
        if (debug_context) {
            debug_context->DumpProgressMessage(m_JobContext->GetJobKey(),
                                               msg, 
                                               m_JobContext->GetJobNumber());
        }           
    } catch (exception& ex) {
        ERR_POST_X(6, "Couldn't send a progress message: " << ex.what());
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
            ERR_POST_X(7, "CWorkerNodeJobContext::SetJobRunTimeout : " 
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
            ERR_POST_X(8, "CWorkerNodeJobContext::JobDelayExpiration : " 
                          << ex.what());
        } 
    }
    else {
        NCBI_THROW(CBlobStorageException,
                   eWriter, "Reporter is not set.");
    }
}

/// @internal
bool CGridThreadContext::PutResult(int ret_code, CNetScheduleJob& new_job)
{
    _ASSERT(m_JobContext);
    m_JobContext->m_Job.ret_code = ret_code;
    if ( m_JobContext->GetCommitStatus() != CWorkerNodeJobContext::eDone ) {
        PutFailure(kEmptyStr);
        return false;
    }
    bool more_jobs = false;
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context || 
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        if (m_Reporter.get()) {
            if (CGridGlobals::GetInstance().
                GetShutdownLevel() != CNetScheduleAdmin::eNoShutdown ||
                CGridGlobals::GetInstance().IsExclusiveMode() ) {
                m_Reporter->PutResult(m_JobContext->m_Job);
            } else {
                more_jobs = 
                    m_Reporter->PutResultGetJob(m_JobContext->m_Job, new_job);
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

            CNetScheduleAPI::EJobStatus status = 
                m_Reporter->GetJobStatus(m_JobContext->GetJobKey());
            IWorkerNodeJobWatcher::EEvent event = IWorkerNodeJobWatcher::eJobReturned;
            switch(status) {
            case CNetScheduleAPI::eJobNotFound:
                event = IWorkerNodeJobWatcher::eJobLost;
                break;
            case CNetScheduleAPI::eFailed:
                event = IWorkerNodeJobWatcher::eJobFailed;
                break;
            case CNetScheduleAPI::eDone:
                event = IWorkerNodeJobWatcher::eJobSucceed;
                break;
            case CNetScheduleAPI::eCanceled:
                event = IWorkerNodeJobWatcher::eJobCanceled;
                break;
            case CNetScheduleAPI::eRunning:
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
void CGridThreadContext::PutFailure(const string& msg)
{
    _ASSERT(m_JobContext);
    if (!msg.empty())
        m_JobContext->m_Job.error_msg = msg;
    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (!debug_context || 
        debug_context->GetDebugMode() != CGridDebugContext::eGDC_Execute) {

        if (m_Reporter.get()) {
            m_Reporter->PutFailure(m_JobContext->m_Job);
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
            CNetScheduleAPI::EJobStatus status = 
                m_Reporter->GetJobStatus(m_JobContext->GetJobKey());
            if (status != CNetScheduleAPI::eRunning)
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
            ERR_POST_X(9, "Could not create an instance of a job class." );
            CGridGlobals::GetInstance().
                RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
            throw;
        }
    }
    if (!ret) {
        CGridGlobals::GetInstance().
            RequestShutdown(CNetScheduleAdmin::eShutdownImmidiate);
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
    m_StatusThrottler.Reset(1,CTimeSpan(m_CheckStatusPeriod, 0));
          

    CGridDebugContext* debug_context = CGridDebugContext::GetInstance();
    if (debug_context) {
        debug_context->DumpOutput(m_JobContext->GetJobKey(),
                                  m_JobContext->GetJobOutput(), 
                                  m_JobContext->GetJobNumber());
    }
}

END_NCBI_SCOPE
