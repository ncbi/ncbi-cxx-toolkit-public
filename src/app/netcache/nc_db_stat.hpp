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
    void AddLockRequest      (void);
    /// Add granted blob lock
    void AddLockAcquired     (double lock_waited_time);
    /// Add blob lock over not existing blob
    void AddNotExistBlobLock (void);
    /// Add incoming request for blob locking from GC
    void AddGCLockRequest    (void);
    /// Add granted blob lock for GC
    void AddGCLockAcquired   (void);
    /// Add time between blob lock request and release
    void AddTotalLockTime    (double add_time);
    /// Add read blob of given size
    void AddBlobRead         (size_t size);
    /// Add read blob chunk of given size
    void AddChunkRead        (size_t size, double add_time);
    /// Add blob reading that was aborted without reading till end
    void AddStoppedRead      (void);
    /// Add written blob of given size
    void AddBlobWritten      (size_t size);
    /// Add written blob chunk of given size
    void AddChunkWritten     (size_t size, double add_time);
    /// Add blob writing that was not finalized
    void AddStoppedWrite     (void);
    /// Add deleted blob
    void AddBlobDelete       (void);
    /// Add truncated blob
    void AddBlobTruncate     (void);
    /// Add collision of trying to create over already existing blob
    void AddCreateHitExisting(void);
    /// Add check for blob existence
    void AddExistenceCheck   (void);

    /// Type of time metrics supported by statistics
    enum ETimeMetric {
        eDbRead,    ///< Time spent reading blob data from database
        eDbWrite,   ///< Time spent writing blob data to database
        eDbOther    ///< Time spent in all other database operations (mostly
                    ///< select statements or access time updates)
    };
    /// Add value for given time metric (value in seconds)
    void AddTimeMetric       (ETimeMetric metric, double add_time);

    /// Print statistics to the given proxy object
    void Print(CPrintTextProxy& proxy);

private:
    enum {
        kMinSizeInChart = 32  ///< Minimum blob size that will be counted for
                              ///< the statistics chart showing distribution
                              ///< of blobs sizes.
    };

    /// Get reference to the chart element for given size
    template <class T>
    T& x_GetSizeElem(vector<T>& chart, size_t size);


    /// Main locking mutex for object
    CFastMutex     m_ObjMutex;

    /// Number of requests to lock blob
    Uint8          m_LockRequests;
    /// Number of blob locks acquired
    Uint8          m_LocksAcquired;
    /// Number of locks acquired for not existing blob
    Uint8          m_NotExistLocks;
    /// Number of requests to lock blob made by GC
    Uint8          m_GCLockRequests;
    /// Number of blob locks acquired by GC
    Uint8          m_GCLocksAcquired;
    /// Total time between blob lock request and release
    double         m_LocksTotalTime;
    /// Time while blob lock was waited to acquire
    double         m_LocksWaitedTime;
    /// Maximum size of blob operated at any moment by storage
    size_t         m_MaxBlobSize;
    /// Number of blobs read from database
    Uint8          m_ReadBlobs;
    /// Number of blobs with reading aborted before blob is finished
    Uint8          m_StoppedReads;
    /// Distribution of number of blobs read by their size interval
    vector<Uint8>  m_ReadBySize;
    /// Total size of data read from database (sum of sizes of all chunks)
    Uint8          m_ReadSize;
    /// Total time spent to read blob chunks
    double         m_ReadTime;
    /// Maximum time spent to read one chunk
    double         m_MaxChunkReadTime;
    /// Number of blob chunks read from database
    Uint8          m_ReadChunks;
    /// Number of blob chunks read depending on chunk size
    vector<int>    m_ReadChunksBySize;
    /// Distribution of time spent reading chunks by their size interval
    vector<double> m_ChunkRTimeBySize;
    /// Maximum time spent reading chunks split by their size interval
    vector<double> m_MaxChunkRTimeBySize;
    /// Number of blobs written to database
    Uint8          m_WrittenBlobs;
    /// Number of blobs with writing aborted before blob is finalized
    Uint8          m_StoppedWrites;
    /// Distribution of number of blobs written by there size interval
    vector<Uint8>  m_WrittenBySize;
    /// Total size of data written to database (sum of sizes of all chunks)
    Uint8          m_WrittenSize;
    /// Total time spent to write blob chunks
    double         m_WriteTime;
    /// Maximum time spent to write one chunk
    double         m_MaxChunkWriteTime;
    /// Number of blob chunks written to database
    Uint8          m_WrittenChunks;
    /// Number of blob chunks written depending on chunk size
    vector<int>    m_WrittenChunksBySize;
    /// Distribution of time spent writing chunks by their size interval
    vector<double> m_ChunkWTimeBySize;
    /// Maximum time spent writing chunks split by their size interval
    vector<double> m_MaxChunkWTimeBySize;
    /// Number of blobs deleted from database
    Uint8          m_DeletedBlobs;
    /// Total time spent on all database operations
    double         m_TotalDbTime;
    /// Number of blobs truncated down to lower size
    Uint8          m_TruncatedBlobs;
    /// Number of create requests that have met already existing blob
    Uint8          m_CreateExists;
    /// Number of checks for blob existence
    Uint8          m_ExistChecks;
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

    /// Get time elapsed since object creation
    double GetElapsed(void);

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
    ++m_LockRequests;
}

