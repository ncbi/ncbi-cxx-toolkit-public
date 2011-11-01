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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: Queue cleaning threads.
 *
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/request_ctx.hpp>


#include <db.h>
#include <db/bdb/bdb_expt.hpp>

#include "queue_database.hpp"
#include "queue_clean_thread.hpp"
#include "ns_handler.hpp"

BEGIN_NCBI_SCOPE

class CQueueDataBase;



CJobQueueCleanerThread::CJobQueueCleanerThread(
        CBackgroundHost &    host,
        CQueueDataBase &     qdb,
        unsigned int         sec_delay,
        unsigned int         nanosec_delay,
        const bool &         logging) :
    m_Host(host),
    m_QueueDB(qdb),
    m_CleaningLogging(logging),
    m_SecDelay(sec_delay),
    m_NanosecDelay(nanosec_delay),
    m_StopSignal(0, 10000000)
{}


CJobQueueCleanerThread::~CJobQueueCleanerThread()
{}


void CJobQueueCleanerThread::RequestStop(void)
{
    m_StopFlag.Add(1);
    m_StopSignal.Post();
    return;
}


void *  CJobQueueCleanerThread::Main(void)
{
    while (1) {
        x_DoJob();

        if (m_StopSignal.TryWait(m_SecDelay, m_NanosecDelay))
            if (m_StopFlag.Get() != 0)
                break;
    } // while (1)

    return 0;
}


void CJobQueueCleanerThread::x_DoJob(void)
{
    if (!m_Host.ShouldRun())
        return;


    bool                    is_log = m_CleaningLogging;
    CRef<CRequestContext>   ctx;

    if (is_log) {
        ctx.Reset(new CRequestContext());
        ctx->SetRequestID();
        GetDiagContext().SetRequestContext(ctx);
        GetDiagContext().PrintRequestStart()
                        .Print("_type", "job_cleaner_thread");
        ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
    }

    try {
        m_QueueDB.Purge();
    }
    catch (CBDB_ErrnoException &  ex) {
        if (ex.IsNoMem()) {
            ERR_POST("BDB reported resource shortage (no mem) in "
                     "cleaning thread. Ignored.");
        } else if (ex.IsDeadLock()) {
            ERR_POST("BDB reported deadlock in cleaning thread. Ignored.");
        } else {
            RequestStop();
            if (ex.BDB_GetErrno() == DB_RUNRECOVERY) {
                string      msg = "Fatal BerkeleyDB error: DB_RUNRECOVERY. " +
                                  string(ex.what());

                ERR_POST(msg);
                // eFatal will request the server to stop
                m_Host.ReportError(CBackgroundHost::eFatal, msg);
            } else {
                ERR_POST(Error << "BDB Error while cleaning job queue: "
                               << ex.what()
                               << " Cleaning thread has been stopped.");
            }
        }
        if (is_log)
            ctx->SetRequestStatus(
                            CNetScheduleHandler::eStatus_QueueCleanerError);
    }
    catch(exception &  ex) {
        RequestStop();
        ERR_POST("Error while cleaning queue: " << ex.what() <<
                 " Cleaning thread has been stopped.");
        if (is_log)
            ctx->SetRequestStatus(
                            CNetScheduleHandler::eStatus_QueueCleanerError);
    }
    catch (...) {
        RequestStop();
        ERR_POST("Unknown error while cleaning queue. "
                 "Cleaning thread has been stopped.");
        if (is_log)
            ctx->SetRequestStatus(
                            CNetScheduleHandler::eStatus_QueueCleanerError);
    }

    if (is_log) {
        GetDiagContext().PrintRequestStop();
        ctx.Reset();
        GetDiagContext().SetRequestContext(NULL);
    }
}



void CJobQueueExecutionWatcherThread::DoJob(void)
{
    if (!m_Host.ShouldRun())
        return;


    bool                    is_log = m_ExecutionLogging;
    CRef<CRequestContext>   ctx;

    if (is_log) {
        ctx.Reset(new CRequestContext());
        ctx->SetRequestID();
        GetDiagContext().SetRequestContext(ctx);
        GetDiagContext().PrintRequestStart()
                        .Print("_type", "job_execution_watcher_thread");
        ctx->SetRequestStatus(CNetScheduleHandler::eStatus_OK);
    }


    try {
        m_QueueDB.CheckExecutionTimeout(is_log);
    }
    catch (CBDB_ErrnoException& ex)
    {
        if (ex.IsNoMem()) {
            ERR_POST("BDB reported resource shortage (no mem) in "
                     "execution watch thread. Ignored.");
        } else if (ex.IsDeadLock()) {
            ERR_POST("BDB reported deadlock in "
                     "execution watch thread. Ignored.");
        } else {
            RequestStop();
            if (ex.BDB_GetErrno() == DB_RUNRECOVERY) {
                string      msg = "Fatal BerkeleyDB error: DB_RUNRECOVERY. " +
                                  string(ex.what());

                ERR_POST(msg);
                // eFatal will request the server to stop
                m_Host.ReportError(CBackgroundHost::eFatal, msg);
            } else {
                ERR_POST("BDB Error in execution watcher thread: " <<
                         ex.what() << " Watcher thread has been stopped.");
            }
        }
        if (is_log)
            ctx->SetRequestStatus(
                            CNetScheduleHandler::eStatus_JobExecWatcherError);
    }
    catch (exception &  ex) {
        RequestStop();
        ERR_POST("Error in execution watcher: " << ex.what() <<
                 " watcher thread has been stopped.");
        if (is_log)
            ctx->SetRequestStatus(
                            CNetScheduleHandler::eStatus_JobExecWatcherError);
    }
    catch (...) {
        RequestStop();
        ERR_POST("Unknown error in execution watcher. "
                 "Watched thread has been stopped.");
        if (is_log)
            ctx->SetRequestStatus(
                            CNetScheduleHandler::eStatus_JobExecWatcherError);
    }

    if (is_log) {
        GetDiagContext().PrintRequestStop();
        ctx.Reset();
        GetDiagContext().SetRequestContext(NULL);
    }
}


END_NCBI_SCOPE

