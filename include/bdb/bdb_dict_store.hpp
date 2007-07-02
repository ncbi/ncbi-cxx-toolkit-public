#ifndef BDB___BDB_DICT_STORE__HPP
#define BDB___BDB_DICT_STORE__HPP

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

#include <bdb/bdb_env.hpp>
#include <bdb/bdb_split_blob.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// Templatized candidate for dictionaries
/// This template defines the basic interface that must be supported by all
/// dictionaries.  Dictionaries do not need to inherit from this, so long as
/// this API is supported.
///

template <typename Key>
class CBDB_BlobDictionary : public CBDB_File
{
public:
    typedef Key   TKey;
    typedef Uint4 TKeyId;

    struct SReverseDictionary : public CBDB_File
    {
        /// key
        typename SBDB_TypeTraits<Uint4>::TFieldType uid;
        /// data
        typename SBDB_TypeTraits<Key>::TFieldType   key;

        SReverseDictionary()
            : CBDB_File(eDuplicatesDisable, eQueue)
            {
                DisableNull();
                SetCacheSize(128 * 1024);
                BindKey ("uid", &uid);
                BindData("key", &key);
            }

        Uint4 GetCurrentUid() const
        {
            return (Uint4)uid;
        }

        TKey GetCurrentKey() const
        {
            return (TKey)key;
        }
    };

    CBDB_BlobDictionary(Uint4 last_uid = 0);

    /// @name Required CBDB_BlobDictionary<> interface
    /// @{

    TKeyId  GetKey(const TKey& key);
    TKeyId  PutKey(const TKey& key);

    /// @}

    /// retrieve the current key
    Key    GetCurrentKey() const;
    TKeyId GetCurrentUid() const;
    TKeyId GetLastUid() const;

    /// read a particular key's value
    EBDB_ErrCode Read (const TKey& key, TKeyId* val);

    /// read the current key's value
    EBDB_ErrCode Read (TKeyId* val);

    /// write a key/value pair to the store
    EBDB_ErrCode Write(const TKey& key, TKeyId val);

private:
    /// key
    typename SBDB_TypeTraits<Key>::TFieldType   m_Key;
    /// data
    typename SBDB_TypeTraits<Uint4>::TFieldType m_Uid;

    TKeyId m_LastUid;
};


////////////////////////////////////////////////////////////////////////////
///
/// Pass-through for Uint4 key types
///

template<>
class CBDB_BlobDictionary<Uint4>
{
public:
    /// @name Required CBDB_BlobDictionary<> interface
    /// @{

    Uint4 GetKey(const Uint4& key) { return key; }
    Uint4 PutKey(const Uint4& key) { return key; }

    /// @}

    /// no-ops for compatibility
    void SetEnv(CBDB_Env& env) {}
    void Open(const char*, CBDB_RawFile::EOpenMode) {}
    bool IsOpen() const { return true; }
};


template<>
class CBDB_BlobDictionary<Int4>
{
public:
    /// @name Required CBDB_BlobDictionary<> interface
    /// @{

    Uint4 GetKey(const Int4& key) { return key; }
    Uint4 PutKey(const Int4& key) { return key; }

    /// @}

    /// no-ops for compatibility
    void SetEnv(CBDB_Env& env) {}
    void Open(const char*, CBDB_RawFile::EOpenMode) {}
    bool IsOpen() const { return true; }
};


/////////////////////////////////////////////////////////////////////////////


template <class BV>
class CBDB_PersistentSplitStore
    : public CBDB_BlobSplitStore<BV, CBDB_BlobDeMuxPersistent>
{
public:
    typedef CBDB_BlobSplitStore<BV, CBDB_BlobDeMuxPersistent> TParent;
    CBDB_PersistentSplitStore(const string& demux_path)
        : TParent(new CBDB_BlobDeMuxPersistent(demux_path,
                                               3.0 * 1024.0 * 1024.0 * 1024.0,
                                               6 * 1000000))
    {
    }
};


/////////////////////////////////////////////////////////////////////////////


template <typename Key,
          typename Dictionary = CBDB_BlobDictionary<Key>,
          typename BvStore = CBDB_PersistentSplitStore< bm::bvector<> > >
class CBDB_BlobDictStore
{
public:
    typedef Key        TKey;
    typedef Uint4      TKeyId;
    typedef Dictionary TDictionary;
    typedef BvStore    TBvStore;

