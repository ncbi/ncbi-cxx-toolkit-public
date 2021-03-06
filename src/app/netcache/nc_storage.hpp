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
class CNCPeerControl;


struct STimeTable_tag;
struct SKeyMap_tag;

typedef intr::set_base_hook< intr::tag<STimeTable_tag>,
                             intr::optimize_size<true> >    TTimeTableHook;
typedef intr::set_base_hook< intr::tag<SKeyMap_tag>,
                             intr::optimize_size<true> >    TKeyMapHook;

#define __NC_CACHEDATA_MONITOR     0

class SNCCacheData : public TTimeTableHook,
                      public TKeyMapHook,
                      public SNCBlobSummary,
                      public CSrvRCUUser
{
public:
    SNCDataCoord coord;
    string key;
    int saved_dead_time;
    Uint2 time_bucket;
    Uint2 map_size;
    Uint4 chunk_size;
    CAtomicCounter_WithAutoInit ref_cnt;
    CMiniMutex lock;

    SNCCacheData(void);
    ~SNCCacheData(void);

    CNCBlobVerManager* Get_ver_mgr(void) const {
        return  ver_mgr;
    }

private:
    CNCBlobVerManager* ver_mgr;

    SNCCacheData(const SNCCacheData&);
    SNCCacheData& operator= (const SNCCacheData&);

#if __NC_CACHEDATA_MONITOR
    void x_Register(void);
    void x_Revoke(void);
#endif

    virtual void ExecuteRCU(void);
    friend class CNCBlobVerManager;
};


/// Blob storage for NetCache
class CNCBlobStorage
{
public:
    static void GetBList(const string& mask, unique_ptr<TNCBufferType>& buffer,
                         SNCBlobFilter* filters, const string& sep);
    static string FindBlob(Uint2 bucket, const string& mask, Uint8 cr_time_hi);

    static bool Initialize(bool do_reinit);
    static void Finalize(void);

    static bool ReConfig(const CNcbiRegistry& new_reg, string& err_message);
    static void WriteSetup(CSrvSocketTask& task);
    static void WriteEnvInfo(CSrvSocketTask& task);
    static void WriteBlobStat(CSrvSocketTask& task);
    static void WriteBlobList(TNCBufferType& buffer, const CTempString& mask);
    static void WriteDbInfo(TNCBufferType& sendBuff, const CTempString& mask);

    static bool IsCleanStart(void);
    static bool NeedStopWrite(void);
    static bool AcceptWritesFromPeers(void);
    static void SetDraining(bool draining);
    static bool IsDraining(void);
    static void AbandonDB(void);
    static bool IsAbandoned(void);

    static string PrintablePassword(const string& pass);
    /// Acquire access to the blob identified by key, subkey and version
    static CNCBlobAccessor* GetBlobAccess(ENCAccessType access,
                                          const string& key,
                                          const string& password,
                                          Uint2         time_bucket);
    static Uint4 GetNewBlobId(void);

    /// Get number of files in the database
    static size_t GetNDBFiles(void);
    /// Get total size of database for the storage
    static Int8 GetDBSize(void);
    static Int8 GetDiskFree(void);
    static Int8 GetAllowedDBSize(Int8 free_space);
    static bool IsDBSizeAlert(void);
    static void CheckDiskSpace(void);
    static void MeasureDB(SNCStateStat& state);

    static int GetLatestBlobExpire(void);
    static void GetFullBlobsList(Uint2 slot, TNCBlobSumList& blobs_lst, const CNCPeerControl* peer);
    static Uint8 GetMaxSyncLogRecNo(void);
    static void SaveMaxSyncLogRecNo(void);

    static string GetPurgeData(void);
    static void SavePurgeData(void);

    static Uint8 GetMaxBlobSizeStore(void);
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

    static void ReferenceCacheData(SNCCacheData* cache_data);
    static void ReleaseCacheData(SNCCacheData* cache_data);
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
      time_bucket(0),
      map_size(0),
      chunk_size(0),
      ver_mgr(NULL)
{
#if __NC_CACHEDATA_MONITOR
    x_Register();
#endif
}

inline
SNCCacheData::~SNCCacheData(void)
{
#ifdef _DEBUG
    if (ver_mgr || !coord.empty() || ref_cnt.Get() != 0) {
        abort();
    }
#endif
#if __NC_CACHEDATA_MONITOR
    x_Revoke();
#endif
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_STORAGE__HPP */
