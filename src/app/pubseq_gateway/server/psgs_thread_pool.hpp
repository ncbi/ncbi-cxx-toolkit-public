#ifndef PSGS_THREAD_POOL__HPP
#define PSGS_THREAD_POOL__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description: thread pool for various processors
 *
 */

#include "pubseq_gateway_logging.hpp"
#include <util/thread_pool.hpp>


BEGIN_NCBI_NAMESPACE;


class CPSGS_ThreadPool_Controller : public CThreadPool_Controller
{
public:
    CPSGS_ThreadPool_Controller(unsigned int min_threads,
                                unsigned int max_threads)
        : CThreadPool_Controller(max_threads, min_threads)
    {}

    void OnEvent(EEvent  event)
    {
        auto pool = GetPool();
        if (pool == nullptr) {
            PSG_ERROR("Null thread pool pointer; bailing.");
            return;
        }

        if (event == eSuspend) {
            return;
        }

        // Simple algorithm -- just ratchet minimum up.  (The
        // default PID controller can yield a fair bit of churn,
        // at least with its default parameters.)
        unsigned int n = (pool->GetExecutingTasksCount() +
                          pool->GetQueuedTasksCount());
        if (n > GetMinThreads()) {
            // Implicitly calls EnsureLimits()
            SetMinThreads(min(n, GetMaxThreads()));
        } else if (n > pool->GetThreadsCount()) {
            EnsureLimits();
        }
    }
};

END_NCBI_NAMESPACE;

#endif  // PSGS_THREAD_POOL__HPP

