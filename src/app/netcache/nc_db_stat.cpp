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
 *
 */

#include <ncbi_pch.hpp>

#include "nc_db_stat.hpp"


BEGIN_NCBI_SCOPE


CNCDB_Stat::CNCDB_Stat(void)
    : m_TotalTimer(CStopWatch::eStart),
      m_LockRequests(0),
      m_LocksAcquired(0),
      m_LocksWaitedTime(0),
      m_LocksHeldTime(0),
      m_ReadBlobs(0),
      m_StoppedReads(0),
      m_ReadChunks(0),
      m_ReadSize(0),
      m_ReadTime(0),
      m_WrittenBlobs(0),
      m_StoppedWrites(0),
      m_WrittenChunks(0),
      m_WrittenSize(0),
      m_WriteTime(0),
      m_DeletedBlobs(0),
      m_DeleteTime(0),
      m_TotalDbTime(0)
{}

void
CNCDB_Stat::Print(CPrintTextProxy& proxy)
{
    proxy << "Total working time             - " << m_TotalTimer.Elapsed() << endl
          << "Lock requests received         - " << m_LockRequests << endl
          << "Locks acquired                 - " << m_LocksAcquired << endl
          << "Time spent waiting for locks   - " << m_LocksWaitedTime << endl
          << "Time locks were held           - " << m_LocksHeldTime << endl
          << "Total time of database io      - " << m_TotalDbTime << endl
          << endl
          << "Number of blobs read           - " << m_ReadBlobs << endl
          << "Number of blob chunks read     - " << m_ReadChunks << endl
          << "Unfinished blob reads          - " << m_StoppedReads << endl
          << "Total size of data read        - " << m_ReadSize << endl
          << "Time spent reading database    - " << m_ReadTime << endl
          << "Number of blobs read by size:" << endl;
    Uint8 sz = kMinSizeInChart;
    for (size_t i = 0; i < m_ReadBySize.size(); ++i, sz <<= 1) {
        proxy << sz << " - " << m_ReadBySize[i] << endl;
    }
    proxy << endl
          << "Number of blobs written        - " << m_WrittenBlobs << endl
          << "Number of blob chunks written  - " << m_WrittenChunks << endl
          << "Unfinished blob writes         - " << m_StoppedWrites << endl
          << "Total size of data written     - " << m_WrittenSize << endl
          << "Time spent writing to database - " << m_WriteTime << endl
          << "Number of blobs written by size:" << endl;
    sz = kMinSizeInChart;
    for (size_t i = 0; i < m_WrittenBySize.size(); ++i, sz <<= 1) {
        proxy << sz << " - " << m_WrittenBySize[i] << endl;
    }
    proxy << endl
          << "Number of blobs deleted        - " << m_DeletedBlobs << endl
          << "Time spent deleting            - " << m_DeleteTime << endl;
}

END_NCBI_SCOPE
