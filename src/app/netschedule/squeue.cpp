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


#include <bdb/bdb_trans.hpp>

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
    qname(queue_name),
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
        db.Close();
        aff_idx.Close();
        affinity_dict.Close();
        m_TagDb.Close();
        ITERATE(vector<string>, it, files) {
            // NcbiCout << "Wipig out " << *it << NcbiEndl;
            CFile(*it).Remove();
        }
    }
    m_StatThread->RequestStop();
    m_StatThread->Join(NULL);
}

void SLockedQueue::Open(CBDB_Env& env, const string& path)
{
    string prefix = string("jsq_") + qname;
    string fname = prefix + ".db";
    db.SetEnv(env);

    db.RevSplitOff();
    db.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);
    files.push_back(path + fname);

    fname = prefix + "_affid.idx";
    aff_idx.SetEnv(env);
    aff_idx.Open(fname.c_str(), CBDB_RawFile::eReadWriteCreate);
    files.push_back(path + fname);

    affinity_dict.Open(env, qname);
    files.push_back(path + prefix + "_affdict.db");
    files.push_back(path + prefix + "_affdict_token.idx");

    fname = prefix + "tag.db";
    m_TagDb.SetEnv(env);
    m_TagDb.Open(fname, CBDB_RawFile::eReadWriteCreate);
    files.push_back(path + fname);
}


unsigned int SLockedQueue::GetNextId()
{
    if (m_LastId.Get() >= kMax_I4) {
        m_LastId.Set(0);
    }

    return (unsigned) m_LastId.Add(1);
}


unsigned int SLockedQueue::GetNextIdBatch(unsigned count)
{
    if (m_LastId.Get() >= kMax_I4) {
        // NB: This wrap-around zero will not work in the
        // case of batch id - client expect monotonously
        // growing range of ids.
        m_LastId.Set(0);
    }
    unsigned int id = (unsigned) m_LastId.Add(count);
    id = id - count + 1;
    return id;
}


void SLockedQueue::Erase(unsigned job_id)
{
    status_tracker.Erase(job_id);
    // Request delayed record delete
    CFastMutexGuard jtd_guard(m_JobsToDeleteLock);
    m_JobsToDelete.set_bit(job_id);
}


void SLockedQueue::SetTagDbTransaction(CBDB_Transaction* trans)
{
    m_TagDb.SetTransaction(trans);
}


void SLockedQueue::AppendTags(TNSTagMap& tag_map, TNSTagList& tags, unsigned job_id)
{
    ITERATE(TNSTagList, it, tags) {
        TNSBitVector bv;
        pair<TNSTagMap::iterator, bool> tag_map_it =
            tag_map.insert(TNSTagMap::value_type((*it), bv));
        (tag_map_it.first)->second.set(job_id);
    }
}


void SLockedQueue::AddTags(TNSTagMap& tag_map)
{
    CWriteLockGuard guard(m_TagLock);
    ITERATE(TNSTagMap, it, tag_map) {
        m_TagDb.key = it->first.first;
        m_TagDb.val = it->first.second;
        TNSBitVector bv;
        m_TagDb.ReadVector(&bv);
        bv |= it->second;
        m_TagDb.WriteVector(bv, STagDB::eCompact);
    }
}


void SLockedQueue::ReadTag(const string& key,
                           const string& val,
                           TBuffer* buf)
{
    // Guarded by m_TagLock through GetTagLock()
    CBDB_Transaction trans(*m_TagDb.GetEnv(), 
                        CBDB_Transaction::eTransASync,
                        CBDB_Transaction::eNoAssociation);
    m_TagDb.SetTransaction(&trans);
    CBDB_FileCursor cur(m_TagDb);
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << key << val;
    if (cur.Fetch(buf) == eBDB_Ok) {
    }
}


void SLockedQueue::ReadTags(const string& key, TNSBitVector* bv)
{
    // Guarded by m_TagLock through GetTagLock()
    CBDB_Transaction trans(*m_TagDb.GetEnv(), 
                        CBDB_Transaction::eTransASync,
                        CBDB_Transaction::eNoAssociation);
    m_TagDb.SetTransaction(&trans);
    CBDB_FileCursor cur(m_TagDb);
    cur.SetCondition(CBDB_FileCursor::eEQ);
    cur.From << key;
    TBuffer buf;
    bm::set_operation op_code = bm::set_ASSIGN;
    while (cur.Fetch(&buf) == eBDB_Ok) {
        bm::operation_deserializer<TNSBitVector>::deserialize(
            *bv,&(buf[0]), 0, op_code);
        op_code = bm::set_OR;
    }
}


void SLockedQueue::x_RemoveTags(CBDB_Transaction& trans,
                                const TNSBitVector& ids)
{
    CWriteLockGuard guard(m_TagLock);
    m_TagDb.SetTransaction(&trans);
    CBDB_FileCursor cur(m_TagDb, trans,
                        CBDB_FileCursor::eReadModifyUpdate);
    // iterate over tags database, deleting ids from every entry
    cur.SetCondition(CBDB_FileCursor::eFirst);
    CBDB_RawFile::TBuffer  buf;
    TNSBitVector bv;
    while (cur.Fetch(&buf) == eBDB_Ok) {
        bv.clear(true);
        bm::deserialize(bv, &buf[0]);
        unsigned before_remove = bv.count();
        bv -= ids;
        if (bv.count() != before_remove) {
            bv.optimize();
            size_t size = bm::serialize(bv, &buf[0]);
            cur.UpdateBlob(&buf[0], size);
        }
    }
}


unsigned SLockedQueue::DeleteBatch(unsigned batch_size)
{
    TNSBitVector batch;
    {{
        CFastMutexGuard guard(m_JobsToDeleteLock);
        unsigned job_id = 0;
        for (unsigned n = 0; n < batch_size &&
                             (job_id = m_JobsToDelete.extract_next(job_id));
             ++n)
        {
            batch.set(job_id);
        }
    }}
    if (batch.none()) return 0;

    CBDB_Transaction trans(*db.GetEnv(), 
                        CBDB_Transaction::eTransASync,
                        CBDB_Transaction::eNoAssociation);

    unsigned del_rec = 0;
    {{
        CFastMutexGuard guard(lock);
        db.SetTransaction(&trans);

        CBDB_FileCursor& cur = *GetCursor(trans);
        CBDB_CursorGuard cg(cur);    
        for (TNSBitVector::enumerator en = batch.first(); en.valid(); ++en) {
            unsigned job_id = *en;
            cur.SetCondition(CBDB_FileCursor::eEQ);
            cur.From << job_id;
            if (cur.FetchFirst() == eBDB_Ok) {
                cur.Delete(CBDB_File::eIgnoreError);
                ++del_rec;
            }
        }
        x_RemoveTags(trans, batch);
    }}
    trans.Commit();
    return del_rec;
}


CBDB_FileCursor* SLockedQueue::GetCursor(CBDB_Transaction& trans)
{
    CBDB_FileCursor* cur = m_Cursor.get();
    if (cur) { 
        cur->ReOpen(&trans);
        return cur;
    }
    cur = new CBDB_FileCursor(db, trans,
                              CBDB_FileCursor::eReadModifyUpdate);
    m_Cursor.reset(cur);
    return cur;
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
