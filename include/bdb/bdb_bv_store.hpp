#ifndef BDB___BV_STORE_BASE__HPP
#define BDB___BV_STORE_BASE__HPP

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
 * Authors:  Mike DiCuccio, Anatoliy Kuznetsov
 *
 * File Description: BDB bitvector storage
 *
 */

/// @file bdb_bv_store.hpp
/// BDB bitvector storage

#include <bdb/bdb_blob.hpp>
#include <bdb/bdb_cursor.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/bmserial.h>

#include <vector>
#include <map>

BEGIN_NCBI_SCOPE

/** @addtogroup BDB_BLOB
 *
 * @{
 */

/// Basic template class for bitvector storage
///
template<class TBV>
class CBDB_BvStore : public CBDB_BLobFile
{
public:
    typedef TBV                   TBitVector; ///< Serializable bitvector
    typedef TBuffer::value_type   TBufferValue;
public:

    /// Construction
    ///
    /// @param initial_serialization_buf_size
    ///    Amount of memory allocated for deserialization buffer
    ///
    CBDB_BvStore(unsigned initial_serialization_buf_size = 16384);
    ~CBDB_BvStore();

    /// Fetch and deserialize the *current* bitvector
    ///
    /// @param bv
    ///    Target bitvector to read from the database
    ///
    /// @note key should be initialized before calling this method
    EBDB_ErrCode ReadVector(TBitVector* bv);
	
	/// Fetch and deserialize the *current* bitvector using deserialization operation
	///
    /// @param bv
    ///    Target bitvector to read from the database
	/// @param op
	///    Logical set operation
	/// @param count
	///    Output for (bit)count set operations
	///
    /// @note key should be initialized before calling this method
	///
	EBDB_ErrCode ReadVector(TBitVector*       bv, 
	                        bm::set_operation op,
							unsigned*         count = 0);
/*
    /// Fetch a given vector and perform a logical AND 
    /// with the supplied vector
	/// 
	/// deprecated (use operation ReadVector)
	NCBI_DEPRECATED
    EBDB_ErrCode ReadVectorAnd(TBitVector* bv);

    /// Fetch a given vector and perform a logical OR 
    /// with the supplied vector
	///
	/// deprecated (use operation ReadVector)
	NCBI_DEPRECATED
    EBDB_ErrCode ReadVectorOr(TBitVector* bv);
*/
    /// Compression options for vector storage
    enum ECompact {
        eNoCompact,
        eCompact
    };

    /// Save a bitvector to the store
    /// 
    /// @param bv
    ///    Bitvector to store
    /// @param compact
    ///    Compression option
    ///
    /// @note Initialize the database key first
    EBDB_ErrCode WriteVector(const TBitVector& bv, 
                             ECompact             compact);

    /// Get access to the internal buffer
    TBuffer& GetBuffer() { return m_Buffer; }

    /// Fetch the next BLOB record to the resiable buffer
    EBDB_ErrCode FetchToBuffer(CBDB_FileCursor& cur);

    void Deserialize(TBitVector* bv, const TBufferValue* buf);

    bm::word_t* GetSerializationTempBlock();

protected:

    /// Fetch, deserialize bvector
    EBDB_ErrCode Read(TBitVector* bv, bool clear_target_vec);

    /// Read bvector, reallocate the internal buffer if necessary
    EBDB_ErrCode ReadRealloc(TBitVector* bv, bool clear_target_vec);

    /// Read buffer, reallocate if necessary
    EBDB_ErrCode ReadRealloc(TBuffer& buffer);

protected:
    /// temporary serialization buffer
    TBuffer                   m_Buffer;
    /// temporary bitset
    TBitVector                m_TmpBVec;
    /// temp block for bitvector serialization
    bm::word_t*               m_STmpBlock;
};

/////////////////////////////////////////////////////////////////////////////
///
/// Id based BV store
///
/// ID -> bvector
///
template<class TBV>
struct SBDB_BvStore_Id : public CBDB_BvStore< TBV >
{
    CBDB_FieldUint4        id;  ///< ID key

    typedef CBDB_BvStore< TBV >         TParent;
    typedef typename TParent::TBitVector TBitVector;

    SBDB_BvStore_Id(unsigned initial_serialization_buf_size = 16384)
        : TParent(initial_serialization_buf_size)
    {
        this->BindKey("id",   &id);
    }

    /// Store vector of bitvector ponters, index in the vector becoms 
    /// an id of the element
    void StoreVectorList(const vector<TBitVector*>& bv_lst);


    /// Bitcount to bvector map (bvector represents the list of ids
    /// with the same bitcount

    typedef map<unsigned, TBitVector> TBitCountMap;

    /// Utility to compute bit-count storage statistics
    /// this statistics is a map bit_count -> list of ids
    /// in other words it is a extended histogram of bitcounts
    /// It is memory prohibitive to compute the complate historgam
    /// so we take only a portion of it
    ///
    /// [bitcount_from, bitcount_to] form a closed interval defining
    /// the spectrum we study
    ///
    /// out_of_interval_ids - list of ids not falling into the mapped 
    /// spectrum
    ///

    void ComputeBitCountMap(TBitCountMap* bc_map, 
                            unsigned  bitcount_from,
                            unsigned  bitcount_to,
                            TBitVector* out_of_interval_ids = 0);

    /// Read id storage keys (id field) into bit-vector
    void ReadIds(TBitVector* id_bv);

};


