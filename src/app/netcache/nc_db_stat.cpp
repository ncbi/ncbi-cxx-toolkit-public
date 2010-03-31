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

#include "nc_db_stat.hpp"


BEGIN_NCBI_SCOPE


/// Initial shift of size value that should be done in
/// CNCDBStat::x_GetSizeIndex() to respect kMinSizeInChart properly.
/// Variable is initialized in CNCDBStat constructor.
static unsigned int s_MinChartSizeShift = 0;


inline
SNCDBStatData::SNCDBStatData(void)
    : m_LockRequests(0),
      m_LocksAcquired(0),
      m_NotExistLocks(0),
      m_GCLockRequests(0),
      m_GCLocksAcquired(0),
      m_LocksTotalTime(0),
      m_LocksWaitedTime(0),
      m_MaxBlobSize(0),
      m_MaxChunkSize(0),
      m_BlobsByCntReads(kNCStatMaxCntReads + 1, 0),
      m_MaxCntReads(0),
      m_ReadBlobs(0),
      m_ReadSize(0),
      m_StoppedReads(0),
      m_ReadBySize(40, 0),
      m_WrittenBlobs(0),
      m_WrittenSize(0),
      m_StoppedWrites(0),
      m_WrittenBySize(40, 0),
      m_DeletedBlobs(0),
      m_TotalDbTime(0),
      m_TruncatedBlobs(0),
      m_CreateExists(0),
      m_ExistChecks(0)
{
    m_CountBlobs      .Initialize();
    m_CountCacheNodes .Initialize();
    m_CacheTreeHeight .Initialize();
    m_NumOfDBParts    .Initialize();
    m_DBPartsIdsSpan  .Initialize();
    m_MetaPartSize    .Initialize();
    m_DataPartSize    .Initialize();
    m_MetaFileSize    .Initialize();
    m_DataFileSize    .Initialize();
    m_TotalMetaSize   .Initialize();
    m_TotalDataSize   .Initialize();
    m_TotalDBSize     .Initialize();
    m_FirstReadTime   .Initialize();
    m_SecondReadTime  .Initialize();
    m_InfoReadTime    .Initialize();
    m_ChunkReadTime   .Initialize();
    m_ChunkRTimeBySize.resize(40, m_ChunkReadTime);
    m_InfoWriteTime   .Initialize();
    m_ChunkWriteTime  .Initialize();
    m_ChunkWTimeBySize.resize(40, m_ChunkWriteTime);
}

inline int
SNCDBStatData::CalcTimePercent(double time)
{
    return m_LocksTotalTime == 0? 0: int(time / m_LocksTotalTime * 100);
}

inline int
SNCDBStatData::CalcTimePercent(const CNCStatFigure<double>& time_fig)
{
    return CalcTimePercent(time_fig.GetSum());
}

inline void
SNCDBStatData::CollectTo(SNCDBStatData* dest)
{
    CSpinGuard guard(m_ObjLock);

    dest->m_CountBlobs      .AddValues(m_CountBlobs);
    dest->m_CountCacheNodes .AddValues(m_CountCacheNodes);
    dest->m_CacheTreeHeight .AddValues(m_CacheTreeHeight);
    dest->m_NumOfDBParts    .AddValues(m_NumOfDBParts);
    dest->m_DBPartsIdsSpan  .AddValues(m_DBPartsIdsSpan);
    dest->m_MetaPartSize    .AddValues(m_MetaPartSize);
    dest->m_DataPartSize    .AddValues(m_DataPartSize);
    dest->m_MetaFileSize    .AddValues(m_MetaFileSize);
    dest->m_DataFileSize    .AddValues(m_DataFileSize);
    dest->m_TotalMetaSize   .AddValues(m_TotalMetaSize);
    dest->m_TotalDataSize   .AddValues(m_TotalDataSize);
    dest->m_TotalDBSize     .AddValues(m_TotalDBSize);
    dest->m_LockRequests    += m_LockRequests;
    dest->m_LocksAcquired   += m_LocksAcquired;
    dest->m_NotExistLocks   += m_NotExistLocks;
    dest->m_GCLockRequests  += m_GCLockRequests;
    dest->m_GCLocksAcquired += m_GCLocksAcquired;
    dest->m_LocksTotalTime  += m_LocksTotalTime;
    dest->m_LocksWaitedTime += m_LocksWaitedTime;
    dest->m_MaxBlobSize      = max(dest->m_MaxBlobSize,  m_MaxBlobSize);
    dest->m_MaxChunkSize     = max(dest->m_MaxChunkSize, m_MaxChunkSize);
    for (size_t i = 0; i <= kNCStatMaxCntReads; ++i) {
        dest->m_BlobsByCntReads[i] += m_BlobsByCntReads[i];
    }
    dest->m_MaxCntReads      = max(dest->m_MaxCntReads, m_MaxCntReads);
    dest->m_FirstReadTime   .AddValues(m_FirstReadTime);
    dest->m_SecondReadTime  .AddValues(m_SecondReadTime);
    dest->m_ReadBlobs       += m_ReadBlobs;
    dest->m_ReadSize        += m_ReadSize;
    dest->m_StoppedReads    += m_StoppedReads;
    for (size_t i = 0; i < m_ReadBySize.size(); ++i) {
        dest->m_ReadBySize[i] += m_ReadBySize[i];
    }
    dest->m_InfoReadTime    .AddValues(m_InfoReadTime);
    dest->m_ChunkReadTime   .AddValues(m_ChunkReadTime);
    for (size_t i = 0; i < m_ChunkRTimeBySize.size(); ++i) {
        dest->m_ChunkRTimeBySize[i].AddValues(m_ChunkRTimeBySize[i]);
    }
    dest->m_WrittenBlobs    += m_WrittenBlobs;
    dest->m_WrittenSize     += m_WrittenSize;
    dest->m_StoppedWrites   += m_StoppedWrites;
    for (size_t i = 0; i < m_WrittenBySize.size(); ++i) {
        dest->m_WrittenBySize[i] += m_WrittenBySize[i];
    }
    dest->m_InfoWriteTime   .AddValues(m_InfoWriteTime);
    dest->m_ChunkWriteTime  .AddValues(m_ChunkWriteTime);
    for (size_t i = 0; i < m_ChunkWTimeBySize.size(); ++i) {
        dest->m_ChunkWTimeBySize[i].AddValues(m_ChunkWTimeBySize[i]);
    }
    dest->m_DeletedBlobs    += m_DeletedBlobs;
    dest->m_TotalDbTime     += m_TotalDbTime;
    dest->m_TruncatedBlobs  += m_TruncatedBlobs;
    dest->m_CreateExists    += m_CreateExists;
    dest->m_ExistChecks     += m_ExistChecks;
}


