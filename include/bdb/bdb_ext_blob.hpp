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
//#include <corelib/ncbitypes.hpp>

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
class NCBI_BDB_EXPORT CBDB_ExtBlobMap
{
public:
    /// BLOB chunk location: offset in file + chunk size
    ///
    struct SBlobChunkLoc
    {
        Uint8  offset;   ///< chunk offset
        Uint8  size;     ///< chunk size

        SBlobChunkLoc() { offset = size = 0; }

        SBlobChunkLoc(Uint8  off, Uint8  s) 
            : offset(off), size(s) 
        {}

    };

    /// BLOB location table (list of chunks and sizes)
    typedef vector<SBlobChunkLoc>  TBlobChunkVec;

    /// Blob id + blob location table (list of chunks and sizes)
    ///
    struct SBlobLoc
    {
        Uint4          blob_id;
        TBlobChunkVec  blob_location_table;

        SBlobLoc() : blob_id(0) {}

        /// Construct one-chunk blob locator
        SBlobLoc(Uint4 id, Uint8 offset, Uint8 size)
            : blob_id(id), blob_location_table(1)
        {
            blob_location_table[0].offset = offset;
            blob_location_table[0].size = size;
        }
    };

    /// Collection of BLOBs (id + allocation table)
    typedef vector<SBlobLoc>   TBlobMap;


    CBDB_ExtBlobMap();


    /// Add BLOB. BLOB consists of one single chunk.
    void Add(Uint4 blob_id, Uint8 offset, Uint8 size);

    /// Returns TRUE if blob exists in the map
    bool HasBlob(Uint4 blob_id) const;

    /// Get BLOB location 
    bool GetBlobLoc(Uint4 blob_id, Uint8* offset, Uint8* size) const;

    /// Number of BLOBs registered in the map
    size_t Size() const { return m_BlobMap.size(); }

    /// Get BLOB id min and max range (closed interval)
    void GetBlobIdRange(Uint4* min_id, Uint4* max_id) const;

    /// @name Serialization
    /// @{

    /// Serialize map for storage
    /// @param buf       
    ///     destination buffer
    /// @param buf_offset
    ///     start offset in the destination buffer
    ///
    void Serialize(CBDB_RawFile::TBuffer* buf,
                   Uint8 buf_offset = 0) const;

    /// DeSerialize map 
    void Deserialize(const CBDB_RawFile::TBuffer& buf, 
                     Uint8                        buf_offset = 0);

    /// Compute maximum serialization size
    size_t ComputeSerializationSize() const; 

    ///@}
private:
    /// Compute serialization size and effective number of bits used 
    /// for offset/size storage (16, 32, 64)
    /// @param bits_used
    ///     Number of bits used to represent offses & sizes
    ///     Valid values: 16, 32, 64
    /// @param is_single_chunk
    ///     returned TRUE if there is no blob fragmentation
    size_t x_ComputeSerializationSize(unsigned* bits_used,
                                      bool* is_single_chunk) const; 
private:
    TBlobMap    m_BlobMap;
};

/// Container of BLOB attributes 
///     Encapsulates:
///         BLOB maps of several BLOBs (offsets there point in super BLOB)
///         Super BLOB location table (offsets and sizes in external file)
///
/// Super BLOB can be put to external file in chunks and then reassembled
/// BLOB map itself allows to read and reassemble BLOBs in the super-blob
///
class NCBI_BDB_EXPORT CBDB_BlobMetaContainer
{
public:
    CBDB_BlobMetaContainer();

    const CBDB_ExtBlobMap& GetBlobMap() const { return m_BlobMap; }
    CBDB_ExtBlobMap& SetBlobMap() { return m_BlobMap; }


    /// @name Interface for BLOB container location table access    
    /// 
    /// @{

    /// Set container location (one chunk)
    ///
    void SetLoc(Uint8 offset, Uint8 size);

    /// Get container location (throws an exception if more than one chunk)
    ///
    void GetLoc(Uint8* offset, Uint8* size);

    /// Get location table of a super BLOB
    /// Location table is used to reassemble BLOB from chunks
    ///
    const CBDB_ExtBlobMap::TBlobChunkVec& 
                            GetSuperLoc() const { return m_Loc; }

    /// Get Edit access to location table
    ///
    CBDB_ExtBlobMap::TBlobChunkVec& SetSuperLoc() { return m_Loc; }

    /// @}



    /// @name Serialization
    /// @{

    void Serialize(CBDB_RawFile::TBuffer* buf,
                   Uint8                  buf_offset = 0) const;

    void Deserialize(const CBDB_RawFile::TBuffer& buf,
                     Uint8                        buf_offset = 0);

    /// Compute maximum serialization size
    size_t ComputeSerializationSize() const;  

    /// @}

private:
    /// Super BLOB location vector
    CBDB_ExtBlobMap::TBlobChunkVec  m_Loc;
    /// Blob attributes (super BLOB content)
    CBDB_ExtBlobMap                 m_BlobMap;

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
struct NCBI_BDB_EXPORT CBlobMetaDB : public CBDB_BLobFile
{
    typedef CBDB_BLobFile TParent;

    CBDB_FieldUint4        id_from;  ///< Id range from
    CBDB_FieldUint4        id_to;    ///< Id range to

    CBlobMetaDB();

    /// Find the meta container storing our target blob_id
    /// Function is doing the cursor range scan sequentially reading 
    /// range-matching BLOB descriptions 
    ///
    /// @param blob_id
    ///    BLOB id to search for
    /// @param meta_container
    ///    Output: Container with BLOBs meta information 
    /// @param id_from
    ///    Output: Range from where BLOB has been found
    /// @param id_to
    ///    Output: Range to where BLOB has been found
    ///
    EBDB_ErrCode FetchMeta(Uint4                   blob_id, 
                           CBDB_BlobMetaContainer* meta_container,
                           Uint4*                  id_from = 0,
                           Uint4*                  id_to = 0);

    /// Insert new super BLOB metainfo. Range (id_from, id_to) 
    /// is determined automatically
    ///
    EBDB_ErrCode UpdateInsert(const CBDB_BlobMetaContainer& meta_container);

private:
    CBlobMetaDB(const CBlobMetaDB&);
    CBlobMetaDB& operator=(const CBlobMetaDB&);
};


/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


END_NCBI_SCOPE


#endif 