/*
/////////////////////////////////////////////////////////////////////////////
///
/// Id based BV store (uses multiple volumes to store vectors)
///
/// ID -> bvector
/// The whole storage is split between several volumes
///
template<class TBV>
class CBDB_IdSplitBvStore
{
public:
    typedef TBV                   TBitVector;  ///< Serializable bitvector
    typedef SBDB_BvStore_Id<TBV>  TBvStore;
    typedef SBDB_BvStore_Id<TBV>  TBvStoreVolumeFile;
    
    /// Record size discriminator for volume splitting
    enum ERecSizeType {  
        eSmall,
        eMedium,
        eLarge
    };

    /// Every volume consists of several database files 
    /// depending on the preferable record size
    ///
    struct SVolume {
        TBvStoreVolumeFile*  db_small;
        TBvStoreVolumeFile*  db_medium;
        TBvStoreVolumeFile*  db_large;

        TBitVector*          bv_descr_small;
        TBitVector*          bv_descr_medium;
        TBitVector*          bv_descr_large;


        SVolume() 
        { 
            db_small = db_medium = db_large = 0; 
            bv_descr_small = bv_descr_medium = bv_descr_large = 0;
        }
        ~SVolume()
        { 
            delete db_small; delete db_medium; delete db_large; 
            delete bv_descr_small; delete bv_descr_medium; 
            delete bv_descr_large;
        }
    };

    /// Split statistics
    struct SplitStat
    {
        unsigned large_cnt;
        unsigned medium_cnt;
        unsigned small_cnt;
        SplitStat() 
        {
            large_cnt = medium_cnt = small_cnt = 0;
        }
    };

    typedef vector<SVolume*>  TVolumeDir;

public:
    CBDB_IdSplitBvStore();
    ~CBDB_IdSplitBvStore();

    void Open(const string&             storage_name, 
              CBDB_RawFile::EOpenMode   open_mode);

    void SetVolumeCacheSize(unsigned int cache_size) 
        { m_VolumeCacheSize = cache_size; }
    /// Associate with the environment. Should be called before opening.
    void SetEnv(CBDB_Env& env);

    /// Save volume dictionary (description bit-vectors)
    void SaveVolumeDict();

    /// Get direct volume access (use carefully)
    SVolume& GetVolume(unsigned vol_num);

    /// Get direct access to volume desriptor (use carefully)
    TBitVector& GetVolumeDescr(unsigned vol_num);

    /// Restore volume description from the volume files by
    /// scanning the database (can be very expensive)
    void LoadVolumeDescr(unsigned vol_num);

    /// Load volume descriptions 
    /// for each volume scans the databases to load volume descriptors
    /// (very expensive)
    void LoadVolumeDescr();

    /// Find first volume with the specified id
    /// #return -1 - not found
    int FindVolume(unsigned id);

    /// Return size of the volume descriptor vector
    /// In other words it is number of volumes splitter opened
    size_t VolumeSize() const { return m_VolumeDict.size(); }

    enum EScanMode {
        eScanAllVolumes,  // scan all volumes
        eScanFirstFound   // scan first volume returning result
    };
	
	/// Find the bitvector
	/// 
	/// @param id
	///     identificator
	/// @param bv
	///     Target bitvector
	/// @param op
	///     Logical operation
	/// @param count
	///     bit-count (for counting set operations)
	///
	EBDB_ErrCode ReadVector(unsigned          id, 
                            TBitVector*       bv, 
							bm::set_operation op,
							unsigned*         count,
                            EScanMode         scan_mode  = eScanAllVolumes,
                            SplitStat*        split_stat = 0);

	/// Look for bitvector in the specified slice(volume) file
	///
	/// @param v
	///    Volume file
	/// @param bv_descr
	///    Volume descriptor (contains all volume ids)
	/// @param id
	///    bitvector id to read
	/// @param bv
	///    Target bitvector
	/// @param op
	///    Logical operation
	///
    EBDB_ErrCode ReadVector(TBvStoreVolumeFile& v,
                            const TBitVector*   bv_descr, 
                            unsigned            id, 
                            TBitVector*         bv,
							bm::set_operation   op,
							unsigned*           count);


	NCBI_DEPRECATED
    EBDB_ErrCode ReadVectorOr(unsigned    id, 
                              TBitVector* bv, 
                              EScanMode   scan_mode = eScanAllVolumes,
                              SplitStat*  split_stat = 0);

	NCBI_DEPRECATED
    EBDB_ErrCode ReadVectorOr(TBvStoreVolumeFile& v,
                              const TBitVector*   bv_descr, 
                              unsigned            id, 
                              TBitVector*         bv);

    EBDB_ErrCode BlobSize(unsigned   id, 
                          size_t*    blob_size, 
                          EScanMode  scan_mode = eScanAllVolumes);

    /// Load an external store into volume storage
    void LoadStore(SBDB_BvStore_Id<TBV>& in_store,
                   unsigned volume_rec_limit = 1000000,
            unsigned blobs_total = 1024 * 1024 * 1024 + 200 * 1024 * 1024,
            bool     recompress = true);

    /// Get pointer on file environment
    /// Return NULL if no environment has been set
    CBDB_Env* GetEnv() { return m_Env; }

protected:

    typedef vector<TBitVector*>          TVolumeDict;

    /// Reload volume ditionary
    void LoadVolumeDict();
    void FreeVolumeDict();
    void CloseVolumes();
    SVolume* MountVolume(unsigned vol_num);
    virtual
    TBvStoreVolumeFile* CreateVolumeFile(ERecSizeType rsize);
    string MakeVolumeFileName(unsigned vol_num, ERecSizeType rsize);

    /// Select volume file best suited for specified record size
    TBvStoreVolumeFile* SelectVolumeFile(SVolume* vol, size_t rec_size);
private:
    CBDB_IdSplitBvStore(const CBDB_IdSplitBvStore&);
    CBDB_IdSplitBvStore& operator=(const CBDB_IdSplitBvStore&);
protected:
    unsigned            m_VolumeCacheSize;
    CBDB_Env*           m_Env;
    auto_ptr<TBvStore>  m_DescFile;    ///< Volume dictionary

    unsigned            m_VolumeCount; ///< Volume counter
    TVolumeDict         m_VolumeDict;  ///< Volume dictionary
    TVolumeDir          m_VolumeDir;   ///< Volume file directory

    string                  m_StorageName;
    CBDB_RawFile::EOpenMode m_OpenMode;
};

*/


