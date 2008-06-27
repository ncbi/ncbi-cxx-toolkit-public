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
 *   Test for IScheduler
 *
 */

#include <ncbi_pch.hpp>
#include <util/scheduler.hpp>

#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;


static IScheduler*
s_GetScheduler(void)
{
    static CIRef<IScheduler> scheduler = IScheduler::Create();

    return scheduler;
}


class CTestSchedTask : public IScheduler_Task, public CObject
{
public:
    virtual ~CTestSchedTask(void) {}

    virtual void Execute(void) {}
};


/// Test AddTask() and correct working of GetNextExecutionTime(),
/// HasTasksToExecute() and GetNextTaskToExecute() with it.
BOOST_AUTO_TEST_CASE(SimpleAdd)
{
    IScheduler* sch = s_GetScheduler();
    CTime t(CTime::eCurrent);

    CRef<CTestSchedTask> task(new CTestSchedTask());
    TScheduler_SeriesID id = sch->AddTask(task, t);

    CTime t_prev = t - CTimeSpan(1, 0);
    CTime t_next = t + CTimeSpan(1, 0);

    BOOST_CHECK_EQUAL(sch->GetNextExecutionTime(), t);
    BOOST_CHECK(!sch->IsEmpty());


    BOOST_CHECK(! sch->HasTasksToExecute(t_prev));

    SScheduler_SeriesInfo task_info = sch->GetNextTaskToExecute(t_prev);

    BOOST_CHECK_EQUAL(task_info.id, TScheduler_SeriesID(0));
    BOOST_CHECK(task_info.task.IsNull());


    BOOST_CHECK( sch->HasTasksToExecute(t) );

    task_info = sch->GetNextTaskToExecute(t);
    BOOST_CHECK(sch->IsEmpty());

    BOOST_CHECK_EQUAL(task_info.id, id);
    BOOST_CHECK_EQUAL(task_info.task.GetPointer(), task.GetPointer());

    sch->TaskExecuted(id, t);
    id = sch->AddTask(task.GetPointer(), t);
    BOOST_CHECK(!sch->IsEmpty());

    BOOST_CHECK( sch->HasTasksToExecute(t_next) );

    task_info = sch->GetNextTaskToExecute(t_next);
    BOOST_CHECK(sch->IsEmpty());

    BOOST_CHECK_EQUAL(task_info.id, id);
    BOOST_CHECK_EQUAL(task_info.task.GetPointer(), task.GetPointer());

    sch->TaskExecuted(id, t);
}

/// Test RemoveSeries(), RemoveTask() and RemoveAllSeries() method
BOOST_AUTO_TEST_CASE(RemoveAdded)
{
    IScheduler* sch = s_GetScheduler();
    CTime t(CTime::eCurrent);
    CTimeSpan period(10, 0);
    vector<SScheduler_SeriesInfo> ret_tasks;

    CRef<CTestSchedTask> task(new CTestSchedTask());
    TScheduler_SeriesID id = sch->AddTask(task, t);
    sch->AddTask(task, t + period);
    sch->AddTask(task, t + period + period);

    BOOST_CHECK(!sch->IsEmpty());
    sch->GetScheduledSeries(&ret_tasks);
    BOOST_CHECK_EQUAL(ret_tasks.size(), size_t(3));

    sch->RemoveSeries(id - 1);

    BOOST_CHECK(!sch->IsEmpty());
    sch->GetScheduledSeries(&ret_tasks);
    BOOST_CHECK_EQUAL(ret_tasks.size(), size_t(3));

    sch->RemoveSeries(id);

    BOOST_CHECK(!sch->IsEmpty());
    sch->GetScheduledSeries(&ret_tasks);
    BOOST_CHECK_EQUAL(ret_tasks.size(), size_t(2));

    sch->RemoveTask(task);

    BOOST_CHECK(sch->IsEmpty());

    sch->AddTask(task, t);
    sch->AddTask(task, t + period);
    sch->AddTask(task, t + period + period);

    sch->RemoveAllSeries();

    BOOST_CHECK(sch->IsEmpty());
}

