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


CNCDBStat::CNCDBStat(void)
    : m_LockRequests(0),
      m_LocksAcquired(0),
      m_NotExistLocks(0),
      m_GCLockRequests(0),
      m_GCLocksAcquired(0),
      m_LocksTotalTime(0),
      m_LocksWaitedTime(0),
      m_MaxBlobSize(0),
      m_MaxChunkSize(0),
      m_ReadBlobs(0),
      m_ReadSize(0),
      m_StoppedReads(0),
      m_ReadBySize(40, 0),
      m_ChunkRTimeBySize(40),
      m_WrittenBlobs(0),
      m_WrittenSize(0),
      m_StoppedWrites(0),
      m_WrittenBySize(40, 0),
      m_ChunkWTimeBySize(40),
      m_DeletedBlobs(0),
      m_TotalDbTime(0),
      m_TruncatedBlobs(0),
      m_CreateExists(0),
      m_ExistChecks(0)
{
    // No concurrent construction -> no race
    if (s_MinChartSizeShift == 0) {
        s_MinChartSizeShift = x_GetSizeIndex(kMinSizeInChart);
    }
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

inline int
CNCDBStat::x_CalcTimePercent(double time)
{
    return m_LocksTotalTime == 0? 0: int(time / m_LocksTotalTime * 100);
}

void
CNCDBStat::Print(CPrintTextProxy& proxy)
{
    CFastMutexGuard guard(m_ObjMutex);

    proxy << "Database size        - "
                        << m_TotalDBSize.GetAverage() << " avg ("
                        << m_TotalDataSize.GetAverage() << " data, "
                        << m_TotalMetaSize.GetAverage() << " meta), "
                        << m_TotalDBSize.GetMaximum() << " max ("
                        << m_TotalDataSize.GetMaximum() << " data, "
                        << m_TotalMetaSize.GetMaximum() << " meta)" << endl
          << "DB files size        - "
                        << m_DataFileSize.GetAverage() << " avg data, "
                        << m_MetaFileSize.GetAverage() << " avg meta, "
                        << m_DataFileSize.GetMaximum() << " max data, "
                        << m_MetaFileSize.GetMaximum() << " max meta" << endl
          << "Number of db parts   - "
                        << m_NumOfDBParts.GetAverage() << " avg, "
                        << m_NumOfDBParts.GetMaximum() << " max" << endl
          << "Parts ids difference - "
                        << m_DBPartsIdsSpan.GetAverage() << " avg, "
                        << m_DBPartsIdsSpan.GetMaximum() << " max" << endl
          << endl;
    proxy << "Locks                  - "
                               << m_LockRequests << " requested ("
                               << m_GCLockRequests << " GC), "
                               << m_LocksAcquired << " acquired ("
                               << m_GCLocksAcquired << " GC)" << endl
          << "Locks on non-existing  - " << m_NotExistLocks << endl
          << "Time waiting for locks - "
                << x_CalcTimePercent(m_LocksWaitedTime) << "%" << endl
          << "Time of database I/O   - "
                << x_CalcTimePercent(m_TotalDbTime) << "%" << endl
          << endl
          << "Blobs deleted by user - " << m_DeletedBlobs << endl
          << "Blobs truncated       - " << m_TruncatedBlobs << endl
          << "Creates over existing - " << m_CreateExists << endl
          << "Checks for existence  - " << m_ExistChecks << endl
          << endl;
    proxy << "Read data    - " << m_ReadBlobs << " blobs ("
                            << m_StoppedReads << " unfinished) of "
                            << m_ChunkReadTime.GetCount() << " chunks of "
                            << m_ReadSize << " bytes" << endl
          << "Time reading - "
                << x_CalcTimePercent(m_InfoReadTime.GetSum()) << "% for meta ("
                << m_InfoReadTime.GetAverage() << " avg, "
                << m_InfoReadTime.GetMaximum() << " max), "
                << x_CalcTimePercent(m_ChunkReadTime.GetSum()) << "% for data ("
                << m_ChunkReadTime.GetAverage() << " avg, "
                << m_ChunkReadTime.GetMaximum() << " max)" << endl
          << "Blobs read by size:" << endl;
    size_t sz = kMinSizeInChart;
    for (size_t i = 0; i < m_ReadBySize.size(); ++i, sz <<= 1) {
        proxy << sz << " - " << m_ReadBySize[i] << endl;
        if (sz >= m_MaxBlobSize)
            break;
    }
    proxy << "Chunks read by size:" << endl;
    sz = kMinSizeInChart;
    for (size_t i = 0; i < m_ChunkRTimeBySize.size(); ++i, sz <<= 1) {
        proxy << sz << " - " << m_ChunkRTimeBySize[i].GetCount() << " cnt, "
              << x_CalcTimePercent(m_ChunkRTimeBySize[i].GetSum()) << "% total time, "
              << m_ChunkRTimeBySize[i].GetAverage() << " avg time, "
              << m_ChunkRTimeBySize[i].GetMaximum() << " max time" << endl;
        if (sz >= m_MaxChunkSize)
            break;
    }
    proxy << endl
          << "Written data - " << m_WrittenBlobs << " blobs ("
                            << m_StoppedWrites << " unfinished) of "
                            << m_ChunkWriteTime.GetCount() << " chunks of "
                            << m_WrittenSize << " bytes" << endl
          << "Time writing - "
                << x_CalcTimePercent(m_InfoWriteTime.GetSum()) << "% for meta ("
                << m_InfoWriteTime.GetAverage() << " avg, "
                << m_InfoWriteTime.GetMaximum() << " max), "
                << x_CalcTimePercent(m_ChunkWriteTime.GetSum()) << "% for data ("
                << m_ChunkWriteTime.GetAverage() << " avg, "
                << m_ChunkWriteTime.GetMaximum() << " max)" << endl
          << "Blobs written by size:" << endl;
    sz = kMinSizeInChart;
    for (size_t i = 0; i < m_WrittenBySize.size(); ++i, sz <<= 1) {
        proxy << sz << " - " << m_WrittenBySize[i] << endl;
        if (sz >= m_MaxBlobSize)
            break;
    }
    proxy << "Chunks written by size:" << endl;
    sz = kMinSizeInChart;
    for (size_t i = 0; i < m_ChunkWTimeBySize.size(); ++i, sz <<= 1) {
        proxy << sz << " - " << m_ChunkWTimeBySize[i].GetCount() << " cnt, "
              << x_CalcTimePercent(m_ChunkWTimeBySize[i].GetSum()) << "% total time, "
              << m_ChunkWTimeBySize[i].GetAverage() << " avg time, "
              << m_ChunkWTimeBySize[i].GetMaximum() << " max time" << endl;
        if (sz >= m_MaxChunkSize)
            break;
    }
}

END_NCBI_SCOPE
