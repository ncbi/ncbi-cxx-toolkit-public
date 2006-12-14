#ifndef BDB___BV_DICT_IDX__HPP
#define BDB___BV_DICT_IDX__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <util/bitset/ncbi_bitset.hpp>
#include <bdb/bdb_dict_store.hpp>

BEGIN_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////


template <typename Key,
          typename Dictionary = CBDB_BlobDictionary<Key>,
          typename BvStore    = CBDB_PersistentSplitStore< bm::bvector<> >,
          typename BV         = bm::bvector<> >
class CBDB_BvSplitDictStore : public CBDB_BlobDictStore<Key, Dictionary, BvStore>
{
public:
    typedef CBDB_BlobDictStore<Key, Dictionary, BvStore> TParent;
    typedef Key   TKey;
    typedef Uint4 TKeyId;
    typedef BV    TBitVector;

    enum EReadOp {
        eOp_Replace,
        eOp_Or,
        eOp_And,
        eOp_Not,
    };

    CBDB_BvSplitDictStore(const string& demux_path = kEmptyStr);
    CBDB_BvSplitDictStore(Dictionary& dict, BvStore& store,
                      EOwnership own = eTakeOwnership);
    ~CBDB_BvSplitDictStore();

    /// read a vector by the relevant key, performing a logical operation
    EBDB_ErrCode ReadVector(const TKey& key, TBitVector* bv,
                            EReadOp op = eOp_Replace);
    EBDB_ErrCode ReadVectorOr (const TKey& key, TBitVector* bv);
    EBDB_ErrCode ReadVectorAnd(const TKey& key, TBitVector* bv);

    /// read a vector by the relevant key ID, performing a logical operation
    /// these are useful in situations in which the key ID is known beforehand
    EBDB_ErrCode ReadVectorById(TKeyId key, TBitVector* bv,
                                EReadOp op = eOp_Replace);
    EBDB_ErrCode ReadVectorOrById (TKeyId key, TBitVector* bv);
    EBDB_ErrCode ReadVectorAndById(TKeyId key, TBitVector* bv);

    enum ECompact {
        eCompact,
        eNoCompact
    };
    EBDB_ErrCode WriteVector(const TKey& key, const TBitVector& bv,
                             ECompact compact);

protected:

    /// Fetch, deserialize bvector
    void Deserialize(TBitVector* bv,
                     CBDB_RawFile::TBuffer::value_type* buf,
                     EReadOp op);

private:
    /// private data buffer
    CBDB_RawFile::TBuffer m_Buffer;

    /// temporary bit-vector for write operations
    TBitVector m_TmpVec;

    /// temp block for bitvector serialization
    bm::word_t*   m_STmpBlock;
};


/////////////////////////////////////////////////////////////////////////////


template <typename Key, typename Dictionary, typename BvStore, typename BV>
inline CBDB_BvSplitDictStore<Key, Dictionary, BvStore, BV>
::CBDB_BvSplitDictStore(const string& demux_path)
: TParent(demux_path)
, m_STmpBlock(NULL)
{
}


template <typename Key, typename Dictionary, typename BvStore, typename BV>
inline CBDB_BvSplitDictStore<Key, Dictionary, BvStore, BV>
::CBDB_BvSplitDictStore(Dictionary& dict,
                    BvStore& store,
                    EOwnership own)
: TParent(dict, store, own)
, m_STmpBlock(NULL)
{
}


template <typename Key, typename Dictionary, typename BvStore, typename BV>
inline
CBDB_BvSplitDictStore<Key, Dictionary, BvStore, BV>::~CBDB_BvSplitDictStore()
{
    if (m_STmpBlock) {
        m_TmpVec.free_tempblock(m_STmpBlock);
    }
}


///
/// Internal read routines
///

template<typename Key, typename Dictionary, typename BvStore, typename BV>
inline void
CBDB_BvSplitDictStore<Key, Dictionary, BvStore, BV>::Deserialize(TBitVector* bv,
                                                             CBDB_RawFile::TBuffer::value_type* buf,
                                                             EReadOp op)
{
    _ASSERT(bv);
    if ( !m_STmpBlock ) {
        m_STmpBlock = m_TmpVec.allocate_tempblock();
    }
    switch (op) {
    case eOp_Replace:
        bv->clear(true);
        bm::deserialize(*bv, (const unsigned char*)buf, m_STmpBlock);
        break;

    case eOp_Or:
        bm::deserialize(*bv, (const unsigned char*)buf, m_STmpBlock);
        break;

    case eOp_And:
        {{
             m_TmpVec.clear(true);
             bm::deserialize(m_TmpVec, (const unsigned char*)buf, m_STmpBlock);
             *bv &= m_TmpVec;
         }}
        break;

    default:
        _ASSERT(false);
        NCBI_THROW(CException, eUnknown,
                   "Deserialize(): unknown op");
    }
}


