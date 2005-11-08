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
#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/bmserial.h>
#include <vector>

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
    typedef  TBV  TBitVector;  ///< Serializable bitvector
public:

    /// Construction
    ///
    /// @param initial_serialization_buf_size
    ///    Amount of memory allocated for deserialization buffer
    ///
    CBDB_BvStore(unsigned initial_serialization_buf_size = 16384);

    /// Fetch and deserialize the *current* bitvector
    ///
    /// @param bv
    ///    Target bitvector to read from the database
    ///
    /// @note key should be initialized before calling this method
    EBDB_ErrCode ReadVector(TBitVector* bv);

    /// Fetch a given vector and perform a logical AND 
    /// with the supplied vector
    EBDB_ErrCode ReadVectorAnd(TBitVector* bv);

    /// Fetch a given vector and perform a logical OR 
    /// with the supplied vector
    EBDB_ErrCode ReadVectorOr(TBitVector* bv);

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
protected:

    /// Fetch, deserialize bvector
    EBDB_ErrCode Read(TBitVector* bv, bool clear_target_vec);

    /// Read bvector, reallocate the internal buffer if necessary
    EBDB_ErrCode ReadRealloc(TBitVector* bv, bool clear_target_vec);

private:
    /// temporary serialization buffer
    vector<unsigned char>     m_Buffer;
    /// temporary bitset
    TBitVector                m_TmpBVec;
};

/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////

template<class TBV>
CBDB_BvStore<TBV>::CBDB_BvStore(unsigned initial_serialization_buf_size)
 : m_Buffer(initial_serialization_buf_size)
{
}

template<class TBV>
EBDB_ErrCode CBDB_BvStore<TBV>::ReadVector(TBitVector* bv)
{
    _ASSERT(bv);
    return ReadRealloc(bv, true); 
}

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

template<class TBV>
EBDB_ErrCode 
CBDB_BvStore<TBV>::WriteVector(const TBitVector&            bv, 
                               CBDB_BvStore<TBV>::ECompact  compact)
{
    TBitVector bv_to_store;
    if (compact == eCompact) {
        m_TmpBVec.clear(true);
        m_TmpBVec = bv;
        m_TmpBVec.optimize();
        m_TmpBVec.optimize_gap_size();
        bv_to_store = m_TmpBVec;
    } else {
        bv_to_store = bv;
    }

    struct TBitVector::statistics st1;
    bv_to_store.calc_stat(&st1);

    if (st1.max_serialize_mem > m_Buffer.size()) {
        m_Buffer.resize(st1.max_serialize_mem);
    }
    size_t size = bm::serialize(bv_to_store, &m_Buffer[0]);
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
        if (ex.IsNoMem()) {
            // increase the buffer and re-read
            unsigned buf_size = LobSize();
            m_Buffer.resize(buf_size);
            err = Read(bv, clear_target_vec);
        }
        throw;
    }
    return err;
}


template<class TBV>
EBDB_ErrCode CBDB_BvStore<TBV>::Read(TBitVector*  bv, 
                                     bool         clear_target_vec)
{
    if (m_Buffer.size() == 0) {
        m_Buffer.resize(16384);
    }
    void* p = &m_Buffer[0];
    EBDB_ErrCode err = Fetch(&p, m_Buffer.size(), eReallocForbidden);
    if (err != eBDB_Ok) {
        return err;
    }
    if (clear_target_vec) {
        bv->clear(true);  // clear vector by memory deallocation
    }
    bm::deserialize(*bv, &m_Buffer[0]);

    return err;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2005/11/08 01:48:23  dicuccio
 * Compilation fixes: use correct include path; remove erroneous ';'.
 * WriteVector(): bm::serialize() expects non-const bit-vector, so copy prior to
 * writing
 *
 * Revision 1.1  2005/11/07 19:37:55  kuznets
 * Initial revision of templetized BV storage
 *
 *
 * Revision 1.1  2005/11/04 01:55:13  dicuccio
 * Copy over from text mining libraries
 *
 * ===========================================================================
 */

#endif  // BDB___BV_STORE_BASE__HPP