/////////////////////////////////////////////////////////////////////////////
///
/// Matrix BV store
///
/// This template stores bit vector collections, storing the most
/// sparse elements as matrixes.
///
/// <pre>
/// Basic design:
///   The whole collection of bitvectors is sorted based on bitcount
///   For example in some cases we may end up with
///
///     bitcount  | Number of vectors
///     -----------------------------
///       1       | 100
///       2       | 150
///       3       | 12045
///       .....
///      100      | 19000
///
/// </pre>
///   In this case we can take all vectors with bitcount 1 and
///   convert then into matrix, where row is equivalent list of
///   integers (enumerated bitvector) and row is bitvectors id.
///   The result matrix in this case will be row sparsed and should
///   be encoded as a bit vector(phisical row idx = bit_count_range [0, id])
///
///  Storage consists of two things (store_type):
///    1. Row-encoding bit vector (serialization)
///    2. Integer matrix (trivial BLOB)
///
template<class TBV, class TM>
class CBDB_MatrixBvStore : public CBDB_BvStore<TBV>
{
public:

    enum EStoreType {
        eBitVector  = 0,   ///< Simple bit vector
        eDescriptor = 1,   ///< Sparse matrix descriptor
        eMatrix     = 2    ///< Sparse matrix
    };

    typedef  CBDB_BvStore<TBV> TParent;
    typedef  TBV               TBitVector;  ///< Serializable bitvector
    typedef  TM                TMatrix;     ///< Bit matrix
    typedef typename TParent::ECompact ECompact;

    /// Sparse matrix descriptor
    struct SMatrixDescr
    {
        unsigned    cols;
        unsigned    rows;
        TBitVector* descr_bv;

        SMatrixDescr(unsigned c=0, unsigned r=0, TBitVector* bv=0) 
            : cols(c), rows(r), descr_bv(bv) {}
        ~SMatrixDescr() { delete descr_bv; }
        SMatrixDescr(const SMatrixDescr& mdesc) 
            : cols(mdesc.cols),
              rows(mdesc.rows)
        {
            if (mdesc.descr_bv) {
                descr_bv = new TBitVector(*mdesc.descr_bv);
            }
        }
        SMatrixDescr& operator=(const SMatrixDescr& mdesc)
        {
            cols = mdesc.cols; rows = mdesc.rows;
            delete descr_bv; descr_bv = 0;
            if (mdesc.descr_bv) {
                descr_bv = new TBitVector(*mdesc.descr_bv);
            }
            return *this;
        }
    };

    typedef vector<SMatrixDescr> TMatrixDescrList;

public:
    // Key fields
    CBDB_FieldUint4    matr_cols;  ///< Number of columns (bit count)
    CBDB_FieldUint4    matr_rows;  ///< Number of rows (number of bvectors)
    CBDB_FieldUint4    store_type; ///< EStoreType

public:
    CBDB_MatrixBvStore(unsigned initial_serialization_buf_size = 16384);
    ~CBDB_MatrixBvStore();

    /// Save sparse matrix 
    EBDB_ErrCode InsertSparseMatrix(const TMatrix&    matr, 
                                    const TBitVector& descr_bv,
                                    ECompact          compact);

    /// Load all matrix descriptions from the store
    ///   WHERE store_type = eDescriptor
    void LoadMatrixDescriptions(TMatrixDescrList* descr_list);

private:
    CBDB_MatrixBvStore(const CBDB_MatrixBvStore&);
    CBDB_MatrixBvStore& operator=(const CBDB_MatrixBvStore&);
};

/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////

template<class TBV>
CBDB_BvStore<TBV>::CBDB_BvStore(unsigned initial_serialization_buf_size)
 : m_Buffer(initial_serialization_buf_size),
   m_STmpBlock(0)
{
}

template<class TBV>
CBDB_BvStore<TBV>::~CBDB_BvStore()
{
    if (m_STmpBlock) {
        m_TmpBVec.free_tempblock(m_STmpBlock);
    }
}

template<class TBV>
EBDB_ErrCode CBDB_BvStore<TBV>::ReadVector(TBitVector* bv)
{
    _ASSERT(bv);
    return ReadRealloc(bv, true); 
}

template<class TBV>
EBDB_ErrCode CBDB_BvStore<TBV>::ReadVector(TBitVector*       bv, 
				   	                       bm::set_operation op,
	  					                   unsigned*         count)
{
	_ASSERT(bv);
	
	EBDB_ErrCode err = CBDB_BLobFile::ReadRealloc(m_Buffer);
    if (err != eBDB_Ok) {
        return err;
    }
	
    if (m_STmpBlock == 0) {
        m_STmpBlock = m_TmpBVec.allocate_tempblock();
    }
	
	unsigned cnt = 
       bm::operation_deserializer<TBV>::deserialize(*bv,
                                                    &(m_Buffer[0]),
      	                                            m_STmpBlock,
                                                    op);
	if (count) {
		*count = cnt;
	}
	return err;
}

