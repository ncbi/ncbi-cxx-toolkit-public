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

#include <util/compress/compress.hpp>
#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/bmserial.h>

#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_cursor.hpp>
#include <bdb/error_codes.hpp>


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

/// External BLOB store. BLOBs are stored in chunks in an external file.
///
/// BLOB attributes are in a BDB attribute dictionary.
///
template<class TBV>
class CBDB_ExtBlobStore 
{
public:
    typedef TBV                    TBitVector;
    CBDB_RawFile::TBuffer          TBuffer;
public:
    CBDB_ExtBlobStore();
    ~CBDB_ExtBlobStore();

    /// External store location
    void SetStoreDataDir(const string& dir_name);

    /// Store attributes DB location 
    /// (by default it is the same as SetStoreDataDir).
    /// Works in the absense of BDB environment
    ///
    /// @sa SetEnv
    ///
    void SetStoreAttrDir(const string& dir_name);


    void SetEnv(CBDB_Env& env) { m_Env = &env; }
    CBDB_Env* GetEnv(void) const { return m_Env; }

    /// Set compressor for external BLOB
    void SetCompressor(ICompression* compressor, 
                       EOwnership    own = eTakeOwnership);

    /// Open external store
    void Open(const string&             storage_name, 
              CBDB_RawFile::EOpenMode   open_mode);

    /// Close store
    void Close();

    /// Save all changes (flush buffers, store attributes, etc.)
    void Save();

    /// Set maximum size of BLOB container. 
    /// All BLOBs smaller than container are getting packed together
    ///
    void SetContainerMaxSize(unsigned max_size) { m_ContainerMax = max_size; }

    /// Get container max size
    unsigned GetContainerMaxSize() const { return m_ContainerMax; }

    /// Add blob to external store.
    ///
    /// BLOB is not written to stor immediately, but accumulated in the buffer
    ///
    void StoreBlob(unsigned blob_id, const CBDB_RawFile::TBuffer& buf);

    /// Flush current container to disk
    void Flush();

    /// Read blob from external store
    EBDB_ErrCode ReadBlob(unsigned blob_id, CBDB_RawFile::TBuffer& buf);

private:
    CBDB_ExtBlobStore(const CBDB_ExtBlobStore&);
    CBDB_ExtBlobStore& operator=(const CBDB_ExtBlobStore&);
private:

    /// Last operation type.
    ///
    enum ELastOp {
        eRead,
        eWrite
    };

    /// Try to read BLOB from the recently loaded container
    bool x_ReadCache(unsigned blob_id, CBDB_RawFile::TBuffer& buf);

protected:
    TBitVector              m_BlobIds;      ///< List of BLOB ids stored
    /// temp block for bitvector serialization
    bm::word_t*             m_STmpBlock;

    CBDB_RawFile::EOpenMode m_OpenMode;
    CBDB_Env*               m_Env;
    CBlobMetaDB*            m_BlobAttrDB;
    CNcbiFstream*           m_ExtStore;

    AutoPtr<ICompression>   m_Compressor;    ///< Record compressor
    CBDB_RawFile::TBuffer   m_CompressBuffer;

    string                  m_StoreDataDir;
    string                  m_StoreAttrDir;

    unsigned                m_ContainerMax;   ///< Max size of a BLOB container
    CBDB_RawFile::TBuffer   m_BlobContainer;  ///< Blob container
    CBDB_BlobMetaContainer* m_AttrContainer;  ///< Blob attributes container
    ELastOp                 m_LastOp;         ///< Last operation status
    unsigned                m_LastFromBlobId; ///< Recently read id interval
    unsigned                m_LastToBlobId;   ///< Recently read id interval
};


/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


template<class TBV>
CBDB_ExtBlobStore<TBV>::CBDB_ExtBlobStore()
: m_OpenMode(CBDB_RawFile::eReadWriteCreate),
  m_Env(0),
  m_BlobAttrDB(0),
  m_ExtStore(0),
  m_Compressor(0, eTakeOwnership),
  m_ContainerMax(64 * 1024),
  m_AttrContainer(0),
  m_LastOp(eRead)
{
    m_BlobContainer.reserve(m_ContainerMax * 2);
    m_LastFromBlobId = m_LastToBlobId = 0;
    m_STmpBlock = m_BlobIds.allocate_tempblock();
}

