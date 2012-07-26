#ifndef NETCACHE__NC_DB_INFO__HPP
#define NETCACHE__NC_DB_INFO__HPP
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


namespace intr = boost::intrusive;


BEGIN_NCBI_SCOPE


static const Uint4  kNCMaxDBFileId = 0xFFFFFFFF;
/// Maximum size of blob chunk stored in database.
/// It's a little bit uneven to be efficient in the current memory manager.
/// I.e. memory manager in TaskServer allocates memory with pages of 65536 bytes,
/// uses some of that memory for page header and then splits the rest in half to
/// obtain the maximum block size it can allocate this way. kNCMaxBlobChunkSize
/// should be equal or less than that size (which is kMMMaxBlockSize in
/// memory_man.cpp in task_server library).
static const size_t kNCMaxBlobChunkSize = 32740;
static const Uint2  kNCMaxChunksInMap = 128;



class CNCBlobVerManager;
struct SNCChunkMaps;


struct ATTR_PACKED SNCDataCoord
{
    Uint4   file_id;
    Uint4   rec_num;


    bool empty(void) const;
    void clear(void);
};

const CSrvDiagMsg& operator<< (const CSrvDiagMsg& msg, SNCDataCoord coord);
bool operator== (SNCDataCoord left, SNCDataCoord right);
bool operator!= (SNCDataCoord left, SNCDataCoord right);


struct SVersMap_tag;
typedef intr::set_base_hook<intr::tag<SVersMap_tag>,
                            intr::optimize_size<true> >     TVerDataMapHook;


/// Full information about NetCache blob (excluding key)
struct SNCBlobVerData : public CObject,
                        public CSrvTask,
                        public TVerDataMapHook
{
public:
    SNCDataCoord coord;
    SNCDataCoord data_coord;
    Uint8   size;
    string  password;
    Uint8   create_time;
    Uint8   create_server;
    Uint4   create_id;
    int     ttl;
    int     expire;
    int     dead_time;
    int     blob_ver;
    int     ver_ttl;
    int     ver_expire;
    Uint4   chunk_size;
    Uint2   map_size;
    Uint1   map_depth;
    bool    has_error;

    bool    is_cur_version;
    bool    meta_has_changed;
    bool    move_or_rewrite;
    bool    is_releasable;
    bool    need_write_all;
    bool    need_stop_write;
    bool    need_mem_release;
    bool    delete_scheduled;
    Uint4   map_move_counter;
    int     last_access_time;
    int     need_write_time;
    Uint8   cnt_chunks;
    Uint8   cur_chunk_num;
    SNCChunkMaps* chunk_maps;

    CNCBlobVerManager* manager;

    int     saved_access_time;
    CMiniMutex wb_mem_lock;
    size_t  meta_mem;
    size_t  data_mem;
    size_t  releasable_mem;
    size_t  releasing_mem;
    vector<char*> chunks;


    SNCBlobVerData(void);
    virtual ~SNCBlobVerData(void);

public:
    void AddChunkMem(char* mem, Uint4 mem_size);
    void RequestDataWrite(void);
    size_t RequestMemRelease(void);
    void SetNotCurrent(void);
    void SetCurrent(void);
    void SetReleasable(void);
    void SetNonReleasable(void);

private:
    SNCBlobVerData(const SNCBlobVerData&);
    SNCBlobVerData& operator= (const SNCBlobVerData&);

    virtual void DeleteThis(void);
    virtual void ExecuteSlice(TSrvThreadNum thr_num);

    void x_FreeChunkMaps(void);
    bool x_WriteBlobInfo(void);
    bool x_WriteCurChunk(char* write_mem, Uint4 write_size);
    bool x_ExecuteWriteAll(void);
    void x_DeleteVersion(void);
};


struct SCompareVerAccessTime
{
    bool operator() (const SNCBlobVerData& left, const SNCBlobVerData& right) const
    {
        return left.saved_access_time < right.saved_access_time;
    }
};

typedef intr::rbtree<SNCBlobVerData,
                     intr::base_hook<TVerDataMapHook>,
                     intr::constant_time_size<false>,
                     intr::compare<SCompareVerAccessTime> > TVerDataMap;


static const Uint1 kNCMaxBlobMapsDepth = 3;

struct SNCChunkMapInfo
{
    Uint4   map_counter;
    Uint2   map_idx;
    SNCDataCoord ATTR_ALIGNED_8 coords[1];
};

