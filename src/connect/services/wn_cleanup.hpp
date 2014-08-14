#ifndef CONNECT_SERVICES__WN_CLEANUP__HPP
#define CONNECT_SERVICES__WN_CLEANUP__HPP

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
 *    NetSchedule Worker Node - per-job and global clean-up, declarations.
 */

#include <connect/services/grid_worker_app.hpp>


#define NCBI_USE_ERRCODE_X   ConnServ_WorkerNode

BEGIN_NCBI_SCOPE

class CWorkerNodeCleanup : public IWorkerNodeCleanupEventSource
{
public:
    typedef set<IWorkerNodeCleanupEventListener*> TListeners;

    virtual void AddListener(IWorkerNodeCleanupEventListener* listener);
    virtual void RemoveListener(IWorkerNodeCleanupEventListener* listener);

    virtual void CallEventHandlers();

    void RemoveListeners(const TListeners& listeners);

protected:
    TListeners m_Listeners;
    CFastMutex m_ListenersLock;
};

class CWorkerNodeJobCleanup : public CWorkerNodeCleanup
{
public:
    CWorkerNodeJobCleanup(CWorkerNodeCleanup* worker_node_cleanup) :
        m_WorkerNodeCleanup(worker_node_cleanup)
    {
    }

    virtual void AddListener(IWorkerNodeCleanupEventListener* listener);
    virtual void RemoveListener(IWorkerNodeCleanupEventListener* listener);

    virtual void CallEventHandlers();

private:
    CWorkerNodeCleanup* m_WorkerNodeCleanup;
};

class CGridCleanupThread : public CThread
{
public:
    CGridCleanupThread(SGridWorkerNodeImpl* worker_node,
        IGridWorkerNodeApp_Listener* listener) :
            m_WorkerNode(worker_node),
            m_Listener(listener),
            m_Semaphore(0, 1)
    {
    }

    bool Wait(unsigned seconds) {return m_Semaphore.TryWait(seconds);}

protected:
    virtual void* Main();

private:
    SGridWorkerNodeImpl* m_WorkerNode;
    IGridWorkerNodeApp_Listener* m_Listener;
    CSemaphore m_Semaphore;
};

END_NCBI_SCOPE

#endif // CONNECT_SERVICES__WN_CLEANUP__HPP