/// Test AddRepetitiveTask() for task executing with rate
BOOST_AUTO_TEST_CASE(AddWithRate)
{
    IScheduler* sch = s_GetScheduler();
    CTime t(CTime::eCurrent);
    CTimeSpan period(10, 0);

    CRef<CTestSchedTask> task(new CTestSchedTask());
    TScheduler_SeriesID id = sch->AddRepetitiveTask(task, t, period,
                                                    IScheduler::eWithRate);
    BOOST_CHECK(!sch->IsEmpty());

    CTime t_prev = t - CTimeSpan(1, 0);
    CTime t_prev_next = t + period - CTimeSpan(1, 0);
    CTime t_next = t + period;

    BOOST_CHECK_EQUAL(sch->GetNextExecutionTime(), t);


    BOOST_CHECK(! sch->HasTasksToExecute(t_prev));

    SScheduler_SeriesInfo task_info = sch->GetNextTaskToExecute(t_prev);

    BOOST_CHECK_EQUAL(task_info.id, TScheduler_SeriesID(0));
    BOOST_CHECK(task_info.task.IsNull());


    BOOST_CHECK(!sch->IsEmpty());
    BOOST_CHECK( sch->HasTasksToExecute(t) );

    task_info = sch->GetNextTaskToExecute(t);
    BOOST_CHECK(!sch->IsEmpty());

    BOOST_CHECK_EQUAL(task_info.id, id);
    BOOST_CHECK_EQUAL(task_info.task.GetPointer(), task.GetPointer());

    sch->TaskExecuted(id, t_next);
    BOOST_CHECK(!sch->IsEmpty());


    BOOST_CHECK_EQUAL(sch->GetNextExecutionTime(), t_next);
    BOOST_CHECK(! sch->HasTasksToExecute(t_prev_next));

    task_info = sch->GetNextTaskToExecute(t_prev_next);

    BOOST_CHECK_EQUAL(task_info.id, TScheduler_SeriesID(0));
    BOOST_CHECK(task_info.task.IsNull());


    BOOST_CHECK(!sch->IsEmpty());
    BOOST_CHECK( sch->HasTasksToExecute(t_next) );

    task_info = sch->GetNextTaskToExecute(t_next);
    BOOST_CHECK(!sch->IsEmpty());

    BOOST_CHECK_EQUAL(task_info.id, id);
    BOOST_CHECK_EQUAL(task_info.task.GetPointer(), task.GetPointer());

    BOOST_CHECK( sch->HasTasksToExecute(t_next + period) );


    sch->TaskExecuted(id, t_next);
    sch->RemoveSeries(id);
    BOOST_CHECK(sch->IsEmpty());
}

/// Test AddRepetitiveTask() for task executing with delay
BOOST_AUTO_TEST_CASE(AddWithDelay)
{
    IScheduler* sch = s_GetScheduler();
    CTime t(CTime::eCurrent);
    CTimeSpan period(10, 0);

    CRef<CTestSchedTask> task(new CTestSchedTask());
    TScheduler_SeriesID id = sch->AddRepetitiveTask(task, t, period,
                                                    IScheduler::eWithDelay);
    BOOST_CHECK(!sch->IsEmpty());

    CTime t_prev = t - CTimeSpan(1, 0);
    CTime t_prev_next = t + period;
    CTime t_fin = t + period + CTimeSpan(1, 0);
    CTime t_next = t_fin + period;

    BOOST_CHECK_EQUAL(sch->GetNextExecutionTime(), t);


    BOOST_CHECK(! sch->HasTasksToExecute(t_prev));

    SScheduler_SeriesInfo task_info = sch->GetNextTaskToExecute(t_prev);
    BOOST_CHECK(!sch->IsEmpty());

    BOOST_CHECK_EQUAL(task_info.id, TScheduler_SeriesID(0));
    BOOST_CHECK(task_info.task.IsNull());


    BOOST_CHECK( sch->HasTasksToExecute(t) );

    task_info = sch->GetNextTaskToExecute(t);
    BOOST_CHECK(!sch->IsEmpty());

    BOOST_CHECK_EQUAL(task_info.id, id);
    BOOST_CHECK_EQUAL(task_info.task.GetPointer(), task.GetPointer());
    BOOST_CHECK(! sch->HasTasksToExecute(t_prev_next));

    sch->TaskExecuted(id, t_fin);
    BOOST_CHECK(!sch->IsEmpty());


    BOOST_CHECK_EQUAL(sch->GetNextExecutionTime(), t_next);
    BOOST_CHECK( sch->HasTasksToExecute(t_next) );

    task_info = sch->GetNextTaskToExecute(t_next);
    BOOST_CHECK(!sch->IsEmpty());

    BOOST_CHECK_EQUAL(task_info.id, id);
    BOOST_CHECK_EQUAL(task_info.task.GetPointer(), task.GetPointer());


    sch->TaskExecuted(id, t_next);
    sch->RemoveSeries(id);
    BOOST_CHECK(sch->IsEmpty());
}