/*
template<class TBV>
EBDB_ErrCode CBDB_BvStore<TBV>::ReadVectorAnd(TBitVector* bv)
{
    _ASSERT(bv);
    EBDB_ErrCode err = ReadRealloc(&m_TmpBVec, true);
    if (err != eBDB_Ok) {
        return err;
    }
    *bv &= m_TmpBVec;
    return err;
}

template<class TBV>
EBDB_ErrCode CBDB_BvStore<TBV>::ReadVectorOr(TBitVector* bv)
{
    _ASSERT(bv);
    // deserilize using OR property of BM deserialization algorithm
    return ReadRealloc(bv, false); 
}
*/

template<class TBV>
bm::word_t* CBDB_BvStore<TBV>::GetSerializationTempBlock()
{
    if (m_STmpBlock == 0) {
        m_STmpBlock = m_TmpBVec.allocate_tempblock();
    }
    return m_STmpBlock;
}


template<class TBV>
EBDB_ErrCode 
CBDB_BvStore<TBV>::WriteVector(const TBitVector&  bv, 
                               ECompact           compact)
{
    if (m_STmpBlock == 0) {
        m_STmpBlock = m_TmpBVec.allocate_tempblock();
    }

    const TBitVector* bv_to_store;
    if (compact == eCompact) {
        m_TmpBVec.clear(true); // clear vector by memory deallocation
        m_TmpBVec = bv;
        m_TmpBVec.optimize();
        //m_TmpBVec.optimize_gap_size();
        bv_to_store = &m_TmpBVec;
    } else {
        bv_to_store = &bv;
    }

    typename TBitVector::statistics st1;
    bv_to_store->calc_stat(&st1);

    if (st1.max_serialize_mem > m_Buffer.size()) {
        m_Buffer.resize_mem(st1.max_serialize_mem);
    }
    size_t size = 
        bm::serialize(*bv_to_store, &m_Buffer[0], 
                      m_STmpBlock, 
                      bm::BM_NO_BYTE_ORDER | bm::BM_NO_GAP_LENGTH);
    return UpdateInsert(&m_Buffer[0], size);
}

template<class TBV>
EBDB_ErrCode CBDB_BvStore<TBV>::ReadRealloc(TBitVector* bv, 
                                            bool        clear_target_vec)
{
    EBDB_ErrCode err;
    try {
        err = Read(bv, clear_target_vec);
    } catch (CBDB_ErrnoException& ex) {
        // check if we have insufficient buffer
        if (ex.IsBufferSmall() || ex.IsNoMem()) {
            // increase the buffer and re-read
            unsigned buf_size = LobSize();
            m_Buffer.resize_mem(buf_size);
            err = Read(bv, clear_target_vec);
        } else {
            throw;
        }
    }
    return err;
}

template<class TBV>
EBDB_ErrCode CBDB_BvStore<TBV>::ReadRealloc(TBuffer& buffer)
{
    return CBDB_BLobFile::ReadRealloc(buffer);
}


template<class TBV>
EBDB_ErrCode CBDB_BvStore<TBV>::Read(TBitVector*  bv, 
                                     bool         clear_target_vec)
{
    if (m_Buffer.size() < m_Buffer.capacity()) {
        m_Buffer.resize_mem(m_Buffer.size());
    }	
	else
    if (m_Buffer.size() == 0) {
        m_Buffer.resize_mem(16384);
    }
	
    void* p = &m_Buffer[0];
    EBDB_ErrCode err = Fetch(&p, m_Buffer.size(), eReallocForbidden);
    if (err != eBDB_Ok) {
        return err;
    }
    if (clear_target_vec) {
        bv->clear(true);  // clear vector by memory deallocation
    }
    Deserialize(bv, &m_Buffer[0]);

    return err;
}

template<class TBV>
void CBDB_BvStore<TBV>::Deserialize(TBitVector* bv, const TBufferValue* buf)
{
    if (m_STmpBlock == 0) {
        m_STmpBlock = m_TmpBVec.allocate_tempblock();
    }
    bm::deserialize(*bv, buf, m_STmpBlock);
}

template<class TBV>
EBDB_ErrCode CBDB_BvStore<TBV>::FetchToBuffer(CBDB_FileCursor& cur)
{
    EBDB_ErrCode err;
    try {
        void* p = &m_Buffer[0];
        err = cur.Fetch(CBDB_FileCursor::eDefault,
                        &p, m_Buffer.size(),
                        CBDB_File::eReallocForbidden);
    }
    catch (CBDB_ErrnoException& e) {
        if (e.IsBufferSmall()  ||  e.IsNoMem()) {
            unsigned buf_size = LobSize();
            m_Buffer.resize(buf_size);
            void* p = &m_Buffer[0];
            err = cur.Fetch(CBDB_FileCursor::eDefault,
                            &p, m_Buffer.size(),
                            CBDB_File::eReallocForbidden);
        } else {
            throw;
        }
    }
    return err;
}

