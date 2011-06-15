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

#include <corelib/obj_pool.hpp>
#include <util/simple_buffer.hpp>

#include "nc_utils.hpp"

#include <vector>


BEGIN_NCBI_SCOPE

class CNCDBFile;


/// 3 bytes
typedef Int4                TNCDBFileId;
///
static const TNCDBFileId kNCMaxDBFileId = 0xFFFFFF;
/// 
typedef Uint4               TNCBlobId;
///
static const TNCBlobId kNCMaxBlobId = 0xFFFFFFFF;
/// Type of blob chunk id in NetCache database.
/// Should occupy at least 5 bytes, i.e. be greater than kNCMinChunkId.
typedef Int8                TNCChunkId;
///
static const TNCChunkId kNCMinChunkId = NCBI_CONST_INT8(0x100000000);
///
static const TNCChunkId kNCMaxChunkId = NCBI_CONST_INT8(0x7FFFFFFFFFFFFFFF);
/// List of chunk ids
typedef vector<TNCChunkId>  TNCChunksList;

///
enum ENCDBFileType {
    eNCMeta  = 0,   ///<
    eNCData  = 1,   ///<
    eNCIndex = 2    ///<
};


/// Coordinates of blob inside NetCache database
struct SNCBlobCoords
{
    TNCBlobId       blob_id;    ///< Id of the blob itself
    TNCDBFileId     meta_id;    ///< 
    TNCDBFileId     data_id;    ///< 
};


/// Maximum size of blob chunk stored in database. Experiments show that
/// reading from blob in SQLite after this point is significantly slower than
/// before that.
static const size_t kNCMaxBlobChunkSize = 2000000;


///
class CNCBlobBuffer : public CObject
{
public:
    ///
    char*  GetData(void);
    const char* GetData(void) const;
    ///
    size_t GetSize(void) const;
    ///
    void   Resize(size_t new_size);

public:
    ///
    CNCBlobBuffer(void);
    virtual ~CNCBlobBuffer(void);

private:
    ///
    virtual void DeleteThis(void);

    ///
    size_t  m_Size;
    ///
    char    m_Data[kNCMaxBlobChunkSize];
};


class CNCBlobVerManager;

/// Full information about NetCache blob (excluding key, subkey, version)
struct SNCBlobVerData : public CObject
{
public:
    SNCBlobCoords   coords;
    TNCChunksList   chunks;
    Uint8   create_time;    ///< Last time blob was moved
    int     ttl;            ///< Timeout for blob living
    int     expire;
    int     dead_time;
    Uint8   size;           ///< Size of the blob
    string  password;
    int     blob_ver;       ///< Blob version
    int     ver_ttl;
    int     ver_expire;
    TNCBlobId create_id;
    Uint8   create_server;
    Uint2   slot;
    bool    need_write;
    bool    need_meta_blob;
    bool    need_data_blob;
    bool    has_error;
    Uint4   generation;
    Uint8   disk_size;
    CRef<CNCBlobBuffer> data;

    CNCBlobVerManager*  manager;
    CNCLongOpTrigger    data_trigger;

    ///
    SNCBlobVerData(void);
    virtual ~SNCBlobVerData(void);

    ///
    SNCBlobVerData(const SNCBlobVerData* other);

private:
    SNCBlobVerData(const SNCBlobVerData&);
    SNCBlobVerData& operator= (const SNCBlobVerData&);

    ///
    virtual void DeleteThis(void);
};

struct SNCBlobSummary
{
    Uint8   size;
    Uint8   create_time;
    Uint8   create_server;
    TNCBlobId create_id;
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
        else if (dead_time != other.dead_time)
            return dead_time < other.dead_time;
        else if (expire != other.expire)
            return expire < other.expire;
        else
            return ver_expire < other.ver_expire;
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


struct SNCBlobListInfo : public SNCBlobCoords, public SNCBlobSummary
{
    string  key;
    Uint2   slot;
};

typedef list< AutoPtr<SNCBlobListInfo> >   TNCBlobsList;


class CNCBlobVerManager;

struct SNCCacheData : public SNCBlobCoords, public SNCBlobSummary
{
    bool  key_deleted;
    int   key_del_time;
    Uint4 generation;
    CSpinLock lock;
    CNCBlobVerManager* ver_mgr;


    SNCCacheData(void);
    SNCCacheData(SNCBlobListInfo* blob_info);

private:
    SNCCacheData(const SNCCacheData&);
    SNCCacheData& operator= (const SNCCacheData&);
};

typedef map<string, SNCCacheData*>   TNCBlobSumList;



///
typedef AutoPtr<CNCDBFile>  TNCDBFilePtr;

/// Information about database part in NetCache storage
struct SNCDBFileInfo
{
    TNCDBFileId  file_id;        ///< Id of database part
    int          create_time;    ///< Time when the part was created
    string       file_name;      ///< Name of meta file for the part
    TNCDBFilePtr file_obj;
    CSpinLock    cnt_lock;
    Uint4        ref_cnt;
    Uint8        useful_blobs;
    Uint8        useful_size;
    Uint8        garbage_blobs;
    Uint8        garbage_size;


