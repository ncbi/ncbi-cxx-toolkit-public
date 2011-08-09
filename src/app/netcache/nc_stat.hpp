#ifndef NETCACHE__NC_STAT__HPP
#define NETCACHE__NC_STAT__HPP
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
#include "nc_db_info.hpp"

#include <vector>


BEGIN_NCBI_SCOPE


class CNCStat;


struct SConstCharCompare
{
    bool operator() (const char* left, const char* right) const;
};


///
class CNCStat_Getter : public CNCTlsObject<CNCStat_Getter, CNCStat>
{
    typedef CNCTlsObject<CNCStat_Getter, CNCStat>  TBase;

public:
    CNCStat_Getter(void);
    ~CNCStat_Getter(void);

    /// Part of interface required by CNCTlsObject<>
    CNCStat* CreateTlsObject(void);
    static void DeleteTlsObject(void* obj_ptr);
};


/// Class collecting statistics about NetCache server.
class CNCStat
{
public:
    typedef CNCStatFigure<double>                           TSpanValue;
    typedef map<const char*, TSpanValue, SConstCharCompare> TCmdsSpansMap;
    typedef map<const char*, Uint8, SConstCharCompare>      TCmdsCountsMap;
    typedef map<int, TCmdsSpansMap>                         TStatCmdsSpansMap;
    typedef map<int, TSpanValue>                            TConnsSpansMap;

    /// Add started command
    ///
    /// @param cmd
    ///   Command name - should be string constant living through all time
    ///   application is running.
    static void AddStartedCmd(const char* cmd);
    /// Add finished command
    ///
    /// @param cmd
    ///   Command name - should be string constant living through all time
    ///   application is running.
    /// @param cmd_span
    ///   Time command was executed
    /// @param cmd_status
    ///   Resultant status code of the command
    static void AddFinishedCmd(const char* cmd,
                               double      cmd_span,
                               int         cmd_status);
    /// Add closed connection
    ///
    /// @param conn_span
    ///   Time which connection stayed opened
    static void AddClosedConnection(double conn_span,
                                    int    conn_status,
                                    Uint8  num_cmds);
    /// Add opened connection
    static void AddOpenedConnection(void);
    /// Remove opened connection (after OnOverflow was called)
    static void SetFakeConnection(void);
    /// Add connection that will be closed because of maximum number of
    /// connections exceeded.
    static void AddOverflowConnection(void);
    /// Add command terminated because of timeout
    static void AddTimedOutCommand(void);
    /// Get number of connections used at the moment
    Uint4 GetConnsUsed(void);
    /// Get number of connections used 1 second ago
    Uint4 GetConnsUsed1Sec(void);
    /// Get number of opened connections during last 1 second
    Uint4 GetConnsOpened1Sec(void);
    /// Get number of closed connections during last 1 second
    Uint4 GetConnsClosed1Sec(void);
    /// Get number of overflowed connections during last 1 second
    Uint4 GetConnsOverflow1Sec(void);
    /// Get number of connections closed with user error during last 1 second
    Uint4 GetUserErrs1Sec(void);
    /// Get number of connections closed with server error during last 1 second
    Uint4 GetServErrs1Sec(void);
    /// Get number of connections used 5 seconds ago
    Uint4 GetConnsUsed5Sec(void);
    /// Get number of opened connections during last 5 seconds
    Uint4 GetConnsOpened5Sec(void);
    /// Get number of closed connections during last 5 seconds
    Uint4 GetConnsClosed5Sec(void);
    /// Get number of overflowed connections during last 5 seconds
    Uint4 GetConnsOverflow5Sec(void);
    /// Get number of connections closed with user error during last 5 seconds
    Uint4 GetUserErrs5Sec(void);
    /// Get number of connections closed with server error during last 5 seconds
    Uint4 GetServErrs5Sec(void);
    /// Get number of commands currently executing
    Uint4 GetProgressCmds(void);
    /// Get number of commands executed 1 second ago
    Uint4 GetProgressCmds1Sec(void);
    /// Get number of commands executed 5 second ago
    Uint4 GetProgressCmds5Sec(void);
    /// Get stat values for number of started commands during last 1 second
    const TCmdsCountsMap& GetStartedCmds1Sec(void);
    /// Get stat values for command execution times during last 1 second
    const TStatCmdsSpansMap& GetCmdSpans1Sec(void);
    /// Get stat values for command execution times during last 5 second
    void GetStartedCmds5Sec(TCmdsCountsMap* cmd_map);
    /// Get stat values for command execution times during last 5 second
    void GetCmdSpans5Sec(TStatCmdsSpansMap* cmd_map);
    /// Get amount of data read during last 1 second
    Uint8 GetReadSize1Sec(void);
    /// Get amount of data written during last 1 second
    Uint8 GetWrittenSize1Sec(void);
    /// Get amount of data read during last 5 second
    Uint8 GetReadSize5Sec(void);
    /// Get amount of data written during last 5 second
    Uint8 GetWrittenSize5Sec(void);

