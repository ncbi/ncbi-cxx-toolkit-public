#ifndef BDB___EXT_BLOB_HPP__
#define BDB___EXT_BLOB_HPP__

/* $Id$
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
 * Author:  Anatoliy Kuznetsov
 *   
 * File Description: BDB library external archival BLOB store.
 *
 */
/// @file bdb_ext_blob.hpp
/// BDB library external archival BLOB store

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimtx.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbitype.hpp>

#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_cursor.hpp>



BEGIN_NCBI_SCOPE

/** @addtogroup BDB_BLOB
 *
 * @{
 */



/// BLOB map, encapsulates collection of BLOB ids and BLOB
/// locations. BLOB location encoded as a BLOB location table 
/// (list of chunks and sizes).
/// This map is easily serializable so it can be stored in BDB file.
///
/// It is intentional that this file supports multi-chunk BLOBs even
/// if neat term use is just single chunk.
///
class NCBI_BDB_CACHE_EXPORT CBDB_ExtBlobMap
{
public:
    /// BLOB chunk location: offset in file + chunk size
    ///
    struct SBlobChunkLoc
    {
        Uint8  offset;   ///< chunk offset
        Uint8  size;     ///< chunk size
    };

    /// BLOB location table (list of chunks and sizes)
    typedef vector<SBlobChunkLoc>  TBlobChunkVec;

    /// Blob id + blob location table (list of chunks and sizes)
    ///
    struct SBlobLoc
    {
        unsigned       blob_id;
        TBlobChunkVec  blob_location_table;
    };

    /// Collection of BLOBs (id + allocation table)
    typedef vector<SBlobLoc>   TBlobMap;


    CBDB_ExtBlobMap();
    CBDB_ExtBlobMap(const CBDB_ExtBlobMap& ext_blob_map);
    CBDB_ExtBlobMap& operator=(const CBDB_ExtBlobMap& ext_blob_map);


    /// Add BLOB. BLOB consists of one single chunk.
    void Add(unsigned blob_id, Uint8 offset, Uint8 size);

    /// Get BLOB location 
    bool GetBlobLoc(unsigned blob_id, Uint8* offset, Uint8* size);

    /// Get min BLOB id covered by the map
    unsigned GetMinBlobId() const;

    /// Get max BLOB id covered by the map
    unsigned GetMaxBlobId() const;

    /// Serialize map for storage
    /// @param buf       
    ///     destination buffer
    /// @param buf_offset
    ///     start offset in the destination buffer
    ///
    void Serialize(CBDB_RawFile::TBuffer* buf,
                   Uint8 buf_offset = 0);

    /// DeSerialize map 
    void Deserialize(const CBDB_RawFile::TBuffer& buf, 
                     Uint8 buf_offset = 0);
};

/// Super BLOB 
///     Encapsulates:
///         BLOB maps of several BLOBs (offsets there point in super BLOB)
///         Super BLOB location table (offsets and sizes in external file)
///
/// Super BLOB can be put to external file in chunks and then reassembled
/// BLOB map itself allows to read and reassemble BLOBs in the super-blob
///
class NCBI_BDB_CACHE_EXPORT CBDB_ExtSuperBlobMap
{
public:
    const CBDB_ExtBlobMap& GetExtBlobMap() const;
    CBDB_ExtBlobMap& SetExtBlobMap();

    /// Get location table of a super BLOB
    const CBDB_ExtBlobMap::TBlobChunkVec& GetSuperLoc() const;
    CBDB_ExtBlobMap::TBlobChunkVec& SetSuperLoc();

    void Serialize(CBDB_RawFile::TBuffer* buf);
    void Deserialize(const CBDB_RawFile::TBuffer& buf);
};



/// Dictionary file, storing references on external BLOB file 
/// (super BLOB structure). 
///
/// All BLOB meta attributes here are stored in 
/// this table and can be transactionally protected and caches. 
/// External file in this case works as a raw partition stores 
/// only BLOB bytes. 
///
/// An important implication is that we almost immediately know 
/// if target range of ids contains our BLOB without reading 
/// the super BLOB (could be slow). Search through overlapping 
/// ranges can relatively fast.
/// 
///
/// File encodes id range -> super BLOB description
///
/// The general design idea is that we take
/// certain number of BLOBs (each is has a unique id)
/// BLOBs are packed together into one large super BLOB, compressed 
/// and stored in an external file. Super BLOB description is stored
/// in a CExtBlobLocDB. 
///
/// Getting a BLOB is a multi-step procedure:
///   - first we find the right id range
///   - read super BLOB description and deserialize it
///   - before reading the super BLOB
///   - reassemble super BLOB from chunks (one chunk in most cases)
///        the most efficient read would probably be mmap
///   - decompress super-BLOB (optional) (may be coupled with mmap)
///   - find blob id and location in the meta-information
///   - read BLOB from super BLOB (reassemble using location table)
///
struct NCBI_BDB_CACHE_EXPORT CExtBlobLocDB : public CBDB_BLobFile
{

    CBDB_FieldUint4        id_from;  ///< Id range from
    CBDB_FieldUint4        id_to;    ///< Id range to

    CExtBlobLocDB()
    {
        DisableNull();

        BindKey("id_from",   &id_from);
        BindKey("id_to",     &id_to);
    }

};


/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE


#endif 