    void IncRefCnt(void);
    void DecRefCnt(void);
    void IncUsefulBlobs(void);
    void IncUsefulBlobs(Uint8 size);
    void DecUsefulBlobs(void);
    void UsefulToGarbage(Uint8 size);
    void Locked_IncRefCnt(void);
    void Locked_DecRefCnt(void);
    void Locked_IncUsefulBlobs(void);
    void Locked_IncUsefulBlobs(Uint8 size);
    void Locked_DecUsefulBlobs(void);
    void Locked_UsefulToGarbage(Uint8 size);
};
/// Information about all database parts in NetCache storage
typedef map<TNCDBFileId, SNCDBFileInfo*> TNCDBFilesMap;



inline char*
CNCBlobBuffer::GetData(void)
{
    return m_Data;
}

inline const char*
CNCBlobBuffer::GetData(void) const
{
    return m_Data;
}

inline size_t
CNCBlobBuffer::GetSize(void) const
{
    return m_Size;
}

inline void
CNCBlobBuffer::Resize(size_t new_size)
{
    m_Size = new_size;
}


inline
SNCBlobVerData::SNCBlobVerData(const SNCBlobVerData* other)
    : create_time(other->create_time),
      ttl(other->ttl),
      expire(other->expire),
      dead_time(other->dead_time),
      size(other->size),
      password(other->password),
      blob_ver(other->blob_ver),
      ver_ttl(other->ver_ttl),
      ver_expire(other->ver_expire),
      create_id(other->create_id),
      create_server(other->create_server),
      slot(other->slot),
      generation(other->generation)
{
    coords.blob_id = other->coords.blob_id;
    coords.meta_id = other->coords.meta_id;
    coords.data_id = other->coords.data_id;
}


inline
SNCCacheData::SNCCacheData(void)
    : key_deleted(false),
      ver_mgr(NULL)
{
    blob_id = create_id = 0;
    meta_id = data_id = 0;
    create_time = create_server = 0;
    dead_time = ver_expire = 0;
}

inline
SNCCacheData::SNCCacheData(SNCBlobListInfo* blob_info)
    : SNCBlobCoords(*blob_info),
      SNCBlobSummary(*blob_info),
      key_deleted(false),
      ver_mgr(NULL)
{}


inline void
SNCDBFileInfo::IncRefCnt(void)
{
    ++ref_cnt;
}

inline void
SNCDBFileInfo::Locked_IncRefCnt(void)
{
    cnt_lock.Lock();
    IncRefCnt();
    cnt_lock.Unlock();
}

inline void
SNCDBFileInfo::DecRefCnt(void)
{
    if (ref_cnt == 0  ||  --ref_cnt < useful_blobs)
        abort();
}

inline void
SNCDBFileInfo::Locked_DecRefCnt(void)
{
    cnt_lock.Lock();
    DecRefCnt();
    cnt_lock.Unlock();
}

inline void
SNCDBFileInfo::IncUsefulBlobs(void)
{
    if (++useful_blobs > ref_cnt)
        abort();
}

inline void
SNCDBFileInfo::Locked_IncUsefulBlobs(void)
{
    cnt_lock.Lock();
    IncUsefulBlobs();
    cnt_lock.Unlock();
}

inline void
SNCDBFileInfo::IncUsefulBlobs(Uint8 size)
{
    IncUsefulBlobs();
    useful_size += size;
}

inline void
SNCDBFileInfo::Locked_IncUsefulBlobs(Uint8 size)
{
    cnt_lock.Lock();
    IncUsefulBlobs(size);
    cnt_lock.Unlock();
}

inline void
SNCDBFileInfo::DecUsefulBlobs(void)
{
    if (useful_blobs == 0)
        abort();
    --useful_blobs;
    DecRefCnt();
}

inline void
SNCDBFileInfo::Locked_DecUsefulBlobs(void)
{
    cnt_lock.Lock();
    DecUsefulBlobs();
    cnt_lock.Unlock();
}

inline void
SNCDBFileInfo::UsefulToGarbage(Uint8 size)
{
    DecUsefulBlobs();
    ++garbage_blobs;
    if (useful_size < size)
        abort();
    useful_size -= size;
    garbage_size += size;
}

inline void
SNCDBFileInfo::Locked_UsefulToGarbage(Uint8 size)
{
    cnt_lock.Lock();
    UsefulToGarbage(size);
    cnt_lock.Unlock();
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_DB_INFO__HPP */