inline void
CNCDB_Stat::AddLockAcquired(double lock_waited_time)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_LocksAcquired;
    m_LocksWaitedTime += lock_waited_time;
}

inline void
CNCDB_Stat::AddNotExistBlobLock(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_NotExistLocks;
}

inline void
CNCDB_Stat::AddGCLockRequest(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_GCLockRequests;
}

inline void
CNCDB_Stat::AddGCLockAcquired(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_GCLocksAcquired;
}

inline void
CNCDB_Stat::AddTotalLockTime(double add_time)
{
    CFastMutexGuard guard(m_ObjMutex);
    m_LocksTotalTime += add_time;
}

template <class T>
inline T&
CNCDB_Stat::x_GetSizeElem(vector<T>& chart, size_t size)
{
    size_t ind = 0;
    while (size >= kMinSizeInChart) {
        size >>= 1;
        ++ind;
    }
    return chart[ind];
}

inline void
CNCDB_Stat::AddBlobRead(size_t size)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_ReadBlobs;
    ++x_GetSizeElem(m_ReadBySize, size);
    m_MaxBlobSize = max(m_MaxBlobSize, size);
}

inline void
CNCDB_Stat::AddChunkRead(size_t size, double add_time)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_ReadChunks;
    ++x_GetSizeElem(m_ReadChunksBySize, size);
    m_ReadSize += size;
    x_GetSizeElem(m_ChunkRTimeBySize, size) += add_time;
    m_MaxChunkReadTime  = max(m_MaxChunkReadTime, add_time);
    double& max_by_size = x_GetSizeElem(m_MaxChunkRTimeBySize, size);
    max_by_size         = max(max_by_size, add_time);
}

inline void
CNCDB_Stat::AddStoppedRead(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_StoppedReads;
}

inline void
CNCDB_Stat::AddBlobWritten(size_t size)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_WrittenBlobs;
    ++x_GetSizeElem(m_WrittenBySize, size);
    m_MaxBlobSize = max(m_MaxBlobSize, size);
}

inline void
CNCDB_Stat::AddChunkWritten(size_t size, double add_time)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_WrittenChunks;
    ++x_GetSizeElem(m_WrittenChunksBySize, size);
    m_WrittenSize += size;
    x_GetSizeElem(m_ChunkWTimeBySize, size) += add_time;
    m_MaxChunkWriteTime = max(m_MaxChunkWriteTime, add_time);
    double& max_by_size = x_GetSizeElem(m_MaxChunkWTimeBySize, size);
    max_by_size         = max(max_by_size, add_time);
}

inline void
CNCDB_Stat::AddStoppedWrite(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_StoppedWrites;
}

inline void
CNCDB_Stat::AddBlobDelete(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_DeletedBlobs;
}

inline void
CNCDB_Stat::AddBlobTruncate(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_TruncatedBlobs;
}

inline void
CNCDB_Stat::AddCreateHitExisting(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_CreateExists;
}

inline void
CNCDB_Stat::AddExistenceCheck(void)
{
    CFastMutexGuard guard(m_ObjMutex);
    ++m_ExistChecks;
}

inline void
CNCDB_Stat::AddTimeMetric(ETimeMetric metric, double add_time)
{
    CFastMutexGuard guard(m_ObjMutex);
    switch (metric) {
    case eDbRead:
        m_ReadTime    += add_time;
        m_TotalDbTime += add_time;
        break;
    case eDbWrite:
        m_WriteTime   += add_time;
        m_TotalDbTime += add_time;
        break;
    case eDbOther:
        m_TotalDbTime += add_time;
        break;
    default:
        break;
    }
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

inline double
CNCStatTimer::GetElapsed(void)
{
    return m_Timer.Elapsed();
}

END_NCBI_SCOPE

#endif /* NETCACHE_NC_DB_STAT__HPP */
