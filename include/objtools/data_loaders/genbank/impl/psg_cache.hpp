#ifndef OBJTOOLS_DATA_LOADERS_PSG___PSG_CACHE__HPP
#define OBJTOOLS_DATA_LOADERS_PSG___PSG_CACHE__HPP

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
 * Author: Eugene Vasilchenko, Aleksey Grichenko
 *
 * File Description: Cache for loaded bioseq info
 *
 * ===========================================================================
 */

#include <corelib/ncbistd.hpp>
#include <objtools/data_loaders/genbank/psg_loader.hpp>

#if defined(HAVE_PSG_LOADER)
#include <objmgr/bioseq_handle.hpp>
#include <mutex>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);
BEGIN_NAMESPACE(psgl);


/////////////////////////////////////////////////////////////////////////////
// SPsgBioseqInfo
/////////////////////////////////////////////////////////////////////////////

struct SPsgBioseqInfo
{
    SPsgBioseqInfo(const CSeq_id_Handle& request_id,
                   const CPSG_BioseqInfo& bioseq_info);
    SPsgBioseqInfo(SPsgBioseqInfo&& parsed);

    typedef underlying_type_t<CPSG_Request_Resolve::EIncludeInfo> TIncludedInfo;
    typedef vector<CSeq_id_Handle> TIds;

    CSeq_id_Handle request_id;
    atomic<TIncludedInfo> included_info;
    CSeq_inst::TMol molecule_type;
    Uint8 length;
    CPSG_BioseqInfo::TState state;
    CPSG_BioseqInfo::TState chain_state;
    TTaxId tax_id;
    int hash;
    TGi gi;
    CSeq_id_Handle canonical;
    TIds ids;
    string psg_blob_id;

    TIncludedInfo Update(const CPSG_BioseqInfo& bioseq_info);
    TIncludedInfo Update(SPsgBioseqInfo& parsed);
    bool IsDead() const;
    CBioseq_Handle::TBioseqStateFlags GetBioseqStateFlags() const;
    CBioseq_Handle::TBioseqStateFlags GetChainStateFlags() const;

    bool KnowsBlobId() const
    {
        return included_info & CPSG_Request_Resolve::fBlobId;
    }
    bool HasBlobId() const;
    const string& GetPSGBlobId() const
    {
        return psg_blob_id;
    }
    CConstRef<CPsgBlobId> GetDLBlobId() const;

private:
    SPsgBioseqInfo(const SPsgBioseqInfo&);
    SPsgBioseqInfo& operator=(const SPsgBioseqInfo&);
};


/////////////////////////////////////////////////////////////////////////////
// CPSGBioseqInfoCache
/////////////////////////////////////////////////////////////////////////////

class CPSGBioseqInfoCache
{
public:
    CPSGBioseqInfoCache(int lifespan, size_t max_size);
    ~CPSGBioseqInfoCache(void);
    CPSGBioseqInfoCache(const CPSGBioseqInfoCache&) = delete;
    CPSGBioseqInfoCache& operator=(const CPSGBioseqInfoCache&) = delete;

    typedef CSeq_id_Handle key_type;
    typedef shared_ptr<SPsgBioseqInfo> mapped_type;
    
    mapped_type Find(const key_type& key);
    mapped_type Add(const key_type& key, const CPSG_BioseqInfo& info);

private:
    struct SNode;
    typedef map<key_type, SNode> TValues;
    typedef typename TValues::iterator TValueIter;
    typedef list<TValueIter> TRemoveList;
    typedef typename TRemoveList::iterator TRemoveIter;
    struct SNode {
        SNode(const mapped_type& value, unsigned lifespan)
            : value(value),
              deadline(lifespan)
        {}
        mapped_type value;
        CDeadline deadline;
        TRemoveIter remove_list_iterator;
    };

    void x_Erase(TValueIter iter);
    void x_PopFront();
    void x_LimitSize();
    void x_Expire();
    
    mutex m_Mutex;
    int m_Lifespan;
    size_t m_MaxSize;
    TValues m_Values;
    TRemoveList m_RemoveList;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGCache_Base
/////////////////////////////////////////////////////////////////////////////

template<class TK, class TV>
class CPSGCache_Base
{
public:
    typedef TK TKey;
    typedef TV TValue;
    typedef CPSGCache_Base<TKey, TValue> TParent;
    typedef TKey key_type;
    typedef TValue mapped_type;