template<class TBV>
CBDB_ExtBlobStore<TBV>::~CBDB_ExtBlobStore()
{
    if (m_BlobAttrDB) { // save if store is still open
        try {
            if (m_OpenMode != CBDB_RawFile::eReadOnly) {
                Save();
            }
            Close();
            
        } 
        catch (std::exception& ex)
        {
            LOG_POST_XX(Bdb_Blob, 1, Error <<
                        "Exception in ~CBDB_ExtBlobStore " << ex.what());
        }
    }
    delete m_AttrContainer;
    if (m_STmpBlock) {
        m_BlobIds.free_tempblock(m_STmpBlock);
    }

}

template<class TBV>
void CBDB_ExtBlobStore<TBV>::SetCompressor(ICompression* compressor, 
                                           EOwnership    own)
{
    m_Compressor.reset(compressor, own);
}

template<class TBV>
void CBDB_ExtBlobStore<TBV>::SetStoreDataDir(const string& dir_name) 
{ 
    m_StoreDataDir = CDirEntry::AddTrailingPathSeparator(dir_name); 
}

template<class TBV>
void CBDB_ExtBlobStore<TBV>::SetStoreAttrDir(const string& dir_name)
{ 
    m_StoreAttrDir = CDirEntry::AddTrailingPathSeparator(dir_name); 
}

template<class TBV>
void CBDB_ExtBlobStore<TBV>::Close()
{
    delete m_BlobAttrDB; m_BlobAttrDB = 0;
    delete m_ExtStore; m_ExtStore = 0;
}

template<class TBV>
void CBDB_ExtBlobStore<TBV>::Save()
{
    Flush();

    typename TBitVector::statistics st1;
    m_BlobIds.optimize(0, TBV::opt_compress, &st1);
    m_CompressBuffer.resize_mem(st1.max_serialize_mem);

    size_t size = bm::serialize(m_BlobIds, 
                               (unsigned char*)&m_CompressBuffer[0],
                                m_STmpBlock, 
                                bm::BM_NO_BYTE_ORDER);
    m_CompressBuffer.resize(size);

    // create a magic record 0-0 storing the sum bit-vector

    m_BlobAttrDB->id_from = 0;
    m_BlobAttrDB->id_to = 0;

    EBDB_ErrCode ret = m_BlobAttrDB->CBDB_BLobFile::UpdateInsert(m_CompressBuffer);
    if (ret != eBDB_Ok) {
        BDB_THROW(eInvalidOperation, "Cannot save ext. blob summary");
    }
    
}

template<class TBV>
void CBDB_ExtBlobStore<TBV>::Open(const string&             storage_name, 
                                  CBDB_RawFile::EOpenMode   open_mode)
{
    Close();

    m_OpenMode = open_mode;

    // Make sure dir exists
    if (!m_StoreAttrDir.empty() && m_Env == 0) {
        CDir dir(m_StoreAttrDir);
        if ( !dir.Exists() ) {
            dir.Create();
        }
    }
    if (!m_StoreDataDir.empty()) {
        CDir dir(m_StoreDataDir);
        if ( !dir.Exists() ) {
            dir.Create();
        }
    }

    // Open attributes database
    {{
    string attr_fname;
    const string attr_fname_postf = "_ext.db";
    m_BlobAttrDB = new CBlobMetaDB;
    if (m_Env) {
        m_BlobAttrDB->SetEnv(*m_Env);
        attr_fname = storage_name + attr_fname_postf;
    } else {
        if (!m_StoreAttrDir.empty()) {
            attr_fname = m_StoreAttrDir + storage_name + attr_fname_postf;
        } 
        else {
            attr_fname = storage_name + attr_fname_postf;
        }
    }
    m_BlobAttrDB->Open(attr_fname, open_mode);
    }}

    // Open external store
    {{
    string ext_fname;
    const string ext_fname_postf = "_store.blob";
    if (!m_StoreDataDir.empty()) {
        ext_fname = m_StoreDataDir + storage_name + ext_fname_postf;
    } 
    else {
        ext_fname = storage_name + ext_fname_postf;
    }

    IOS_BASE::openmode om;
    switch (open_mode) {
    case CBDB_RawFile::eReadWriteCreate:
    case CBDB_RawFile::eReadWrite:
        om = IOS_BASE::in | IOS_BASE::out;
        break;
    case CBDB_RawFile::eReadOnly:
        om = IOS_BASE::in;
        break;
    case CBDB_RawFile::eCreate:
        om = IOS_BASE::in | IOS_BASE::out | IOS_BASE::trunc;
        break;
    default:
        _ASSERT(0);
    } // switch
    om |= IOS_BASE::binary;
    m_ExtStore = new CNcbiFstream(ext_fname.c_str(), om);
    if (!m_ExtStore->is_open() || m_ExtStore->bad()) {
        delete m_ExtStore; m_ExtStore = 0;
        BDB_THROW(eFileIO, "Cannot open file " + ext_fname);
    }
    }}

    m_BlobAttrDB->id_from = 0;
    m_BlobAttrDB->id_to = 0;

    EBDB_ErrCode ret = m_BlobAttrDB->ReadRealloc(m_CompressBuffer);
    if (ret == eBDB_Ok) {
        bm::operation_deserializer<TBitVector>::deserialize(m_BlobIds,
                                            m_CompressBuffer.data(),
                                            m_STmpBlock,
                                            bm::set_ASSIGN);

    } else {
        m_BlobIds.clear();
    }
}