/*

/////////////////////////////////////////////////////////////////////////////

template<class TBV>
CBDB_IdSplitBvStore<TBV>::CBDB_IdSplitBvStore()
: m_VolumeCacheSize(0),
  m_Env(0)
{
}
 
template<class TBV>
CBDB_IdSplitBvStore<TBV>::~CBDB_IdSplitBvStore()
{
    FreeVolumeDict();
    CloseVolumes();
}

template<class TBV>
void CBDB_IdSplitBvStore<TBV>::FreeVolumeDict()
{
    NON_CONST_ITERATE(typename TVolumeDict, it, m_VolumeDict) {
        TBitVector* bv = *it;
        delete bv;
    }
    m_VolumeDict.resize(0);
}

template<class TBV>
void CBDB_IdSplitBvStore<TBV>::CloseVolumes()
{
    NON_CONST_ITERATE(typename TVolumeDir, it, m_VolumeDir) {
        delete *it;
    }
    m_VolumeDir.resize(0);
}

template<class TBV>
int CBDB_IdSplitBvStore<TBV>::FindVolume(unsigned id)
{
    int vol_idx = 0;
    ITERATE(typename TVolumeDict, it, m_VolumeDict) {
        const TBitVector* bv = *it;
        if (bv && (*bv)[id]) {
            return vol_idx;
        }
        ++vol_idx;
    }
    return -1; // not found
}

template<class TBV>
EBDB_ErrCode 
CBDB_IdSplitBvStore<TBV>::ReadVector(TBvStoreVolumeFile& v,
                        			 const TBitVector*   bv_descr, 
			                         unsigned            id, 
             			             TBitVector*         bv,
		   						     bm::set_operation   op,
			 						 unsigned*           count)
{
    // first check if description bitvector contains this id
    // pre-screening eliminates a lot of expensive BDB scans
    bool search_in_slice = bv_descr && bv_descr->test(id);
    if (search_in_slice) {
        v.id = id;
        return v.ReadVector(bv, op, count);
    }
    return eBDB_NotFound;

}

template<class TBV>
EBDB_ErrCode CBDB_IdSplitBvStore<TBV>::ReadVector(
							unsigned          id, 
                            TBitVector*       bv, 
							bm::set_operation op,
							unsigned*         count,
                            EScanMode         scan_mode,
                            SplitStat*        split_stat)

{
    EBDB_ErrCode err = eBDB_NotFound;
    for (unsigned int i = 0; i < m_VolumeDict.size(); ++i) {
        const TBitVector* bv_dict = m_VolumeDict[i];
        if (bv_dict && bv_dict->test(id)) {
            SVolume& vol = GetVolume(i);

            {{
            EBDB_ErrCode e = this->ReadVector(
										 *vol.db_large, 
                                          vol.bv_descr_large, id, bv,
										  op, count);
            if (e == eBDB_Ok) { 
                if (split_stat) {
                    split_stat->large_cnt++;
                }
                err = e;
                if (scan_mode == eScanFirstFound) break;
            }
            
            }}
            {{
            EBDB_ErrCode e = this->ReadVector(
										 *vol.db_medium,
                                          vol.bv_descr_medium,
                                          id, bv,
										  op, count);
            if (e == eBDB_Ok) { 
                if (split_stat) {
                    split_stat->medium_cnt++;
                }
                err = e;
                if (scan_mode == eScanFirstFound) break;
            }
            }}
            {{
            EBDB_ErrCode e = this->ReadVector(
			                             *vol.db_small, 
                                          vol.bv_descr_small,
                                          id, bv,
										  op, count);
            if (e == eBDB_Ok) { 
                if (split_stat) {
                    split_stat->small_cnt++;
                }
                err = e;
                if (scan_mode == eScanFirstFound) break;
            }
            }}

        }
    } // for
    return err;
}



template<class TBV>
EBDB_ErrCode 
CBDB_IdSplitBvStore<TBV>::ReadVectorOr(TBvStoreVolumeFile& v,
                                       const TBitVector*   bv_descr,
                                       unsigned            id, 
                                       TBitVector*         bv)
{
    // first check if description bitvector contains this id
    // pre-screening eliminates a lot of expensive BDB scans
    bool search_in_slice;
    if (bv_descr) {
        search_in_slice = bv_descr->test(id); 
    } else {
        search_in_slice = true;  // no descriptor vector, search in file
    }

    if (search_in_slice) {
        v.id = id;
        return v.ReadVector(bv);
    }
    return eBDB_NotFound;
}


template<class TBV>
EBDB_ErrCode 
CBDB_IdSplitBvStore<TBV>::ReadVectorOr(unsigned    id, 
                                       TBitVector* bv,
                                       EScanMode   scan_mode,
                                       SplitStat*  split_stat)
{
    EBDB_ErrCode err = eBDB_NotFound;
    for (unsigned int i = 0; i < m_VolumeDict.size(); ++i) {
        const TBitVector* bv_dict = m_VolumeDict[i];
        if (bv_dict && bv_dict->test(id)) {
            SVolume& vol = GetVolume(i);

            {{
            EBDB_ErrCode e = this->ReadVectorOr(*vol.db_large, 
                                          vol.bv_descr_large, id, bv);
            if (e == eBDB_Ok) { 
                if (split_stat) {
                    split_stat->large_cnt++;
                }
                err = e;
                if (scan_mode == eScanFirstFound) break;
            }
            
            }}
            {{
            EBDB_ErrCode e = this->ReadVectorOr(*vol.db_medium,
                                          vol.bv_descr_medium,
                                          id, bv);
            if (e == eBDB_Ok) { 
                if (split_stat) {
                    split_stat->medium_cnt++;
                }
                err = e;
                if (scan_mode == eScanFirstFound) break;
            }
            }}
            {{
            EBDB_ErrCode e = this->ReadVectorOr(*vol.db_small, 
                                          vol.bv_descr_small,
                                          id, bv);
            if (e == eBDB_Ok) { 
                if (split_stat) {
                    split_stat->small_cnt++;
                }
                err = e;
                if (scan_mode == eScanFirstFound) break;
            }
            }}

        }
    } // for
    return err;
}

template<class TBV>
EBDB_ErrCode 
CBDB_IdSplitBvStore<TBV>::BlobSize(unsigned   id, 
                                   size_t*    blob_size, 
                                   EScanMode  scan_mode)
{
    _ASSERT(blob_size);

    EBDB_ErrCode err = eBDB_NotFound;
    *blob_size = 0;
    for (unsigned int i = 0; i < m_VolumeDict.size(); ++i) {
        const TBitVector* bv_dict = m_VolumeDict[i];
        if (bv_dict && (*bv_dict)[id]) {
            SVolume& vol = GetVolume(i);

            {{
            vol.db_large->id = id;
            EBDB_ErrCode e = vol.db_large->Fetch();
            if (e == eBDB_Ok) {
                err = e;
                *blob_size += vol.db_large->LobSize();
                if (scan_mode == eScanFirstFound) break;
            }
            }}
            {{
            vol.db_medium->id = id;
            EBDB_ErrCode e = vol.db_medium->Fetch();
            if (e == eBDB_Ok) {
                err = e;
                *blob_size += vol.db_medium->LobSize();
                if (scan_mode == eScanFirstFound) break;
            }
            }}
            {{
            vol.db_small->id = id;
            EBDB_ErrCode e = vol.db_small->Fetch();
            if (e == eBDB_Ok) {
                err = e;
                *blob_size += vol.db_small->LobSize();
                if (scan_mode == eScanFirstFound) break;
            }
            }}

        }
    } // for
    return err;

}



template<class TBV>
void CBDB_IdSplitBvStore<TBV>::SetEnv(CBDB_Env& env)
{
    m_Env = &env;
}

template<class TBV>
void CBDB_IdSplitBvStore<TBV>::Open(const string&    storage_name,
                            CBDB_RawFile::EOpenMode  open_mode)
{
    CloseVolumes();

    m_DescFile.reset(new TBvStore);
    if (m_Env) {
        m_DescFile->SetEnv(*m_Env);
    }
    m_StorageName = storage_name; 
    string dict_fname(m_StorageName);
    dict_fname.append(".dict");

    m_DescFile->Open(dict_fname.c_str(), open_mode);
    m_OpenMode = open_mode;
    LoadVolumeDict();

    m_VolumeDir.resize(m_VolumeDict.size());
    NON_CONST_ITERATE(typename TVolumeDir, it, m_VolumeDir) {
        *it = 0;
    }
}

template<class TBV>
void CBDB_IdSplitBvStore<TBV>::LoadVolumeDict()
{
    _ASSERT(m_DescFile.get());

    FreeVolumeDict();

    EBDB_ErrCode err;

    CBDB_FileCursor cur(*m_DescFile);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    typename TBvStore::TBuffer& buf = m_DescFile->GetBuffer();

    for (m_VolumeCount = 0; ;++m_VolumeCount) {
        err = m_DescFile->FetchToBuffer(cur);
        if (err != eBDB_Ok) {
            break;
        }
        auto_ptr<TBitVector>  bv(new TBitVector(bm::BM_GAP));
        m_DescFile->Deserialize(bv.get(), &buf[0]);

        m_VolumeDict.push_back(bv.release());

    } // for
}

template<class TBV>
typename CBDB_IdSplitBvStore<TBV>::TBitVector& 
CBDB_IdSplitBvStore<TBV>::GetVolumeDescr(unsigned vol_num)
{
    while (vol_num >= m_VolumeDict.size()) {
        m_VolumeDict.push_back(0);
    }
    typename CBDB_IdSplitBvStore<TBV>::TBitVector* bv = m_VolumeDict[vol_num];
    if (!bv) {
        m_VolumeDict[vol_num] = bv = new TBitVector(bm::BM_GAP);
    }
    return *bv;
}

template<class TBV>
void CBDB_IdSplitBvStore<TBV>::SaveVolumeDict()
{
    m_DescFile->StoreVectorList(m_VolumeDict);
}


template<class TBV>
typename CBDB_IdSplitBvStore<TBV>::SVolume* 
CBDB_IdSplitBvStore<TBV>::MountVolume(unsigned vol_num)
{
    SVolume* vol = m_VolumeDir[vol_num];
    if (vol) {
        delete vol; vol = 0;
    }

    auto_ptr<SVolume> v_ptr(new SVolume);
    v_ptr->db_small = CreateVolumeFile(eSmall);
    v_ptr->db_medium = CreateVolumeFile(eMedium);
    v_ptr->db_large = CreateVolumeFile(eLarge);

    string fname;
    fname = MakeVolumeFileName(vol_num, eSmall);
    v_ptr->db_small->Open(fname.c_str(), m_OpenMode);
    fname = MakeVolumeFileName(vol_num, eMedium);
    v_ptr->db_medium->Open(fname.c_str(), m_OpenMode);
    fname = MakeVolumeFileName(vol_num, eLarge);
    v_ptr->db_large->Open(fname.c_str(), m_OpenMode);

    m_VolumeDir[vol_num] = vol= v_ptr.release();
    return vol;
}

template<class TBV>
typename CBDB_IdSplitBvStore<TBV>::SVolume&
CBDB_IdSplitBvStore<TBV>::GetVolume(unsigned vol_num)
{
    while (vol_num >= m_VolumeDir.size()) {
        m_VolumeDir.push_back(0);
    }

    SVolume* vol = m_VolumeDir[vol_num];
    if (!vol) {
        return *MountVolume(vol_num);
    }
    return *vol;
}

template<class TBV>
string CBDB_IdSplitBvStore<TBV>::MakeVolumeFileName(unsigned vol_num, 
                                                    ERecSizeType rsize)
{
    string fname = m_StorageName;
    fname.append("." + NStr::UIntToString(vol_num) + "_");
    switch (rsize) {
    case eSmall:
        fname.append("sml");
        break;
    case eMedium:
        fname.append("med");
        break;
    case eLarge:
        fname.append("big");
        break;
    default:
        _ASSERT(0);
    }
    return fname;
}

template<class TBV>
typename CBDB_IdSplitBvStore<TBV>::TBvStoreVolumeFile* 
CBDB_IdSplitBvStore<TBV>::CreateVolumeFile(ERecSizeType rsize)
{
    TBvStoreVolumeFile* vol = new TBvStoreVolumeFile;
    if (m_Env) {
        vol->SetEnv(*m_Env);
    } else {
        if (m_VolumeCacheSize && (rsize != eSmall)) {
            vol->SetCacheSize(m_VolumeCacheSize);
        }
    }
    if (rsize == eLarge) {
        vol->SetPageSize(32 * 1024);
    } else {
        if (rsize == eMedium) {
            vol->SetPageSize(16 * 1024);
        }
    }
    return vol;
}

template<class TBV>
typename CBDB_IdSplitBvStore<TBV>::TBvStoreVolumeFile* 
CBDB_IdSplitBvStore<TBV>::SelectVolumeFile(SVolume* vol, size_t rec_size)
{
    if (rec_size <= 384) {
        return vol->db_small;
    }
    if (rec_size <= 4096) {
        return vol->db_medium;
    }
    return vol->db_large;
}

template<class TBV>
void CBDB_IdSplitBvStore<TBV>::LoadVolumeDescr()
{
    for (unsigned int i = 0; i < m_VolumeDict.size(); ++i) {
        this->LoadVolumeDescr(i);
    }
}

template<class TBV>
void CBDB_IdSplitBvStore<TBV>::LoadVolumeDescr(unsigned vol_num)
{
    SVolume& vol = this->GetVolume(vol_num);

    if (vol.bv_descr_small == 0) {
        vol.bv_descr_small = new TBitVector(bm::BM_GAP);
    } else {
        vol.bv_descr_small->clear(true);
    }
    if (vol.bv_descr_medium == 0) {
        vol.bv_descr_medium = new TBitVector(bm::BM_GAP);
    } else {
        vol.bv_descr_medium->clear(true);
    }
    if (vol.bv_descr_large == 0) {
        vol.bv_descr_large = new TBitVector(bm::BM_GAP);
    } else {
        vol.bv_descr_large->clear(true);
    }

    vol.db_small->ReadIds(vol.bv_descr_small);
    vol.db_medium->ReadIds(vol.bv_descr_medium);
    vol.db_large->ReadIds(vol.bv_descr_large);

}



template<class TBV>
void CBDB_IdSplitBvStore<TBV>::LoadStore(SBDB_BvStore_Id<TBV>& in_store,
                                         unsigned              volume_rec_limit,
                                         unsigned              blobs_total,
                                         bool                  recompress)
{
    unsigned rec_cnt = 0;
    unsigned vol_cnt = 0;

    EBDB_ErrCode err;

    CBDB_FileCursor cur(in_store);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    unsigned curr_vol_idx = 0;
    TBitVector* vol_descr_bv = 0;
    SVolume* vol = 0;

    TBitVector bv_tmp(bm::BM_GAP);
    bm::word_t* ser_tmp = in_store.GetSerializationTempBlock();

    do {
        err = in_store.FetchToBuffer(cur);
        if (err != eBDB_Ok)
            break;
        unsigned id = in_store.id;
        typename SBDB_BvStore_Id<TBV>::TBuffer& buf = in_store.GetBuffer();

        // save the buffer into volume

        if (vol_descr_bv == 0) {
            vol_descr_bv = &GetVolumeDescr(curr_vol_idx);
            vol          = &GetVolume(curr_vol_idx);
        }

        
        vol_descr_bv->set(id);
        
        unsigned blob_size = in_store.LobSize();
        typename SBDB_BvStore_Id<TBV>::TBuffer::value_type* blob_ptr = &buf[0];

        if (recompress) { // try to re-compress the vector
            bv_tmp.clear(true);
            in_store.Deserialize(&bv_tmp, blob_ptr);
            typename TBitVector::statistics st;
            bv_tmp.calc_stat(&st);
            if (buf.size() < st.max_serialize_mem) {
                buf.resize(st.max_serialize_mem);
                blob_ptr = &buf[0];
            }
            blob_size = bm::serialize(bv_tmp, blob_ptr, 
                                      ser_tmp, bm::BM_NO_BYTE_ORDER);
        }


        TBvStoreVolumeFile* vol_file = SelectVolumeFile(vol, blob_size);
        
        vol_file->id = id;
        vol_file->UpdateInsert(blob_ptr, blob_size);

        vol_cnt += blob_size;        
        if ((++rec_cnt == volume_rec_limit && volume_rec_limit != 0) || 
            (blobs_total != 0 && vol_cnt > blobs_total)) {
            LOG_POST("BDB Split volume: " << curr_vol_idx);
            ++curr_vol_idx;
            rec_cnt = vol_cnt = 0; vol_descr_bv = 0; vol_file = 0;
        }

    } while (true);

    SaveVolumeDict();
}
*/

