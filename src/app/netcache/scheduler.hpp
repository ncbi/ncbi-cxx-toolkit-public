#ifndef NETCACHE__SCHEDULER__HPP
#define NETCACHE__SCHEDULER__HPP
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


struct SSrvThread;
struct SSchedInfo;



void ConfigureScheduler(CNcbiRegistry* reg, CTempString section);
void AssignThreadSched(SSrvThread* thr);
void ReleaseThreadSched(SSrvThread* thr);
void SchedCheckOverloads(void);
void SchedExecuteTask(SSrvThread* thr);
void SchedStartJiffy(SSrvThread* thr);
bool SchedIsAllIdle(void);
void MarkTaskTerminated(CSrvTask* task, bool immediate = true);



enum ESrvTaskFlags {
    fTaskRunnable        = 1 << 0,
    fTaskQueued          = 1 << 1,
    fTaskRunning         = 1 << 2,
    fTaskOnTimer         = 1 << 3,
    fTaskNeedTermination = 1 << 4,
    fTaskTerminated      = 1 << 5
};


struct SPrtyExecMap_tag;
typedef intr::set_base_hook<intr::tag<SPrtyExecMap_tag>,
                            intr::optimize_size<true> >     TPrtyExecMapHook;

struct SPrtyExecQueue : public TPrtyExecMapHook
{
    Uint1 priority;
    Uint4 exec_time;
    TSrvTaskList tasks;
};

struct SPrtyExecCompare
{
    bool operator() (const SPrtyExecQueue& left, const SPrtyExecQueue& right) const
    {
        return left.priority < right.priority;
    }
    bool operator() (Uint1 left, const SPrtyExecQueue& right) const
    {
        return left < right.priority;
    }
    bool operator() (const SPrtyExecQueue& left, Uint1 right) const
    {
        return left.priority < right;
    }
};

typedef intr::set<SPrtyExecQueue,
                  intr::base_hook<TPrtyExecMapHook>,
                  intr::constant_time_size<false>,
                  intr::compare<SPrtyExecCompare> >     TPrtyExecMap;


struct SSchedInfo
{
    TPrtyExecMap tasks_map;
    CMiniMutex tasks_lock;
    CFutex cnt_signal;
    Uint4 max_tasks;
    Uint4 done_tasks;
    Uint8 done_time;
    Uint8 wait_time;
    Uint8 max_slice;
    CSrvTime jfy_start_time;
    CSrvTime last_exec_time;
    TSrvThreadNum prefer_thr_num;
};


END_NCBI_SCOPE

#endif /* NETCACHE__SCHEDULER__HPP */
