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

#include <ncbi_pch.hpp>

#include "nc_stat.hpp"


BEGIN_NCBI_SCOPE


/// Initial shift of size value that should be done in
/// CNCDBStat::x_GetSizeIndex() to respect kMinSizeInChart properly.
/// Variable is initialized in CNCDBStat constructor.
static unsigned int s_MinChartSizeShift = 0;


CNCStat_Getter  CNCStat::sm_Getter;
CNCStat         CNCStat::sm_Instances[kNCMaxThreadsCnt];



inline bool
SConstCharCompare::operator() (const char* left, const char* right) const
{
    return NStr::strcmp(left, right) < 0;
}


unsigned int
CNCStat::x_GetSizeIndex(Uint8 size)
{
    if (size <= 1)
        return 0;
    --size;
    unsigned int result = g_GetLogBase2(size);
    if (result >= s_MinChartSizeShift)
        return result - s_MinChartSizeShift;
    else
        return 0;
}


CNCStat_Getter::CNCStat_Getter(void)
{
    // No concurrent construction -> no race
    if (s_MinChartSizeShift == 0) {
        s_MinChartSizeShift = CNCStat::x_GetSizeIndex(CNCStat::kMinSizeInChart);
    }

    TBase::Initialize();
}

CNCStat_Getter::~CNCStat_Getter(void)
{
    TBase::Finalize();
}

CNCStat*
CNCStat_Getter::CreateTlsObject(void)
{
    return &CNCStat::sm_Instances[g_GetNCThreadIndex() % kNCMaxThreadsCnt];
}

void
CNCStat_Getter::DeleteTlsObject(void*)
{}


CNCStat::CNCStat(void)
    : m_OpenedConns(0),
      m_OverflowConns(0),
      m_FakeConns(0),
      m_StartedCmds(0),
      m_TimedOutCmds(0),
      m_MaxBlobSize(0),
      m_MaxChunkSize(0),
      m_ReadBlobs(0),
      m_ReadChunks(0),
      m_DBReadSize(0),
      m_ClientReadSize(0),
      m_PartialReads(0),
      m_ReadBySize(40, 0),
      m_ChunksRCntBySize(40, 0),
      m_WrittenBlobs(0),
      m_WrittenChunks(0),
      m_WrittenSize(0),
      m_PartialWrites(0),
      m_WrittenBySize(40, 0),
      m_ChunksWCntBySize(40, 0)
{
    m_ConnSpan        .Initialize();
    m_CmdSpan         .Initialize();
    m_NumCmdsPerConn  .Initialize();
    memset(m_HistoryTimes, 0, sizeof(m_HistoryTimes));
    memset(m_HistOpenedConns, 0, sizeof(m_HistOpenedConns));
    memset(m_HistClosedConns, 0, sizeof(m_HistClosedConns));
    memset(m_HistUsedConns, 0, sizeof(m_HistUsedConns));
    memset(m_HistOverflowConns, 0, sizeof(m_HistOverflowConns));
    memset(m_HistUserErrs, 0, sizeof(m_HistUserErrs));
    memset(m_HistServErrs, 0, sizeof(m_HistServErrs));
    // m_HistStartedCmds and m_HistCmdSpan are initialized by themselves
    memset(m_HistProgressCmds, 0, sizeof(m_HistProgressCmds));
    memset(m_HistReadSize, 0, sizeof(m_HistReadSize));
    memset(m_HistWriteSize, 0, sizeof(m_HistWriteSize));
    m_CntBlobs      .Initialize();
    m_NumOfFiles    .Initialize();
    m_DBSize        .Initialize();
    m_GarbageSize   .Initialize();
}

template <class Map, class Key>
inline typename Map::iterator
CNCStat::x_GetSpanFigure(Map& map, Key key)
{
    typename Map::iterator it_span = map.find(key);
    if (it_span == map.end()) {
        TSpanValue value;
        value.Initialize();
        it_span = map.insert(typename Map::value_type(key, value)).first;
    }
    return it_span;
}