/// Test correctness of GetScheduledTasks() method
BOOST_AUTO_TEST_CASE(GetSeries)
{
    IScheduler* sch = s_GetScheduler();
    CTime t(CTime::eCurrent);
    CTimeSpan period(10, 0);
    const size_t num_tasks = 10;

    CRef<CTestSchedTask> tasks[num_tasks];
    for (size_t i = 0; i < num_tasks; ++i, t += period) {
        tasks[i] = new CTestSchedTask();
        sch->AddTask(tasks[i], t);
    }

    vector<SScheduler_SeriesInfo> ret_tasks;
    sch->GetScheduledSeries(&ret_tasks);
    BOOST_CHECK_EQUAL(ret_tasks.size(), num_tasks);

    size_t num_found = 0;
    for (size_t i = 0; i < num_tasks; ++i) {
        bool found = false;
        for (size_t j = 0; j < num_tasks; ++j) {
            if (tasks[j].GetPointer() == ret_tasks[i].task.GetPointer()) {
                found = true;
                break;
            }
        }
        BOOST_CHECK(found);
        if (found)
            ++num_found;
    }
    BOOST_CHECK_EQUAL(num_found, num_tasks);

    for (size_t i = 0; i < num_tasks; ++i) {
        sch->RemoveSeries(ret_tasks[i].id);
    }

    sch->GetScheduledSeries(&ret_tasks);
    BOOST_CHECK_EQUAL(ret_tasks.size(), size_t(0));
}


class CTestSchedListener : public IScheduler_Listener
{
public:
    CTestSchedListener(void) : m_EventFired(false) {}

    virtual void OnNextExecutionTimeChange(IScheduler* scheduler) {
        m_EventFired = true;
    }

    bool GetEventFired(void) {
        bool result = m_EventFired;
        m_EventFired = false;
        return result;
    }

private:
    bool m_EventFired;
};


/// Test calling to listener whenever it's needed
BOOST_AUTO_TEST_CASE(Listening)
{
    IScheduler* sch = s_GetScheduler();
    CTime t(CTime::eCurrent);
    CTimeSpan period(10, 0);
    CRef<CTestSchedTask> task(new CTestSchedTask());
    CTestSchedListener lstn1, lstn2;
    sch->RegisterListener(&lstn1);
    sch->RegisterListener(&lstn2);

    TScheduler_SeriesID id1 = sch->AddTask(task, t);
    BOOST_CHECK(lstn1.GetEventFired());
    BOOST_CHECK(lstn2.GetEventFired());

    TScheduler_SeriesID id2 = sch->AddTask(task, t - period);
    BOOST_CHECK(lstn1.GetEventFired());
    BOOST_CHECK(lstn2.GetEventFired());

    TScheduler_SeriesID id3 = sch->AddTask(task, t + period);
    BOOST_CHECK(! lstn1.GetEventFired());
    BOOST_CHECK(! lstn2.GetEventFired());

    sch->RemoveSeries(id1);
    BOOST_CHECK(! lstn1.GetEventFired());
    BOOST_CHECK(! lstn2.GetEventFired());

    sch->RemoveSeries(id2);
    BOOST_CHECK(lstn1.GetEventFired());
    BOOST_CHECK(lstn2.GetEventFired());

    sch->RemoveSeries(id3);
    BOOST_CHECK(lstn1.GetEventFired());
    BOOST_CHECK(lstn2.GetEventFired());


    id1 = sch->AddRepetitiveTask(task, t, period, IScheduler::eWithRate);
    BOOST_CHECK(lstn1.GetEventFired());
    BOOST_CHECK(lstn2.GetEventFired());

    SScheduler_SeriesInfo task_info = sch->GetNextTaskToExecute(t);
    BOOST_CHECK(lstn1.GetEventFired());
    BOOST_CHECK(lstn2.GetEventFired());

    sch->TaskExecuted(id1, t + period);
    BOOST_CHECK(! lstn1.GetEventFired());
    BOOST_CHECK(! lstn2.GetEventFired());

    sch->RemoveSeries(id1);
    BOOST_CHECK(lstn1.GetEventFired());
    BOOST_CHECK(lstn2.GetEventFired());


    id1 = sch->AddRepetitiveTask(task, t, period, IScheduler::eWithDelay);
    BOOST_CHECK(lstn1.GetEventFired());
    BOOST_CHECK(lstn2.GetEventFired());

    task_info = sch->GetNextTaskToExecute(t);
    BOOST_CHECK(lstn1.GetEventFired());
    BOOST_CHECK(lstn2.GetEventFired());

    sch->TaskExecuted(id1, t + period);
    BOOST_CHECK(lstn1.GetEventFired());
    BOOST_CHECK(lstn2.GetEventFired());

    sch->RemoveSeries(id1);
    BOOST_CHECK(lstn1.GetEventFired());
    BOOST_CHECK(lstn2.GetEventFired());


    sch->UnregisterListener(&lstn1);
    sch->UnregisterListener(&lstn2);

    id1 = sch->AddTask(task.GetPointer(), t);
    BOOST_CHECK(! lstn1.GetEventFired());
    BOOST_CHECK(! lstn2.GetEventFired());

    sch->RemoveSeries(id1);
    BOOST_CHECK(! lstn1.GetEventFired());
    BOOST_CHECK(! lstn2.GetEventFired());
}