/////////////////////////////////////////////////////////////////////////////

template<class TBV>
void SBDB_BvStore_Id<TBV>::StoreVectorList(const vector<TBitVector*>& bv_lst)
{
    for (size_t i = 0; i < bv_lst.size(); ++i)
    {
        id = i;
        const TBitVector* bv = bv_lst[i];
        WriteVector(*bv, TParent::eCompact);
    }
}

template<class TBV>
void SBDB_BvStore_Id<TBV>::ReadIds(TBitVector* id_bv)
{
    _ASSERT(id_bv);

    EBDB_ErrCode err;

    CBDB_FileCursor cur(*this);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    TBitVector bv(bm::BM_GAP);
	
    while (true) {
        try {
            err = cur.Fetch();
            if (err != eBDB_Ok) {
                break;
            }
            unsigned bv_id = id;
            id_bv->set(bv_id);
        }
        catch (CBDB_ErrnoException& e) {
            if (e.IsBufferSmall()  ||  e.IsNoMem()) {
                unsigned bv_id = id;
                id_bv->set(bv_id);
                cur.SetCondition(CBDB_FileCursor::eGE);
                cur.From << ++bv_id;
                continue;
            } else {
                throw;
            }
        }

    } // while
}




template<class TBV>
void 
SBDB_BvStore_Id<TBV>::ComputeBitCountMap(TBitCountMap* bc_map, 
                                         unsigned    bitcount_from,
                                         unsigned    bitcount_to,
                                         TBitVector*   out_of_interval_ids)
{
    _ASSERT(bc_map);

    if (this->m_STmpBlock == 0) {
        this->m_STmpBlock = this->m_TmpBVec.allocate_tempblock();
    }

    EBDB_ErrCode err;

    CBDB_FileCursor cur(*this);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0;

    TBitVector bv(bm::BM_GAP);

    while (true) {
        void* p = &this->m_Buffer[0];
        try {
            err = cur.Fetch(CBDB_FileCursor::eDefault,
                            &p, this->m_Buffer.size(),
                            CBDB_File::eReallocForbidden);
            if (err != eBDB_Ok) {
                break;
            }
        }
        catch (CBDB_ErrnoException& e) {
            if (e.IsBufferSmall()  ||  e.IsNoMem()) {
                unsigned buf_size = this->LobSize();
                unsigned bv_id = id;
                if (out_of_interval_ids) {
                    out_of_interval_ids->set(bv_id);
                }
/*
                cur.SetCondition(CBDB_FileCursor::eGE);
                cur.From << ++bv_id;
                continue;
*/
                this->m_Buffer.resize(buf_size);
                void* p = &this->m_Buffer[0];
                err = cur.Fetch(CBDB_FileCursor::eDefault,
                                &p, this->m_Buffer.size(),
                                CBDB_File::eReallocForbidden);
            } else {
                throw;
            }
        }
        unsigned bv_id = id;
        {{
            bv.clear(true);
            bm::deserialize(bv, &this->m_Buffer[0], this->m_STmpBlock);

            unsigned bitcount = bv.count();
            if (bitcount >= bitcount_from && bitcount <= bitcount_to) {
                TBitVector& map_vector = (*bc_map)[bitcount];
                map_vector.set(bv_id);
            } else {
                if (out_of_interval_ids) {
                    out_of_interval_ids->set(bv_id);
                }
            }
        }}


    } // while

}