    CBDB_BlobDictStore(const string& demux_path = kEmptyStr);
    CBDB_BlobDictStore(Dictionary& dict,
                       BvStore&    store,
                       EOwnership  own = eNoOwnership);
    ~CBDB_BlobDictStore();

    void      SetEnv(CBDB_Env& env);
    CBDB_Env* GetEnv();
    void Open(const string& fname, CBDB_RawFile::EOpenMode mode);

    const string& GetFileName() const;

    EBDB_ErrCode Read    (const Key& key, CBDB_RawFile::TBuffer& data);
    EBDB_ErrCode ReadById(TKeyId     key, CBDB_RawFile::TBuffer& data);
    EBDB_ErrCode Write    (const Key& key, const CBDB_RawFile::TBuffer& data);
    EBDB_ErrCode Write    (const Key& key, const void* data, size_t size);
    EBDB_ErrCode WriteById(TKeyId key_id, const CBDB_RawFile::TBuffer& data);
    EBDB_ErrCode WriteById(TKeyId key_id, const void* data, size_t size);
    EBDB_ErrCode UpdateInsert(Uint4 uid,
                              const void* data,
                              size_t size);

    /// Delete the underlying blob
    EBDB_ErrCode Delete(unsigned id, 
	                    CBDB_RawFile::EIgnoreError on_error = 
						                        CBDB_RawFile::eThrowOnError,
                        CBDB_Transaction* trans = 0);

    Dictionary& GetDictionary() { return *m_Dict; }
    BvStore&    GetBvStore()    { return *m_Store; }

protected:
    AutoPtr<Dictionary> m_Dict;
    AutoPtr<BvStore>    m_Store;
};


//////////////////////////////////////////////////////////////////////////////


template <typename Key>
inline
CBDB_BlobDictionary<Key>::CBDB_BlobDictionary(Uint4 last_uid)
    : m_LastUid(last_uid)
{
    BindKey ("key", &m_Key);
    BindData("uid", &m_Uid);
}

template <typename Key>
inline
EBDB_ErrCode CBDB_BlobDictionary<Key>::Read(const Key& key, Uint4* val)
{
    m_Key = key;
    return Read(val);
}


template <typename Key>
inline
EBDB_ErrCode CBDB_BlobDictionary<Key>::Read(Uint4* val)
{
    EBDB_ErrCode err = Fetch();
    if (err == eBDB_Ok  &&  val) {
        *val = m_Uid;
    }
    return err;
}


template <typename Key>
inline
EBDB_ErrCode CBDB_BlobDictionary<Key>::Write(const Key& key, Uint4 val)
{
    m_Key = key;
    m_Uid = val;
    return UpdateInsert();
}


template <typename Key>
inline
Key CBDB_BlobDictionary<Key>::GetCurrentKey() const
{
    return (Key)m_Key;
}


template <typename Key>
inline
typename CBDB_BlobDictionary<Key>::TKeyId
CBDB_BlobDictionary<Key>::GetCurrentUid() const
{
    return (TKeyId)m_Uid;
}

template <typename Key>
inline
typename CBDB_BlobDictionary<Key>::TKeyId
CBDB_BlobDictionary<Key>::GetLastUid() const
{
    return m_LastUid;
}


template <typename Key>
inline
Uint4 CBDB_BlobDictionary<Key>::GetKey(const Key& key)
{
    Uint4 uid = 0;
    Read(key, &uid) ;
    return uid;
}


