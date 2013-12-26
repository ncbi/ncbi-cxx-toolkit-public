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
 * Author: Pavel Ivanov
 */

#include "nc_pch.hpp"

#include <corelib/request_ctx.hpp>

#include "nc_stat.hpp"
#include "netcached.hpp"
#include "distribution_conf.hpp"
#include "nc_storage.hpp"
#include <set>


BEGIN_NCBI_SCOPE


static const Uint1 kCntStatPeriods = 10;
static const char* const kStatPeriodName[kCntStatPeriods]
        = {"5s", "1min", "5min", "1h", "1d", "1w", "1mon", "3mon", "1y", "life"};
static Uint1 kCollectPeriodsCnt[kCntStatPeriods]
        = {0,    12,     5,      12,   24,   7,    31,     3,      4,    0};
static const Uint1 kMinStatPeriod = 5;
static const Uint1 kDayPeriodIdx = 4;
static const Uint1 kMonthPeriodIdx = 6;
static const Uint1 kLifePeriodIdx = 9;


static CNCStat* s_ThreadStat = NULL;
static CNCStat** s_CurPeriodStat = NULL;
static CNCStat** s_PrevPeriodStat = NULL;
static CStatRotator* s_Rotator = NULL;
static CMiniMutex s_RotateLock;
static Uint1 s_PeriodsCollected[kCntStatPeriods] = {0};
static int s_LastRotateSecs = 0;

static CMiniMutex s_CommonStatLock;
static set<Uint8> s_SyncSrv;
static set<Uint8> s_UnknownSrv;
typedef map<Uint8, CSrvTime> TSyncTimes;
static TSyncTimes s_SyncSucceeded;
static TSyncTimes s_SyncFailed;
static TSyncTimes s_SyncPeriodic;


static void
s_InitPeriodsCollected(void)
{
#ifdef NCBI_OS_LINUX
    time_t cur_secs = CSrvTime::CurSecs();
    time_t adj_secs = cur_secs + CSrvTime::TZAdjustment();
    struct tm t;
    gmtime_r(&adj_secs, &t);
    s_PeriodsCollected[1] = t.tm_sec / kMinStatPeriod;
    s_PeriodsCollected[2] = t.tm_min % 5;
    s_PeriodsCollected[3] = t.tm_min / 5;
    s_PeriodsCollected[4] = t.tm_hour;
    s_PeriodsCollected[5] = t.tm_wday;
    s_PeriodsCollected[6] = t.tm_mday - 1;
    s_PeriodsCollected[7] = t.tm_mon % 3;
    s_PeriodsCollected[8] = t.tm_mon / 3;
    s_LastRotateSecs = cur_secs;
#endif
}

static void
s_CheckTZChange(void)
{
#ifdef NCBI_OS_LINUX
    time_t cur_secs = CSrvTime::CurSecs() + CSrvTime::TZAdjustment();
    struct tm t;
    gmtime_r(&cur_secs, &t);
    s_PeriodsCollected[kDayPeriodIdx] = t.tm_hour;
#endif
}

static void
s_SetCurMonthSize(void)
{
#ifdef NCBI_OS_LINUX
    time_t cur_secs = CSrvTime::CurSecs() + CSrvTime::TZAdjustment();
    struct tm t;
    gmtime_r(&cur_secs, &t);
    if (t.tm_mon == 1) {
        ++t.tm_mon;
        t.tm_hour = t.tm_min = t.tm_sec = 0;
        t.tm_isdst = -1;
        time_t next_loc_time = mktime(&t);
        --next_loc_time;
        localtime_r(&next_loc_time, &t);
        kCollectPeriodsCnt[kMonthPeriodIdx] = t.tm_mday;
    }
    else {
        switch (t.tm_mon) {
        case 0:
        case 2:
        case 4:
        case 6:
        case 7:
        case 9:
        case 11:
            kCollectPeriodsCnt[kMonthPeriodIdx] = 31;
            break;
        default:
            kCollectPeriodsCnt[kMonthPeriodIdx] = 30;
            break;
        }
    }
#endif
}


inline bool
SConstCharCompare::operator() (const char* left, const char* right) const
{
    return NStr::strcmp(left, right) < 0;
}


static inline CNCStat*
s_Stat(void)
{
    return &s_ThreadStat[CTaskServer::GetCurThreadNum()];
}

static unsigned int
s_SizeIndex(Uint8 size)
{
    if (size <= 1)
        return 0;
    --size;
    return g_GetLogBase2(size);
}

void
CNCStat::AddSyncServer(Uint8 srv_id)
{
    CMiniMutexGuard guard(s_CommonStatLock);
    s_SyncSrv.insert(srv_id);
}

bool CNCStat::AddUnknownServer(Uint8 srv_id)
{
    CMiniMutexGuard guard(s_CommonStatLock);
    if (s_UnknownSrv.find(srv_id) == s_UnknownSrv.end()) {
        s_UnknownSrv.insert(srv_id);
        return true;
    }
    return false;
}

void
CNCStat::InitialSyncDone(Uint8 srv_id, bool succeeded)
{
    CMiniMutexGuard guard(s_CommonStatLock);
    if (succeeded) {
        s_SyncSucceeded[srv_id] = CSrvTime::Current();
    } else {
        s_SyncFailed[srv_id] = CSrvTime::Current();
    }
}

void
CNCStat::Initialize(void)
{
    s_ThreadStat = new CNCStat[CTaskServer::GetMaxRunningThreads()];
    s_CurPeriodStat = (CNCStat**)malloc(kCntStatPeriods * sizeof(CNCStat*));
    s_PrevPeriodStat = (CNCStat**)malloc(kCntStatPeriods * sizeof(CNCStat*));
    for (Uint1 i = 0; i < kCntStatPeriods; ++i) {
        CNCStat* stat = new CNCStat();
        stat->AddReference();
        stat->InitStartState();
        s_CurPeriodStat[i] = stat;

        stat = new CNCStat();
        stat->AddReference();
        stat->InitStartState();
        s_PrevPeriodStat[i] = stat;
    }

    s_InitPeriodsCollected();
    s_SetCurMonthSize();

    s_Rotator = new CStatRotator();
    s_Rotator->CalcNextRun();
}

CNCStat::CNCStat(void)
    : m_SrvStat(new CSrvStat())
{
    x_ClearStats();
}