///
/// Public read interface
/// These interfaces read values given a key
///
template <typename Key, typename Dictionary, typename BvStore, typename BV>
inline EBDB_ErrCode
CBDB_BvSplitDictStore<Key, Dictionary, BvStore, BV>::ReadVector(const Key& key,
                                                            TBitVector* bv,
                                                            EReadOp op)
{
    _ASSERT(bv);
    EBDB_ErrCode err = eBDB_NotFound;
    if (bv) {
        err = Read(key, m_Buffer);
        if (err == eBDB_Ok) {
            Deserialize(bv, &m_Buffer[0], op);
        }
    }
    return err;
}


template <typename Key, typename Dictionary, typename BvStore, typename BV>
inline EBDB_ErrCode
CBDB_BvSplitDictStore<Key, Dictionary, BvStore, BV>::ReadVectorOr(const Key& key,
                                                              TBitVector* bv)
{
    return ReadVector(key, bv, eOp_Or);
}


template <typename Key, typename Dictionary, typename BvStore, typename BV>
inline EBDB_ErrCode
CBDB_BvSplitDictStore<Key, Dictionary, BvStore, BV>::ReadVectorAnd(const Key& key,
                                                               TBitVector* bv)
{
    return ReadVector(key, bv, eOp_And);
}


///
/// These interfaces read given a key ID
///

template <typename Key, typename Dictionary, typename BvStore, typename BV>
inline EBDB_ErrCode
CBDB_BvSplitDictStore<Key, Dictionary, BvStore, BV>::ReadVectorById(TKeyId key,
                                                                TBitVector* bv,
                                                                EReadOp op)
{
    _ASSERT(bv);
    EBDB_ErrCode err = eBDB_NotFound;
    if (bv) {
        err = TParent::ReadById(key, m_Buffer);
        if (err == eBDB_Ok) {
            Deserialize(bv, &m_Buffer[0], op);
        }
    }
    return err;
}


template <typename Key, typename Dictionary, typename BvStore, typename BV>
inline EBDB_ErrCode
CBDB_BvSplitDictStore<Key, Dictionary, BvStore, BV>::ReadVectorOrById(TKeyId key,
                                                                  TBitVector* bv)
{
    return ReadVectorById(key, bv, eOp_Or);
}


template <typename Key, typename Dictionary, typename BvStore, typename BV>
inline EBDB_ErrCode
CBDB_BvSplitDictStore<Key, Dictionary, BvStore, BV>::ReadVectorAndById(TKeyId key,
                                                                   TBitVector* bv)
{
    return ReadVectorById(key, bv, eOp_And);
}


///
/// Public write interface
///

template <typename Key, typename Dictionary, typename BvStore, typename BV>
inline EBDB_ErrCode
CBDB_BvSplitDictStore<Key, Dictionary, BvStore, BV>::WriteVector(const Key& key,
                                                             const TBitVector& bv,
                                                             ECompact compact)
{
    if ( !m_STmpBlock ) {
        m_STmpBlock = m_TmpVec.allocate_tempblock();
    }

    const TBitVector* bv_to_store = &bv;
    if (compact == eCompact) {
        m_TmpVec.clear(true); // clear vector by memory deallocation
        m_TmpVec = bv;
        m_TmpVec.optimize();
        m_TmpVec.optimize_gap_size();
        bv_to_store = &m_TmpVec;
    }

    typename TBitVector::statistics st1;
    bv_to_store->calc_stat(&st1);

    if (st1.max_serialize_mem > m_Buffer.size()) {
        m_Buffer.resize(st1.max_serialize_mem);
    }
    size_t size = bm::serialize(*bv_to_store, (unsigned char*)&m_Buffer[0], 
                                m_STmpBlock, bm::BM_NO_BYTE_ORDER);
    m_Buffer.resize(size);
    return Write(key, m_Buffer);
}



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/12/14 13:11:31  dicuccio
 * Added components for creating split blob stores with arbitrarily defined key
 * spaces (including multi-component keys)
 *
 * Revision 1.6  2006/12/05 19:45:58  dicuccio
 * Drop unneeded LOG_POST
 *
 * Revision 1.5  2006/12/05 19:34:49  dicuccio
 * Wrap long lines.  Use BV split store shim
 *
 * Revision 1.4  2006/12/04 12:55:02  dicuccio
 * Added template parameter for underlying BV store
 *
 * Revision 1.3  2006/12/01 13:16:38  dicuccio
 * Use standard buffer types.  Permit dictionary to be supplied externally
 *
 * Revision 1.2  2006/11/28 17:11:31  dicuccio
 * Use enumerated bv op type for serialization
 *
 * Revision 1.1  2006/11/21 13:52:39  dicuccio
 * Large-scale reorganization of indexing.  Support multiple index types, split
 * dictionary-based index stores.
 *
 * ===========================================================================
 */

#endif  // BDB___BV_DICT_IDX__HPP