template <typename Key>
inline
Uint4 CBDB_BlobDictionary<Key>::PutKey(const Key& key)
{
    Uint4 uid = GetKey(key);
    if (uid != 0) {
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


/////////////////////////////////////////////////////////////////////////////


template <typename Key, typename Dictionary, typename BvStore>
inline CBDB_BlobDictStore<Key, Dictionary, BvStore>
::CBDB_BlobDictStore(const string& demux_path)
    : m_Dict(NULL)
    , m_Store(NULL)
    , m_DictOwned(new Dictionary)
    , m_StoreOwned(new BvStore(demux_path))
{
    m_Dict = m_DictOwned.get();
    m_Store = m_StoreOwned.get();
}


template <typename Key, typename Dictionary, typename BvStore>
inline CBDB_BlobDictStore<Key, Dictionary, BvStore>
::CBDB_BlobDictStore(Dictionary& generator,
                     BvStore&    store,
                     EOwnership  own)
    : m_Dict(&generator, own),
      m_Store(&store, own)
{
}


template <typename Key, typename Dictionary, typename BvStore>
inline
CBDB_BlobDictStore<Key, Dictionary, BvStore>::~CBDB_BlobDictStore()
{
}


template <typename Key, typename Dictionary, typename BvStore>
inline
CBDB_Env* CBDB_BlobDictStore<Key, Dictionary, BvStore>::GetEnv()
{
    return m_Store->GetEnv();
}


template <typename Key, typename Dictionary, typename BvStore>
inline
const string& CBDB_BlobDictStore<Key, Dictionary, BvStore>::GetFileName() const
{
    return m_Store->GetFileName();
}


template <typename Key, typename Dictionary, typename BvStore>
inline
void CBDB_BlobDictStore<Key, Dictionary, BvStore>::SetEnv(CBDB_Env& env)
{
    m_Dict->SetEnv(env);
    m_Store->SetEnv(env);
}


template <typename Key, typename Dictionary, typename BvStore>
inline
void CBDB_BlobDictStore<Key, Dictionary, BvStore>::Open(const string& fname,
                                                        CBDB_RawFile::EOpenMode mode)
{
    /// open our dictionary
    if ( !m_Dict->IsOpen() ) {
        m_Dict->Open((fname + ".dict").c_str(), mode);
    }
    m_Store->Open(fname, mode);
}


template <typename Key, typename Dictionary, typename BvStore>
inline EBDB_ErrCode
CBDB_BlobDictStore<Key, Dictionary, BvStore>::Read(const Key& key,
                                                   CBDB_RawFile::TBuffer& data)
{
    TKeyId key_id = m_Dict->GetKey(key);
    if ( !key_id ) {
        return eBDB_NotFound;
    }
    return ReadById(key_id, data);
}


template <typename Key, typename Dictionary, typename BvStore>
inline EBDB_ErrCode
CBDB_BlobDictStore<Key, Dictionary, BvStore>::ReadById(TKeyId key_id,
                                                       CBDB_RawFile::TBuffer& data)
{
    return m_Store->ReadRealloc(key_id, data);
}


template <typename Key, typename Dictionary, typename BvStore>
inline EBDB_ErrCode
CBDB_BlobDictStore<Key, Dictionary, BvStore>::Write(const Key& key,
                                                    const CBDB_RawFile::TBuffer& data)
{
    return Write(key, &data[0], data.size());
}


template <typename Key, typename Dictionary, typename BvStore>
inline EBDB_ErrCode
CBDB_BlobDictStore<Key, Dictionary, BvStore>::Write(const Key& key,
                                                    const void* data,
                                                    size_t size)
{
    TKeyId key_id = m_Dict->PutKey(key);
    if ( !key_id ) {
        NCBI_THROW(CException, eUnknown,
                   "Failed to insert key value");
    }
    return UpdateInsert(key_id, data, size);
}


template <typename Key, typename Dictionary, typename BvStore>
inline EBDB_ErrCode
CBDB_BlobDictStore<Key, Dictionary, BvStore>::WriteById(TKeyId key_id,
                                                        const void* data,
                                                        size_t size)
{
    return UpdateInsert(key_id, data, size);
}


template <typename Key, typename Dictionary, typename BvStore>
inline EBDB_ErrCode
CBDB_BlobDictStore<Key, Dictionary, BvStore>::WriteById(TKeyId key_id,
                                                        const CBDB_RawFile::TBuffer& data)
{
    return UpdateInsert(key_id, &data[0], data.size());
}


template <typename Key, typename Dictionary, typename BvStore>
inline EBDB_ErrCode
CBDB_BlobDictStore<Key, Dictionary, BvStore>::UpdateInsert(Uint4 uid,
                                                           const void* data,
                                                           size_t size)
{
    return m_Store->UpdateInsert(uid, data, size);
}


template <typename Key, typename Dictionary, typename BvStore>
inline EBDB_ErrCode
CBDB_BlobDictStore<Key, Dictionary, BvStore>::Delete(unsigned id, 
                                                     CBDB_RawFile::EIgnoreError on_error,
                                                     CBDB_Transaction* trans)
{
    return m_Store->Delete(id, on_error, trans);
}

END_NCBI_SCOPE

#endif  // BDB___BDB_DICT_STORE__HPP