    CPSGCache_Base(int lifespan, size_t max_size, TValue def_val = TValue())
        : m_Default(def_val),
          m_Lifespan(lifespan),
          m_MaxSize(max_size)
    {}
    CPSGCache_Base(const CPSGCache_Base&) = delete;
    CPSGCache_Base& operator=(const CPSGCache_Base&) = delete;

    TValue Find(const TKey& key) {
        lock_guard guard(m_Mutex);
        x_Expire();
        auto found = m_Values.find(key);
        return found != m_Values.end() ? found->second.value : m_Default;
    }
    
    void Add(const TKey& key, const TValue& value) {
        lock_guard guard(m_Mutex);
        auto iter = m_Values.lower_bound(key);
        if ( iter != m_Values.end() && key == iter->first ) {
            // erase old value
            x_Erase(iter++);
        }
        // insert
        iter = m_Values.insert(iter,
                               typename TValues::value_type(key, SNode(value, m_Lifespan)));
        iter->second.remove_list_iterator = m_RemoveList.insert(m_RemoveList.end(), iter);
        x_LimitSize();
    }

protected:
    // Map blob-id to blob info
    struct SNode;
    typedef map<key_type, SNode> TValues;
    typedef typename TValues::iterator TValueIter;
    typedef list<TValueIter> TRemoveList;
    typedef typename TRemoveList::iterator TRemoveIter;
    struct SNode {
        SNode(const mapped_type& value, unsigned lifespan)
            : value(value),
              deadline(lifespan)
        {}
        mapped_type value;
        CDeadline deadline;
        TRemoveIter remove_list_iterator;
    };

    void x_Erase(TValueIter iter) {
        m_RemoveList.erase(iter->second.remove_list_iterator);
        m_Values.erase(iter);
    }
    void x_Expire() {
        while ( !m_RemoveList.empty() &&
                m_RemoveList.front()->second.deadline.IsExpired() ) {
            x_PopFront();
        }
    }
    void x_LimitSize() {
        while ( m_Values.size() > m_MaxSize ) {
            x_PopFront();
        }
    }
    void x_PopFront() {
        _ASSERT(!m_RemoveList.empty());
        _ASSERT(m_RemoveList.front() != m_Values.end());
        _ASSERT(m_RemoveList.front()->second.remove_list_iterator == m_RemoveList.begin());
        m_Values.erase(m_RemoveList.front());
        m_RemoveList.pop_front();
    }

    TValue m_Default;
    mutex m_Mutex;
    int m_Lifespan;
    size_t m_MaxSize;
    TValues m_Values;
    TRemoveList m_RemoveList;
};


/////////////////////////////////////////////////////////////////////////////
// CPSGNoCDDCache
/////////////////////////////////////////////////////////////////////////////

class CPSGNoCDDCache : public CPSGCache_Base<string, bool>
{
public:
    CPSGNoCDDCache(int lifespan, size_t max_size)
        : TParent(lifespan, max_size, false) {}
};


/////////////////////////////////////////////////////////////////////////////
// SPsgBlobInfo
/////////////////////////////////////////////////////////////////////////////

struct SPsgBlobInfo
{
    explicit SPsgBlobInfo(const CPSG_BlobInfo& blob_info);
    explicit SPsgBlobInfo(const CTSE_Info& tse);

    string blob_id_main;
    string id2_info;
    CBioseq_Handle::TBioseqStateFlags blob_state_flags;
    Int8 last_modified;

    int GetBlobVersion() const { return int(last_modified/60000); /* ms to minutes */ }

    bool IsSplit() const { return !id2_info.empty(); }

private:
    SPsgBlobInfo(const SPsgBlobInfo&);
    SPsgBlobInfo& operator=(const SPsgBlobInfo&);
};


/////////////////////////////////////////////////////////////////////////////
// CPSGBlobInfoCache
/////////////////////////////////////////////////////////////////////////////

class CPSGBlobInfoCache : public CPSGCache_Base<string, shared_ptr<SPsgBlobInfo>>
{
public:
    CPSGBlobInfoCache(int lifespan, size_t max_size)
        : TParent(lifespan, max_size) {}