    /// Add measurement for number of database parts and difference between
    /// highest and lowest ids of database parts.
    static void AddDBMeasurement(Uint4 num_files,
                                 Uint8 db_size,
                                 Uint8 garbage,
                                 size_t cache_size,
                                 size_t cache_cnt,
                                 Uint8 cnt_blobs,
                                 Uint8 unfinished_blobs,
                                 Uint4 write_tasks);

    /// Add read blob of given size
    static void AddBlobRead         (Uint8 blob_size, Uint8 read_size);
    /// Add read blob chunk of given size
    static void AddChunkRead        (size_t size);
    /// Add written blob of given size
    static void AddBlobWritten      (Uint8 written_size, bool completed);
    /// Add written blob chunk of given size
    static void AddChunkWritten     (size_t size);

    /// Constructor initializing all data
    CNCStat(void);
    /// Collect statistics from all threads
    static void CollectAllStats(CNCStat& stat);
    /// Print statistics to the given proxy object
    static void Print(CPrintTextProxy& proxy);

private:
    friend class CNCStat_Getter;


    enum {
        kMinSizeInChart = 8,  ///< Minimum blob size that will be counted for
                              ///< the statistics chart showing distribution
                              ///< of blobs sizes. Should be a power of 2.
        kCntHistoryValues = 7
    };

    /// Get index of chart element for given size
    static unsigned int x_GetSizeIndex(Uint8 size);
    /// Get pointer to element in given span map related to given command name.
    /// If there's no such element then it's created and initialized.
    template <class Map, class Key>
    static typename Map::iterator x_GetSpanFigure(Map& map, Key key);

    /// Collect all data from here to another statistics object.
    void x_CollectTo(CNCStat* dest);
    /// Shift history values as next second came
    void x_ShiftHistory(int cur_time);


    /// Main locking mutex for object
    CSpinLock                       m_ObjLock;

