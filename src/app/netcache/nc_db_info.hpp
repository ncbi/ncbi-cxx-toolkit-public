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
static const TNCChunkId kNCMinChunkId = 0x100000000;
///
static const TNCChunkId kNCMaxChunkId = 0x7FFFFFFFFFFFFFFF;
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


/// Full identifying information about blob: coordinates, key, subkey
/// and version.
struct SNCBlobShortInfo : public SNCBlobCoords
{
    string key;
    Uint8  size;
    Int8   cnt_reads;
};

typedef list<SNCBlobShortInfo>   TNCBlobsList;


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
    virtual void DeleteThis(void) const;

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
    SNCBlobCoords       coords;         ///<
    TNCChunksList       chunks;         ///<
    int                 ttl;            ///< Timeout for blob living
    Int8                create_time;    ///< Last time blob was moved
    int                 dead_time;      ///< 
    Int8                size;           ///< Size of the blob
    string              password;       ///< 
    bool                old_style;      ///< 
    Int8                disk_size;      ///< 
    Int8                cnt_reads;      ///< 
    int                 old_dead_time;  ///< 
    Uint8               generation;     ///< 
    bool                need_write;     ///<
    CRef<CNCBlobBuffer> data;           ///<

    CNCBlobVerManager*  manager;        ///<
    CNCLongOpTrigger    data_trigger;   ///<

    ///
    SNCBlobVerData(void);
    virtual ~SNCBlobVerData(void);

    ///
    SNCBlobVerData(const SNCBlobVerData* other);

private:
    SNCBlobVerData(const SNCBlobVerData&);
    SNCBlobVerData& operator= (const SNCBlobVerData&);

    ///
    virtual void DeleteThis(void) const;
};


///
typedef AutoPtr<CNCDBFile>  TNCDBFilePtr;

/// Information about database part in NetCache storage
struct SNCDBFileInfo
{
    TNCDBFileId  file_id;        ///< Id of database part
    int          create_time;    ///< Time when the part was created
    string       file_name;      ///< Name of meta file for the part
    Int8         file_size;
    Uint8        garbage_amt;
    Uint8        total_amt;
    TNCDBFilePtr file_obj;
};
/// Information about all database parts in NetCache storage
typedef map<TNCDBFileId, SNCDBFileInfo>  TNCDBFilesMap;



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
    : ttl(other->ttl),
      create_time(other->create_time),
      dead_time(other->dead_time),
      size(other->size),
      password(other->password),
      cnt_reads(other->cnt_reads),
      generation(other->generation)
{
    coords.blob_id = other->coords.blob_id;
    coords.data_id = other->coords.data_id;
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_DB_INFO__HPP */
