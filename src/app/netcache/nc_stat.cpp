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


inline unsigned int
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
      m_TimedOutCmds(0),
      m_BlockedOps(0),
      m_MaxBlobSize(0),
      m_MaxChunkSize(0),
      m_BlobsByCntReads(kMaxCntReads + 1, 0),
      m_MaxCntReads(0),
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
    m_CountBlobs      .Initialize();
    m_CountCacheNodes .Initialize();
    m_CacheTreeHeight .Initialize();
    m_NumOfMetaFiles  .Initialize();
    m_NumOfDataFiles  .Initialize();
    m_TotalMetaSize   .Initialize();
    m_TotalDataSize   .Initialize();
    m_TotalDBSize     .Initialize();
    m_TotalUsefulCntMeta.Initialize();
    m_TotalGarbageCntMeta.Initialize();
    m_TotalUsefulCntData.Initialize();
    m_TotalGarbageCntData.Initialize();
    m_TotalUsefulSizeData.Initialize();
    m_TotalGarbageSizeData.Initialize();
    m_MetaFileSize    .Initialize();
    m_MetaFileUsefulCnt.Initialize();
    m_MetaFileGarbageCnt.Initialize();
    m_DataFileSize    .Initialize();
    m_DataFileUsefulCnt.Initialize();
    m_DataFileGarbageCnt.Initialize();
    m_DataFileUsefulSize.Initialize();
    m_DataFileGarbageSize.Initialize();
    m_FirstReadTime   .Initialize();
    m_SecondReadTime  .Initialize();
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
CNCStat::AddFinishedCmd(const char* cmd, double cmd_span, int cmd_status)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();

    stat->m_CmdSpan.AddValue(cmd_span);
    TCmdsSpansMap& cmd_span_map = stat->m_CmdSpanByCmd[cmd_status];
    TCmdsSpansMap::iterator it_span = x_GetSpanFigure(cmd_span_map, cmd);
    it_span->second.AddValue(cmd_span);

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
    stat->m_ObjLock.Unlock();
}

void
CNCStat::AddBlobWritten(Uint8 writen_size, bool completed)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_WrittenBlobs;
    ++stat->m_WrittenBySize[x_GetSizeIndex(writen_size)];
    if (completed) {
        ++stat->m_BlobsByCntReads[0];
    }
    else {
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
    stat->m_ObjLock.Unlock();
}