/////////////////////////////////////////////////////////////////////////////


template<class TBV, class TM>
CBDB_MatrixBvStore<TBV, TM>::CBDB_MatrixBvStore(unsigned initial_buf_size)
: TParent(initial_buf_size)
{
    this->BindKey("matr_cols",  &matr_cols);
    this->BindKey("matr_rows",  &matr_rows);
    this->BindKey("store_type", &store_type);
}

template<class TBV, class TM>
CBDB_MatrixBvStore<TBV, TM>::~CBDB_MatrixBvStore()
{}

template<class TBV, class TM>
EBDB_ErrCode 
CBDB_MatrixBvStore<TBV, TM>::InsertSparseMatrix(const TMatrix&    matr, 
                                                const TBitVector& descr_bv,
                                                ECompact          compact)
{
    EBDB_ErrCode err;

    // store the descriptor

    matr_cols = matr.cols();
    matr_rows = matr.rows();
    store_type = (unsigned) eDescriptor;
    err = TParent::WriteVector(descr_bv, compact);
    if (err != eBDB_Ok) {
        return err;
    }

    // store the matrix


    matr_cols = matr.cols();
    matr_rows = matr.rows();
    store_type = (unsigned) eMatrix;

    unsigned msize = 
        matr.cols() * matr.rows() * sizeof(TMatrix::value_type);

    err = UpdateInsert(matr.data(), msize);
    return err;
}