void
CNCStat::x_ClearStats(void)
{
    m_StartedCmds = 0;
    m_ClDataWrite = 0;
    m_ClDataRead = 0;
    m_PeerDataWrite = 0;
    m_PeerDataRead = 0;
    m_DiskDataWrite = 0;
    m_DiskDataRead = 0;
    m_MaxBlobSize = 0;
    m_ClWrBlobs = 0;
    m_ClWrBlobSize = 0;
    m_ClWrLenBySize.resize(40);
    m_ClRbackBlobs = 0;
    m_ClRbackSize = 0;
    m_ClRdBlobs = 0;
    m_ClRdBlobSize = 0;
    m_ClRdLenBySize.resize(40);
    m_DiskWrBlobs = 0;
    m_DiskWrBlobSize = 0;
    m_DiskWrBySize.resize(0);
    m_DiskWrBySize.resize(40, 0);
    m_PeerSyncs = 0;
    m_PeerSynOps = 0;
    m_CntCleanedFiles = 0;
    m_CntFailedFiles = 0;
    m_CmdLens.Initialize();
    m_CmdsByName.clear();
    m_LensByStatus.clear();
    m_ConnCmds.Initialize();
    for (int i = 0; i < 40; ++i) {
        m_ClWrLenBySize[i].Initialize();
        m_ClRdLenBySize[i].Initialize();
    }
    m_CheckedRecs.Initialize();
    m_MovedRecs.Initialize();
    m_MovedSize.Initialize();
    m_CntFiles.Initialize();
    m_DBSize.Initialize();
    m_GarbageSize.Initialize();
    m_CntBlobs.Initialize();
    m_CntKeys.Initialize();
    m_MirrorQSize.Initialize();
    m_SyncLogSize.Initialize();
    m_WBMemSize.Initialize();
    m_WBReleasable.Initialize();
    m_WBReleasing.Initialize();
}

void
CNCStat::InitStartState(void)
{
    m_StartState.state_time = CSrvTime::Current();
    m_StartState.progress_cmds = 0;
    CNCServer::ReadCurState(m_StartState);
    m_EndState = m_StartState;
    m_SrvStat->InitStartState();
}

void
CNCStat::TransferEndState(CNCStat* src_stat)
{
    src_stat->m_StatLock.Lock();
    m_EndState = m_StartState = src_stat->m_EndState;
    m_SrvStat->TransferEndState(src_stat->m_SrvStat.get());
    src_stat->m_StatLock.Unlock();
}

void
CNCStat::x_CopyStartState(CNCStat* src_stat)
{
    m_StartState = src_stat->m_StartState;
    m_SrvStat->CopyStartState(src_stat->m_SrvStat.get());
}

void
CNCStat::x_CopyEndState(CNCStat* src_stat)
{
    m_EndState = src_stat->m_EndState;
    m_SrvStat->CopyEndState(src_stat->m_SrvStat.get());
}

void
CNCStat::x_SaveEndState(void)
{
    m_EndState.state_time = CSrvTime::Current();
    m_EndState.progress_cmds = Uint4(m_StartState.progress_cmds
                                     + m_StartedCmds - m_CmdLens.GetCount());
    CNCServer::ReadCurState(m_EndState);
    m_SrvStat->SaveEndState();
}

void
CNCStat::x_AddStats(CNCStat* src_stat)
{
    m_StartedCmds += src_stat->m_StartedCmds;
    m_CmdLens.AddValues(src_stat->m_CmdLens);
    ITERATE(TCmdCountsMap, it, src_stat->m_CmdsByName) {
        m_CmdsByName[it->first] += it->second;
    }
    ITERATE(TStatusCmdLens, it_stat, src_stat->m_LensByStatus) {
        const TCmdLensMap& src_lens = it_stat->second;
        TCmdLensMap& dst_lens = m_LensByStatus[it_stat->first];
        ITERATE(TCmdLensMap, it_cmd, src_lens) {
            TSrvTimeTerm& time_term = g_SrvTimeTerm(dst_lens, it_cmd->first);
            time_term.AddValues(it_cmd->second);
        }
    }
    m_ConnCmds.AddValues(src_stat->m_ConnCmds);
    m_ClDataWrite += src_stat->m_ClDataWrite;
    m_ClDataRead += src_stat->m_ClDataRead;
    m_PeerDataWrite += src_stat->m_PeerDataWrite;
    m_PeerDataRead += src_stat->m_PeerDataRead;
    m_DiskDataWrite += src_stat->m_DiskDataWrite;
    m_DiskDataRead += src_stat->m_DiskDataRead;
    m_MaxBlobSize = max(m_MaxBlobSize, src_stat->m_MaxBlobSize);
    m_ClWrBlobs += src_stat->m_ClWrBlobs;
    m_ClWrBlobSize += src_stat->m_ClWrBlobSize;
    for (size_t i = 0; i < src_stat->m_ClWrLenBySize.size(); ++i) {
        m_ClWrLenBySize[i].AddValues(src_stat->m_ClWrLenBySize[i]);
        m_ClRdLenBySize[i].AddValues(src_stat->m_ClRdLenBySize[i]);
        m_DiskWrBySize[i] += src_stat->m_DiskWrBySize[i];
    }
    m_ClRbackBlobs += src_stat->m_ClRbackBlobs;
    m_ClRbackSize += src_stat->m_ClRbackSize;
    m_ClRdBlobs += src_stat->m_ClRdBlobs;
    m_ClRdBlobSize += src_stat->m_ClRdBlobSize;
    m_DiskWrBlobs += src_stat->m_DiskWrBlobs;
    m_DiskWrBlobSize += src_stat->m_DiskWrBlobSize;
    m_PeerSyncs += src_stat->m_PeerSyncs;
    m_PeerSynOps += src_stat->m_PeerSynOps;
    m_CntCleanedFiles += src_stat->m_CntCleanedFiles;
    m_CntFailedFiles += src_stat->m_CntFailedFiles;
    m_CheckedRecs.AddValues(src_stat->m_CheckedRecs);
    m_MovedRecs.AddValues(src_stat->m_MovedRecs);
    m_MovedSize.AddValues(src_stat->m_MovedSize);
    m_CntFiles.AddValues(src_stat->m_CntFiles);
    m_DBSize.AddValues(src_stat->m_DBSize);
    m_GarbageSize.AddValues(src_stat->m_GarbageSize);
    m_CntBlobs.AddValues(src_stat->m_CntBlobs);
    m_CntKeys.AddValues(src_stat->m_CntKeys);
    m_MirrorQSize.AddValues(src_stat->m_MirrorQSize);
    m_SyncLogSize.AddValues(src_stat->m_SyncLogSize);
    m_WBMemSize.AddValues(src_stat->m_WBMemSize);
    m_WBReleasable.AddValues(src_stat->m_WBReleasable);
    m_WBReleasing.AddValues(src_stat->m_WBReleasing);
}

