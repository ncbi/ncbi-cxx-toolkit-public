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
 * Authors:  Victor Joukov
 *
 * File Description:
 *   NetSchedule queue structure and parameters
 */
#include <ncbi_pch.hpp>

#include "squeue.hpp"
#include "job_time_line.hpp"
#include "nslb.hpp"


BEGIN_NCBI_SCOPE


void SQueueParameters::Read(const IRegistry& reg, const string& sname)
{
    // Read general parameters
    timeout = reg.GetInt(sname, "timeout", 3600, 0, IRegistry::eReturn);
    notif_timeout =
        reg.GetInt(sname, "notif_timeout", 7, 0, IRegistry::eReturn);
    run_timeout =
        reg.GetInt(sname, "run_timeout", 
                                    timeout, 0, IRegistry::eReturn);
    run_timeout_precision =
        reg.GetInt(sname, "run_timeout_precision", 
                                    run_timeout, 0, IRegistry::eReturn);

    program_name = reg.GetString(sname, "program", kEmptyStr);

    delete_when_done = reg.GetBool(sname, "delete_done", 
                                        false, 0, IRegistry::eReturn);
    failed_retries = 
        reg.GetInt(sname, "failed_retries", 
                                    0, 0, IRegistry::eReturn);

    empty_lifetime =
        reg.GetInt(sname, "empty_lifetime", 
                                    -1, 0, IRegistry::eReturn);

    subm_hosts = reg.GetString(sname,  "subm_host",  kEmptyStr);
    wnode_hosts = reg.GetString(sname, "wnode_host", kEmptyStr);
    dump_db = reg.GetBool(sname, "dump_db", false, 0, IRegistry::eReturn);

    // Read load balancing parameters
    lb_flag = reg.GetBool(sname, "lb", false, 0, IRegistry::eReturn);
    lb_service = reg.GetString(sname, "lb_service", kEmptyStr);
    lb_collect_time =
        reg.GetInt(sname, "lb_collect_time", 5, 0, IRegistry::eReturn);
    lb_unknown_host = reg.GetString(sname, "lb_unknown_host", kEmptyStr);
    lb_exec_delay_str = reg.GetString(sname, "lb_exec_delay", kEmptyStr);
    lb_stall_time_mult = 
        reg.GetDouble(sname, "lb_exec_delay_mult", 0.5, 0, IRegistry::eReturn);

    string curve_type = reg.GetString(sname, "lb_curve", kEmptyStr);
    if (curve_type.empty() || 
        NStr::CompareNocase(curve_type, "linear") == 0) {
        lb_curve = eLBLinear;
        lb_curve_high = reg.GetDouble(sname,
                                      "lb_curve_high",
                                      0.6,
                                      0,
                                      IRegistry::eReturn);
        lb_curve_linear_low = reg.GetDouble(sname,
                                            "lb_curve_linear_low",
                                            0.15,
                                            0,
                                            IRegistry::eReturn);
//        LOG_POST(Info << sname 
//                      << " initializing linear LB curve"
//                      << " y0=" << lb_curve_high
//                      << " yN=" << lb_curve_linear_low);
    } else if (NStr::CompareNocase(curve_type, "regression") == 0) {
        lb_curve = eLBRegression;
        lb_curve_high = reg.GetDouble(sname,
                                      "lb_curve_high",
                                      0.85,
                                      0,
                                      IRegistry::eReturn);
        lb_curve_regression_a = reg.GetDouble(sname,
                                              "lb_curve_regression_a",
                                              -0.2,
                                              0,
                                              IRegistry::eReturn);
//        LOG_POST(Info << sname 
//                      << " initializing regression LB curve."
//                      << " y0=" << lb_curve_high 
//                      << " a="  << lb_curve_regression_a);
    }
}


SLockedQueue::SLockedQueue(const string& queue_name,
                           const string& qclass_name,
                           TQueueKind queue_kind)
  :
    qclass(qclass_name),
    kind(queue_kind),
    timeout(3600), 
    notif_timeout(7), 
    delete_done(false),
    failed_retries(0),
    empty_lifetime(-1),
    became_empty(-1),
    last_notif(0), 
    q_notif("NCBI_JSQ_"),
    run_time_line(0),
    rec_dump("jsqd_"+queue_name+".dump", 10 * (1024 * 1024)),
    rec_dump_flag(false),
    lb_flag(false),
    lb_coordinator(0),
    lb_stall_delay_type(eNSLB_Constant),
    lb_stall_time(6),
    lb_stall_time_mult(1.0),
    delete_database(false)
{
    _ASSERT(!queue_name.empty());
    q_notif.append(queue_name);
    for (TStatEvent n = 0; n < eStatNumEvents; ++n) {
        m_EventCounter[n].Set(0);
    }
    m_StatThread.Reset(new CStatisticsThread(*this));
    m_StatThread->Run();
    m_LastId.Set(0);
}

SLockedQueue::~SLockedQueue()
{
    NON_CONST_ITERATE(TListenerList, it, wnodes) {
        SQueueListener* node = *it;
        delete node;
    }
    delete run_time_line;
    delete lb_coordinator;
    if (delete_database) {
        // TODO: remove queue database files
        db.Close();
        aff_idx.Close();
        affinity_dict.Close();
        ITERATE(vector<string>, it, files) {
            // NcbiCout << "Wipig out " << *it << NcbiEndl;
            CFile(*it).Remove();
        }
    }
    void *pData;
    m_StatThread->RequestStop();
    m_StatThread->Join(&pData);
}


unsigned int SLockedQueue::GetNextId()
{
    unsigned int id;

    if (m_LastId.Get() >= kMax_I4) {
        m_LastId.Set(0);
    }
    id = (unsigned) m_LastId.Add(1); 

    /*
    if ((id % 1000) == 0) {
        m_Env->TransactionCheckpoint();
    }
    if ((id % 1000000) == 0) {
        m_Env->CleanLog();
    }
    */

    return id;
}


unsigned int SLockedQueue::GetNextIdBatch(unsigned count)
{
    if (m_LastId.Get() >= kMax_I4) {
        m_LastId.Set(0);
    }
    unsigned int id = (unsigned) m_LastId.Add(count);
    id = id - count + 1;
    return id;
}


const unsigned kMeasureInterval = 1;
// for informational purposes only, see kDecayExp below
const unsigned kDecayInterval = 10;
const unsigned kFixedShift = 7;
const unsigned kFixed_1 = 1 << kFixedShift;
// kDecayExp = 2 ^ kFixedShift / 2 ^ ( kMeasureInterval * log2(e) / kDecayInterval)
const unsigned kDecayExp = 116;

SLockedQueue::CStatisticsThread::CStatisticsThread(TContainer& container)
    : CThreadNonStop(kMeasureInterval),
        m_Container(container)
{
}


void SLockedQueue::CStatisticsThread::DoJob(void) {
    unsigned counter;
    for (TStatEvent n = 0; n < eStatNumEvents; ++n) {
        counter = m_Container.m_EventCounter[n].Get();
        m_Container.m_EventCounter[n].Add(-counter);
        m_Container.m_Average[n] = (kDecayExp * m_Container.m_Average[n] +
                                (kFixed_1-kDecayExp) * (counter << kFixedShift)
                                   ) >> kFixedShift;
    }
}


void SLockedQueue::CountEvent(TStatEvent event)
{
    m_EventCounter[event].Add(1);
}


double SLockedQueue::GetAverage(TStatEvent n_event)
{
    return m_Average[n_event] / double(kFixed_1 * kMeasureInterval);
}


END_NCBI_SCOPE