template<class TBV, class TM>
void CBDB_MatrixBvStore<TBV, TM>::LoadMatrixDescriptions(
                                            TMatrixDescrList* descr_list)
{
    _ASSERT(descr_list);
    descr_list->resize(0);
    EBDB_ErrCode err;

    if (this->m_Buffer.size() == 0) {
        this->m_Buffer.resize(16384);
    }

    CBDB_FileCursor cur(*this);
    cur.SetCondition(CBDB_FileCursor::eGE);
    cur.From << 0 << 0 << 0;

    while (true) {
        void* p = &this->m_Buffer[0];
        try {
            err = cur.Fetch(CBDB_FileCursor::eDefault,
                            &p, this->m_Buffer.size(),
                            CBDB_File::eReallocForbidden);
        }
        catch (CBDB_ErrnoException& e) {
            if (e.IsBufferSmall()  ||  e.IsNoMem()) {
                unsigned buf_size = this->LobSize();
                this->m_Buffer.resize(buf_size);
                void* p = &this->m_Buffer[0];
                err = cur.Fetch(CBDB_FileCursor::eCurrent,
                                &p, this->m_Buffer.size(),
                                CBDB_File::eReallocForbidden);
            } else {
                throw;
            }
        }

        unsigned cols = matr_cols;
        unsigned rows = matr_rows;
        TBitVector* bv = new TBitVector(bm::BM_GAP);
        descr_list->push_back(SMatrixDescr(cols, rows, bv));

        bm::deserialize(*bv, &this->m_Buffer[0]);

    } // while
}



END_NCBI_SCOPE


#endif  // BDB___BV_STORE_BASE__HPP