void
CNCStat::AddAllStats(CNCStat* src_stat)
{
    m_StatLock.Lock();
    src_stat->m_StatLock.Lock();

    x_AddStats(src_stat);
    m_SrvStat->AddAllStats(src_stat->m_SrvStat.get());
    x_CopyEndState(src_stat);

    src_stat->m_StatLock.Unlock();
    m_StatLock.Unlock();
}

void
CNCStat::CollectThreads(CNCStat* dst_stat, bool need_clear)
{
    dst_stat->m_StatLock.Lock();
    TSrvThreadNum cnt_threads = CTaskServer::GetMaxRunningThreads();
    for (TSrvThreadNum i = 0; i < cnt_threads; ++i) {
        CNCStat* src_stat = &s_ThreadStat[i];
        src_stat->m_StatLock.Lock();
        dst_stat->x_AddStats(src_stat);
        if (need_clear)
            src_stat->x_ClearStats();
        src_stat->m_StatLock.Unlock();
    }
    dst_stat->m_SrvStat->CollectThreads(need_clear);
    dst_stat->x_SaveEndState();
    dst_stat->m_StatLock.Unlock();
}

CSrvRef<CNCStat>
CNCStat::GetStat(const string& stat_type, bool is_prev)
{
    Uint1 idx = 0;
    while (idx < kCntStatPeriods  &&  kStatPeriodName[idx] != stat_type)
        ++idx;
    if (idx >= kCntStatPeriods)
        return CSrvRef<CNCStat>();

    CSrvRef<CNCStat> stat;
    s_RotateLock.Lock();
    if (is_prev)
        stat = s_PrevPeriodStat[idx];
    else
        stat = s_CurPeriodStat[idx];
    stat->m_StatName = stat_type;
    s_RotateLock.Unlock();

    if (is_prev)
        return stat;

    CSrvRef<CNCStat> stat_copy(new CNCStat());
    stat_copy->m_StatName = stat_type;
    stat_copy->x_CopyStartState(stat);
    stat_copy->AddAllStats(stat);
    CollectThreads(stat_copy, false);

    return stat_copy;
}

Uint4
CNCStat::GetCntRunningCmds(void)
{
    s_RotateLock.Lock();
    CNCStat* stat = s_CurPeriodStat[0];
    Uint8 cnt_cmds = stat->m_StartState.progress_cmds
                     + stat->m_StartedCmds - stat->m_CmdLens.GetCount();
    s_RotateLock.Unlock();

    TSrvThreadNum cnt_threads = CTaskServer::GetMaxRunningThreads();
    for (TSrvThreadNum i = 0; i < cnt_threads; ++i) {
        stat = &s_ThreadStat[i];
        stat->m_StatLock.Lock();
        cnt_cmds += stat->m_StartedCmds;
        cnt_cmds -= stat->m_CmdLens.GetCount();
        stat->m_StatLock.Unlock();
    }

    return Uint4(cnt_cmds);
}

void
CNCStat::CmdStarted(const char* cmd)
{
    CNCStat* stat = s_Stat();
    stat->m_StatLock.Lock();
    ++stat->m_StartedCmds;
    ++stat->m_CmdsByName[cmd];
    stat->m_StatLock.Unlock();
}

void
CNCStat::CmdFinished(const char* cmd, Uint8 len_usec, int status)
{
    CNCStat* stat = s_Stat();
    stat->m_StatLock.Lock();
    stat->m_CmdLens.AddValue(len_usec);
    TCmdLensMap& cmd_lens = stat->m_LensByStatus[status];
    TSrvTimeTerm& time_term = g_SrvTimeTerm(cmd_lens, cmd);
    time_term.AddValue(len_usec);
    stat->m_StatLock.Unlock();
}

void
CNCStat::ConnClosing(Uint8 cnt_cmds)
{
    CNCStat* stat = s_Stat();
    stat->m_StatLock.Lock();
    stat->m_ConnCmds.AddValue(cnt_cmds);
    stat->m_StatLock.Unlock();
}

void
CNCStat::ClientDataWrite(size_t data_size)
{
    AtomicAdd(s_Stat()->m_ClDataWrite, data_size);
}

void
CNCStat::ClientDataRead(size_t data_size)
{
    AtomicAdd(s_Stat()->m_ClDataRead, data_size);
}

void
CNCStat::ClientBlobWrite(Uint8 blob_size, Uint8 len_usec)
{
    CNCStat* stat = s_Stat();
    stat->m_StatLock.Lock();
    ++stat->m_ClWrBlobs;
    stat->m_ClWrBlobSize += blob_size;
    stat->m_MaxBlobSize = max(stat->m_MaxBlobSize, blob_size);
    Uint4 size_idx = s_SizeIndex(blob_size);
    stat->m_ClWrLenBySize[size_idx].AddValue(len_usec);
    stat->m_StatLock.Unlock();
}

void
CNCStat::ClientBlobRollback(Uint8 written_size)
{
    CNCStat* stat = s_Stat();
    AtomicAdd(stat->m_ClRbackBlobs, 1);
    AtomicAdd(stat->m_ClRbackSize, written_size);
}

void
CNCStat::ClientBlobRead(Uint8 blob_size, Uint8 len_usec)
{
    CNCStat* stat = s_Stat();
    stat->m_StatLock.Lock();
    ++stat->m_ClRdBlobs;
    stat->m_ClRdBlobSize += blob_size;
    stat->m_MaxBlobSize = max(stat->m_MaxBlobSize, blob_size);
    Uint4 size_idx = s_SizeIndex(blob_size);
    stat->m_ClRdLenBySize[size_idx].AddValue(len_usec);
    stat->m_StatLock.Unlock();
}

