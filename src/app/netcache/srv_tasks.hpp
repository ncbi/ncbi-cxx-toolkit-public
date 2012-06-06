#ifndef NETCACHE__SRV_TASKS__HPP
#define NETCACHE__SRV_TASKS__HPP
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
 * Authors:  Pavel Ivanov
 *
 * File Description: 
 */


namespace intr = boost::intrusive;


BEGIN_NCBI_SCOPE


class CRequestContext;
struct STimerTicket;


struct SSrvTaskList_tag;
typedef intr::slist_member_hook<intr::tag<SSrvTaskList_tag> >   TSrvTaskListHook;


class CSrvTaskTerminator : public CSrvRCUUser
{
public:
    CSrvTaskTerminator(void);
    virtual ~CSrvTaskTerminator(void);

private:
    virtual void ExecuteRCU(void);
};


class CSrvTask
{
public:
    CSrvTask(void);
    void SetRunnable(void);
    void RunAfter(Uint4 delay_sec);
    // virtual for internal use only
    virtual void Terminate(void);

    void SetPriority(Uint1 prty);
    Uint1 GetPriority(void);

    void CreateNewDiagCtx(void);
    void SetDiagCtx(CRequestContext* ctx);
    CRequestContext* GetDiagCtx(void);
    void ReleaseDiagCtx(void);

public:
    virtual void ExecuteSlice(TSrvThreadNum thr_num) = 0;

private:
    CSrvTask(const CSrvTask&);
    CSrvTask& operator= (const CSrvTask&);

public:
    virtual ~CSrvTask(void);

    virtual void InternalRunSlice(TSrvThreadNum thr_num);


    TSrvTaskListHook m_TaskListHook;
    TSrvThreadNum m_LastThread;
    TSrvTaskFlags m_TaskFlags;
    Uint1 m_Priority;
    CRequestContext* m_DiagCtx;
    CRequestContext** m_DiagChain;
    STimerTicket* m_Timer;
    CSrvTaskTerminator m_Terminator;
};


typedef intr::member_hook<CSrvTask,
                          TSrvTaskListHook,
                          &CSrvTask::m_TaskListHook>    TSrvTaskListOpt;
typedef intr::slist<CSrvTask,
                    TSrvTaskListOpt,
                    intr::cache_last<true>,
                    intr::constant_time_size<true>,
                    intr::size_type<Uint4> >            TSrvTaskList;


template <class Derived>
class CSrvStatesTask : public virtual CSrvTask
{
protected:
    typedef Derived Me;
    struct State;
    typedef State (Me::*FStateFunc)(void);
    struct State {
        FStateFunc func;

        State(void) : func(NULL) {}
        State(FStateFunc func) : func(func) {}
        State& operator= (FStateFunc func_) {func = func_; return *this;}
        bool operator== (FStateFunc func_) {return func == func_;}
        bool operator!= (FStateFunc func_) {return func != func_;}
    };


    CSrvStatesTask(void)
        : m_CurState(NULL)
    {}
    virtual ~CSrvStatesTask(void)
    {}
    void SetState(State state)
    {
        m_CurState = state.func;
    }

protected:
    virtual void ExecuteSlice(TSrvThreadNum /* thr_num */)
    {
        for (;;) {
            FStateFunc next_state = (((Me*)this)->*m_CurState)().func;
            if (next_state == NULL)
                return;
            m_CurState = next_state;
        }
    }

private:
    FStateFunc      m_CurState;
};


class CSrvTransConsumer;

struct SSrvConsList_Tag;
typedef intr::list_base_hook<intr::tag<SSrvConsList_Tag> >  TSrvConsListHook;
typedef intr::list<CSrvTransConsumer,
                   intr::base_hook<TSrvConsListHook>,
                   intr::constant_time_size<false> >        TSrvConsList;

class CSrvTransitionTask : public virtual CSrvTask
{
public:
    CSrvTransitionTask(void);
    virtual ~CSrvTransitionTask(void);

    void RequestTransition(CSrvTransConsumer* consumer);
    bool IsTransStateFinal(void);
    void CancelTransRequest(CSrvTransConsumer* consumer);

protected:
    void FinishTransition(void);

private:
    enum ETransState {
        eState_Initial,
        eState_Transition,
        eState_Final
    };

    CMiniMutex m_TransLock;
    ETransState m_TransState;
    TSrvConsList m_Consumers;
};


class CSrvTransConsumer : public virtual CSrvTask,
                          public TSrvConsListHook
{
public:
    CSrvTransConsumer(void);
    virtual ~CSrvTransConsumer(void);

    bool IsTransFinished(void);

public:
    bool m_TransFinished;
};


END_NCBI_SCOPE

#endif /* NETCACHE__SRV_TASKS__HPP */
