#ifndef CONNECT_SERVICES__GRID_WORKER_IMPL__HPP
#define CONNECT_SERVICES__GRID_WORKER_IMPL__HPP

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
 *    Common NetSchedule Worker Node declarations
 */

#include <corelib/ncbistl.hpp>

BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//

///@internal
class CWorkerNodeRequest : public CStdRequest
{
public:
    CWorkerNodeRequest(CWorkerNodeJobContext* context) :
        m_JobContext(context)
    {
    }

    virtual void Process();

private:
    CWorkerNodeJobContext* m_JobContext;
};


/////////////////////////////////////////////////////////////////////////////
//

#define NO_EVENT ((void*) 0)
#define SUSPEND_EVENT ((void*) 1)
#define RESUME_EVENT ((void*) 2)

/////////////////////////////////////////////////////////////////////////////
//

bool g_IsRequestStartEventEnabled();
bool g_IsRequestStopEventEnabled();

/////////////////////////////////////////////////////////////////////////////
//
/// @internal
class CMainLoopThread : public CThread
{
public:
    CMainLoopThread(CGridWorkerNode* worker_node) :
        m_WorkerNode(worker_node),
        m_Semaphore(0, 1)
    {
    }

    void Stop();

private:
    virtual void* Main();

    CGridWorkerNode* m_WorkerNode;
    CSemaphore m_Semaphore;
};

END_NCBI_SCOPE

#endif /* CONNECT_SERVICES__GRID_WORKER_IMPL__HPP */