void
CNCStat::PeerDataWrite(size_t data_size)
{
    AtomicAdd(s_Stat()->m_PeerDataWrite, data_size);
}

void
CNCStat::PeerDataRead(size_t data_size)
{
    AtomicAdd(s_Stat()->m_PeerDataRead, data_size);
}

void
CNCStat::PeerSyncFinished(Uint8 srv_id, Uint8 cnt_ops, bool success)
{
    CNCStat* stat = s_Stat();
    if (success)
        AtomicAdd(stat->m_PeerSyncs, 1);
    AtomicAdd(stat->m_PeerSynOps, cnt_ops);
    if (success) {
        CMiniMutexGuard guard(s_CommonStatLock);
        s_SyncPeriodic[srv_id] = CSrvTime::Current();
    }
}

void
CNCStat::DiskDataWrite(size_t data_size)
{
    AtomicAdd(s_Stat()->m_DiskDataWrite, data_size);
}

void
CNCStat::DiskDataRead(size_t data_size)
{
    AtomicAdd(s_Stat()->m_DiskDataRead, data_size);
}

void
CNCStat::DiskBlobWrite(Uint8 blob_size)
{
    CNCStat* stat = s_Stat();
    stat->m_StatLock.Lock();
    ++stat->m_DiskWrBlobs;
    stat->m_DiskWrBlobSize += blob_size;
    stat->m_MaxBlobSize = max(stat->m_MaxBlobSize, blob_size);
    ++stat->m_DiskWrBySize[s_SizeIndex(blob_size)];
    stat->m_StatLock.Unlock();
}

void
CNCStat::DBFileCleaned(bool success, Uint4 seen_recs,
                       Uint4 moved_recs, Uint4 moved_size)
{
    CNCStat* stat = s_Stat();
    stat->m_StatLock.Lock();
    if (success)
        ++stat->m_CntCleanedFiles;
    else
        ++stat->m_CntFailedFiles;
    stat->m_CheckedRecs.AddValue(seen_recs);
    stat->m_MovedRecs.AddValue(moved_recs);
    stat->m_MovedSize.AddValue(moved_size);
    stat->m_StatLock.Unlock();
}

void
CNCStat::SaveCurStateStat(const SNCStateStat& state)
{
    CNCStat* stat = s_Stat();
    stat->m_StatLock.Lock();
    stat->m_CntFiles.AddValue(state.db_files);
    stat->m_DBSize.AddValue(state.db_size);
    stat->m_GarbageSize.AddValue(state.db_garb);
    stat->m_CntBlobs.AddValue(state.cnt_blobs);
    stat->m_CntKeys.AddValue(state.cnt_keys);
    stat->m_MirrorQSize.AddValue(state.mirror_queue_size);
    stat->m_SyncLogSize.AddValue(state.sync_log_size);
    stat->m_WBMemSize.AddValue(state.wb_size);
    stat->m_WBReleasable.AddValue(state.wb_releasable);
    stat->m_WBReleasing.AddValue(state.wb_releasing);
    stat->m_StatLock.Unlock();

    CSrvRef<CNCStat> stat_5s = GetStat(kStatPeriodName[0], false);
    stat_5s->m_SrvStat->SaveEndStateStat();
}

void
CNCStat::x_PrintUnstructured(CSrvPrintProxy& proxy)
{
    if (m_CmdLens.GetCount() != 0) {
        proxy << "Commands by status and type:" << endl;
        ITERATE(TStatusCmdLens, it_st, m_LensByStatus) {
            proxy << it_st->first << ":" << endl;
            const TCmdLensMap& lens_map = it_st->second;
            ITERATE(TCmdLensMap, it_cmd, lens_map) {
                const TSrvTimeTerm& time_term = it_cmd->second;
                proxy << it_cmd->first << " - "
                      << g_ToSmartStr(time_term.GetCount()) << " (cnt), "
                      << g_AsMSecStat(time_term.GetAverage()) << " (avg msec), "
                      << g_AsMSecStat(time_term.GetMaximum()) << " (max msec)" << endl;
            }
        }
        proxy << endl;
    }
    if (m_ClWrBlobs != 0) {
        proxy << "Client writes by size:" << endl;
        Uint8 prev_size = 0, size = 2;
        for (size_t i = 0; i < m_ClWrLenBySize.size()  &&  prev_size < m_MaxBlobSize
                         ; ++i, prev_size = size + 1, size <<= 1)
        {
            if (m_ClWrLenBySize[i].GetCount() == 0)
                continue;

            TSrvTimeTerm& time_term = m_ClWrLenBySize[i];
            proxy << g_ToSizeStr(prev_size) << "-" << g_ToSizeStr(size) << ": "
                  << g_ToSmartStr(time_term.GetCount()) << " (cnt), "
                  << g_AsMSecStat(time_term.GetAverage()) << " (avg msec), "
                  << g_AsMSecStat(time_term.GetMaximum()) << " (max msec)" << endl;
        }
        proxy << endl;
    }
    if (m_ClRdBlobs != 0) {
        proxy << "Client reads by size:" << endl;
        Uint8 prev_size = 0, size = 2;
        for (size_t i = 0; i < m_ClRdLenBySize.size()  &&  prev_size < m_MaxBlobSize
                         ; ++i, prev_size = size + 1, size <<= 1)
        {
            if (m_ClRdLenBySize[i].GetCount() == 0)
                continue;

            TSrvTimeTerm& time_term = m_ClRdLenBySize[i];
            proxy << g_ToSizeStr(prev_size) << "-" << g_ToSizeStr(size) << ": "
                  << g_ToSmartStr(time_term.GetCount()) << " (cnt), "
                  << g_AsMSecStat(time_term.GetAverage()) << " (avg msec), "
                  << g_AsMSecStat(time_term.GetMaximum()) << " (max msec)" << endl;
        }
        proxy << endl;
    }
    if (m_DiskWrBlobs != 0) {
        proxy << "Disk writes by size:" << endl;
        Uint8 prev_size = 0, size = 2;
        for (size_t i = 0; i < m_DiskWrBySize.size()  &&  prev_size < m_MaxBlobSize
                         ; ++i, prev_size = size + 1, size <<= 1)
        {
            if (m_DiskWrBySize[i] != 0) {
                proxy << g_ToSizeStr(prev_size) << "-" << g_ToSizeStr(size) << ": "
                      << g_ToSmartStr(m_DiskWrBySize[i]) << " (cnt)" << endl;
            }
        }
        proxy << endl;
    }
}