template<class TBV>
void CBDB_ExtBlobStore<TBV>::StoreBlob(unsigned                     blob_id, 
                                       const CBDB_RawFile::TBuffer& buf)
{
    _ASSERT(blob_id);

    if (m_BlobIds[blob_id]) {
        string msg = 
            "External store already has BLOB id=" 
                                + NStr::UIntToString(blob_id);
        BDB_THROW(eInvalidValue, msg); 
    }

    unsigned offset;
    if (m_LastOp == eRead && m_AttrContainer) {
        delete m_AttrContainer; m_AttrContainer = 0;
    }

    m_LastOp = eWrite;

    if (!m_AttrContainer) {
        m_AttrContainer = new CBDB_BlobMetaContainer;
        m_BlobContainer.resize_mem(buf.size());
        offset = 0;
        if (buf.size()) {
            ::memcpy(m_BlobContainer.data(), buf.data(), buf.size());
        }

        CBDB_ExtBlobMap& ext_attr = m_AttrContainer->SetBlobMap();
        ext_attr.Add(blob_id, offset, buf.size());

        if (m_BlobContainer.size() > m_ContainerMax) {
            Flush();
        }
        m_BlobIds.set(blob_id);
        return;
    }

    if (buf.size() + m_BlobContainer.size() > m_ContainerMax) {
        Flush();
        this->StoreBlob(blob_id, buf);
        return;
    }

    offset = m_BlobContainer.size();
    m_BlobContainer.resize(m_BlobContainer.size() + buf.size());
    if (buf.size()) {
        ::memcpy(m_BlobContainer.data() + offset, buf.data(), buf.size());
    }
    CBDB_ExtBlobMap& ext_attr = m_AttrContainer->SetBlobMap();
    ext_attr.Add(blob_id, offset, buf.size());
    m_BlobIds.set(blob_id);
}

template<class TBV>
void CBDB_ExtBlobStore<TBV>::Flush()
{
    if (!m_AttrContainer) {
        return; // nothing to do
    }

    _ASSERT(m_ExtStore);
    _ASSERT(m_BlobAttrDB);

    CNcbiStreampos pos = m_ExtStore->tellg();
    Int8 stream_offset = NcbiStreamposToInt8(pos);


    // compress and write blob container
    //
    if (m_Compressor.get()) {
        m_CompressBuffer.resize_mem(m_BlobContainer.size() * 2);

        size_t compressed_len;
        bool compressed = 
            m_Compressor->CompressBuffer(m_BlobContainer.data(), 
                                         m_BlobContainer.size(),
                                         m_CompressBuffer.data(),
                                         m_CompressBuffer.size(),
                                         &compressed_len);
       if (!compressed) {
            BDB_THROW(eInvalidOperation, "Cannot compress BLOB");
       }

       m_ExtStore->write((char*)m_CompressBuffer.data(), 
                         compressed_len);
       m_AttrContainer->SetLoc(stream_offset, compressed_len);
    } else {
        m_ExtStore->write((char*)m_BlobContainer.data(), 
                          m_BlobContainer.size());
        m_AttrContainer->SetLoc(stream_offset, m_BlobContainer.size());
    }

    if (m_ExtStore->bad()) {
        BDB_THROW(eFileIO, "Cannot write to external store file ");
    }

    EBDB_ErrCode ret = m_BlobAttrDB->UpdateInsert(*m_AttrContainer);
    delete m_AttrContainer; m_AttrContainer = 0;
    if (ret != eBDB_Ok) {
        BDB_THROW(eInvalidOperation, "Cannot store BLOB metainfo");
    }

}

