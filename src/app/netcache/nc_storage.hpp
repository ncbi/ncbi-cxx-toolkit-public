#ifndef NETCACHE__NC_STORAGE__HPP
#define NETCACHE__NC_STORAGE__HPP
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


#include "nc_utils.hpp"
#include "nc_db_info.hpp"


namespace intr = boost::intrusive;


BEGIN_NCBI_SCOPE


class CNCBlobAccessor;
struct SNCStateStat;


struct STimeTable_tag;
struct SKeyMap_tag;

typedef intr::set_base_hook< intr::tag<STimeTable_tag>,
                             intr::optimize_size<true> >    TTimeTableHook;
typedef intr::set_base_hook< intr::tag<SKeyMap_tag>,
                             intr::optimize_size<true> >    TKeyMapHook;


struct SNCCacheData : public TTimeTableHook,
                      public TKeyMapHook,
                      public SNCBlobSummary,
                      public CSrvRCUUser
{
    SNCDataCoord coord;
    string key;
    int saved_dead_time;
    Uint2 time_bucket;
    Uint2 map_size;
    Uint4 chunk_size;
    CAtomicCounter ref_cnt;
    CMiniMutex lock;
    CNCBlobVerManager* ver_mgr;


    SNCCacheData(void);

private:
    SNCCacheData(const SNCCacheData&);
    SNCCacheData& operator= (const SNCCacheData&);

    virtual void ExecuteRCU(void);
};


/// Blob storage for NetCache
class CNCBlobStorage
{
public:
    static bool Initialize(bool do_reinit);
    static void Finalize(void);

    static void WriteSetup(CSrvSocketTask& task);

    static bool IsCleanStart(void);
    static bool NeedStopWrite(void);
    static bool AcceptWritesFromPeers(void);
    static void SetDraining(bool draining);
    static bool IsDraining(void);

    static void PackBlobKey(string*      packed_key,
                            CTempString  cache_name,
                            CTempString  blob_key,
                            CTempString  blob_subkey);
    static void UnpackBlobKey(const string& packed_key,
                              string&       cache_name,
                              string&       key,
                              string&       subkey);
    static string UnpackKeyForLogs(const string& packed_key);
    static string PrintablePassword(const string& pass);
    /// Acquire access to the blob identified by key, subkey and version
    static CNCBlobAccessor* GetBlobAccess(ENCAccessType access,
                                          const string& key,
                                          const string& password,
                                          Uint2         time_bucket);
    static Uint4 GetNewBlobId(void);

    /// Get number of files in the database
    static int GetNDBFiles(void);
    /// Get total size of database for the storage
    static Uint8 GetDBSize(void);
    static Uint8 GetDiskFree(void);
    static Uint8 GetAllowedDBSize(Uint8 free_space);
    static bool IsDBSizeAlert(void);
    static void CheckDiskSpace(void);
    static void MeasureDB(SNCStateStat& state);

    static void GetFullBlobsList(Uint2 slot, TNCBlobSumList& blobs_lst);
    static Uint8 GetMaxSyncLogRecNo(void);
    static void SaveMaxSyncLogRecNo(void);

    static string GetPurgeData(void);
    static void SavePurgeData(void);

public:
    // For internal use only

    /// Get meta information about blob with given coordinates.
    ///
    /// @return
    ///   TRUE if meta information exists in the database and was successfully
    ///   read. FALSE if database is corrupted.
    static bool ReadBlobInfo(SNCBlobVerData* ver_data);
    /// Write meta information about blob into the database.
    static bool WriteBlobInfo(const string& blob_key,
                              SNCBlobVerData* ver_data,
                              SNCChunkMaps* maps,
                              Uint8 cnt_chunks,
                              SNCCacheData* cache_data);
    /// Delete blob from database.
    static void DeleteBlobInfo(const SNCBlobVerData* ver_data,
                               SNCChunkMaps* maps);

    static bool ReadChunkData(SNCBlobVerData* ver_data,
                              SNCChunkMaps* maps,
                              Uint8 chunk_num,
                              char*& buffer,
                              Uint4& buf_size);
    static char* WriteChunkData(SNCBlobVerData* ver_data,
                                SNCChunkMaps* maps,
                                SNCCacheData* cache_data,
                                Uint8 chunk_num,
                                char* buffer,
                                Uint4 buf_size);

    static void ReferenceCacheData(SNCCacheData* data);
    static void ReleaseCacheData(SNCCacheData* data);
    static void ChangeCacheDeadTime(SNCCacheData* cache_data);

private:
    CNCBlobStorage(void);
};



//////////////////////////////////////////////////////////////////////////
//  Inline functions
//////////////////////////////////////////////////////////////////////////

inline
SNCCacheData::SNCCacheData(void)
    : saved_dead_time(0),
      ver_mgr(NULL)
{
    coord.file_id = 0;
    coord.rec_num = 0;
    create_id = 0;
    create_time = create_server = 0;
    dead_time = ver_expire = 0;
    ref_cnt.Set(0);
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_STORAGE__HPP */
