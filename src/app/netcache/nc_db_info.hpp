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

#include <list>


BEGIN_NCBI_SCOPE

/// Type of blob id in NetCache database
typedef Int8                             TNCBlobId;
/// Type of blob chunk id in NetCache database
typedef Int8                             TNCChunkId;
/// Type of database part id
typedef Int8                             TNCDBPartId;
/// Type of volume id inside a database part
typedef unsigned int                     TNCDBVolumeId;
/// List of chunk ids
typedef list<TNCChunkId>                 TNCChunksList;
/// Type of buffer used for reading/writing blobs
typedef CSimpleBufferT<char>             TNCBlobBuffer;


/// Coordinates of blob inside NetCache database
struct SNCBlobCoords
{
    TNCDBPartId   part_id;   ///< Id of database part which blob is written to
    TNCDBVolumeId volume_id; ///< Id of volume inside the part
    TNCBlobId     blob_id;   ///< Id of the blob itself

    SNCBlobCoords(void);
    SNCBlobCoords(TNCDBPartId   _part_id,
                  TNCDBVolumeId _volume_id,
                  TNCBlobId     _blob_id);
    /// Clear the coordinates (set all ids to 0)
    void Clear(void);
    /// Copy coordinates from another structure
    void AssignCoords(const SNCBlobCoords& coords);
    /// Check whether this coordinates are equal to other ones
    bool operator==  (const SNCBlobCoords& coords) const;
};

/// Keys-related "coordinates" of blob inside NetCache
struct SNCBlobKeys
{
    string key;          ///< Blob key
    string subkey;       ///< Blob subkey
    int    version;      ///< Blob version

    SNCBlobKeys(void);
    SNCBlobKeys(const string& _key, const string& _subkey, int _version);
    /// Clear the coordinates
    void Clear(void);
    /// Copy coordinates from another structure
    void AssignKeys(const SNCBlobKeys& keys);

    /// Special transformation of the structure to pointer to it necessary
    /// for implementation of CNCBlobStorage_Specific.
    operator const SNCBlobKeys* (void) const;
    /// Special transformation of the structure to string necessary
    /// for implementation of CNCBlobStorage_Specific. Returns value of the
    /// key.
    operator const string& (void) const;
};

/// Full identifying information about blob: coordinates, key, subkey
/// and version.
struct SNCBlobIdentity : public SNCBlobCoords, public SNCBlobKeys
{
    SNCBlobIdentity(void);
    /// Clear all identity information
    void Clear(void);
};

typedef list< AutoPtr<SNCBlobIdentity> >   TNCBlobsList;

/// Full information about NetCache blob
struct SNCBlobInfo : public SNCBlobIdentity
{
    string owner;        ///< Blob owner
    int    ttl;          ///< Timeout for blob living
    int    create_time;  ///< Last time blob was accessed
    int    dead_time;    ///< Time when blob will expire
    bool   expired;      ///< Flag if blob has already expired but was not
                         ///< deleted yet
    size_t size;         ///< Size of the blob
    Int8   cnt_reads;    ///< Number of reads that was called on the blob
    bool   corrupted;    ///< Special flag pointing that blob information is
                         ///< corrupted in database and need to be deleted.

    SNCBlobInfo(void);
    /// Clear blob information
    void Clear(void);
};


/// Information about database part in NetCache storage
struct SNCDBPartInfo
{
    TNCDBPartId   part_id;        ///< Id of database part
    int           create_time;    ///< Time when the part was created
    int           last_rot_time;  ///< Time when the part was last "rotated"
    string        meta_file;      ///< Name of meta file for the part
    string        data_file;      ///< Name of file with blob data for the part
    TNCDBVolumeId cnt_volumes;    ///< Number of volumes in part's files set
    Int8          meta_size;      ///< Size of all meta files measured last time
    Int8          meta_size_diff; ///< Difference between maximum meta file size
                                  ///< and minimum meta file size
    Int8          data_size;      ///< Size of all data files measured last time
    Int8          data_size_diff; ///< Difference between maximum data file size
                                  ///< and minimum data file size
};
/// Information about all database parts in NetCache storage
typedef list<SNCDBPartInfo*>  TNCDBPartsList;



inline void
SNCBlobCoords::Clear(void)
{
    part_id   = 0;
    volume_id = 0;
    blob_id   = 0;
}

inline
SNCBlobCoords::SNCBlobCoords(void)
{
    Clear();
}

inline
SNCBlobCoords::SNCBlobCoords(TNCDBPartId   _part_id,
                             TNCDBVolumeId _volume_id,
                             TNCBlobId     _blob_id)
    : part_id(_part_id), volume_id(_volume_id), blob_id(_blob_id)
{}

inline void
SNCBlobCoords::AssignCoords(const SNCBlobCoords& coords)
{
    part_id   = coords.part_id;
    volume_id = coords.volume_id;
    blob_id   = coords.blob_id;
}

inline bool
SNCBlobCoords::operator== (const SNCBlobCoords& coords) const
{
    return blob_id == coords.blob_id
           &&  part_id == coords.part_id  &&  volume_id == coords.volume_id;
}


inline void
SNCBlobKeys::Clear(void)
{
    key.clear();
    subkey.clear();
    version = 0;
}

inline
SNCBlobKeys::SNCBlobKeys(void)
{
    Clear();
}

inline
SNCBlobKeys::SNCBlobKeys(const string& _key,
                         const string& _subkey,
                         int           _version)
    : key(_key),
      subkey(_subkey),
      version(_version)
{}

inline void
SNCBlobKeys::AssignKeys(const SNCBlobKeys& keys)
{
    key     = keys.key;
    subkey  = keys.subkey;
    version = keys.version;
}

inline
SNCBlobKeys::operator const SNCBlobKeys*(void) const
{
    return this;
}

inline
SNCBlobKeys::operator const string& (void) const
{
    return key;
}


inline void
SNCBlobIdentity::Clear(void)
{
    SNCBlobCoords::Clear();
    SNCBlobKeys::Clear();
}

inline
SNCBlobIdentity::SNCBlobIdentity(void)
{
    Clear();
}


inline void
SNCBlobInfo::Clear(void)
{
    SNCBlobIdentity::Clear();
    owner.clear();
    ttl = create_time = dead_time = 0;
    expired = corrupted = false;
    size = 0;
    cnt_reads = 0;
}

inline
SNCBlobInfo::SNCBlobInfo(void)
{
    Clear();
}

END_NCBI_SCOPE

#endif /* NETCACHE__NC_DB_INFO__HPP */
