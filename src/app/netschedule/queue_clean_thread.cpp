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
 * File Description: Queue cleaning thread.
 *                   
 *
 */

#include <ncbi_pch.hpp>


#include <db.h>
#include <bdb/bdb_expt.hpp>

#include "bdb_queue.hpp"
#include "queue_clean_thread.hpp"

BEGIN_NCBI_SCOPE

class CQueueDataBase;

void CJobQueueCleanerThread::DoJob(void)
{
    if (!m_Host.ShouldRun()) return;
    try {
        m_QueueDB.Purge();
#ifdef _DEBUG
    if (m_DbgTriggerDBRecover)
        BDB_ERRNO_THROW(DB_RUNRECOVERY, "Test of error processing");
#endif
    } 
    catch (CBDB_ErrnoException& ex)
    {
        if (ex.IsNoMem()) {
            ERR_POST(Warning << 
                "BDB reported resource shortage in cleaning thread. Ignored.");
            return;
        }

        if (ex.IsDeadLock()) {
            ERR_POST(Warning << 
                "BDB reported deadlock in cleaning thread. Ignored.");
            return;
        }

        int err_no = ex.BDB_GetErrno();
        if (err_no == DB_RUNRECOVERY) {
            string msg("Fatal BerkeleyDB error: DB_RUNRECOVERY. ");
            msg += ex.what();
            ERR_POST(msg);
            m_Host.ReportError(CBackgroundHost::eFatal, msg);
        } else {
            ERR_POST(Error << "BDB Error when cleaning job queue: " 
                           << ex.what()
                           << " cleaning thread has been stopped.");
        }
        RequestStop();
    }
    catch(exception& ex)
    {
        RequestStop();
        ERR_POST(Error << "Error when cleaning queue: " 
                        << ex.what()
                        << " cleaning thread has been stopped.");
        RequestStop();
    }
}


void CJobQueueExecutionWatcherThread::DoJob(void)
{
    try {
        m_QueueDB.CheckExecutionTimeout();
    } 
    catch (CBDB_ErrnoException& ex)
    {
        if (ex.IsNoMem()) {
            ERR_POST(Warning << 
                "BDB reported resource shortage in exe watch thread. Ignored.");
            return;
        }

        if (ex.IsDeadLock()) {
            ERR_POST(Warning << 
                "BDB reported deadlock in exe watch thread. Ignored.");
            return;
        }

        int err_no = ex.BDB_GetErrno();
        if (err_no == DB_RUNRECOVERY) {
            string msg("Fatal BerkeleyDB error: DB_RUNRECOVERY. ");
            msg += ex.what();
            ERR_POST(msg);
            m_Host.ReportError(CBackgroundHost::eFatal, msg);
        } else {
            ERR_POST(Error << "BDB Error in execution watcher thread: " 
                           << ex.what()
                           << " watcher thread has been stopped.");
        }
        RequestStop();
    }
    catch(exception& ex)
    {
        RequestStop();
        ERR_POST(Error << "Error in execution watcher: " 
                        << ex.what()
                        << " watcher thread has been stopped.");
    }
}


END_NCBI_SCOPE
