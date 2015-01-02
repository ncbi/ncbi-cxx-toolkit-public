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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *    Worker node offline mode implementation.
 */

#include <ncbi_pch.hpp>

#include "wn_commit_thread.hpp"
#include "wn_cleanup.hpp"
#include "grid_worker_impl.hpp"
#include "netschedule_api_impl.hpp"

#include <connect/services/grid_globals.hpp>
#include <connect/services/grid_worker_app.hpp>
#include <connect/services/grid_rw_impl.hpp>
#include <connect/services/ns_job_serializer.hpp>
#include <corelib/rwstream.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//


struct SOfflineJobContextImpl : public SWorkerNodeJobContextImpl
{
public:
    SOfflineJobContextImpl(SGridWorkerNodeImpl* worker_node,
            const string& output_dir_name,
            CCompoundIDPool::TInstance compound_id_pool) :
        SWorkerNodeJobContextImpl(worker_node),
        m_OutputDirName(output_dir_name),
        m_CompoundIDPool(compound_id_pool)
    {
    }

    virtual void PutProgressMessage(const string& msg,
        bool send_immediately = false);
    virtual CNetScheduleAdmin::EShutdownLevel GetShutdownLevel();
    virtual void JobDelayExpiration(unsigned runtime_inc);
    virtual void x_RunJob();

    string m_OutputDirName;
    CCompoundIDPool m_CompoundIDPool;
};


void SOfflineJobContextImpl::PutProgressMessage(const string& msg,
        bool send_immediately)
{
}

CNetScheduleAdmin::EShutdownLevel SOfflineJobContextImpl::GetShutdownLevel()
{
    return CGridGlobals::GetInstance().GetShutdownLevel();
}

void SOfflineJobContextImpl::JobDelayExpiration(unsigned runtime_inc)
{
}

void SOfflineJobContextImpl::x_RunJob()
{
    CWorkerNodeJobContext this_job_context(this);

    m_RequestContext->SetRequestID((int) this_job_context.GetJobNumber());
    m_RequestContext->SetAppState(eDiagAppState_RequestBegin);

    CRequestContextSwitcher request_state_guard(m_RequestContext);

    if (g_IsRequestStartEventEnabled())
        GetDiagContext().PrintRequestStart().Print("jid", m_Job.job_id);

    m_RequestContext->SetAppState(eDiagAppState_Request);

    try {
        this_job_context.SetJobRetCode(
                m_WorkerNode->GetJobProcessor()->Do(this_job_context));
    }
    catch (CGridWorkerNodeException& ex) {
        switch (ex.GetErrCode()) {
        case CGridWorkerNodeException::eJobIsLost:
            ERR_POST(ex);
            break;

        case CGridWorkerNodeException::eExclusiveModeIsAlreadySet:
            if (this_job_context.IsLogRequested()) {
                LOG_POST_X(21, "Job " << this_job_context.GetJobKey() <<
                    " requested exclusive mode while "
                    "another exclusive job is already running.");
            }
            if (!this_job_context.IsJobCommitted())
                this_job_context.CommitJobWithFailure(
                        "Exclusive mode denied");
            break;

        default:
            ERR_POST_X(62, ex);
            if (!this_job_context.IsJobCommitted())
                this_job_context.CommitJobWithFailure(ex.GetMsg());
        }
    }
    catch (exception& e) {
        ERR_POST_X(18, "job" << this_job_context.GetJobKey() <<
                " failed: " << e.what());
        if (!this_job_context.IsJobCommitted())
            this_job_context.CommitJobWithFailure(e.what());
    }

    this_job_context.CloseStreams();

    if (m_WorkerNode->IsExclusiveMode() && m_ExclusiveJob)
        m_WorkerNode->LeaveExclusiveMode();

    if (!m_OutputDirName.empty()) {
        CNetScheduleJobSerializer job_serializer(m_Job, m_CompoundIDPool);

        switch (this_job_context.GetCommitStatus()) {
        case CWorkerNodeJobContext::eCS_Done:
            job_serializer.SaveJobOutput(CNetScheduleAPI::eDone,
                    m_OutputDirName, m_NetCacheAPI);
            break;

        case CWorkerNodeJobContext::eCS_NotCommitted:
            this_job_context.CommitJobWithFailure(
                    "Job was not explicitly committed");
            /* FALL THROUGH */

        case CWorkerNodeJobContext::eCS_Failure:
            job_serializer.SaveJobOutput(CNetScheduleAPI::eFailed,
                    m_OutputDirName, m_NetCacheAPI);
            break;

        default: /* eCS_Return, eCS_Rescheduled, etc. */
            // No job results to save.
            break;
        }
    }

    x_PrintRequestStop();
}