    /// Number of connections opened
    Uint8                           m_OpenedConns;
    /// Number of connections closed because of maximum number of opened
    /// connections limit.
    Uint8                           m_OverflowConns;
    /// Number of connections not sending any data (just connect and disconnect)
    Uint8                           m_FakeConns;
    /// Sum of times all connections stayed opened
    CNCStatFigure<double>           m_ConnSpan;
    ///
    TConnsSpansMap                  m_ConnSpanByStat;
    ///
    CNCStatFigure<Uint8>            m_NumCmdsPerConn;
    /// Number of started commands
    Uint8                           m_StartedCmds;
    /// Maximum time one command was executed
    CNCStatFigure<double>           m_CmdSpan;
    /// Maximum time of each command type executed
    TStatCmdsSpansMap               m_CmdSpanByCmd;
    /// Number of commands terminated because of command timeout
    Uint8                           m_TimedOutCmds;
    /// Time for which history values were measured
    int                   m_HistoryTimes[kCntHistoryValues];
    /// Number of open connections in each second
    Uint4                 m_HistOpenedConns[kCntHistoryValues];
    /// Number of closed connections in each second
    Uint4                 m_HistClosedConns[kCntHistoryValues];
    /// Number of used connections at the beginning of each second
    Uint4                 m_HistUsedConns[kCntHistoryValues];
    /// Number of overflowed connections in each second
    Uint4                 m_HistOverflowConns[kCntHistoryValues];
    /// Number of user errors in each second
    Uint4                 m_HistUserErrs[kCntHistoryValues];
    /// Number of server errors in each second
    Uint4                 m_HistServErrs[kCntHistoryValues];
    /// Number of started commands per command type in each second
    TCmdsCountsMap        m_HistStartedCmds[kCntHistoryValues];
    /// Time of command execution for each command type and result status in each second
    TStatCmdsSpansMap     m_HistCmdSpan[kCntHistoryValues];
    /// Number of executing commands at the beginning of each second
    Uint4                 m_HistProgressCmds[kCntHistoryValues];
    /// Size of data read from database in each second
    Uint8                 m_HistReadSize[kCntHistoryValues];
    /// Size of data written to database in each second
    Uint8                 m_HistWriteSize[kCntHistoryValues];
    /// Number of blobs in the database
    CNCStatFigure<Uint8>  m_CntBlobs;
    CNCStatFigure<Uint8>  m_UnfinishedBlobs;
    CNCStatFigure<Uint4>  m_NumOfFiles;
    /// Total size of database
    CNCStatFigure<Uint8>  m_DBSize;
    CNCStatFigure<Uint8>  m_GarbageSize;
    CNCStatFigure<size_t> m_CacheSize;
    CNCStatFigure<size_t> m_CacheCnt;
    CNCStatFigure<Uint4>  m_CntWriteTasks;
    /// Maximum size of blob operated at any moment by storage
    Uint8                           m_MaxBlobSize;
    /// Maximum size of blob chunk operated at any moment by storage
    size_t                          m_MaxChunkSize;
    /// Number of blobs read from database
    Uint8                           m_ReadBlobs;
    ///
    Uint8                           m_ReadChunks;
    /// Total size of data read from database (sum of sizes of all chunks)
    Uint8                           m_DBReadSize;
    ///
    Uint8                           m_ClientReadSize;
    /// Number of blobs with reading aborted before blob is finished
    Uint8                           m_PartialReads;
    /// Distribution of number of blobs read by their size interval
    vector<Uint8>                   m_ReadBySize;
    /// Distribution of time spent reading chunks by their size interval
    vector<Uint8>                   m_ChunksRCntBySize;
    /// Number of blobs written to database
    Uint8                           m_WrittenBlobs;
    ///
    Uint8                           m_WrittenChunks;
    /// Total size of data written to database (sum of sizes of all chunks)
    Uint8                           m_WrittenSize;
    /// Number of blobs with writing aborted before blob is finalized
    Uint8                           m_PartialWrites;
    /// Distribution of number of blobs written by there size interval
    vector<Uint8>                   m_WrittenBySize;
    /// Distribution of time spent writing chunks by their size interval
    vector<Uint8>                   m_ChunksWCntBySize;

    /// Object differentiating statistics instances over threads
    static CNCStat_Getter sm_Getter;
    /// All instances of statistics used in application
    static CNCStat        sm_Instances[kNCMaxThreadsCnt];
};



inline void
CNCStat::AddTimedOutCommand(void)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_TimedOutCmds;
    stat->m_ObjLock.Unlock();
}

inline Uint4
CNCStat::GetConnsUsed(void)
{
    return Uint4(m_OpenedConns - m_ConnSpan.GetCount());
}

inline Uint4
CNCStat::GetProgressCmds(void)
{
    return Uint4(m_StartedCmds - m_CmdSpan.GetCount());
}

inline Uint4
CNCStat::GetConnsUsed1Sec(void)
{
    return m_HistUsedConns[1];
}

inline Uint4
CNCStat::GetConnsOpened1Sec(void)
{
    return m_HistOpenedConns[1];
}

