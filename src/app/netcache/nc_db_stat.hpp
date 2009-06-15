#ifndef NETCACHE_NC_DB_STAT__HPP
#define NETCACHE_NC_DB_STAT__HPP
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

#include <corelib/ncbitime.hpp>
#include <corelib/ncbimtx.hpp>

#include "nc_utils.hpp"

#include <vector>


BEGIN_NCBI_SCOPE


/// Object accumulating statistics for NetCache storage
class CNCDB_Stat
{
public:
    CNCDB_Stat(void);

    /// Add incoming request for blob locking
    void AddLockRequest (void);
    /// Add granted blob lock
    void AddLockAcquired(double lock_waited_time);
    /// Add read blob of given size
    void AddBlobRead    (size_t size);
    /// Add read blob chunk of given size
    void AddChunkRead   (size_t size);
    /// Add blob reading that was aborted without reading till end
    void AddStoppedRead (void);
    /// Add written blob of given size
    void AddBlobWritten (size_t size);
    /// Add written blob chunk of given size
    void AddChunkWritten(size_t size);
    /// Add blob writing that was not finalized
    void AddStoppedWrite(void);
    /// Add deleted blob
    void AddBlobDelete  (void);

    /// Type of time metrics supported by statistics
    enum ETimeMetric {
        eLockWait,  ///< Time while blob lock was waited for
        eLockHeld,  ///< Time since blob lock was acquired till released
        eDbRead,    ///< Time spent reading from database
        eDbWrite,   ///< Time spent writing to database
        eDbDelete,  ///< Time spent deleting blobs
        eDbOther    ///< Time spent in all other database operations (mostly
                    ///< select statements or access time updates)
    };

    /// Add value for given time metric (value in seconds)
    void AddTimeMetric    (ETimeMetric metric, double add_time);
    /// Add time between blob lock was acquired till it was released
    void AddLockHeldTime  (double add_time);

    /// Print statistics to the given proxy object
    void Print(CPrintTextProxy& proxy);

private:
    enum {
        kMinSizeInChart = 32  ///< Minimum blob size that will be counted for
                              ///< the statistics chart showing distribution
                              ///< of blobs sizes.
    };

    /// Get reference to the chart element for given blob size
    Uint8& x_GetSizeElem(vector<Uint8>& chart, size_t size);


    /// Main locking mutex for object
    CFastMutex     m_ObjMutex;

    /// Timer of total time statistics was gathered
    CStopWatch     m_TotalTimer;
    /// Number of requests to lock blob
    Uint8          m_LockRequests;
    /// Number of blob locks acquired
    Uint8          m_LocksAcquired;
    /// Time while blob lock was waited for
    double         m_LocksWaitedTime;
    /// Time between blob lock was acquired till it was released
    double         m_LocksHeldTime;
    /// Number of blobs read from database
    Uint8          m_ReadBlobs;
    /// Number of blobs with reading aborted before blob is finished
    Uint8          m_StoppedReads;
    /// Distribution of number of blobs read by there size interval
    vector<Uint8>  m_ReadBySize;
    /// Number of blob chunks read from database
    Uint8          m_ReadChunks;
    /// Total size of data read from database (sum of sizes of all chunks)
    Uint8          m_ReadSize;
    /// Total time spent to read blob chunks
    double         m_ReadTime;
    /// Number of blobs written to database
    Uint8          m_WrittenBlobs;
    /// Number of blobs with writing aborted before blob is finalized
    Uint8          m_StoppedWrites;
    /// Distribution of number of blobs written by there size interval
    vector<Uint8>  m_WrittenBySize;
    /// Number of blob chunks written to database
    Uint8          m_WrittenChunks;
    /// Total size of data written to database (sum of sizes of all chunks)
    Uint8          m_WrittenSize;
    /// Total time spent to write blob chunks
    double         m_WriteTime;
    /// Number of blobs deleted from database
    Uint8          m_DeletedBlobs;
    /// Total time spent to delete blobs
    double         m_DeleteTime;
    /// Total time spent on all database operations
    double         m_TotalDbTime;
};