inline void
CNCStat::x_CollectTo(CNCStat* dest)
{
    CSpinGuard guard(m_ObjLock);

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
    dest->m_BlockedOps      += m_BlockedOps;
    dest->m_CountBlobs      .AddValues(m_CountBlobs);
    dest->m_CountCacheNodes .AddValues(m_CountCacheNodes);
    dest->m_CacheTreeHeight .AddValues(m_CacheTreeHeight);
    dest->m_NumOfMetaFiles  .AddValues(m_NumOfMetaFiles);
    dest->m_NumOfDataFiles  .AddValues(m_NumOfDataFiles);
    dest->m_TotalMetaSize   .AddValues(m_TotalMetaSize);
    dest->m_TotalDataSize   .AddValues(m_TotalDataSize);
    dest->m_TotalDBSize     .AddValues(m_TotalDBSize);
    dest->m_TotalUsefulCntMeta.AddValues(m_TotalUsefulCntMeta);
    dest->m_TotalGarbageCntMeta.AddValues(m_TotalGarbageCntMeta);
    dest->m_TotalUsefulCntData.AddValues(m_TotalUsefulCntData);
    dest->m_TotalGarbageCntData.AddValues(m_TotalGarbageCntData);
    dest->m_TotalUsefulSizeData.AddValues(m_TotalUsefulSizeData);
    dest->m_TotalGarbageSizeData.AddValues(m_TotalGarbageSizeData);
    dest->m_MetaFileSize    .AddValues(m_MetaFileSize);
    dest->m_MetaFileUsefulCnt.AddValues(m_MetaFileUsefulCnt);
    dest->m_MetaFileGarbageCnt.AddValues(m_MetaFileGarbageCnt);
    dest->m_DataFileSize    .AddValues(m_DataFileSize);
    dest->m_DataFileUsefulCnt.AddValues(m_DataFileUsefulCnt);
    dest->m_DataFileGarbageCnt.AddValues(m_DataFileGarbageCnt);
    dest->m_DataFileUsefulSize.AddValues(m_DataFileUsefulSize);
    dest->m_DataFileGarbageSize.AddValues(m_DataFileGarbageSize);
    dest->m_MaxBlobSize      = max(dest->m_MaxBlobSize,  m_MaxBlobSize);
    dest->m_MaxChunkSize     = max(dest->m_MaxChunkSize, m_MaxChunkSize);
    for (size_t i = 0; i <= kMaxCntReads; ++i) {
        dest->m_BlobsByCntReads[i] += m_BlobsByCntReads[i];
    }
    dest->m_MaxCntReads      = max(dest->m_MaxCntReads, m_MaxCntReads);
    dest->m_FirstReadTime   .AddValues(m_FirstReadTime);
    dest->m_SecondReadTime  .AddValues(m_SecondReadTime);
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
CNCStat::Print(CPrintTextProxy& proxy)
{
    CNCStat stat;
    for (unsigned int i = 0; i < kNCMaxThreadsCnt; ++i) {
        sm_Instances[i].x_CollectTo(&stat);
    }

    proxy << "Connections - " << stat.m_OpenedConns           << " (open), "
                              << stat.m_ConnSpan.GetCount()   << " (closed), "
                              << stat.m_OverflowConns         << " (overflow), "
                              << stat.m_FakeConns             << " (fake), "
                              << stat.m_ConnSpan.GetAverage() << " (avg alive), "
                              << stat.m_ConnSpan.GetMaximum() << " (max alive)" << endl
          << "By status:" << endl;
    ITERATE(TConnsSpansMap, it_span, stat.m_ConnSpanByStat) {
        proxy << it_span->first << " - "
                              << it_span->second.GetCount()   << " (cnt), "
                              << it_span->second.GetAverage() << " (avg time), "
                              << it_span->second.GetMaximum() << " (max time)" << endl;
    }
    proxy << "Commands    - " << stat.m_CmdSpan.GetCount()    << " (cnt), "
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
                              << it_span->second.GetCount()   << " (cnt), "
                              << it_span->second.GetAverage() << " (avg time), "
                              << it_span->second.GetMaximum() << " (max time)" << endl;
        }
    }
    proxy << endl
          << "Specs    - "
                        << stat.m_BlockedOps   << " (blocked ops)" << endl
          << "Cache    - "
                        << stat.m_CountBlobs.GetAverage()      << " avg blobs ("
                        << stat.m_CountBlobs.GetMaximum()      << " max), "
                        << stat.m_CountCacheNodes.GetAverage() << " avg nodes ("
                        << stat.m_CountCacheNodes.GetMaximum() << " max), "
                        << stat.m_CacheTreeHeight.GetAverage() << " avg height ("
                        << stat.m_CacheTreeHeight.GetMaximum() << " max)" << endl
          << "DB size  - "
                        << stat.m_TotalDBSize.GetAverage()   << " avg ("
                        << stat.m_TotalMetaSize.GetAverage() << " meta, "
                        << stat.m_TotalDataSize.GetAverage() << " data), "
                        << stat.m_TotalDBSize.GetMaximum()   << " max ("
                        << stat.m_TotalMetaSize.GetMaximum() << " meta, "
                        << stat.m_TotalDataSize.GetMaximum() << " data)" << endl
          << "DB files - "
                        << stat.m_NumOfMetaFiles.GetAverage()   << " avg cnt meta ("
                        << stat.m_NumOfMetaFiles.GetMaximum()   << " max), "
                        << stat.m_MetaFileSize.GetAverage()     << " avg size meta ("
                        << stat.m_MetaFileSize.GetMaximum()     << " max), "
                        << stat.m_NumOfDataFiles.GetAverage()   << " avg cnt data ("
                        << stat.m_NumOfDataFiles.GetMaximum()   << " max), "
                        << stat.m_DataFileSize.GetAverage()     << " avg size data ("
                        << stat.m_DataFileSize.GetMaximum()     << " max)" << endl;
    proxy << "DB meta  - useful: "
                        << stat.m_TotalUsefulCntMeta.GetAverage() << " avg ("
                        << stat.m_TotalUsefulCntMeta.GetMaximum() << " max); garbage: "
                        << stat.m_TotalGarbageCntMeta.GetAverage() << " avg ("
                        << stat.m_TotalGarbageCntMeta.GetMaximum() << " max)" << endl
          << " by file - useful: "
                        << stat.m_MetaFileUsefulCnt.GetAverage() << " avg ("
                        << stat.m_MetaFileUsefulCnt.GetMaximum() << " max); garbage: "
                        << stat.m_MetaFileGarbageCnt.GetAverage() << " avg ("
                        << stat.m_MetaFileGarbageCnt.GetMaximum() << " max)" << endl
          << "DB data  - useful: "
                        << stat.m_TotalUsefulCntData.GetAverage() << " avg cnt ("
                        << stat.m_TotalUsefulCntData.GetMaximum() << " max), "
                        << stat.m_TotalUsefulSizeData.GetAverage() << " avg size ("
                        << stat.m_TotalUsefulSizeData.GetMaximum() << " max); garbage: "
                        << stat.m_TotalGarbageCntData.GetAverage() << " avg cnt ("
                        << stat.m_TotalGarbageCntData.GetMaximum() << " max), "
                        << stat.m_TotalGarbageSizeData.GetAverage() << " avg size ("
                        << stat.m_TotalGarbageSizeData.GetMaximum() << " max)" << endl
          << " by file - useful: "
                        << stat.m_DataFileUsefulCnt.GetAverage() << " avg cnt ("
                        << stat.m_DataFileUsefulCnt.GetMaximum() << " max), "
                        << stat.m_DataFileUsefulSize.GetAverage() << " avg size ("
                        << stat.m_DataFileUsefulSize.GetMaximum() << " max); garbage: "
                        << stat.m_DataFileGarbageCnt.GetAverage() << " avg cnt ("
                        << stat.m_DataFileGarbageCnt.GetMaximum() << " max), "
                        << stat.m_DataFileGarbageSize.GetAverage() << " avg size ("
                        << stat.m_DataFileGarbageSize.GetMaximum() << " max)" << endl
          << endl;
    if (stat.m_DBReadSize == 0) {
        proxy << "Read    - " << stat.m_ReadBlobs << " blobs" << endl;
    }
    else {
        proxy << "Read - " << stat.m_ReadBlobs << " full, "
                           << stat.m_PartialReads << " partial, "
                           << stat.m_ReadChunks << " chunks, "
                           << stat.m_DBReadSize << " db bytes, "
                           << stat.m_ClientReadSize << " client bytes" << endl
              << "By size:" << endl;
        size_t sz = kMinSizeInChart;
        for (size_t i = 0; i < stat.m_ReadBySize.size(); ++i, sz <<= 1) {
            if (stat.m_ReadBySize[i] != 0)
                proxy << sz << " - " << stat.m_ReadBySize[i] << endl;
            if (sz >= stat.m_MaxBlobSize)
                break;
        }
        proxy << "Chunks by size:" << endl;
        sz = kMinSizeInChart;
        for (size_t i = 0; i < stat.m_ChunksRCntBySize.size(); ++i, sz <<= 1) {
            if (stat.m_ChunksRCntBySize[i] != 0) {
                proxy << sz << " - " << stat.m_ChunksRCntBySize[i] << endl;
            }
            if (sz >= stat.m_MaxChunkSize)
                break;
        }
        proxy << endl;
    }
    if (stat.m_WrittenSize == 0) {
        proxy << "Written - " << stat.m_WrittenBlobs << " blobs" << endl;
    }
    else {
        proxy << "Written - " << stat.m_WrittenBlobs << " full, "
                              << stat.m_PartialWrites << " partial, "
                              << stat.m_WrittenChunks << " chunks, "
                              << stat.m_WrittenSize << " bytes" << endl
              << "Read cnt: ";
        for (size_t i = 0; i < kMaxCntReads; ++i) {
            proxy << i << " - " << stat.m_BlobsByCntReads[i] << ", ";
        }
        proxy << int(kMaxCntReads) << " or more - "
                    << stat.m_BlobsByCntReads[kMaxCntReads] << endl
              << "          max cnt: " << stat.m_MaxCntReads << "; 1st: "
                           << stat.m_FirstReadTime.GetAverage() << " avg, "
                           << stat.m_FirstReadTime.GetMaximum() << " max; 2nd: "
                           << stat.m_SecondReadTime.GetAverage() << " avg, "
                           << stat.m_SecondReadTime.GetMaximum() << " max" << endl
              << "By size:" << endl;
        size_t sz = kMinSizeInChart;
        for (size_t i = 0; i < stat.m_WrittenBySize.size(); ++i, sz <<= 1) {
            if (stat.m_WrittenBySize[i] != 0)
                proxy << sz << " - " << stat.m_WrittenBySize[i] << endl;
            if (sz >= stat.m_MaxBlobSize)
                break;
        }
        proxy << "Chunks by size:" << endl;
        sz = kMinSizeInChart;
        for (size_t i = 0; i < stat.m_ChunksWCntBySize.size(); ++i, sz <<= 1) {
            if (stat.m_ChunksWCntBySize[i] != 0) {
                proxy << sz << " - " << stat.m_ChunksWCntBySize[i] << endl;
            }
            if (sz >= stat.m_MaxChunkSize)
                break;
        }
    }
}

END_NCBI_SCOPE