void
CNCStat::PrintToLogs(CTempString stat_name)
{
    char buf[50];
    CSrvTime t;

    m_StatLock.Lock();
    CSrvRef<CRequestContext> ctx(new CRequestContext());
    ctx->SetRequestID();
    CSrvDiagMsg diag;
    diag.StartRequest(ctx);
    diag.PrintParam("_type", "stat").PrintParam("name", stat_name);
    m_StartState.state_time.Print(buf, CSrvTime::eFmtLogging);
    diag.PrintParam("start_time", buf);
    m_EndState.state_time.Print(buf, CSrvTime::eFmtLogging);
    diag.PrintParam("end_time", buf);
    CSrvTime time_diff = m_EndState.state_time;
    time_diff -= m_StartState.state_time;
    Uint8 time_secs = time_diff.AsUSec() / kUSecsPerSecond;
    if (time_secs == 0)
        time_secs = 1;

    diag.PrintParam("start_run_cmds", m_StartState.progress_cmds)
        .PrintParam("end_run_cmds", m_EndState.progress_cmds)
        .PrintParam("cmds_started", m_StartedCmds)
        .PrintParam("cmds_finished", m_CmdLens.GetCount())
        .PrintParam("avg_conn_cmds", m_ConnCmds.GetAverage())
        .PrintParam("max_conn_cmds", m_ConnCmds.GetMaximum());
    diag.PrintParam("start_db_size", m_StartState.db_size)
        .PrintParam("end_db_size", m_EndState.db_size)
        .PrintParam("avg_db_size", m_DBSize.GetAverage())
        .PrintParam("max_db_size", m_DBSize.GetMaximum());
    diag.PrintParam("start_garbage", m_StartState.db_garb)
        .PrintParam("end_garbage", m_EndState.db_garb)
        .PrintParam("avg_garbage", m_GarbageSize.GetAverage())
        .PrintParam("max_garbage", m_GarbageSize.GetMaximum())
        .PrintParam("start_garb_pct", g_CalcStatPct(m_StartState.db_garb, m_StartState.db_size))
        .PrintParam("end_garb_pct", g_CalcStatPct(m_EndState.db_garb, m_EndState.db_size));
    diag.PrintParam("start_db_files", m_StartState.db_files)
        .PrintParam("end_db_files", m_EndState.db_files)
        .PrintParam("avg_db_files", m_CntFiles.GetAverage())
        .PrintParam("max_db_files", m_CntFiles.GetMaximum())
        .PrintParam("start_blobs", m_StartState.cnt_blobs)
        .PrintParam("end_blobs", m_EndState.cnt_blobs)
        .PrintParam("avg_blobs", m_CntBlobs.GetAverage())
        .PrintParam("max_blobs", m_CntBlobs.GetMaximum())
        .PrintParam("start_keys", m_StartState.cnt_keys)
        .PrintParam("end_keys", m_EndState.cnt_keys)
        .PrintParam("avg_keys", m_CntKeys.GetAverage())
        .PrintParam("max_keys", m_CntKeys.GetMaximum())
        .PrintParam("start_mirror_q_size", m_StartState.mirror_queue_size)
        .PrintParam("end_mirror_q_size", m_EndState.mirror_queue_size)
        .PrintParam("avg_mirror_q_size", m_MirrorQSize.GetAverage())
        .PrintParam("max_mirror_q_size", m_MirrorQSize.GetMaximum())
        .PrintParam("start_sync_log_size", m_StartState.sync_log_size)
        .PrintParam("end_sync_log_size", m_EndState.sync_log_size)
        .PrintParam("avg_sync_log_size", m_SyncLogSize.GetAverage())
        .PrintParam("max_sync_log_size", m_SyncLogSize.GetMaximum());
    diag.PrintParam("start_wb_size", m_StartState.wb_size)
        .PrintParam("end_wb_size", m_EndState.wb_size)
        .PrintParam("avg_wb_size", m_WBMemSize.GetAverage())
        .PrintParam("max_wb_size", m_WBMemSize.GetMaximum())
        .PrintParam("start_wb_releasable", m_StartState.wb_releasable)
        .PrintParam("end_wb_releasable", m_EndState.wb_releasable)
        .PrintParam("avg_wb_releasable", m_WBReleasable.GetAverage())
        .PrintParam("max_wb_releasable", m_WBReleasable.GetMaximum())
        .PrintParam("start_wb_releasing", m_StartState.wb_releasing)
        .PrintParam("end_wb_releasing", m_EndState.wb_releasing)
        .PrintParam("avg_wb_releasing", m_WBReleasing.GetAverage())
        .PrintParam("max_wb_releasing", m_WBReleasing.GetMaximum());
    if (m_StartState.min_dead_time != 0) {
        t.Sec() = m_StartState.min_dead_time;
        t.Print(buf, CSrvTime::eFmtLogging);
        diag.PrintParam("start_dead", buf);
    }
    if (m_EndState.min_dead_time != 0) {
        t.Sec() = m_EndState.min_dead_time;
        t.Print(buf, CSrvTime::eFmtLogging);
        diag.PrintParam("end_dead", buf);
    }
    diag.PrintParam("cl_write", m_ClDataWrite)
        .PrintParam("avg_cl_write", m_ClDataWrite / time_secs)
        .PrintParam("cl_read", m_ClDataRead)
        .PrintParam("avg_cl_read", m_ClDataRead / time_secs)
        .PrintParam("peer_write", m_PeerDataWrite)
        .PrintParam("avg_peer_write", m_PeerDataWrite / time_secs)
        .PrintParam("peer_read", m_PeerDataRead)
        .PrintParam("avg_peer_read", m_PeerDataRead / time_secs)
        .PrintParam("disk_write", m_DiskDataWrite)
        .PrintParam("avg_disk_write", m_DiskDataWrite / time_secs)
        .PrintParam("disk_read", m_DiskDataRead)
        .PrintParam("avg_disk_read", m_DiskDataRead / time_secs);
    diag.PrintParam("cl_wr_blobs", m_ClWrBlobs)
        .PrintParam("cl_wr_avg_blobs", m_ClWrBlobs / time_secs)
        .PrintParam("cl_wr_size", m_ClWrBlobSize)
        .PrintParam("cl_rback_blobs", m_ClRbackBlobs)
        .PrintParam("cl_rback_size", m_ClRbackSize)
        .PrintParam("cl_rd_blobs", m_ClRdBlobs)
        .PrintParam("cl_rd_avg_blobs", m_ClRdBlobs / time_secs)
        .PrintParam("cl_rd_size", m_ClRdBlobSize)
        .PrintParam("disk_wr_blobs", m_DiskWrBlobs)
        .PrintParam("disk_wr_avg_blobs", m_DiskWrBlobs / time_secs)
        .PrintParam("disk_wr_size", m_DiskWrBlobSize);
    diag.PrintParam("peer_syncs", m_PeerSyncs)
        .PrintParam("peer_syn_ops", m_PeerSynOps)
        .PrintParam("cleaned_files", m_CntCleanedFiles)
        .PrintParam("failed_cleans", m_CntFailedFiles)
        .PrintParam("checked_recs", m_CheckedRecs.GetSum())
        .PrintParam("avg_checked_recs", m_CheckedRecs.GetAverage())
        .PrintParam("moved_recs", m_MovedRecs.GetSum())
        .PrintParam("avg_moved_recs", m_MovedRecs.GetAverage())
        .PrintParam("moved_size", m_MovedSize.GetSum())
        .PrintParam("avg_moved_size", m_MovedSize.GetAverage());
    diag.Flush();

    CSrvPrintProxy proxy(ctx);
    m_SrvStat->PrintToLogs(ctx, proxy);
    x_PrintUnstructured(proxy);

    diag.StopRequest(ctx);
    m_StatLock.Unlock();
}