/// Object to measure time consumed for some operations worth remembering in
/// storage statistics. Timer automatically starts at object creation and
/// stops at object destruction. Time spent is automatically remembered
/// during object destruction.
class CNCStatTimer
{
public:
    /// Create timer for given object gathering statistics and for given time
    /// metric.
    CNCStatTimer(CNCDB_Stat* stat, CNCDB_Stat::ETimeMetric metric);

    ~CNCStatTimer(void);

private:
    /// Object gathering statistics
    CNCDB_Stat*             m_Stat;
    /// Metric to measure by this object
    CNCDB_Stat::ETimeMetric m_Metric;
    /// Timer ticking
    CStopWatch              m_Timer;
};



inline void
CNCDB_Stat::AddLockRequest(void)
{
    CFastMutexGuard guard(m_ObjMutex);

    m_LockRequests += 1;
}

inline void
CNCDB_Stat::AddLockAcquired(double lock_waited_time)
{
    CFastMutexGuard guard(m_ObjMutex);

    m_LocksAcquired += 1;
    m_LocksWaitedTime += lock_waited_time;
}

inline Uint8&
CNCDB_Stat::x_GetSizeElem(vector<Uint8>& chart, size_t size)
{
    size_t ind = 0;
    while (size >= kMinSizeInChart) {
        size >>= 1;
        ++ind;
    }
    if (chart.size() <= ind) {
        chart.resize(ind + 1, 0);
    }
    return chart[ind];
}

inline void
CNCDB_Stat::AddBlobRead(size_t size)
{
    CFastMutexGuard guard(m_ObjMutex);

    m_ReadBlobs += 1;
    ++x_GetSizeElem(m_ReadBySize, size);
}

inline void
CNCDB_Stat::AddChunkRead(size_t size)
{
    CFastMutexGuard guard(m_ObjMutex);

    m_ReadChunks += 1;
    m_ReadSize   += size;
}

inline void
CNCDB_Stat::AddStoppedRead(void)
{
    CFastMutexGuard guard(m_ObjMutex);

    m_StoppedReads += 1;
}

inline void
CNCDB_Stat::AddBlobWritten(size_t size)
{
    CFastMutexGuard guard(m_ObjMutex);

    m_WrittenBlobs += 1;
    ++x_GetSizeElem(m_WrittenBySize, size);
}

inline void
CNCDB_Stat::AddChunkWritten(size_t size)
{
    CFastMutexGuard guard(m_ObjMutex);

    m_WrittenChunks += 1;
    m_WrittenSize   += size;
}

inline void
CNCDB_Stat::AddStoppedWrite(void)
{
    CFastMutexGuard guard(m_ObjMutex);

    m_StoppedWrites += 1;
}

inline void
CNCDB_Stat::AddBlobDelete(void)
{
    CFastMutexGuard guard(m_ObjMutex);

    m_DeletedBlobs += 1;
}

inline void
CNCDB_Stat::AddTimeMetric(ETimeMetric metric, double add_time)
{
    CFastMutexGuard guard(m_ObjMutex);

    switch (metric) {
    case eLockWait:
        m_LocksWaitedTime += add_time;
        break;
    case eLockHeld:
        m_LocksHeldTime   += add_time;
        break;
    case eDbRead:
        m_ReadTime        += add_time;
        m_TotalDbTime     += add_time;
        break;
    case eDbWrite:
        m_WriteTime       += add_time;
        m_TotalDbTime     += add_time;
        break;
    case eDbDelete:
        m_DeleteTime      += add_time;
        m_TotalDbTime     += add_time;
        break;
    case eDbOther:
        m_TotalDbTime     += add_time;
        break;
    }
}

inline void
CNCDB_Stat::AddLockHeldTime(double add_time)
{
    AddTimeMetric(eLockHeld, add_time);
}



inline
CNCStatTimer::CNCStatTimer(CNCDB_Stat*             stat,
                           CNCDB_Stat::ETimeMetric metric)
    : m_Stat(stat),
      m_Metric(metric),
      m_Timer(CStopWatch::eStart)
{
    _ASSERT(stat);
}

inline
CNCStatTimer::~CNCStatTimer(void)
{
    m_Stat->AddTimeMetric(m_Metric, m_Timer.Elapsed());
}

END_NCBI_SCOPE

#endif /* NETCACHE_NC_DB_STAT__HPP */
