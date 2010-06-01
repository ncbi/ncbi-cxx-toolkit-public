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
    ///
    static void AddBlockedOperation(void);

    /// Add measurement for number of blobs in the database
    static void AddCacheMeasurement (unsigned int num_blobs,
                                     unsigned int num_nodes,
                                     unsigned int tree_height);
    /// Add measurement for number of database parts and difference between
    /// highest and lowest ids of database parts.
    static void AddDBMeasurement    (unsigned int num_meta,
                                     unsigned int num_data,
                                     Int8         size_meta,
                                     Int8         size_data,
                                     Uint8        useful_cnt_meta,
                                     Uint8        garbage_cnt_meta,
                                     Uint8        useful_cnt_data,
                                     Uint8        garbage_cnt_data,
                                     Int8         useful_size_data,
                                     Int8         garbage_size_data);
    /// 
    static void AddMetaFileMeasurement(Int8  size,
                                       Uint8 useful_cnt,
                                       Uint8 garbage_cnt);
    /// 
    static void AddDataFileMeasurement(Int8  size,
                                       Uint8 useful_cnt,
                                       Uint8 garbage_cnt,
                                       Int8  useful_size,
                                       Int8  garbage_size);

    /// Register attempt to read the blob
    static void AddBlobReadAttempt  (Int8 cnt_reads, int create_time);
    /// Register cached blob (for the puprose of counting number of reads
    /// requested for each blob)
    static void AddBlobCached       (Int8 cnt_reads);
    /// Add read blob of given size
    static void AddBlobRead         (Uint8 blob_size, Uint8 read_size);
    /// Add read blob chunk of given size
    static void AddChunkRead        (size_t size);
    /// Add written blob of given size
    static void AddBlobWritten      (Uint8 written_size, bool completed);
    /// Add written blob chunk of given size
    static void AddChunkWritten     (size_t size);

    /// Print statistics to the given proxy object
    static void Print(CPrintTextProxy& proxy);

private:
    friend class CNCStat_Getter;


    typedef CNCStatFigure<double>                           TSpanValue;
    typedef map<const char*, TSpanValue, SConstCharCompare> TCmdsSpansMap;
    typedef map<int, TCmdsSpansMap>                         TStatCmdsSpansMap;
    typedef map<int, TSpanValue>                            TConnsSpansMap;


    enum {
        kMinSizeInChart = 32, ///< Minimum blob size that will be counted for
                              ///< the statistics chart showing distribution
                              ///< of blobs sizes. Should be a power of 2.
        kMaxCntReads = 5      ///< 
    };

    /// Get index of chart element for given size
    static unsigned int x_GetSizeIndex(Uint8 size);
    /// Get pointer to element in given span map related to given command name.
    /// If there's no such element then it's created and initialized.
    template <class Map, class Key>
    static typename Map::iterator x_GetSpanFigure(Map& map, Key key);

    /// Constructor initializing all data
    CNCStat(void);
    /// Collect all data from here to another statistics object.
    void x_CollectTo(CNCStat* dest);


    /// Main locking mutex for object
    CSpinLock                       m_ObjLock;

    /// Number of connections opened
    Uint8                           m_OpenedConns;
    /// Number of connections closed because of maximum number of opened
    /// connections limit.
    Uint8                           m_OverflowConns;
    ///
    Uint8                           m_FakeConns;
    /// Sum of times all connections stayed opened
    CNCStatFigure<double>           m_ConnSpan;
    ///
    TConnsSpansMap                  m_ConnSpanByStat;
    ///
    CNCStatFigure<Uint8>            m_NumCmdsPerConn;
    /// Maximum time one command was executed
    CNCStatFigure<double>           m_CmdSpan;
    /// Maximum time of each command type executed
    TStatCmdsSpansMap               m_CmdSpanByCmd;
    /// Number of commands terminated because of command timeout
    Uint8                           m_TimedOutCmds;
    ///
    Uint8                           m_BlockedOps;
    /// Number of blobs in the database
    CNCStatFigure<unsigned int>     m_CountBlobs;
    ///
    CNCStatFigure<unsigned int>     m_CountCacheNodes;
    ///
    CNCStatFigure<unsigned int>     m_CacheTreeHeight;
    /// 
    CNCStatFigure<Uint8>            m_NumOfMetaFiles;
    /// 
    CNCStatFigure<Uint8>            m_NumOfDataFiles;
    /// Total size of all meta files in database
    CNCStatFigure<Int8>             m_TotalMetaSize;
    /// Total size of all data files in database
    CNCStatFigure<Int8>             m_TotalDataSize;
    /// Total size of database
    CNCStatFigure<Int8>             m_TotalDBSize;
    ///
    CNCStatFigure<Uint8>            m_TotalUsefulCntMeta;
    ///
    CNCStatFigure<Uint8>            m_TotalGarbageCntMeta;
    ///
    CNCStatFigure<Uint8>            m_TotalUsefulCntData;
    ///
    CNCStatFigure<Uint8>            m_TotalGarbageCntData;
    ///
    CNCStatFigure<Uint8>            m_TotalUsefulSizeData;
    ///
    CNCStatFigure<Uint8>            m_TotalGarbageSizeData;
    /// Size of individual meta file in database part
    CNCStatFigure<Int8>             m_MetaFileSize;
    ///
    CNCStatFigure<Uint8>            m_MetaFileUsefulCnt;
    ///
    CNCStatFigure<Uint8>            m_MetaFileGarbageCnt;
    /// Size of individual data file in database part
    CNCStatFigure<Int8>             m_DataFileSize;
    ///
    CNCStatFigure<Uint8>            m_DataFileUsefulCnt;
    ///
    CNCStatFigure<Uint8>            m_DataFileGarbageCnt;
    ///
    CNCStatFigure<Uint8>            m_DataFileUsefulSize;
    ///
    CNCStatFigure<Uint8>            m_DataFileGarbageSize;
    /// Maximum size of blob operated at any moment by storage
    Uint8                           m_MaxBlobSize;
    /// Maximum size of blob chunk operated at any moment by storage
    size_t                          m_MaxChunkSize;
    /// Number of blobs that were put into database distributed by number of
    /// reads requested on those blobs
    vector<Uint8>                   m_BlobsByCntReads;
    /// Maximum number of reads ever requested for single blob
    Int8                            m_MaxCntReads;
    /// Time between writing and first reading of the blob
    CNCStatFigure<double>           m_FirstReadTime;
    /// Time between writing and second reading of the blob
    CNCStatFigure<double>           m_SecondReadTime;
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
CNCStat::AddOpenedConnection(void)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_OpenedConns;
    stat->m_ObjLock.Unlock();
}