void
CNCStat::x_ShiftHistory(int cur_time)
{
    Uint4 cnt_conns = Uint4(m_OpenedConns - m_ConnSpan.GetCount());
    Uint4 cnt_cmds  = Uint4(m_StartedCmds - m_CmdSpan.GetCount());
repeat:
    if (m_HistoryTimes[0] != 0) {
        int shift = cur_time - m_HistoryTimes[0];
        if (shift < kCntHistoryValues) {
            memmove(&m_HistoryTimes[shift], &m_HistoryTimes[0], (kCntHistoryValues - shift) * sizeof(m_HistoryTimes[0]));
            memmove(&m_HistOpenedConns[shift], &m_HistOpenedConns[0], (kCntHistoryValues - shift) * sizeof(m_HistOpenedConns[0]));
            memmove(&m_HistClosedConns[shift], &m_HistClosedConns[0], (kCntHistoryValues - shift) * sizeof(m_HistClosedConns[0]));
            memmove(&m_HistUsedConns[shift], &m_HistUsedConns[0], (kCntHistoryValues - shift) * sizeof(m_HistUsedConns[0]));
            memmove(&m_HistOverflowConns[shift], &m_HistOverflowConns[0], (kCntHistoryValues - shift) * sizeof(m_HistOverflowConns[0]));
            memmove(&m_HistUserErrs[shift], &m_HistUserErrs[0], (kCntHistoryValues - shift) * sizeof(m_HistUserErrs[0]));
            memmove(&m_HistServErrs[shift], &m_HistServErrs[0], (kCntHistoryValues - shift) * sizeof(m_HistServErrs[0]));
            for (int i = kCntHistoryValues - 1; i - shift >= 0; --i) {
                m_HistStartedCmds[i].swap(m_HistStartedCmds[i - shift]);
                m_HistCmdSpan[i].swap(m_HistCmdSpan[i - shift]);
            }
            memmove(&m_HistProgressCmds[shift], &m_HistProgressCmds[0], (kCntHistoryValues - shift) * sizeof(m_HistProgressCmds[0]));
            memmove(&m_HistReadSize[shift], &m_HistReadSize[0], (kCntHistoryValues - shift) * sizeof(m_HistReadSize[0]));
            memmove(&m_HistWriteSize[shift], &m_HistWriteSize[0], (kCntHistoryValues - shift) * sizeof(m_HistWriteSize[0]));
        }
        else {
            memset(m_HistoryTimes, 0, sizeof(m_HistoryTimes));
            memset(m_HistOpenedConns, 0, sizeof(m_HistOpenedConns));
            memset(m_HistClosedConns, 0, sizeof(m_HistClosedConns));
            memset(m_HistUsedConns, 0, sizeof(m_HistUsedConns));
            memset(m_HistOverflowConns, 0, sizeof(m_HistOverflowConns));
            memset(m_HistUserErrs, 0, sizeof(m_HistUserErrs));
            memset(m_HistServErrs, 0, sizeof(m_HistServErrs));
            for (int i = 0; i < kCntHistoryValues; ++i) {
                m_HistStartedCmds[i].clear();
                m_HistCmdSpan[i].clear();
            }
            memset(m_HistProgressCmds, 0, sizeof(m_HistProgressCmds));
            memset(m_HistReadSize, 0, sizeof(m_HistReadSize));
            memset(m_HistWriteSize, 0, sizeof(m_HistWriteSize));
            goto repeat;
        }
        for (int i = 0; i < shift; ++i) {
            m_HistoryTimes[i] = cur_time + i;
            m_HistOpenedConns[i] = 0;
            m_HistClosedConns[i] = 0;
            m_HistUsedConns[i] = cnt_conns;
            m_HistOverflowConns[i] = 0;
            m_HistUserErrs[i] = 0;
            m_HistServErrs[i] = 0;
            m_HistStartedCmds[i].clear();
            m_HistCmdSpan[i].clear();
            m_HistProgressCmds[i] = cnt_cmds;
            m_HistReadSize[i] = 0;
            m_HistWriteSize[i] = 0;
        }
    }
    else {
        m_HistoryTimes[0] = cur_time;
        for (int i = 0; i < kCntHistoryValues; ++i) {
            m_HistUsedConns[i] = cnt_conns;
            m_HistProgressCmds[i] = cnt_cmds;
        }
    }
}

void
CNCStat::AddStartedCmd(const char* cmd)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_StartedCmds;
    int cur_time = int(time(NULL));
    if (cur_time != stat->m_HistoryTimes[0])
        stat->x_ShiftHistory(cur_time);
    ++stat->m_HistStartedCmds[0][cmd];
    stat->m_ObjLock.Unlock();
}