int SGridWorkerNodeImpl::OfflineRun()
{
    x_WNCoreInit();

    m_QueueEmbeddedOutputSize = numeric_limits<size_t>().max();

    const CArgs& args = m_App.GetArgs();

    string input_dir_name(args["offline-input-dir"].AsString());
    string output_dir_name;

    if (args["offline-output-dir"])
        output_dir_name = args["offline-output-dir"].AsString();

    CDir input_dir(input_dir_name);

    auto_ptr<CDir::TEntries> dir_contents(input_dir.GetEntriesPtr(kEmptyStr,
            CDir::fIgnoreRecursive | CDir::fIgnorePath));

    if (dir_contents.get() == NULL) {
        ERR_POST("Cannot read input directory '" << input_dir_name << '\'');
        return 4;
    }

    m_Listener->OnGridWorkerStart();

    x_StartWorkerThreads();

    LOG_POST("Reading job input files (*.in)...");

    string path;
    if (!input_dir_name.empty())
        path = CDirEntry::AddTrailingPathSeparator(input_dir_name);

    unsigned job_count = 0;

    ITERATE(CDir::TEntries, it, *dir_contents) {
        CDirEntry& dir_entry = **it;

        string name = dir_entry.GetName();

        if (!NStr::EndsWith(name, ".in"))
            continue;

        dir_entry.Reset(CDirEntry::MakePath(path, name));

        if (dir_entry.IsFile()) {
            try {
                CRef<SOfflineJobContextImpl> job_context_impl(
                        new SOfflineJobContextImpl(this, output_dir_name,
                                m_NetScheduleAPI.GetCompoundIDPool()));

                CWorkerNodeJobContext job_context(job_context_impl);

                job_context_impl->ResetJobContext();

                CNetScheduleJobSerializer job_serializer(job_context.GetJob(),
                        m_NetScheduleAPI.GetCompoundIDPool());

                job_serializer.LoadJobInput(dir_entry.GetPath());

                for (;;) {
                    try {
                        m_ThreadPool->WaitForRoom(m_ThreadPoolTimeout);
                        break;
                    }
                    catch (CBlockingQueueException&) {
                        if (CGridGlobals::GetInstance().IsShuttingDown())
                            break;
                    }
                }

                if (CGridGlobals::GetInstance().IsShuttingDown())
                    break;

                try {
                    m_ThreadPool->AcceptRequest(CRef<CStdRequest>(
                            new CWorkerNodeRequest(job_context)));
                } catch (CBlockingQueueException& ex) {
                    ERR_POST(ex);
                    // that must not happen after CBlockingQueue is fixed
                    _ASSERT(0);
                }

                job_context_impl = NULL;

                if (CGridGlobals::GetInstance().IsShuttingDown())
                    break;
            } catch (exception& ex) {
                ERR_POST(ex.what());
                if (TWorkerNode_StopOnJobErrors::GetDefault())
                    CGridGlobals::GetInstance().RequestShutdown(
                            CNetScheduleAdmin::eShutdownImmediate);
            }
            ++job_count;
        }
    }

    while (!m_ThreadPool->IsEmpty())
        SleepMilliSec(30);

    x_StopWorkerThreads();

    if (job_count == 1) {
        LOG_POST("Processed 1 job input file.");
    } else {
        LOG_POST("Processed " << job_count << " job input files.");
    }

    return x_WNCleanUp();
}

END_NCBI_SCOPE
