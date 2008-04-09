#ifndef BDB___BV_PROPERTY_STORE__HPP
#define BDB___BV_PROPERTY_STORE__HPP

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


#include <bdb/bdb_bv_split_store.hpp>
#include <bdb/bdb_types.hpp>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////

template <class PropKey, class PropValue>
class CBDB_PropertyDictionary : public CBDB_File
{
public:
    typedef pair<PropKey, PropValue> TKey;
    typedef Uint4                    TKeyId;

    CBDB_PropertyDictionary(Uint4 last_uid = 0);

    /// @name Interface required for split dictionary store
    /// @{

    Uint4 GetKey(const TKey& key);
    Uint4 PutKey(const TKey& key);

    /// @}

    TKey   GetCurrentKey() const;
    TKeyId GetCurrentUid() const;
    TKeyId GetLastUid() const;

    EBDB_ErrCode Read(TKeyId* key_idx);
    EBDB_ErrCode Read(const TKey& key, TKeyId* key_idx);
    EBDB_ErrCode Read(const PropKey& prop, const PropValue& value,
                      TKeyId* key_idx);
    EBDB_ErrCode Write(const TKey& key, TKeyId key_idx);
    EBDB_ErrCode Write(const PropKey& prop,
                       const PropValue& value,
                       TKeyId key_idx);

private:
    typename SBDB_TypeTraits<PropKey>::TFieldType   m_PropKey;
    typename SBDB_TypeTraits<PropValue>::TFieldType m_PropVal;
    typename SBDB_TypeTraits<TKeyId>::TFieldType    m_Uid;

    TKeyId m_LastUid;
};



//////////////////////////////////////////////////////////////////////////////

template <typename PropKey, typename PropValue,
          typename Dictionary = CBDB_PropertyDictionary<PropKey, PropValue>,
          typename BvStore    = CBDB_PersistentSplitStore< bm::bvector<> >,
          typename BV         = bm::bvector<>
          >
class CBDB_BvPropertyStore
    : public CBDB_BvSplitDictStore< pair<PropKey, PropValue>,
                                    Dictionary, BvStore, BV >
{
public:
    typedef CBDB_BvSplitDictStore< pair<PropKey, PropValue>,
                                   Dictionary, BvStore, BV > TParent;
    typedef pair<PropKey, PropValue> TKey;
    typedef Dictionary               TDictionary;
    typedef BvStore                  TStore;
    typedef BV                       TBitVector;
    typedef Uint4                    TKeyId;

    CBDB_BvPropertyStore(const string& demux_path = kEmptyStr);
    CBDB_BvPropertyStore(TDictionary& dict, TStore& store,
                         EOwnership own = eTakeOwnership);
};


/////////////////////////////////////////////////////////////////////////////


template <class PropKey, class PropValue>
inline
CBDB_PropertyDictionary<PropKey, PropValue>::CBDB_PropertyDictionary(Uint4 last_uid)
    : m_LastUid(last_uid)
{
    BindKey ("key", &m_PropKey);
    BindKey ("val", &m_PropVal);
    BindData("uid", &m_Uid);
}


template <class PropKey, class PropValue>
inline
typename CBDB_PropertyDictionary<PropKey, PropValue>::TKey
CBDB_PropertyDictionary<PropKey, PropValue>::GetCurrentKey() const
{
    TKey key((PropKey)m_PropKey, (PropValue)m_PropVal);
    return key;
}


template <class PropKey, class PropValue>
inline
typename CBDB_PropertyDictionary<PropKey, PropValue>::TKeyId
CBDB_PropertyDictionary<PropKey, PropValue>::GetCurrentUid() const
{
    return (TKeyId)m_Uid;
}


template <class PropKey, class PropValue>
inline
typename CBDB_PropertyDictionary<PropKey, PropValue>::TKeyId
CBDB_PropertyDictionary<PropKey, PropValue>::GetLastUid() const
{
    return m_LastUid;
}


template <class PropKey, class PropValue>
inline
Uint4 CBDB_PropertyDictionary<PropKey, PropValue>::GetKey(const TKey& key)
{
    TKeyId uid = 0;
    Read(key.first, key.second, &uid);
    return uid;
}


template <class PropKey, class PropValue>
inline
Uint4 CBDB_PropertyDictionary<PropKey, PropValue>::PutKey(const TKey& key)
{
    Uint4 uid = GetKey(key);
    if (uid) {
        return uid;
    }

    ++m_LastUid;
    uid = m_LastUid;
    if (uid) {
        if (Write(key, uid) != eBDB_Ok) {
            uid = 0;
        }
    }
    return uid;
}


template <class PropKey, class PropValue>
inline EBDB_ErrCode
CBDB_PropertyDictionary<PropKey, PropValue>::Read(TKeyId* key_idx)
{
    EBDB_ErrCode err = Fetch();
    if (err == eBDB_Ok  &&  key_idx) {
        *key_idx = m_Uid;
    }
    return err;
}


template <class PropKey, class PropValue>
inline EBDB_ErrCode
CBDB_PropertyDictionary<PropKey, PropValue>::Read(const TKey& key,
                                                  TKeyId* key_idx)
{
    m_PropKey = key.first;
    m_PropVal = key.second;
    return Read(key_idx);
}


template <class PropKey, class PropValue>
inline EBDB_ErrCode
CBDB_PropertyDictionary<PropKey, PropValue>::Read(const PropKey& prop,
                                                  const PropValue& value,
                                                  TKeyId* key_idx)
{
    m_PropKey = prop;
    m_PropVal = value;
    return Read(key_idx);
}


template <class PropKey, class PropValue>
inline EBDB_ErrCode
CBDB_PropertyDictionary<PropKey, PropValue>::Write(const TKey& key,
                                                   TKeyId key_idx)
{
    m_PropKey = key.first;
    m_PropVal = key.second;
    m_Uid     = key_idx;
    return UpdateInsert();
}


template <class PropKey, class PropValue>
inline EBDB_ErrCode
CBDB_PropertyDictionary<PropKey, PropValue>::Write(const PropKey& prop,
                                                   const PropValue& value,
                                                   TKeyId key_idx)
{
    m_PropKey = prop;
    m_PropVal = value;
    m_Uid     = key_idx;
    return UpdateInsert();
}


/////////////////////////////////////////////////////////////////////////////

template <typename PropKey, typename PropValue,
          typename Dictionary, typename BvStore, typename BV>
inline CBDB_BvPropertyStore<PropKey, PropValue, Dictionary, BvStore, BV>
::CBDB_BvPropertyStore(const string& demux_path)
: TParent(demux_path)
{
}


template <typename PropKey, typename PropValue,
          typename Dictionary, typename BvStore, typename BV>
inline CBDB_BvPropertyStore<PropKey, PropValue, Dictionary, BvStore, BV>
::CBDB_BvPropertyStore(TDictionary& dict,
                       TStore& store,
                       EOwnership own)
: TParent(dict, store, own)
{
}


END_NCBI_SCOPE

#endif  // BDB___BV_PROPERTY_STORE__HPP
