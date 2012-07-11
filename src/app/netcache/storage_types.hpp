#ifndef NETCACHE__STORAGE_TYPES__HPP
#define NETCACHE__STORAGE_TYPES__HPP
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


struct SFileRecHeader;
struct SFileMetaRec;
struct SNCCacheData;
struct SCacheDeadCompare;
struct SCacheKeyCompare;

typedef intr::rbtree<SNCCacheData,
                     intr::base_hook<TTimeTableHook>,
                     intr::constant_time_size<false>,
                     intr::compare<SCacheDeadCompare> >     TTimeTableMap;
typedef intr::rbtree<SNCCacheData,
                     intr::base_hook<TKeyMapHook>,
                     intr::constant_time_size<true>,
                     intr::compare<SCacheKeyCompare> >      TKeyMap;


enum EFileRecType {
    eFileRecNone = 0,
    eFileRecMeta,
    eFileRecChunkMap,
    eFileRecChunkData,
    eFileRecAny
};

struct ATTR_PACKED SFileIndexRec
{
    Uint4   prev_num;
    Uint4   next_num;
    Uint4   offset;
    Uint4   rec_size:24;
    Uint1   rec_type:8;
    SNCDataCoord chain_coord;
    SNCCacheData* cache_data;
};

// Meta-type records (kMetaSignature)
struct ATTR_PACKED SFileMetaRec
{
    Uint1   has_password;
    Uint1   align_reserve;
    Uint2   map_size;       // max number of down_coords in map record - see SFileChunkMapRec
    Uint4   chunk_size;
    Uint8   size;           // blob size
    Uint8   create_time;    // time of completion of blob storage request
    Uint8   create_server;  // unique server id
    Uint4   create_id;      // blob id (unique for each server id)
    Int4    dead_time;      // time when this blob should be deleted from db
    Int4    ttl;            // time to live since last read, or creation
    Int4    expire;         // blob expiration time (blob ceases to exist for client)
    Int4    blob_ver;       // blob version (client supplied)
    Int4    ver_ttl;        // time to live for this version
    Int4    ver_expire;     // blob version expiration time
    char    key_data[1];    // key + MD5 of password, if any
};

// Map-type records (kMapsSignature)
struct ATTR_PACKED SFileChunkMapRec
{
    Uint2 map_idx;         // index of this map in higher level map, if that exists
    Uint1 map_depth;       // map depth; it can be tree of maps
    SNCDataCoord ATTR_ALIGNED_8 down_coords[1];  // coords of lower levels
};

// Data-type records (kDataSignature)
struct ATTR_PACKED SFileChunkDataRec
{
    Uint8   chunk_num;     // chunk index in blob
    Uint2   chunk_idx;     // chunk index in map
    Uint1   chunk_data[1]; // chunk data, see kNCMaxBlobChunkSize
};


struct SCacheDeadCompare
{
    bool operator() (const SNCCacheData& x, const SNCCacheData& y) const
    {
        return x.saved_dead_time < y.saved_dead_time;
    }
};

struct SCacheKeyCompare
{
    bool operator() (const SNCCacheData& x, const SNCCacheData& y) const
    {
        return x.key < y.key;
    }
    bool operator() (const string& key, const SNCCacheData& y) const
    {
        return key < y.key;
    }
    bool operator() (const SNCCacheData& x, const string& key) const
    {
        return x.key < key;
    }
};


struct SWritingInfo
{
    CSrvRef<SNCDBFileInfo> cur_file;
    CSrvRef<SNCDBFileInfo> next_file;
    Uint4 next_rec_num;
    Uint4 next_offset;
    Uint4 left_file_size;
};


struct SNCTempBlobInfo
{
    string  key;
    Uint8   create_time;
    Uint8   create_server;
    SNCDataCoord coord;
    Uint4   create_id;
    int     dead_time;
    int     expire;
    int     ver_expire;

    SNCTempBlobInfo(const SNCCacheData& cache_info)
        : key(cache_info.key),
          create_time(cache_info.create_time),
          create_server(cache_info.create_server),
          coord(cache_info.coord),
          create_id(cache_info.create_id),
          dead_time(cache_info.dead_time),
          expire(cache_info.expire),
          ver_expire(cache_info.ver_expire)
    {
    }
};


typedef set<Uint4>              TRecNumsSet;
typedef map<Uint4, TRecNumsSet> TFileRecsMap;