struct SNCChunkMaps
{
    SNCChunkMapInfo* maps[kNCMaxBlobMapsDepth + 1];


    SNCChunkMaps(Uint2 map_size);
    ~SNCChunkMaps(void);
};

struct SNCBlobSummary
{
    Uint8   size;
    Uint8   create_time;
    Uint8   create_server;
    Uint4   create_id;
    int     dead_time;
    int     expire;
    int     ver_expire;

    bool isOlder(const SNCBlobSummary& other) const
    {
        if (create_time != other.create_time)
            return create_time < other.create_time;
        else if (create_server != other.create_server)
            return create_server < other.create_server;
        else if (create_id != other.create_id)
            return create_id < other.create_id;
        else if (expire != other.expire)
            return expire < other.expire;
        else if (ver_expire != other.ver_expire)
            return ver_expire < other.ver_expire;
        else
            return dead_time < other.dead_time;
    }

    bool isSameData(const SNCBlobSummary& other) const
    {
        return create_time == other.create_time
               &&  create_server == other.create_server
               &&  create_id == other.create_id;
    }

    bool isEqual(const SNCBlobSummary& other) const
    {
        return create_time == other.create_time
               &&  create_server == other.create_server
               &&  create_id == other.create_id
               &&  dead_time == other.dead_time
               &&  expire == other.expire
               &&  ver_expire == other.ver_expire;
    }
};


class CNCBlobVerManager;

struct SNCBlobSummary;
typedef map<string, SNCBlobSummary*>   TNCBlobSumList;



enum ENCDBFileType {
    eDBFileMeta = 0x01,
    eDBFileData = 0x02,
    eDBFileMaps = 0x03
    //fDBFileForMove = 0x80,
    //eDBFileMoveMeta = eDBFileMeta + fDBFileForMove,
    //eDBFileMoveData = eDBFileData + fDBFileForMove,
    //eDBFileMoveMaps = eDBFileMaps + fDBFileForMove
};

static ENCDBFileType const s_AllFileTypes[]
                    = {eDBFileMeta, eDBFileData, eDBFileMaps
                       /*, eDBFileMoveMeta, eDBFileMoveData, eDBFileMoveMaps*/};
static Uint1 const s_CntAllFiles = sizeof(s_AllFileTypes) / sizeof(s_AllFileTypes[0]);

enum EDBFileIndex {
    eFileIndexMeta = 0,
    eFileIndexData = 1,
    eFileIndexMaps = 2
    //eFileIndexMoveShift = 3,
    //eFileIndexMoveMeta = eFileIndexMeta + eFileIndexMoveShift,
    //eFileIndexMoveData = eFileIndexData + eFileIndexMoveShift,
    //eFileIndexMoveMaps = eFileIndexMaps + eFileIndexMoveShift
};


#if defined(NCBI_OS_MSWIN)
  typedef HANDLE TFileHandle;
#else
  typedef int TFileHandle;
#endif


struct SFileIndexRec;

/// Information about database part in NetCache storage
struct SNCDBFileInfo : public CObject
{
    char*        file_map;
    Uint4        file_id;
    Uint4        file_size;
    Uint4        garb_size;
    Uint4        used_size;
    SFileIndexRec* index_head;
    CMiniMutex   info_lock;
    bool         is_releasing;
    TFileHandle  fd;
    int          create_time;
    int          next_shrink_time;
    CAtomicCounter cnt_unfinished;
    ENCDBFileType file_type;
    EDBFileIndex type_index;
    string       file_name;


    SNCDBFileInfo(void);
    virtual ~SNCDBFileInfo(void);
};
/// Information about all database parts in NetCache storage
typedef map<Uint4, CSrvRef<SNCDBFileInfo> >  TNCDBFilesMap;



inline bool
SNCDataCoord::empty(void) const
{
    return file_id == 0  &&  rec_num == 0;
}

inline void
SNCDataCoord::clear(void)
{
    file_id = rec_num = 0;
}

inline const CSrvDiagMsg&
operator<< (const CSrvDiagMsg& msg, SNCDataCoord coord)
{
    msg << "(" << coord.file_id << ", " << coord.rec_num << ")";
    return msg;
}

inline bool
operator== (SNCDataCoord left, SNCDataCoord right)
{
    return left.file_id == right.file_id  &&  left.rec_num == right.rec_num;
}

inline bool
operator!= (SNCDataCoord left, SNCDataCoord right)
{
    return !(left == right);
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_DB_INFO__HPP */
