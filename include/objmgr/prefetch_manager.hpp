#ifndef PREFETCH_MANAGER__HPP
#define PREFETCH_MANAGER__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   Prefetch manager
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbithr.hpp>
#include <util/thread_pool.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;

/** @addtogroup ObjectManagerCore
 *
 * @{
 */


class CPrefetchRequest;
class CPrefetchManager;
class CPrefetchManager_Impl;

struct SPrefetchTypes
{
    enum EState {
        eInvalid,   // no prefetch token available
        eQueued,    // placed in queue
        eStarted,   // moved from queue to processing
        eAdvanced,  // got new data while processing
        eCompleted, // finished processing successfully
        eCanceled,  // canceled by user request
        eFailed     // finished processing unsuccessfully
    };
    typedef EState EEvent;
    
    typedef int TPriority;
    typedef int TProgress;
};


class NCBI_XOBJMGR_EXPORT IPrefetchAction : public SPrefetchTypes
{
public:
    virtual ~IPrefetchAction(void);
    
    virtual bool Execute(CRef<CPrefetchRequest> token) = 0;
};


class NCBI_XOBJMGR_EXPORT IPrefetchActionSource
{
public:
    virtual ~IPrefetchActionSource(void);

    virtual CIRef<IPrefetchAction> GetNextAction(void) = 0;
};


class NCBI_XOBJMGR_EXPORT IPrefetchListener : public SPrefetchTypes
{
public:
    virtual ~IPrefetchListener(void);

    virtual void PrefetchNotify(CRef<CPrefetchRequest> token, EEvent event) = 0;
};


class NCBI_XOBJMGR_EXPORT CPrefetchManager :
    public CObject,
    public SPrefetchTypes
{
public:
    CPrefetchManager(void);
    explicit CPrefetchManager(unsigned max_threads,
                              CThread::TRunMode threads_mode = CThread::fRunDefault);
    ~CPrefetchManager(void);

    CRef<CPrefetchRequest> AddAction(TPriority priority,
                                     IPrefetchAction* action,
                                     IPrefetchListener* listener = 0);
    CRef<CPrefetchRequest> AddAction(IPrefetchAction* action,
                                     IPrefetchListener* listener = 0);

    // Checks if prefetch is active in current thread.
    // Throws CPrefetchCanceled exception if the current token is canceled.
    static bool IsActive(void);
    
    // Send cancel requests to all tasks, queued and executing
    void CancelAllTasks(void);

    // Clears manager queue and stops all worker threads.
    void Shutdown(void);
    
private:
    CRef<CPrefetchManager_Impl> m_Impl;

private:
    CPrefetchManager(const CPrefetchManager&);
    void operator=(const CPrefetchManager&);
};


/// This exception is used to report failed actions
class NCBI_XOBJMGR_EXPORT CPrefetchFailed : public CException
{
public:
    enum EErrCode {
        eFailed
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CPrefetchFailed,CException);
};


/// This exception is used to interrupt actions canceled by user
class NCBI_XOBJMGR_EXPORT CPrefetchCanceled : public CException
{
public:
    enum EErrCode {
        eCanceled
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CPrefetchCanceled,CException);
};


class NCBI_XOBJMGR_EXPORT CPrefetchSequence : public CObject
{
public:
    CPrefetchSequence(CPrefetchManager& manager,
                      IPrefetchActionSource* source,
                      size_t active_size = 10);
    ~CPrefetchSequence(void);
    
    /// Returns next action waiting for its result if necessary
    CRef<CPrefetchRequest> GetNextToken(void);

protected:
    void EnqueNextAction(void);

private:
    CRef<CPrefetchManager>          m_Manager;
    CIRef<IPrefetchActionSource>    m_Source;
    CMutex                          m_Mutex;
    list< CRef<CPrefetchRequest> >  m_ActiveTokens;
};


class NCBI_XOBJMGR_EXPORT CPrefetchRequest
    : public CThreadPool_Task,
      public SPrefetchTypes
{
public:
    CPrefetchRequest(CObjectFor<CMutex>* state_mutex,
                     IPrefetchAction* action,
                     IPrefetchListener* listener,
                     unsigned int priority);
    ~CPrefetchRequest(void);
    
    IPrefetchAction* GetAction(void) const
        {
            return m_Action.GetNCPointer();
        }

    IPrefetchListener* GetListener(void) const
        {
            return m_Listener.GetNCPointerOrNull();
        }
    void SetListener(IPrefetchListener* listener);
    
    EState GetState(void) const;

    // in one of final states: completed, failed, canceled 
    bool IsDone(void) const
        {
            return IsFinished();
        }

    TProgress GetProgress(void) const
        {
            return m_Progress;
        }
    TProgress SetProgress(TProgress progress);

    virtual EStatus Execute(void);

    virtual void OnStatusChange(EStatus /* old */);

private:
    friend class CPrefetchManager;
    friend class CPrefetchManager_Impl;

    // back references
    CRef<CObjectFor<CMutex> >   m_StateMutex;

    CIRef<IPrefetchAction>      m_Action;
    CIRef<IPrefetchListener>    m_Listener;
    TProgress                   m_Progress;
};


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // PREFETCH_MANAGER__HPP
