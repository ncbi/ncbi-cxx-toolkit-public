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

BEGIN_NCBI_SCOPE



CBDB_CheckPointThread::CBDB_CheckPointThread(CBDB_Env& env,
                                             int      memp_trickle,
                                             unsigned run_delay,
                                             unsigned stop_request_poll)
: CThreadNonStop(run_delay, stop_request_poll),
    m_Env(env),
    m_MempTrickle(memp_trickle)
{}

void CBDB_CheckPointThread::DoJob(void)
{
    try {
        if (m_Env.IsTransactional()) {
            m_Env.TransactionCheckpoint();
        }
        if (m_MempTrickle) {
            int nwrotep = 0;
            m_Env.MempTrickle(m_MempTrickle, &nwrotep);
            if (nwrotep) {
                LOG_POST(Info << "CBDB_CheckPointThread::DoJob(): trickled "
                         << nwrotep << " pages");
            }
        }
    } 
    catch(exception& ex)
    {
        RequestStop();
        LOG_POST(Error << "Error in checkpoint thread: " 
                        << ex.what()
                        << " checkpoint thread has been stopped.");
    }
}



END_NCBI_SCOPE