inline void
CNCStat::SetFakeConnection(void)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    --stat->m_OpenedConns;
    ++stat->m_FakeConns;
    stat->m_ObjLock.Unlock();
}

inline void
CNCStat::AddOverflowConnection(void)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_OverflowConns;
    stat->m_ObjLock.Unlock();
}

inline void
CNCStat::AddTimedOutCommand(void)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_TimedOutCmds;
    stat->m_ObjLock.Unlock();
}

inline void
CNCStat::AddBlockedOperation(void)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    ++stat->m_BlockedOps;
    stat->m_ObjLock.Unlock();
}

inline void
CNCStat::AddCacheMeasurement(unsigned int num_blobs,
                             unsigned int num_nodes,
                             unsigned int tree_height)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    stat->m_CountBlobs.AddValue(num_blobs);
    stat->m_CountCacheNodes.AddValue(num_nodes);
    stat->m_CacheTreeHeight.AddValue(tree_height);
    stat->m_ObjLock.Unlock();
}

inline void
CNCStat::AddDBMeasurement(unsigned int  num_meta,
                          unsigned int  num_data,
                          Int8          size_meta,
                          Int8          size_data,
                          Uint8         useful_cnt_meta,
                          Uint8         garbage_cnt_meta,
                          Uint8         useful_cnt_data,
                          Uint8         garbage_cnt_data,
                          Int8          useful_size_data,
                          Int8          garbage_size_data)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    stat->m_NumOfMetaFiles.AddValue(num_meta);
    stat->m_NumOfDataFiles.AddValue(num_data);
    stat->m_TotalMetaSize.AddValue(size_meta);
    stat->m_TotalDataSize.AddValue(size_data);
    stat->m_TotalDBSize.AddValue(size_meta + size_data);
    stat->m_TotalUsefulCntMeta.AddValue(useful_cnt_meta);
    stat->m_TotalGarbageCntMeta.AddValue(garbage_cnt_meta);
    stat->m_TotalUsefulCntData.AddValue(useful_cnt_data);
    stat->m_TotalGarbageCntData.AddValue(garbage_cnt_data);
    stat->m_TotalUsefulSizeData.AddValue(useful_size_data);
    stat->m_TotalGarbageSizeData.AddValue(garbage_size_data);
    stat->m_ObjLock.Unlock();
}

inline void
CNCStat::AddMetaFileMeasurement(Int8 size, Uint8 useful_cnt, Uint8 garbage_cnt)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    stat->m_MetaFileSize.AddValue(size);
    stat->m_MetaFileUsefulCnt.AddValue(useful_cnt);
    stat->m_MetaFileGarbageCnt.AddValue(garbage_cnt);
    stat->m_ObjLock.Unlock();
}

inline void
CNCStat::AddDataFileMeasurement(Int8  size,
                                Uint8 useful_cnt,
                                Uint8 garbage_cnt,
                                Int8  useful_size,
                                Int8  garbage_size)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    stat->m_DataFileSize.AddValue(size);
    stat->m_DataFileUsefulCnt.AddValue(useful_cnt);
    stat->m_DataFileGarbageCnt.AddValue(garbage_cnt);
    stat->m_DataFileUsefulSize.AddValue(useful_size);
    stat->m_DataFileGarbageSize.AddValue(garbage_size);
    stat->m_ObjLock.Unlock();
}

inline void
CNCStat::AddBlobReadAttempt(Int8 cnt_reads, int create_time)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    stat->m_MaxCntReads = max(stat->m_MaxCntReads, cnt_reads);
    if (cnt_reads <= kMaxCntReads) {
        --stat->m_BlobsByCntReads[int(cnt_reads) - 1];
        ++stat->m_BlobsByCntReads[int(cnt_reads)];
        int after_create = int(time(NULL)) - create_time;
        if (cnt_reads == 1) {
            stat->m_FirstReadTime .AddValue(after_create);
        }
        else if (cnt_reads == 2) {
            stat->m_SecondReadTime.AddValue(after_create);
        }
    }
    stat->m_ObjLock.Unlock();
}

inline void
CNCStat::AddBlobCached(Int8 cnt_reads)
{
    CNCStat* stat = sm_Getter.GetObjPtr();
    stat->m_ObjLock.Lock();
    stat->m_MaxCntReads = max(stat->m_MaxCntReads, cnt_reads);
    int ind = int(min(cnt_reads, Int8(kMaxCntReads)));
    ++stat->m_BlobsByCntReads[ind];
    stat->m_ObjLock.Unlock();
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_STAT__HPP */
