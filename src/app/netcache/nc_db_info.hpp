#ifndef NETCACHE_NC_DB_INFO__HPP
#define NETCACHE_NC_DB_INFO__HPP
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
/// Universal type of list of ids
typedef list<Int8>                       TNCIdsList;
/// Type of buffer used for reading/writing blobs
typedef CSimpleBufferT<char>             TNCBlobBuffer;


/// Coordinates of blob inside NetCache database
struct SNCBlobCoords
{
    TNCDBPartId part_id;  ///< Id of database part which blob is written to
    TNCBlobId   blob_id;  ///< Id of the blob itself

    SNCBlobCoords(void);
    SNCBlobCoords(TNCDBPartId _part_id, TNCBlobId _blob_id);
    /// Clear the coordinates (set both ids to 0)
    void Clear(void);
    /// Copy coordinates from another structure
    void AssignCoords(const SNCBlobCoords& coords);
};

/// Full identifying information about blob: coordinates, key, subkey
/// and version.
struct SNCBlobIdentity : public SNCBlobCoords
{
    string key;          ///< Blob key
    string subkey;       ///< Blob subkey
    int    version;      ///< Blob version

    SNCBlobIdentity(void);
    /// Create identity with empty key information
    SNCBlobIdentity(const SNCBlobCoords& coords);
    /// Create identity with empty coordinates and empty subkey and version
    SNCBlobIdentity(const string& _key);
    /// Create identity with empty coordinates
    SNCBlobIdentity(const string& _key, const string& _subkey, int _version);
    /// Clear all identity information
    void Clear(void);
};

/// Comparator for sorting blob identities by key information
struct SNCBlobCompareKeys
{
    bool operator() (const SNCBlobIdentity& left,
                     const SNCBlobIdentity& right) const;
};

/// Comparator for sorting blob identities by blob ids
struct SNCBlobCompareIds
{
    bool operator() (const SNCBlobIdentity& left,
                     const SNCBlobIdentity& right) const;
};

/// Full information about NetCache blob
struct SNCBlobInfo : public SNCBlobIdentity
{
    string owner;        ///< Blob owner
    int    ttl;          ///< Timeout for blob living
    int    access_time;  ///< Last time blob was accessed
    bool   expired;      ///< Flag if blob has already expired but was not
                         ///< deleted yet
    size_t size;         ///< Size of the blob
    bool   corrupted;    ///< Special flag pointing that blob information is
                         ///< corrupted in database and need to be deleted.

    SNCBlobInfo(void);
    /// Clear blob information
    void Clear(void);
};


/// Information about database part in NetCache storage
struct SNCDBPartInfo
{
    TNCDBPartId part_id;       ///< Id of database part
    int         create_time;   ///< Time when the part was created
    int         last_rot_time; ///< Time when the part was last "rotated"
    TNCBlobId   min_blob_id;   ///< Minimum value of blob id inside the part
    string      meta_file;     ///< Name of meta file for the part
    string      data_file;     ///< Name of file with blob data for the part
    Int8        meta_size;     ///< Size of meta file measured last time
    Int8        data_size;     ///< Size of data file measured last time
};
/// Information about all database parts in NetCache storage
typedef list<SNCDBPartInfo>  TNCDBPartsList;



inline void
SNCBlobCoords::Clear(void)
{
    part_id = 0;
    blob_id = 0;
}

inline
SNCBlobCoords::SNCBlobCoords(void)
{
    Clear();
}

inline
SNCBlobCoords::SNCBlobCoords(TNCDBPartId _part_id, TNCBlobId _blob_id)
    : part_id(_part_id), blob_id(_blob_id)
{}

inline void
SNCBlobCoords::AssignCoords(const SNCBlobCoords& coords)
{
    part_id = coords.part_id;
    blob_id = coords.blob_id;
}


inline void
SNCBlobIdentity::Clear(void)
{
    SNCBlobCoords::Clear();
    key.clear();
    subkey.clear();
    version = 0;
}

inline
SNCBlobIdentity::SNCBlobIdentity(void)
{
    Clear();
}

inline
SNCBlobIdentity::SNCBlobIdentity(const SNCBlobCoords& coords)
    : SNCBlobCoords(coords),
      version(0)
{}

inline
SNCBlobIdentity::SNCBlobIdentity(const string& _key)
    : key(_key),
      version(0)
{}

inline
SNCBlobIdentity::SNCBlobIdentity(const string& _key,
                                 const string& _subkey,
                                 int           _version)
    : key(_key),
      subkey(_subkey),
      version(_version)
{}


inline bool
SNCBlobCompareKeys::operator() (const SNCBlobIdentity& left,
                                const SNCBlobIdentity& right) const
{
    if (left.key.size() != right.key.size())
        return left.key.size() < right.key.size();
    int ret = left.key.compare(right.key);
    if (ret != 0)
        return ret < 0;
    if (left.subkey.size() != right.subkey.size())
        return left.subkey.size() < right.subkey.size();
    ret = left.subkey.compare(right.subkey);
    if (ret != 0)
        return ret < 0;
    return left.version < right.version;
}


inline bool
SNCBlobCompareIds::operator() (const SNCBlobIdentity& left,
                               const SNCBlobIdentity& right) const
{
    return left.blob_id < right.blob_id;
}


inline void
SNCBlobInfo::Clear(void)
{
    SNCBlobIdentity::Clear();
    owner.clear();
    ttl = access_time = 0;
    expired = corrupted = false;
    size = 0;
}

inline
SNCBlobInfo::SNCBlobInfo(void)
{
    Clear();
}

END_NCBI_SCOPE

#endif /* NETCACHE_NC_DB_INFO__HPP */