void
CNCStat::AddFinishedCmd(const char* cmd, double cmd_span, int cmd_status)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();

    stat->m_CmdSpan.AddValue(cmd_span);
    TCmdsSpansMap* cmd_span_map = &stat->m_CmdSpanByCmd[cmd_status];
    TCmdsSpansMap::iterator it_span = x_GetSpanFigure(*cmd_span_map, cmd);
    it_span->second.AddValue(cmd_span);

    int cur_time = int(time(NULL));
    if (cur_time != stat->m_HistoryTimes[0])
        stat->x_ShiftHistory(cur_time);
    cmd_span_map = &stat->m_HistCmdSpan[0][cmd_status];
    it_span = x_GetSpanFigure(*cmd_span_map, cmd);
    it_span->second.AddValue(cmd_span);

    stat->m_ObjLock.Unlock();
}

void
CNCStat::GetStartedCmds5Sec(TCmdsCountsMap* cmd_map)
{
    cmd_map->clear();
    for (int i = 1; i <= 5; ++i) {
        ITERATE(TCmdsCountsMap, it_count, m_HistStartedCmds[i]) {
            (*cmd_map)[it_count->first] += it_count->second;
        }
    }
}

void
CNCStat::GetCmdSpans5Sec(TStatCmdsSpansMap* cmd_map)
{
    cmd_map->clear();
    for (int i = 1; i <= 5; ++i) {
        ITERATE(TStatCmdsSpansMap, it_stat, m_HistCmdSpan[i]) {
            const TCmdsSpansMap& span_map = it_stat->second;
            TCmdsSpansMap& out_map  = (*cmd_map)[it_stat->first];
            ITERATE(TCmdsSpansMap, it_span, span_map) {
                TCmdsSpansMap::iterator out_span = x_GetSpanFigure(out_map, it_span->first);
                out_span->second.AddValues(it_span->second);
            }
        }
    }
}

void
CNCStat::AddOpenedConnection(void)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_OpenedConns;
    int cur_time = int(time(NULL));
    if (cur_time != stat->m_HistoryTimes[0])
        stat->x_ShiftHistory(cur_time);
    ++stat->m_HistOpenedConns[0];
    stat->m_ObjLock.Unlock();
}

void
CNCStat::SetFakeConnection(void)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    --stat->m_OpenedConns;
    --stat->m_HistOpenedConns[0];
    ++stat->m_FakeConns;
    stat->m_ObjLock.Unlock();
}

void
CNCStat::AddOverflowConnection(void)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_OverflowConns;
    int cur_time = int(time(NULL));
    if (cur_time != stat->m_HistoryTimes[0])
        stat->x_ShiftHistory(cur_time);
    ++stat->m_HistOverflowConns[0];
    stat->m_ObjLock.Unlock();
}

void
CNCStat::AddClosedConnection(double conn_span,
                             int    conn_status,
                             Uint8  num_cmds)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();

    stat->m_ConnSpan.AddValue(conn_span);
    stat->m_NumCmdsPerConn.AddValue(num_cmds);
    TConnsSpansMap::iterator it_span
                            = x_GetSpanFigure(stat->m_ConnSpanByStat, conn_status);
    it_span->second.AddValue(conn_span);

    int cur_time = int(time(NULL));
    if (cur_time != stat->m_HistoryTimes[0])
        stat->x_ShiftHistory(cur_time);
    ++stat->m_HistClosedConns[0];
    if (conn_status >= 400  &&  conn_status < 500)
        ++stat->m_HistUserErrs[0];
    else if (conn_status >= 500)
        ++stat->m_HistServErrs[0];

    stat->m_ObjLock.Unlock();
}

void
CNCStat::AddBlobRead(Uint8 blob_size, Uint8 read_size)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_ReadBlobs;
    stat->m_ClientReadSize += read_size;
    ++stat->m_ReadBySize[x_GetSizeIndex(read_size)];
    if (read_size != blob_size) {
        ++stat->m_PartialReads;
    }
    stat->m_MaxBlobSize = max(stat->m_MaxBlobSize, blob_size);
    stat->m_ObjLock.Unlock();
}

void
CNCStat::AddChunkRead(size_t size)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    stat->m_DBReadSize += size;
    ++stat->m_ReadChunks;
    ++stat->m_ChunksRCntBySize[x_GetSizeIndex(size)];
    stat->m_MaxChunkSize = max(stat->m_MaxChunkSize, size);
    int cur_time = int(time(NULL));
    if (cur_time != stat->m_HistoryTimes[0])
        stat->x_ShiftHistory(cur_time);
    stat->m_HistReadSize[0] += size;
    stat->m_ObjLock.Unlock();
}

