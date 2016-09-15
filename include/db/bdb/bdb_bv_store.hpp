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

#include <db/bdb/bdb_blob.hpp>
#include <db/bdb/bdb_cursor.hpp>

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

    /// Fetch and deserialize the *current* bitvector using deserialization
    /// operation
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
    typename TBitVector::statistics st1;
    if (compact == eCompact) {
        m_TmpBVec.clear(true); // clear vector by memory deallocation
        m_TmpBVec = bv;
        m_TmpBVec.optimize(0, TBitVector::opt_compress, &st1);
        bv_to_store = &m_TmpBVec;
    } else {
        bv_to_store = &bv;
        bv_to_store->calc_stat(&st1);
    }

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
        m_Buffer.resize_mem(m_Buffer.capacity());
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
            size_t buf_size = LobSize();
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
                p = &this->m_Buffer[0];
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
                p = &this->m_Buffer[0];
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