template<class TBV>
bool 
CBDB_ExtBlobStore<TBV>::x_ReadCache(unsigned               blob_id, 
                                    CBDB_RawFile::TBuffer& buf)
{
    if (m_AttrContainer) {
        // check if this call can be resolved out of the same BLOB
        // container

        if (blob_id >= m_LastFromBlobId && blob_id <= m_LastToBlobId) {
            const CBDB_ExtBlobMap& ext_attr = m_AttrContainer->GetBlobMap();
            Uint8 offset, size;
            bool has_blob = ext_attr.GetBlobLoc(blob_id, &offset, &size);
            if (has_blob) {
                // empty BLOB
                if (!size) {
                    buf.resize_mem((size_t)size);
                    return true;
                }
                buf.resize_mem((size_t)size);
                if (m_Compressor.get()) {
                    // check logicall correctness of the decompressed container
                    Uint8 sz = m_CompressBuffer.size();
                    _ASSERT(offset < sz);
                    _ASSERT(offset + size <= sz);
                    ::memcpy(buf.data(), 
                             m_CompressBuffer.data() + (size_t)offset, 
                             (size_t)size);
                } else {
                    ::memcpy(buf.data(), 
                             m_BlobContainer.data() + (size_t)offset, 
                             (size_t)size);
                }
                return true;
            }
        }
    }

    return false;
}

template<class TBV>
EBDB_ErrCode 
CBDB_ExtBlobStore<TBV>::ReadBlob(unsigned blob_id, CBDB_RawFile::TBuffer& buf)
{
    // Current implementation uses attribute container for both read and write
    // the read-after-write mechanism is underdeveloped, so you have always to
    // call Flush to turn between read and write. Something to improve in the
    // future.
    //
    if (m_LastOp == eWrite && m_AttrContainer) {
        BDB_THROW(eInvalidOperation, "Cannot read on unflushed data. ");
    }
    m_LastOp = eRead;

    if (!m_BlobIds[blob_id]) {
        return eBDB_NotFound;
    }

    // check if BLOB can be restored from the current container
    if (m_AttrContainer) {
        if (x_ReadCache(blob_id, buf)) {
            return eBDB_Ok;
        }
        delete m_AttrContainer; m_AttrContainer = 0;
    }

    // open new container
    m_AttrContainer = new CBDB_BlobMetaContainer;

    EBDB_ErrCode ret;
    ret = m_BlobAttrDB->FetchMeta(blob_id, 
                                  m_AttrContainer, 
                                  &m_LastFromBlobId, 
                                  &m_LastToBlobId);
    if (ret != eBDB_Ok) {
        delete m_AttrContainer; m_AttrContainer = 0;
        return ret;
    }

    // read the container from the external file

    {{
    Uint8 offset, size;
    m_AttrContainer->GetLoc(&offset, &size);

    CNcbiStreampos pos = NcbiInt8ToStreampos(offset);
    m_ExtStore->seekg(pos, IOS_BASE::beg);
    if (m_ExtStore->bad()) {
        BDB_THROW(eFileIO, "Cannot read from external store file ");
    }
    m_BlobContainer.resize_mem((size_t)size);
    m_ExtStore->read((char*)m_BlobContainer.data(), (size_t)size);
    if (m_ExtStore->bad()) {
        BDB_THROW(eFileIO, "Cannot read from external store file ");
    }

    if (m_Compressor.get()) {
        m_CompressBuffer.resize_mem((size_t)m_ContainerMax * 10);
        size_t dst_len;
        bool ok = m_Compressor->DecompressBuffer(m_BlobContainer.data(),
                                                 m_BlobContainer.size(),
                                                 m_CompressBuffer.data(), 
                                                 m_CompressBuffer.size(), 
                                                 &dst_len);
        if (!ok) {
            BDB_THROW(eInvalidOperation, "Cannot decompress BLOB");
        }
        m_CompressBuffer.resize(dst_len);
    }

    }}

    if (x_ReadCache(blob_id, buf)) {
        return eBDB_Ok;
    }
    delete m_AttrContainer; m_AttrContainer = 0;
    return eBDB_NotFound;
}



END_NCBI_SCOPE


#endif 