void
CNCStat::AddBlobWritten(Uint8 writen_size, bool completed)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_WrittenBlobs;
    ++stat->m_WrittenBySize[x_GetSizeIndex(writen_size)];
    if (!completed) {
        ++stat->m_PartialWrites;
    }
    stat->m_MaxBlobSize = max(stat->m_MaxBlobSize, writen_size);
    stat->m_ObjLock.Unlock();
}

void
CNCStat::AddChunkWritten(size_t size)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    stat->m_WrittenSize += size;
    ++stat->m_WrittenChunks;
    ++stat->m_ChunksWCntBySize[x_GetSizeIndex(size)];
    stat->m_MaxChunkSize = max(stat->m_MaxChunkSize, size);
    int cur_time = int(time(NULL));
    if (cur_time != stat->m_HistoryTimes[0])
        stat->x_ShiftHistory(cur_time);
    stat->m_HistWriteSize[0] += size;
    stat->m_ObjLock.Unlock();
}

void
CNCStat::x_CollectTo(CNCStat* dest)
{
    CSpinGuard guard(m_ObjLock);

    int cur_time = int(time(NULL));
    if (cur_time != m_HistoryTimes[0])
        x_ShiftHistory(cur_time);

    dest->m_OpenedConns     += m_OpenedConns;
    dest->m_OverflowConns   += m_OverflowConns;
    dest->m_FakeConns       += m_FakeConns;
    dest->m_ConnSpan        .AddValues(m_ConnSpan);
    ITERATE(TConnsSpansMap, it_span, m_ConnSpanByStat) {
        TConnsSpansMap::iterator other_span
                        = x_GetSpanFigure(dest->m_ConnSpanByStat, it_span->first);
        other_span->second.AddValues(it_span->second);
    }
    dest->m_NumCmdsPerConn  .AddValues(m_NumCmdsPerConn);
    dest->m_StartedCmds     += m_StartedCmds;
    dest->m_CmdSpan         .AddValues(m_CmdSpan);
    ITERATE(TStatCmdsSpansMap, it_st, m_CmdSpanByCmd) {
        const TCmdsSpansMap& cmd_span_map = it_st->second;
        TCmdsSpansMap& other_cmd_span     = dest->m_CmdSpanByCmd[it_st->first];
        ITERATE(TCmdsSpansMap, it_span, cmd_span_map) {
            TCmdsSpansMap::iterator other_span
                            = x_GetSpanFigure(other_cmd_span, it_span->first);
            other_span->second.AddValues(it_span->second);
        }
    }
    dest->m_TimedOutCmds    += m_TimedOutCmds;
    if (dest->m_HistoryTimes[0] == 0)
        memcpy(dest->m_HistoryTimes, m_HistoryTimes, sizeof(m_HistoryTimes));
    int shift = m_HistoryTimes[0] - dest->m_HistoryTimes[0];
    if (shift < 0)
        shift = 0;
    for (int i = 0; i + shift < kCntHistoryValues; ++i) {
        dest->m_HistOpenedConns[i] += m_HistOpenedConns[i + shift];
        dest->m_HistClosedConns[i] += m_HistClosedConns[i + shift];
        dest->m_HistUsedConns[i] += m_HistUsedConns[i + shift];
        dest->m_HistOverflowConns[i] += m_HistOverflowConns[i + shift];
        dest->m_HistUserErrs[i] += m_HistUserErrs[i + shift];
        dest->m_HistServErrs[i] += m_HistServErrs[i + shift];
        const TCmdsCountsMap& cmd_count_map = m_HistStartedCmds[i + shift];
        TCmdsCountsMap& other_cmd_count     = dest->m_HistStartedCmds[i];
        ITERATE(TCmdsCountsMap, it_count, cmd_count_map) {
            other_cmd_count[it_count->first] += it_count->second;
        }
        ITERATE(TStatCmdsSpansMap, it_st, m_HistCmdSpan[i + shift]) {
            const TCmdsSpansMap& cmd_span_map = it_st->second;
            TCmdsSpansMap& other_cmd_span     = dest->m_HistCmdSpan[i][it_st->first];
            ITERATE(TCmdsSpansMap, it_span, cmd_span_map) {
                TCmdsSpansMap::iterator other_span
                                = x_GetSpanFigure(other_cmd_span, it_span->first);
                other_span->second.AddValues(it_span->second);
            }
        }
        dest->m_HistProgressCmds[i] += m_HistProgressCmds[i + shift];
        dest->m_HistReadSize[i] += m_HistReadSize[i + shift];
        dest->m_HistWriteSize[i] += m_HistWriteSize[i + shift];
    }
    dest->m_CntBlobs        .AddValues(m_CntBlobs);
    dest->m_NumOfFiles      .AddValues(m_NumOfFiles);
    dest->m_DBSize          .AddValues(m_DBSize);
    dest->m_GarbageSize     .AddValues(m_GarbageSize);
    dest->m_MaxBlobSize      = max(dest->m_MaxBlobSize,  m_MaxBlobSize);
    dest->m_MaxChunkSize     = max(dest->m_MaxChunkSize, m_MaxChunkSize);
    dest->m_ReadBlobs       += m_ReadBlobs;
    dest->m_ReadChunks      += m_ReadChunks;
    dest->m_DBReadSize      += m_DBReadSize;
    dest->m_ClientReadSize  += m_ClientReadSize;
    dest->m_PartialReads    += m_PartialReads;
    for (size_t i = 0; i < m_ReadBySize.size(); ++i) {
        dest->m_ReadBySize[i] += m_ReadBySize[i];
    }
    for (size_t i = 0; i < m_ChunksRCntBySize.size(); ++i) {
        dest->m_ChunksRCntBySize[i] += m_ChunksRCntBySize[i];
    }
    dest->m_WrittenBlobs    += m_WrittenBlobs;
    dest->m_WrittenChunks   += m_WrittenChunks;
    dest->m_WrittenSize     += m_WrittenSize;
    dest->m_PartialWrites   += m_PartialWrites;
    for (size_t i = 0; i < m_WrittenBySize.size(); ++i) {
        dest->m_WrittenBySize[i] += m_WrittenBySize[i];
    }
    for (size_t i = 0; i < m_ChunksWCntBySize.size(); ++i) {
        dest->m_ChunksWCntBySize[i] += m_ChunksWCntBySize[i];
    }
}

