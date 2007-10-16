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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Transaction checkpoint / memptrickle thread
 *
 */

#include <ncbi_pch.hpp>

#include <bdb/bdb_checkpoint_thread.hpp>
#include <bdb/bdb_env.hpp>
#include <bdb/error_codes.hpp>

#include <connect/server_monitor.hpp>


#define NCBI_USE_ERRCODE_X   Bdb_Checkpoint

BEGIN_NCBI_SCOPE



CBDB_CheckPointThread::CBDB_CheckPointThread(CBDB_Env& env,
                                             int      memp_trickle,
                                             unsigned run_delay,
                                             unsigned stop_request_poll)
: CThreadNonStop(run_delay, stop_request_poll),
    m_Env(env),
    m_MempTrickle(memp_trickle),
    m_ErrCnt(0),
    m_MaxErrors(100)
{
    m_Flags = CBDB_Env::eBackground_MempTrickle |
              CBDB_Env::eBackground_Checkpoint  |
              CBDB_Env::eBackground_DeadLockDetect;
}

CBDB_CheckPointThread::~CBDB_CheckPointThread()
{
//    LOG_POST_X(1, "~CBDB_CheckPointThread()");
}


void CBDB_CheckPointThread::SetMaxErrors(unsigned max_err)
{
    m_MaxErrors = max_err;
}

void CBDB_CheckPointThread::DoJob(void)
{
    try {
        if (m_Env.IsTransactional() && 
           (m_Flags & CBDB_Env::eBackground_Checkpoint)) {
            m_Env.TransactionCheckpoint();
        }

        if (m_MempTrickle && 
           (m_Flags & CBDB_Env::eBackground_MempTrickle)) {
            int nwrotep = 0;
            m_Env.MempTrickle(m_MempTrickle, &nwrotep);
            if (nwrotep) {
                LOG_POST_X(2, Info << "CBDB_CheckPointThread::DoJob(): trickled "
                              << nwrotep << " pages");
            }
        }
        if (m_Flags & CBDB_Env::eBackground_DeadLockDetect) {
            m_Env.DeadLockDetect();
        }
    } 
    catch (CBDB_ErrnoException& ex)
    {
        if (m_MaxErrors) {
            ++m_ErrCnt;            
        }
        if (ex.IsRecovery()) {
            // fatal database error, stop right now!
            RequestStop();
            string msg ="Fatal Berkeley DB error: DB_RUNRECOVERY." 
                        " Checkpoint thread has been stopped.";
            LOG_POST_X(3, Error << msg);
        } else {
            LOG_POST_X(4, Error << "Error in checkpoint thread(supressed) " 
                                << ex.what());
        }

        if (m_ErrCnt > m_MaxErrors) {
            RequestStop();
            LOG_POST_X(5, Error << 
                       "Checkpoint thread has been stopped (too many errors)");
        }
    }
    catch(exception& ex)
    {
        if (m_MaxErrors) {
            ++m_ErrCnt;            
        }
        LOG_POST_X(6, Error << "Error in checkpoint thread: " 
                            << ex.what());

        if (m_ErrCnt > m_MaxErrors) {
            RequestStop();
            LOG_POST_X(7, Error << 
                       "Checkpoint thread has been stopped (too many errors)");
        }
    }
}



END_NCBI_SCOPE