CNCDBStat::CNCDBStat(void)
{
    // No concurrent construction -> no race
    if (s_MinChartSizeShift == 0) {
        s_MinChartSizeShift = x_GetSizeIndex(kMinSizeInChart);
    }

    TBase::Initialize();
}

CNCDBStat::~CNCDBStat(void)
{
    TBase::Finalize();
}

unsigned int
CNCDBStat::x_GetSizeIndex(size_t size)
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

void
CNCDBStat::Print(CPrintTextProxy& proxy)
{
    SNCDBStatData data;
    for (unsigned int i = 0; i < kNCMaxThreadsCnt; ++i) {
        m_Data[i].CollectTo(&data);
    }

    proxy << "Cache    - "
                        << data.m_CountBlobs.GetAverage()      << " avg blobs ("
                        << data.m_CountBlobs.GetMaximum()      << " max), "
                        << data.m_CountCacheNodes.GetAverage() << " avg nodes ("
                        << data.m_CountCacheNodes.GetMaximum() << " max), "
                        << data.m_CacheTreeHeight.GetAverage() << " avg height ("
                        << data.m_CacheTreeHeight.GetMaximum() << " max)" << endl;
    proxy << "DB size  - "
                        << data.m_TotalDBSize.GetAverage()   << " avg ("
                        << data.m_TotalDataSize.GetAverage() << " data, "
                        << data.m_TotalMetaSize.GetAverage() << " meta), "
                        << data.m_TotalDBSize.GetMaximum()   << " max ("
                        << data.m_TotalDataSize.GetMaximum() << " data, "
                        << data.m_TotalMetaSize.GetMaximum() << " meta)" << endl
          << "   Parts - "
                        << data.m_DataPartSize.GetAverage() << " avg data, "
                        << data.m_MetaPartSize.GetAverage() << " avg meta, "
                        << data.m_DataPartSize.GetMaximum() << " max data, "
                        << data.m_MetaPartSize.GetMaximum() << " max meta" << endl
          << "DB files - "
                        << data.m_DataFileSize.GetAverage() << " avg data, "
                        << data.m_MetaFileSize.GetAverage() << " avg meta, "
                        << data.m_DataFileSize.GetMaximum() << " max data, "
                        << data.m_MetaFileSize.GetMaximum() << " max meta" << endl
          << "DB parts - "
                        << data.m_NumOfDBParts.GetAverage()   << " avg, "
                        << data.m_NumOfDBParts.GetMaximum()   << " max, "
                        << data.m_DBPartsIdsSpan.GetAverage() << " avg diff, "
                        << data.m_DBPartsIdsSpan.GetMaximum() << " max diff" << endl;
    proxy << "Locks    - "
                               << data.m_LockRequests << " requested (+"
                               << data.m_GCLockRequests << " GC), "
                               << data.m_LocksAcquired << " acquired (+"
                               << data.m_GCLocksAcquired << " GC)" << endl
          << "I/O      - "
                        << data.CalcTimePercent(data.m_TotalDbTime) << "% (db), "
                        << data.CalcTimePercent(data.m_LocksWaitedTime) << "% (locks), "
                        << data.m_NotExistLocks << " (non-exist), "
                        << data.m_DeletedBlobs << " (del), "
                        << data.m_TruncatedBlobs << " (trunc), "
                        << data.m_CreateExists << " (re-wr), "
                        << data.m_ExistChecks << " (check)" << endl
          << endl;
    if (data.m_ReadBlobs == 0) {
        proxy << "Read    - " << data.m_ReadBlobs << " blobs" << endl;
    }
    else {
        proxy << "Read - " << data.m_ReadBlobs << " blobs ("
                           << data.m_StoppedReads << " unfinished) of "
                           << data.m_ChunkReadTime.GetCount() << " chunks of "
                           << data.m_ReadSize << " bytes" << endl
              << "Time - "
                    << data.CalcTimePercent(data.m_InfoReadTime) << "% for meta ("
                    << data.m_InfoReadTime.GetAverage() << " avg, "
                    << data.m_InfoReadTime.GetMaximum() << " max), "
                    << data.CalcTimePercent(data.m_ChunkReadTime) << "% for data ("
                    << data.m_ChunkReadTime.GetAverage() << " avg, "
                    << data.m_ChunkReadTime.GetMaximum() << " max)" << endl
              << "By size:" << endl;
        size_t sz = kMinSizeInChart;
        for (size_t i = 0; i < data.m_ReadBySize.size(); ++i, sz <<= 1) {
            if (data.m_ReadBySize[i] != 0)
                proxy << sz << " - " << data.m_ReadBySize[i] << endl;
            if (sz >= data.m_MaxBlobSize)
                break;
        }
        proxy << "Chunks by size:" << endl;
        sz = kMinSizeInChart;
        for (size_t i = 0; i < data.m_ChunkRTimeBySize.size(); ++i, sz <<= 1) {
            if (data.m_ChunkRTimeBySize[i].GetCount() != 0) {
                proxy << sz << " - " << data.m_ChunkRTimeBySize[i].GetCount() << " cnt, "
                      << data.CalcTimePercent(data.m_ChunkRTimeBySize[i]) << "% total time, "
                      << data.m_ChunkRTimeBySize[i].GetAverage() << " avg time, "
                      << data.m_ChunkRTimeBySize[i].GetMaximum() << " max time" << endl;
            }
            if (sz >= data.m_MaxChunkSize)
                break;
        }
        proxy << endl;
    }
    if (data.m_WrittenBlobs == 0) {
        proxy << "Written - " << data.m_WrittenBlobs << " blobs" << endl;
    }
    else {
        proxy << "Written - " << data.m_WrittenBlobs << " blobs ("
                                   << data.m_StoppedWrites << " unfinished) of "
                                   << data.m_ChunkWriteTime.GetCount() << " chunks of "
                                   << data.m_WrittenSize << " bytes" << endl
              << "Time    - "
                    << data.CalcTimePercent(data.m_InfoWriteTime) << "% for meta ("
                    << data.m_InfoWriteTime.GetAverage() << " avg, "
                    << data.m_InfoWriteTime.GetMaximum() << " max), "
                    << data.CalcTimePercent(data.m_ChunkWriteTime) << "% for data ("
                    << data.m_ChunkWriteTime.GetAverage() << " avg, "
                    << data.m_ChunkWriteTime.GetMaximum() << " max)" << endl
              << "Read cnt: ";
        for (size_t i = 0; i < kNCStatMaxCntReads; ++i) {
            proxy << i << " - " << data.m_BlobsByCntReads[i] << ", ";
        }
        proxy << int(kNCStatMaxCntReads) << " or more - "
                    << data.m_BlobsByCntReads[kNCStatMaxCntReads] << endl
              << "          max cnt: " << data.m_MaxCntReads << "; 1st: "
                           << data.m_FirstReadTime.GetAverage() << " avg, "
                           << data.m_FirstReadTime.GetMaximum() << " max; 2nd: "
                           << data.m_SecondReadTime.GetAverage() << " avg, "
                           << data.m_SecondReadTime.GetMaximum() << " max" << endl
              << "By size:" << endl;
        size_t sz = kMinSizeInChart;
        for (size_t i = 0; i < data.m_WrittenBySize.size(); ++i, sz <<= 1) {
            if (data.m_WrittenBySize[i] != 0)
                proxy << sz << " - " << data.m_WrittenBySize[i] << endl;
            if (sz >= data.m_MaxBlobSize)
                break;
        }
        proxy << "Chunks by size:" << endl;
        sz = kMinSizeInChart;
        for (size_t i = 0; i < data.m_ChunkWTimeBySize.size(); ++i, sz <<= 1) {
            if (data.m_ChunkWTimeBySize[i].GetCount()) {
                proxy << sz << " - " << data.m_ChunkWTimeBySize[i].GetCount() << " cnt, "
                      << data.CalcTimePercent(data.m_ChunkWTimeBySize[i]) << "% total time, "
                      << data.m_ChunkWTimeBySize[i].GetAverage() << " avg time, "
                      << data.m_ChunkWTimeBySize[i].GetMaximum() << " max time" << endl;
            }
            if (sz >= data.m_MaxChunkSize)
                break;
        }
    }
}

END_NCBI_SCOPE