void
CNCStat::CollectAllStats(CNCStat& stat)
{
    for (unsigned int i = 0; i < kNCMaxThreadsCnt; ++i) {
        sm_Instances[i].x_CollectTo(&stat);
    }
}

void
CNCStat::Print(CPrintTextProxy& proxy)
{
    CNCStat stat;
    CollectAllStats(stat);

    proxy << "Connections - " << (stat.m_OpenedConns
                                  - stat.m_ConnSpan.GetCount()) << " (now open), "
                              << g_ToSmartStr(stat.m_ConnSpan.GetCount()) << " (closed), "
                              << g_ToSmartStr(stat.m_OverflowConns) << " (overflow), "
                              << g_ToSmartStr(stat.m_FakeConns) << " (fake), "
                              << stat.m_ConnSpan.GetAverage() << " (avg alive), "
                              << stat.m_ConnSpan.GetMaximum() << " (max alive)" << endl
          << "By status:" << endl;
    ITERATE(TConnsSpansMap, it_span, stat.m_ConnSpanByStat) {
        proxy << it_span->first << " - "
                              << g_ToSmartStr(it_span->second.GetCount()) << " (cnt), "
                              << it_span->second.GetAverage() << " (avg time), "
                              << it_span->second.GetMaximum() << " (max time)" << endl;
    }
    proxy << "Commands    - " << g_ToSmartStr(stat.m_CmdSpan.GetCount()) << " (cnt), "
                              << stat.m_CmdSpan.GetAverage()  << " (avg time), "
                              << stat.m_CmdSpan.GetMaximum()  << " (max time), "
                              << stat.m_NumCmdsPerConn.GetAverage() << " (avg/conn), "
                              << stat.m_NumCmdsPerConn.GetMaximum() << " (max/conn), "
                              << stat.m_TimedOutCmds          << " (t/o)" << endl
          << "By status and type:" << endl;
    ITERATE(TStatCmdsSpansMap, it_span_st, stat.m_CmdSpanByCmd) {
        proxy << it_span_st->first << ":" << endl;
        ITERATE(TCmdsSpansMap, it_span, it_span_st->second) {
            proxy << it_span->first << " - "
                              << g_ToSmartStr(it_span->second.GetCount()) << " (cnt), "
                              << it_span->second.GetAverage() << " (avg time), "
                              << it_span->second.GetMaximum() << " (max time)" << endl;
        }
    }
    proxy << endl
          << "Storage - "
                        << g_ToSmartStr(stat.m_CntBlobs.GetAverage()) << " avg blobs ("
                        << g_ToSmartStr(stat.m_CntBlobs.GetMaximum()) << " max)" << endl
          << "Disk data - "
                        << g_ToSizeStr(stat.m_DBSize.GetAverage()) << " avg size ("
                        << g_ToSizeStr(stat.m_DBSize.GetMaximum()) << " max), "
                        << stat.m_NumOfFiles.GetAverage()   << " avg files ("
                        << stat.m_NumOfFiles.GetMaximum()   << " max)" << endl
          << "Garbage - "
                        << g_ToSizeStr(stat.m_GarbageSize.GetAverage()) << " avg size ("
                        << g_ToSizeStr(stat.m_GarbageSize.GetMaximum())  << " max),"
                        << (stat.m_DBSize.GetAverage() == 0? 0:
                            stat.m_GarbageSize.GetAverage() * 100 / stat.m_DBSize.GetAverage()) << "% avg ("
                        << (stat.m_DBSize.GetMaximum() == 0? 0:
                            stat.m_GarbageSize.GetMaximum() * 100 / stat.m_DBSize.GetMaximum()) << "% max)"
                        << endl;
    if (stat.m_DBReadSize == 0) {
        proxy << "Read    - " << g_ToSmartStr(stat.m_ReadBlobs) << " blobs" << endl;
    }
    else {
        proxy << "Read - " << g_ToSmartStr(stat.m_ReadBlobs) << " full, "
                           << g_ToSmartStr(stat.m_PartialReads) << " partial, "
                           << g_ToSmartStr(stat.m_ReadChunks) << " chunks, "
                           << g_ToSizeStr(stat.m_DBReadSize) << " from db, "
                           << g_ToSizeStr(stat.m_ClientReadSize) << " for client" << endl
              << "By size:" << endl;
        size_t sz = kMinSizeInChart;
        for (size_t i = 0; i < stat.m_ReadBySize.size(); ++i, sz <<= 1) {
            if (stat.m_ReadBySize[i] != 0)
                proxy << "<=" << sz << " - "
                              << g_ToSmartStr(stat.m_ReadBySize[i]) << endl;
            if (sz >= stat.m_MaxBlobSize)
                break;
        }
        proxy << "Chunks by size:" << endl;
        sz = kMinSizeInChart;
        for (size_t i = 0; i < stat.m_ChunksRCntBySize.size(); ++i, sz <<= 1) {
            if (stat.m_ChunksRCntBySize[i] != 0) {
                proxy << "<=" << sz << " - "
                              << g_ToSmartStr(stat.m_ChunksRCntBySize[i]) << endl;
            }
            if (sz >= stat.m_MaxChunkSize)
                break;
        }
        proxy << endl;
    }
    if (stat.m_WrittenSize == 0) {
        proxy << "Written - " << g_ToSmartStr(stat.m_WrittenBlobs) << " blobs" << endl;
    }
    else {
        proxy << "Written - " << g_ToSmartStr(stat.m_WrittenBlobs) << " full, "
                              << g_ToSmartStr(stat.m_PartialWrites) << " partial, "
                              << g_ToSmartStr(stat.m_WrittenChunks) << " chunks, "
                              << g_ToSizeStr(stat.m_WrittenSize) << " in size" << endl;
        size_t sz = kMinSizeInChart;
        for (size_t i = 0; i < stat.m_WrittenBySize.size(); ++i, sz <<= 1) {
            if (stat.m_WrittenBySize[i] != 0)
                proxy << "<=" << sz << " - "
                              << g_ToSmartStr(stat.m_WrittenBySize[i]) << endl;
            if (sz >= stat.m_MaxBlobSize)
                break;
        }
        proxy << "Chunks by size:" << endl;
        sz = kMinSizeInChart;
        for (size_t i = 0; i < stat.m_ChunksWCntBySize.size(); ++i, sz <<= 1) {
            if (stat.m_ChunksWCntBySize[i] != 0) {
                proxy << "<=" << sz << " - "
                              << g_ToSmartStr(stat.m_ChunksWCntBySize[i]) << endl;
            }
            if (sz >= stat.m_MaxChunkSize)
                break;
        }
    }
}

END_NCBI_SCOPE
