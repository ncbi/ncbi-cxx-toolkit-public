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
 */

#include <ncbi_pch.hpp>

#include "nc_db_stat.hpp"


BEGIN_NCBI_SCOPE


CNCDB_Stat::CNCDB_Stat(void)
    : m_LockRequests(0),
      m_LocksAcquired(0),
      m_NotExistLocks(0),
      m_GCLockRequests(0),
      m_GCLocksAcquired(0),
      m_LocksTotalTime(0),
      m_LocksWaitedTime(0),
      m_MaxBlobSize(0),
      m_ReadBlobs(0),
      m_StoppedReads(0),
      m_ReadBySize(40, 0),
      m_ReadSize(0),
      m_ReadTime(0),
      m_MaxChunkReadTime(0),
      m_ReadChunks(0),
      m_ReadChunksBySize(17, 0),
      m_ChunkRTimeBySize(17, 0),
      m_MaxChunkRTimeBySize(17, 0),
      m_WrittenBlobs(0),
      m_StoppedWrites(0),
      m_WrittenBySize(40, 0),
      m_WrittenSize(0),
      m_WriteTime(0),
      m_MaxChunkWriteTime(0),
      m_WrittenChunks(0),
      m_WrittenChunksBySize(17, 0),
      m_ChunkWTimeBySize(17, 0),
      m_MaxChunkWTimeBySize(17, 0),
      m_DeletedBlobs(0),
      m_TotalDbTime(0),
      m_TruncatedBlobs(0),
      m_CreateExists(0),
      m_ExistChecks(0)
{}

void
CNCDB_Stat::Print(CPrintTextProxy& proxy)
{
    CFastMutexGuard guard(m_ObjMutex);

    proxy << "Locks requested        - " << m_LockRequests << endl
          << "Locks acquired         - " << m_LocksAcquired << endl
          << "Locks on non-existing  - " << m_NotExistLocks << endl
          << "GC locks requested     - " << m_GCLockRequests << endl
          << "GC locks acquired      - " << m_GCLocksAcquired << endl
          << "Time waiting for locks - "
          << int(g_SafeDiv(m_LocksWaitedTime, m_LocksTotalTime) * 100) << "%" << endl
          << "Time of database I/O   - "
          << int(g_SafeDiv(m_TotalDbTime, m_LocksTotalTime) * 100) << "%" << endl
          << endl
          << "Blobs deleted by user - " << m_DeletedBlobs << endl
          << "Blobs truncated       - " << m_TruncatedBlobs << endl
          << "Creates over existing - " << m_CreateExists << endl
          << "Checks for existence  - " << m_ExistChecks << endl
          << endl
          << "Blobs read              - " << m_ReadBlobs << endl
          << "Blob chunks read        - " << m_ReadChunks << endl
          << "Unfinished blob reads   - " << m_StoppedReads << endl
          << "Total size of data read - " << m_ReadSize << endl
          << "Time reading blob data  - "
          << int(g_SafeDiv(m_ReadTime, m_LocksTotalTime) * 100) << "%" << endl
          << "Blobs read by size:" << endl;
    size_t sz = kMinSizeInChart;
    for (size_t i = 0; i < m_ReadBySize.size(); ++i, sz <<= 1) {
        proxy << sz << " - " << m_ReadBySize[i] << endl;
        if (sz >= m_MaxBlobSize)
            break;
    }
    proxy << "Maximum time reading chunk - " << m_MaxChunkReadTime << endl
          << "Average time reading chunk - "
                               << g_SafeDiv(m_ReadTime, m_ReadChunks) << endl
          << "Chunks read by size:" << endl;
    sz = kMinSizeInChart;
    for (size_t i = 0; i < m_ChunkRTimeBySize.size(); ++i, sz <<= 1) {
        proxy << sz << " - " << m_ReadChunksBySize[i] << " (cnt), "
              << int(g_SafeDiv(m_ChunkRTimeBySize[i], m_LocksTotalTime) * 100) << "% (total time), "
              << m_MaxChunkRTimeBySize[i] << " (max time), "
              << g_SafeDiv(m_ChunkRTimeBySize[i], m_ReadChunksBySize[i]) << " (avg time)" << endl;
    }
    proxy << endl
          << "Blobs written              - " << m_WrittenBlobs << endl
          << "Blob chunks written        - " << m_WrittenChunks << endl
          << "Unfinished blob writes     - " << m_StoppedWrites << endl
          << "Total size of data written - " << m_WrittenSize << endl
          << "Time writing blob data     - "
          << int(g_SafeDiv(m_WriteTime, m_LocksTotalTime) * 100) << "%" << endl
          << "Blobs written by size:" << endl;
    sz = kMinSizeInChart;
    for (size_t i = 0; i < m_WrittenBySize.size(); ++i, sz <<= 1) {
        proxy << sz << " - " << m_WrittenBySize[i] << endl;
        if (sz >= m_MaxBlobSize)
            break;
    }
    proxy << "Maximum time writing chunk - " << m_MaxChunkWriteTime << endl
          << "Average time writing chunk - "
                           << g_SafeDiv(m_WriteTime, m_WrittenChunks) << endl
          << "Chunks written by size:" << endl;
    sz = kMinSizeInChart;
    for (size_t i = 0; i < m_ChunkWTimeBySize.size(); ++i, sz <<= 1) {
        proxy << sz << " - " << m_WrittenChunksBySize[i] << " (cnt), "
              << int(g_SafeDiv(m_ChunkWTimeBySize[i], m_LocksTotalTime) * 100) << "% (total time), "
              << m_MaxChunkWTimeBySize[i] << " (max time), "
              << g_SafeDiv(m_ChunkWTimeBySize[i], m_WrittenChunksBySize[i]) << " (avg time)" << endl;
    }
}

END_NCBI_SCOPE
