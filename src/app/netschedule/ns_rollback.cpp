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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   NetSchedule limited rollback support
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>

#include "ns_rollback.hpp"
#include "ns_queue.hpp"


BEGIN_NCBI_SCOPE


void CNSSubmitRollback::Rollback(CQueue *  queue)
{
    ERR_POST(Warning << "Cancelling the submitted job due to "
                        "a network error while reporting the job key.");

    try {
        // true -> it is a cancel due to rollback
        queue->Cancel(m_Client, m_JobId, queue->MakeJobKey(m_JobId), true);
    } catch (const exception &  ex) {
        ERR_POST("Error while rolling back job submission: " << ex.what());
    } catch (...) {
        ERR_POST("Unknown error while rolling back job submission.");
    }
}


void CNSBatchSubmitRollback::Rollback(CQueue *  queue)
{
    ERR_POST(Warning << "Cancelling the submitted job batch due to "
                        "a network error while reporting the job keys.");

    try {
        for (size_t  k = 0; k < m_BatchSize; ++k) {
            // true -> it is a cancel due to rollback
            queue->Cancel(m_Client, m_FirstJobId + k,
                          queue->MakeJobKey(m_FirstJobId + k), true);
        }
    } catch (const exception &  ex) {
        ERR_POST("Error while rolling back job batch submission: " << ex.what());
    } catch (...) {
        ERR_POST("Unknown error while rolling back job batch submission");
    }
}


void CNSGetJobRollback::Rollback(CQueue *  queue)
{
    ERR_POST(Warning << "Rolling back job request due to "
                        "a network error while reporting the job key.");

    // It is basically the same as returning a job but without putting the job
    // into a blacklist.
    try {
        string  warning;    // used for auth tokens only, so
                            // not analyzed here

        // true -> returned due to rollback
        queue->ReturnJob(m_Client, m_JobId, queue->MakeJobKey(m_JobId),
                         "", warning, CQueue::eRollback);
    } catch (const exception &  ex) {
        ERR_POST("Error while rolling back requested job: " << ex.what());
    } catch (...) {
        ERR_POST("Unknown error while rolling back requested job");
    }
}


void CNSReadJobRollback::Rollback(CQueue *  queue)
{
    ERR_POST(Warning << "Rolling back reading job request due to "
                        "a network error while reporting the job key.");

    // It is the same as read rollback but without putting the job
    // into the reading blacklist
    try {
        // true -> returned due to rollback
        queue->ReturnReadingJob(m_Client, m_JobId, queue->MakeJobKey(m_JobId),
                                "", true);
    } catch (const exception &  ex) {
        ERR_POST("Error while rolling back read requested job: " << ex.what());
    } catch (...) {
        ERR_POST("Unknown error while rolling back read requested job");
    }
}


END_NCBI_SCOPE