void CNCStat::PrintState(CSrvSocketTask& task)
{
    string is("\": "), iss("\": \""), eol(",\n\""), str("_str");
    task.WriteText(eol).WriteText("db_files"     ).WriteText(is ).WriteNumber( m_EndState.db_files);
    task.WriteText(eol).WriteText("db_size"      ).WriteText(str).WriteText(iss)
                                     .WriteText( NStr::UInt8ToString_DataSize( m_EndState.db_size)).WriteText("\"");
    task.WriteText(eol).WriteText("db_size"      ).WriteText(is ).WriteNumber( m_EndState.db_size);
    task.WriteText(eol).WriteText("db_garb"      ).WriteText(is ).WriteNumber( m_EndState.db_garb);
    task.WriteText(eol).WriteText("cnt_blobs"    ).WriteText(is ).WriteNumber( m_EndState.cnt_blobs);
    task.WriteText(eol).WriteText("cnt_keys"     ).WriteText(is ).WriteNumber( m_EndState.cnt_keys);
    task.WriteText(eol).WriteText("wb_size"      ).WriteText(str).WriteText(iss)
                                      .WriteText(NStr::UInt8ToString_DataSize( m_EndState.wb_size)).WriteText("\"");
    task.WriteText(eol).WriteText("wb_size"      ).WriteText(is ).WriteNumber( m_EndState.wb_size);
    task.WriteText(eol).WriteText("wb_releasable").WriteText(str).WriteText(iss)
                                      .WriteText(NStr::UInt8ToString_DataSize( m_EndState.wb_releasable)).WriteText("\"");
    task.WriteText(eol).WriteText("wb_releasable").WriteText(is ).WriteNumber( m_EndState.wb_releasable);
    task.WriteText(eol).WriteText("wb_releasing" ).WriteText(str).WriteText(iss)
                                      .WriteText(NStr::UInt8ToString_DataSize( m_EndState.wb_releasing)).WriteText("\"");
    task.WriteText(eol).WriteText("wb_releasing" ).WriteText(is ).WriteNumber( m_EndState.wb_releasing);
    task.WriteText(eol).WriteText("progress_cmds").WriteText(is ).WriteNumber( m_EndState.progress_cmds);
    m_SrvStat->PrintState(task);
}

