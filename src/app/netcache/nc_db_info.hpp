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
#include <corelib/ncbifile.hpp>
#include <util/simple_buffer.hpp>

#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE


static const Uint4  kNCMaxDBFileId = 0xFFFFFFFF;
/// Maximum size of blob chunk stored in database.
static const size_t kNCMaxBlobChunkSize = 32768;
static const Uint2  kNCMaxChunksInMap = 128;


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

struct ATTR_PACKED SNCDataCoord
{
    Uint4   file_id;
    Uint4   rec_num;


    bool empty(void) const;
    void clear(void);
};

const CNcbiDiag& operator<< (const CNcbiDiag& diag, SNCDataCoord coord);
bool operator== (SNCDataCoord left, SNCDataCoord right);
bool operator!= (SNCDataCoord left, SNCDataCoord right);


/// Full information about NetCache blob (excluding key, subkey, version)
struct SNCBlobVerData : public CObject
{
public:
    SNCDataCoord coord;
    Uint8   create_time;
    int     ttl;
    int     expire;
    int     dead_time;
    int     blob_ver;
    int     ver_ttl;
    int     ver_expire;
    Uint8   size;
    string  password;
    Uint8   create_server;
    Uint4   create_id;
    Uint4   chunk_size;
    Uint2   map_size;
    Uint1   map_depth;
    bool    need_write;
    bool    has_error;
    SNCDataCoord data_coord;

    CNCRef<CNCBlobBuffer> data;
    CNCBlobVerManager*  manager;
    CNCLongOpTrigger    data_trigger;


    SNCBlobVerData(void);
    virtual ~SNCBlobVerData(void);

private:
    SNCBlobVerData(const SNCBlobVerData&);
    SNCBlobVerData& operator= (const SNCBlobVerData&);

    virtual void DeleteThis(void);
};


static const Uint1 kNCMaxBlobMapsDepth = 3;

struct SNCChunkMapInfo
{
    Uint2   map_idx;
    SNCDataCoord ATTR_ALIGNED_8 coords[1];
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

struct SNCCacheData;
typedef map<string, SNCCacheData*>   TNCBlobSumList;



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
static size_t const s_CntAllFiles = sizeof(s_AllFileTypes) / sizeof(s_AllFileTypes[0]);

enum EDBFileIndex {
    eFileIndexMeta = 0,
    eFileIndexData = 1,
    eFileIndexMaps = 2
    //eFileIndexMoveShift = 3,
    //eFileIndexMoveMeta = eFileIndexMeta + eFileIndexMoveShift,
    //eFileIndexMoveData = eFileIndexData + eFileIndexMoveShift,
    //eFileIndexMoveMaps = eFileIndexMaps + eFileIndexMoveShift
};


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
typedef map<Uint4, CNCRef<SNCDBFileInfo> >  TNCDBFilesMap;



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

inline const CNcbiDiag&
operator<< (const CNcbiDiag& diag, SNCDataCoord coord)
{
    diag << "(" << coord.file_id << ", " << coord.rec_num << ")";
    return diag;
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