    void DropBlob(const CPsgBlobId& blob_id) {
        //ERR_POST("DropBlob("<<blob_id.ToPsgId()<<")");
        lock_guard guard(m_Mutex);
        auto iter = m_Values.find(blob_id.ToPsgId());
        if ( iter != m_Values.end() ) {
            x_Erase(iter);
        }
    }
};


/////////////////////////////////////////////////////////////////////////////
// CPSGIpgTaxIdCache
/////////////////////////////////////////////////////////////////////////////

class CPSGIpgTaxIdCache : public CPSGCache_Base<CSeq_id_Handle, TTaxId>
{
public:
    CPSGIpgTaxIdCache(int lifespan, size_t max_size)
        : TParent(lifespan, max_size, INVALID_TAX_ID) {}
};


/////////////////////////////////////////////////////////////////////////////
// CPSGAnnotInfoCache
/////////////////////////////////////////////////////////////////////////////

struct SPsgAnnotInfo
{
    typedef CDataLoader::TIds TIds;
    typedef list<shared_ptr<CPSG_NamedAnnotInfo>> TInfos;

    SPsgAnnotInfo(const pair<string, TIds>& key,
                  const TInfos& infos);

    string name;
    TIds ids;
    TInfos infos;

private:
    SPsgAnnotInfo(const SPsgAnnotInfo&);
    SPsgAnnotInfo& operator=(const SPsgAnnotInfo&);
};


class CPSGAnnotInfoCache
{
public:
    CPSGAnnotInfoCache(int lifespan, size_t max_size);
    ~CPSGAnnotInfoCache();
    CPSGAnnotInfoCache(const CPSGAnnotInfoCache&) = delete;
    CPSGAnnotInfoCache& operator=(const CPSGAnnotInfoCache&) = delete;

    typedef CDataLoader::TIds TIds;
    typedef string key1_type;
    typedef TIds key2_type;
    typedef pair<key1_type, key2_type> key_type;
    typedef shared_ptr<SPsgAnnotInfo> mapped_type;

    mapped_type Find(const key_type& key);
    mapped_type Add(const key_type& key, const SPsgAnnotInfo::TInfos& infos);

private:
    struct SNode;
    typedef map<key_type, SNode> TValues;
    typedef typename TValues::iterator TValueIter;
    typedef list<TValueIter> TRemoveList;
    typedef typename TRemoveList::iterator TRemoveIter;
    struct SNode {
        SNode(const mapped_type& value, unsigned lifespan)
            : value(value),
              deadline(lifespan)
        {}
        mapped_type value;
        CDeadline deadline;
        TRemoveIter remove_list_iterator;
    };

    void x_Erase(TValueIter iter);
    void x_PopFront();
    void x_LimitSize();
    void x_Expire();
    
    mutex m_Mutex;
    int m_Lifespan;
    size_t m_MaxSize;
    TValues m_Values;
    TRemoveList m_RemoveList;
};


class CPSGCaches
{
public:
    CPSGCaches(int data_lifespan, size_t data_max_size,
               int no_data_lifespan, size_t no_data_max_size);
    ~CPSGCaches();

    // cached data, with larger expiration time
    CPSGBioseqInfoCache m_BioseqInfoCache; // seq_id -> bioseq_info
    CPSGBlobInfoCache m_BlobInfoCache; // blob_id -> blob_info
    CPSGAnnotInfoCache m_AnnotInfoCache; // NA/seq_ids -> annot_info
    CPSGIpgTaxIdCache m_IpgTaxIdCache; // seq_id -> TaxId

    // cached absence of data, with smaller expiration time
    CPSGCache_Base<CPSGBioseqInfoCache::key_type, bool> m_NoBioseqInfoCache; // seq_id -> true
    CPSGCache_Base<CPSGBlobInfoCache::key_type, EPSG_Status> m_NoBlobInfoCache; // blob_id -> reason
    CPSGCache_Base<CPSGAnnotInfoCache::key_type, bool> m_NoAnnotInfoCache; // NA/seq_ids -> true
    CPSGCache_Base<CPSGIpgTaxIdCache::key_type, bool> m_NoIpgTaxIdCache; // seq_id -> true
    CPSGCache_Base<string, bool> m_NoCDDCache; // seq_ids -> true
};


END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER

#endif // OBJTOOLS_DATA_LOADERS_PSG___PSG_CACHE__HPP