void
CNCStat::PrintToSocket(CSrvSocketTask* sock)
{
    m_StatLock.Lock();
    CSrvPrintProxy proxy(sock);
    char buf[50];
    CSrvTime t;

    proxy << "Stat type - " << m_StatName;
    m_StartState.state_time.Print(buf, CSrvTime::eFmtHumanUSecs);
    proxy << ", start " << buf;
    m_EndState.state_time.Print(buf, CSrvTime::eFmtHumanUSecs);
    proxy << ", end " << buf <<endl;
    proxy << "PID - " <<  (Uint8)CProcess::GetCurrentPid() << endl;
    if (CNCBlobStorage::IsDraining()) {
       proxy << "Draining started" << endl;
    }
    {
        CMiniMutexGuard guard(s_CommonStatLock);
        Uint8 now = CSrvTime::Current().AsUSec();
        if (!s_SyncSrv.empty()) {
            vector<string> t;
            ITERATE(set<Uint8>, s, s_SyncSrv) {
                t.push_back(CNCDistributionConf::GetPeerName(*s));
            }
            proxy << "Sync servers - " <<  NStr::Join(t,",") << endl;
            ITERATE(TSyncTimes, s, s_SyncSucceeded) {
                s->second.Print(buf, CSrvTime::eFmtHumanUSecs);
                proxy << "Initial Sync succeeded - " <<
                    CNCDistributionConf::GetPeerName(s->first) << " at " << buf << endl;
            }
            if (!s_SyncFailed.empty()) {
                ITERATE(TSyncTimes, s, s_SyncFailed) {
                    s->second.Print(buf, CSrvTime::eFmtHumanUSecs);
                    proxy << "Initial Sync failed    - " <<
                        CNCDistributionConf::GetPeerName(s->first) << " at " << buf << endl;
                }
            }
            ITERATE(TSyncTimes, s, s_SyncPeriodic) {
                Uint8 agoSec = (now - s->second.AsUSec())/(kUSecsPerMSec*kMSecsPerSecond);
                Uint8 agoUsec = (now - s->second.AsUSec())%(kUSecsPerMSec*kMSecsPerSecond);
                s->second.Print(buf, CSrvTime::eFmtHumanUSecs);
                proxy << "Periodic Sync          - " <<
                    CNCDistributionConf::GetPeerName(s->first) << " at " << buf <<
                    ", " << agoSec << "." << agoUsec << "s ago" << endl;
            }
        }
    }

    CSrvTime time_diff = m_EndState.state_time;
    time_diff -= m_StartState.state_time;
    Uint8 time_secs = time_diff.AsUSec() / kUSecsPerSecond;
    if (time_secs == 0)
        time_secs = 1;

    proxy << "DB start - "
                    << m_StartState.db_files << " files, "
                    << g_ToSizeStr(m_StartState.db_size)
                    << " (garb - " << g_ToSizeStr(m_StartState.db_garb)
                    << ", " << g_CalcStatPct(m_StartState.db_garb, m_StartState.db_size) << "%)";
    if (m_StartState.min_dead_time != 0) {
        t.Sec() = m_StartState.min_dead_time;
        t.Print(buf, CSrvTime::eFmtHumanSeconds);
        proxy << ", dead: " << buf;
    }
    proxy << endl;
    proxy << "DB end - "
                    << m_EndState.db_files << " files, "
                    << g_ToSizeStr(m_EndState.db_size) << " (garb - "
                    << g_ToSizeStr(m_EndState.db_garb) << ", "
                    << g_CalcStatPct(m_EndState.db_garb, m_EndState.db_size) << "%)";
    if (m_EndState.min_dead_time != 0) {
        t.Sec() = m_EndState.min_dead_time;
        t.Print(buf, CSrvTime::eFmtHumanSeconds);
        proxy << ", dead: " << buf;
    }
    proxy << endl;
    proxy << "DB avg - "
                    << m_CntFiles.GetAverage() << " files, "
                    << g_ToSizeStr(m_DBSize.GetAverage()) << " (garb - "
                    << g_ToSizeStr(m_GarbageSize.GetAverage()) << ")" << endl;
    proxy << "DB max - "
                    << m_CntFiles.GetMaximum() << " files, "
                    << g_ToSizeStr(m_DBSize.GetMaximum()) << " (garb - "
                    << g_ToSizeStr(m_GarbageSize.GetMaximum()) << ")" << endl;
    proxy << "Start cache - "
                    << g_ToSmartStr(m_StartState.cnt_blobs) << " blobs, "
                    << g_ToSmartStr(m_StartState.cnt_keys) << " keys" << endl;
    proxy << "End cache - "
                    << g_ToSmartStr(m_EndState.cnt_blobs) << " blobs, "
                    << g_ToSmartStr(m_EndState.cnt_keys) << " keys" << endl;
    proxy << "Avg cache - "
                    << g_ToSmartStr(m_CntBlobs.GetAverage()) << " blobs, "
                    << g_ToSmartStr(m_CntKeys.GetAverage()) << " keys" << endl;
    proxy << "Max cache - "
                    << g_ToSmartStr(m_CntBlobs.GetMaximum()) << " blobs, "
                    << g_ToSmartStr(m_CntKeys.GetMaximum()) << " keys" << endl;
    proxy << "WB start - "
                    << g_ToSizeStr(m_StartState.wb_size) << ", releasable "
                    << g_ToSizeStr(m_StartState.wb_releasable) << ", releasing "
                    << g_ToSizeStr(m_StartState.wb_releasing) << endl;
    proxy << "WB end - "
                    << g_ToSizeStr(m_EndState.wb_size) << ", releasable "
                    << g_ToSizeStr(m_EndState.wb_releasable) << ", releasing "
                    << g_ToSizeStr(m_EndState.wb_releasing) << endl;
    proxy << "WB avg - "
                    << g_ToSizeStr(m_WBMemSize.GetAverage()) << ", releasable "
                    << g_ToSizeStr(m_WBReleasable.GetAverage()) << ", releasing "
                    << g_ToSizeStr(m_WBReleasing.GetAverage()) << endl;
    proxy << "WB max - "
                    << g_ToSizeStr(m_WBMemSize.GetMaximum()) << ", releasable "
                    << g_ToSizeStr(m_WBReleasable.GetMaximum()) << ", releasing "
                    << g_ToSizeStr(m_WBReleasing.GetMaximum()) << endl;
    proxy << "Start queues - "
                    << g_ToSmartStr(m_StartState.mirror_queue_size) << " mirror, "
                    << g_ToSmartStr(m_StartState.sync_log_size) << " sync log" << endl;
    proxy << "End queues - "
                    << g_ToSmartStr(m_EndState.mirror_queue_size) << " mirror, "
                    << g_ToSmartStr(m_EndState.sync_log_size) << " sync log" << endl;
    proxy << "Avg queues - "
                    << g_ToSmartStr(m_MirrorQSize.GetAverage()) << " mirror, "
                    << g_ToSmartStr(m_SyncLogSize.GetAverage()) << " sync log" << endl;
    proxy << "Max queues - "
                    << g_ToSmartStr(m_MirrorQSize.GetMaximum()) << " blobs, "
                    << g_ToSmartStr(m_SyncLogSize.GetMaximum()) << " sync log" << endl;
    proxy << "Cmds progress - "
                    << m_StartState.progress_cmds << " (start), "
                    << m_EndState.progress_cmds << " (end)" << endl;
    proxy << "Cmds stat - "
                    << g_ToSmartStr(m_StartedCmds) << " (start), "
                    << g_ToSmartStr(m_CmdLens.GetCount()) << " (finish), "
                    << g_ToSmartStr(m_ConnCmds.GetAverage()) << " (avg conn), "
                    << g_ToSmartStr(m_ConnCmds.GetMaximum()) << " (max conn)" << endl;
    proxy << "Client writes - "
                    << g_ToSizeStr(m_ClDataWrite) << ", "
                    << g_ToSizeStr(m_ClDataWrite / time_secs) << "/s, "
                    << g_ToSmartStr(m_ClWrBlobs) << " blobs, "
                    << g_ToSmartStr(m_ClWrBlobs / time_secs) << " blobs/s" << endl;
    proxy << "Client unfinished - "
                    << g_ToSmartStr(m_ClRbackBlobs) << " blobs of "
                    << g_ToSizeStr(m_ClRbackSize) << endl;
    proxy << "Client reads - "
                    << g_ToSizeStr(m_ClDataRead) << ", "
                    << g_ToSizeStr(m_ClDataRead / time_secs) << "/s, "
                    << g_ToSmartStr(m_ClRdBlobs) << " blobs, "
                    << g_ToSmartStr(m_ClRdBlobs / time_secs) << " blobs/s" << endl;
    proxy << "Peer writes - "
                    << g_ToSizeStr(m_PeerDataWrite) << ", "
                    << g_ToSizeStr(m_PeerDataWrite / time_secs) << "/s" << endl;
    proxy << "Peer reads - "
                    << g_ToSizeStr(m_PeerDataRead) << ", "
                    << g_ToSizeStr(m_PeerDataRead / time_secs) << "/s" << endl;
    proxy << "Peer syncs - "
                    << g_ToSmartStr(m_PeerSyncs) << " syncs of "
                    << g_ToSmartStr(m_PeerSynOps) << " ops";
    if (m_PeerSyncs != 0)
        proxy << ", " << double(m_PeerSynOps) / m_PeerSyncs << " ops/sync";
    proxy << endl;
    proxy << "Disk writes - "
                    << g_ToSizeStr(m_DiskDataWrite) << ", "
                    << g_ToSizeStr(m_DiskDataWrite / time_secs) << "/s, "
                    << g_ToSmartStr(m_DiskWrBlobs) << " blobs, "
                    << g_ToSmartStr(m_DiskWrBlobs / time_secs) << " blobs/s" << endl;
    proxy << "Disk reads - "
                    << g_ToSizeStr(m_DiskDataRead) << ", "
                    << g_ToSizeStr(m_DiskDataRead / time_secs) << "/s" << endl;
    proxy << "Shrink check - "
                    << g_ToSmartStr(m_CntCleanedFiles) << " files ("
                    << g_ToSmartStr(m_CntFailedFiles) << " failed), "
                    << g_ToSmartStr(m_CheckedRecs.GetSum()) << " recs ("
                    << g_ToSmartStr(m_CheckedRecs.GetAverage()) << " r/file)" << endl;
    proxy << "Shrink moves - "
                    << g_ToSmartStr(m_MovedRecs.GetSum()) << " recs, "
                    << g_ToSizeStr(m_MovedSize.GetSum()) << " (per file "
                    << g_ToSmartStr(m_MovedRecs.GetAverage()) << " recs, "
                    << g_ToSizeStr(m_MovedSize.GetAverage()) << ")" << endl;
    proxy << endl;

    m_SrvStat->PrintToSocket(proxy);
    x_PrintUnstructured(proxy);

    m_StatLock.Unlock();
}