inline Uint4
CNCStat::GetConnsClosed1Sec(void)
{
    return m_HistClosedConns[1];
}

inline Uint4
CNCStat::GetConnsOverflow1Sec(void)
{
    return m_HistOverflowConns[1];
}

inline Uint4
CNCStat::GetUserErrs1Sec(void)
{
    return m_HistUserErrs[1];
}

inline Uint4
CNCStat::GetServErrs1Sec(void)
{
    return m_HistServErrs[1];
}

inline Uint4
CNCStat::GetProgressCmds1Sec(void)
{
    return m_HistProgressCmds[1];
}

inline const CNCStat::TCmdsCountsMap&
CNCStat::GetStartedCmds1Sec(void)
{
    return m_HistStartedCmds[1];
}
inline const CNCStat::TStatCmdsSpansMap&
CNCStat::GetCmdSpans1Sec(void)
{
    return m_HistCmdSpan[1];
}

inline Uint4
CNCStat::GetConnsUsed5Sec(void)
{
    return m_HistUsedConns[5];
}

inline Uint4
CNCStat::GetConnsOpened5Sec(void)
{
    return m_HistOpenedConns[1] + m_HistOpenedConns[2] + m_HistOpenedConns[3] + m_HistOpenedConns[4] + m_HistOpenedConns[5];
}

inline Uint4
CNCStat::GetConnsClosed5Sec(void)
{
    return m_HistClosedConns[1] + m_HistClosedConns[2] + m_HistClosedConns[3] + m_HistClosedConns[4] + m_HistClosedConns[5];
}

inline Uint4
CNCStat::GetConnsOverflow5Sec(void)
{
    return m_HistOverflowConns[1] + m_HistOverflowConns[2] + m_HistOverflowConns[3] + m_HistOverflowConns[4] + m_HistOverflowConns[5];
}

inline Uint4
CNCStat::GetUserErrs5Sec(void)
{
    return m_HistUserErrs[1] + m_HistUserErrs[2] + m_HistUserErrs[3] + m_HistUserErrs[4] + m_HistUserErrs[5];
}

inline Uint4
CNCStat::GetServErrs5Sec(void)
{
    return m_HistServErrs[1] + m_HistServErrs[2] + m_HistServErrs[3] + m_HistServErrs[4] + m_HistServErrs[5];
}

inline Uint4
CNCStat::GetProgressCmds5Sec(void)
{
    return m_HistProgressCmds[5];
}

inline Uint8
CNCStat::GetReadSize1Sec(void)
{
    return m_HistReadSize[1];
}

inline Uint8
CNCStat::GetWrittenSize1Sec(void)
{
    return m_HistWriteSize[1];
}

inline Uint8
CNCStat::GetReadSize5Sec(void)
{
    return m_HistReadSize[1] + m_HistReadSize[2] + m_HistReadSize[3] + m_HistReadSize[4] + m_HistReadSize[5];
}

inline Uint8
CNCStat::GetWrittenSize5Sec(void)
{
    return m_HistWriteSize[1] + m_HistWriteSize[2] + m_HistWriteSize[3] + m_HistWriteSize[4] + m_HistWriteSize[5];
}

inline void
CNCStat::AddDBMeasurement(Uint4 num_files,
                          Uint8 db_size,
                          Uint8 garbage,
                          size_t cache_size,
                          size_t cache_cnt,
                          Uint8 cnt_blobs,
                          Uint8 unfinished_blobs,
                          Uint4 write_tasks)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    stat->m_NumOfFiles.AddValue(num_files);
    stat->m_DBSize.AddValue(db_size);
    stat->m_GarbageSize.AddValue(garbage);
    stat->m_CacheSize.AddValue(cache_size);
    stat->m_CacheCnt.AddValue(cache_cnt);
    stat->m_CntBlobs.AddValue(cnt_blobs);
    stat->m_UnfinishedBlobs.AddValue(unfinished_blobs);
    stat->m_CntWriteTasks.AddValue(write_tasks);
    stat->m_ObjLock.Unlock();
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_STAT__HPP */
