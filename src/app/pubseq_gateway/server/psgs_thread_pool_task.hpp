#ifndef PSGS_THREAD_POOL_TASK__HPP
#define PSGS_THREAD_POOL_TASK__HPP

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
 * Authors: Aleksey Grichenko
 *
 * Base class for CThreadPool task for WGS/SNP/CDD processors.
 *
 */

#include <util/thread_pool.hpp>
#include <atomic>

BEGIN_NCBI_NAMESPACE;

class CThreadPool;

BEGIN_NAMESPACE(psg);

template<class TProcessor>
class CPSGS_ThreadPoolTask: public CThreadPool_Task
{
public:
    using TMethod = void (TProcessor::*)();

    CPSGS_ThreadPoolTask(TProcessor& processor, TMethod method)
        : m_Processor(processor)
        , m_Method(method)
    {}

    virtual EStatus Execute(void) override
    {
        ETaskState expected = ETaskState::ePending;
        if (!m_State.compare_exchange_strong(expected, ETaskState::eExecuting, std::memory_order_acq_rel)) {
            // The task has been canceled.
            return eCanceled;
        }
        (m_Processor.*m_Method)();
        return eCompleted;
    }

    enum class ETaskState {
        ePending,
        eExecuting,
        eFinished
    };

    // If still pending, set state to finished and return true - OK to finish the request immediately.
    // If already executing, return false - the request will be finished on task completion.
    bool TryFinish(void)
    {
        ETaskState expected = ETaskState::ePending;
        return m_State.compare_exchange_strong(expected, ETaskState::eFinished, std::memory_order_acq_rel);
    }

private:
    TProcessor&             m_Processor;
    TMethod                 m_Method;
    std::atomic<ETaskState> m_State{ETaskState::ePending};
};


END_NAMESPACE(psg);
END_NCBI_NAMESPACE;

#endif  // PSGS_THREAD_POOL_TASK__HPP