void
CNCStat::DumpAllStats(void)
{
    if (!s_ThreadStat)
        return;

    CNCStat* last_stat = s_CurPeriodStat[0];
    CNCStat::CollectThreads(last_stat, false);
    last_stat->PrintToLogs(kStatPeriodName[0]);
    for (Uint1 cur_idx = 1; cur_idx < kCntStatPeriods; ++cur_idx) {
        s_CurPeriodStat[cur_idx]->AddAllStats(last_stat);
        s_CurPeriodStat[cur_idx]->PrintToLogs(kStatPeriodName[cur_idx]);
    }
}


static void
s_ShiftStats(Uint1 idx)
{
    CNCStat* prev_stat = s_PrevPeriodStat[idx];
    CNCStat* cur_stat = s_CurPeriodStat[idx];
    CNCStat* new_stat = new CNCStat();
    new_stat->AddReference();
    new_stat->TransferEndState(cur_stat);

    s_RotateLock.Lock();
    s_CurPeriodStat[idx] = new_stat;
    s_PrevPeriodStat[idx] = cur_stat;
    s_RotateLock.Unlock();

    prev_stat->RemoveReference();
}

static void
s_CollectCurStats(void)
{
    CNCStat* last_stat = s_CurPeriodStat[0];
    CNCStat::CollectThreads(last_stat, true);
    s_ShiftStats(0);
    last_stat->PrintToLogs(kStatPeriodName[0]);

    for (Uint1 cur_idx = 1; cur_idx < kCntStatPeriods; ++cur_idx) {
        s_CurPeriodStat[cur_idx]->AddAllStats(last_stat);
    }
    for (Uint1 cur_idx = 1; cur_idx < kCntStatPeriods; ++cur_idx) {
        Uint1 collected = ++s_PeriodsCollected[cur_idx];
        if (cur_idx == kDayPeriodIdx) {
            s_CheckTZChange();
            collected = s_PeriodsCollected[cur_idx];
        }
        if (cur_idx != kLifePeriodIdx  &&  collected == kCollectPeriodsCnt[cur_idx]) {
            s_CurPeriodStat[cur_idx]->PrintToLogs(kStatPeriodName[cur_idx]);
            s_ShiftStats(cur_idx);
            s_PeriodsCollected[cur_idx] = 0;
            if (cur_idx == kMonthPeriodIdx)
                s_SetCurMonthSize();
        }
        else if (cur_idx + 1 != kMonthPeriodIdx)
            break;
    }
}


CStatRotator::CStatRotator(void)
{}

CStatRotator::~CStatRotator(void)
{}

void
CStatRotator::CalcNextRun(void)
{
#ifdef NCBI_OS_LINUX
    time_t cur_secs = CSrvTime::CurSecs();
    struct tm t;
    gmtime_r(&cur_secs, &t);
    RunAfter(kMinStatPeriod - t.tm_sec % kMinStatPeriod);
#endif
}

void
CStatRotator::ExecuteSlice(TSrvThreadNum /* thr_num */)
{
    int cur_secs = CSrvTime::CurSecs();
    if (cur_secs != s_LastRotateSecs) {
        int cnt_iter = (cur_secs - s_LastRotateSecs) / kMinStatPeriod;
        if (cnt_iter == 0)
            cnt_iter = 1;
        for (int i = 0; i < cnt_iter; ++i) {
            if (CTaskServer::IsInShutdown())
                return;

// collect statistics from all threads
// and print it into log.

// there are several types of stat; see  kStatPeriodName above

            s_CollectCurStats();
        }
        s_LastRotateSecs = cur_secs;
    }
    else if (CTaskServer::IsInShutdown())
        return;
// every 5 sec
    CalcNextRun();
}

END_NCBI_SCOPE