class CBlobCacher : public CSrvStatesTask<CBlobCacher>
{
public:
    CBlobCacher(void);
    virtual ~CBlobCacher(void);

private:
    State x_StartCaching(void);
    State x_PreCacheRecNums(void);
    State x_CancelCaching(void);
    State x_StartCreateFiles(void);
    State x_CreateInitialFile(void);
    State x_DelFileAndRetryCreate(void);
    State x_StartCacheBlobs(void);
    State x_CacheNextFile(void);
    State x_CacheNextRecord(void);
    State x_CleanOrphanRecs(void);
    State x_FinishCaching(void);

    bool x_CacheMetaRec(SNCDBFileInfo* file_info,
                        SFileIndexRec* ind_rec,
                        SNCDataCoord coord);
    bool x_CacheMapRecs(SNCDataCoord map_coord,
                        Uint1 map_depth,
                        SNCDataCoord up_coord,
                        Uint2 up_index,
                        SNCCacheData* cache_data,
                        Uint8 cnt_chunks,
                        Uint8& chunk_num,
                        map<Uint4, Uint4>& sizes_map);
    void x_DeleteIndexes(SNCDataCoord map_coord, Uint1 map_depth);


    TFileRecsMap m_RecsMap;
    TRecNumsSet m_NewFileIds;
    TNCDBFilesMap::const_iterator m_CurFile;
    TRecNumsSet* m_CurRecsSet;
    TRecNumsSet::iterator m_CurRecIt;
    Uint1 m_CurCreatePass;
    Uint1 m_CurCreateFile;
};


class CNewFileCreator : public CSrvTask
{
public:
    virtual ~CNewFileCreator(void);

private:
    virtual void ExecuteSlice(TSrvThreadNum thr_num);
};


class CDiskFlusher : public CSrvStatesTask<CDiskFlusher>
{
public:
    CDiskFlusher(void);
    virtual ~CDiskFlusher(void);

private:
    State x_CheckFlushTime(void);
    State x_FlushNextFile(void);


    Uint4 m_LastId;
};


class CRecNoSaver : public CSrvTask
{
public:
    virtual ~CRecNoSaver(void);

private:
    virtual void ExecuteSlice(TSrvThreadNum thr_num);
};


class CSpaceShrinker : public CSrvStatesTask<CSpaceShrinker>,
                       public CSrvTransConsumer
{
public:
    CSpaceShrinker(void);
    virtual ~CSpaceShrinker(void);

private:
    State x_PrepareToShrink(void);
    State x_DeleteNextFile(void);
    State x_StartMoves(void);
    State x_MoveNextRecord(void);
    State x_CheckCurVersion(void);
    State x_MoveRecord(void);
    State x_FinishMoveRecord(void);
    State x_FinishMoves(void);
    State x_FinishSession(void);

    SNCDataCoord x_FindMetaCoord(SNCDataCoord coord, Uint1 max_map_depth);


    typedef vector<CSrvRef<SNCDBFileInfo> > TFilesList;

    TFilesList m_FilesToDel;
    TFilesList::iterator m_CurDelFile;
    CSrvRef<SNCDBFileInfo> m_MaxFile;
    SFileIndexRec* m_IndRec;
    SNCCacheData* m_CacheData;
    CNCBlobVerManager* m_VerMgr;
    CSrvRef<SNCBlobVerData> m_CurVer;
    int m_StartTime;
    Uint4 m_RecNum;
    Uint4 m_PrevRecNum;
    Uint4 m_LastAlive;
    Uint4 m_CntProcessed;
    Uint4 m_CntMoved;
    Uint4 m_SizeMoved;
    bool m_Failed;
    bool m_MovingMeta;
};


class CExpiredCleaner : public CSrvStatesTask<CExpiredCleaner>
{
public:
    CExpiredCleaner(void);
    virtual ~CExpiredCleaner(void);

private:
    State x_StartSession(void);
    State x_CleanNextBucket(void);
    State x_DeleteNextData(void);
    State x_FinishSession(void);


    int m_StartTime;
    int m_NextDead;
    Uint4 m_ExtraGCTime;
    Uint2 m_CurBucket;
    Uint2 m_CurDelData;
    Uint2 m_BatchSize;
    bool m_DoExtraGC;
    vector<SNCCacheData*> m_CacheDatas;
};


class CMovedRecDeleter : public CSrvRCUUser
{
public:
    CMovedRecDeleter(SNCDBFileInfo* file_info, SFileIndexRec* ind_rec);
    virtual ~CMovedRecDeleter(void);

private:
    virtual void ExecuteRCU(void);


    CSrvRef<SNCDBFileInfo> m_FileInfo;
    SFileIndexRec* m_IndRec;
};

END_NCBI_SCOPE

#endif /* NETCACHE__STORAGE_TYPES__HPP */
