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

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/impl/psg_cache.hpp>
#include <objtools/data_loaders/genbank/impl/psg_cdd.hpp>
#include <objmgr/impl/tse_info.hpp>

#if defined(HAVE_PSG_LOADER)

BEGIN_NCBI_NAMESPACE;
//#define NCBI_USE_ERRCODE_X   PSGLoader
//NCBI_DEFINE_ERR_SUBCODE_X(1);
BEGIN_NAMESPACE(objects);
BEGIN_NAMESPACE(psgl);

/////////////////////////////////////////////////////////////////////////////
// CPSGBioseqInfoCache
/////////////////////////////////////////////////////////////////////////////


CPSGBioseqInfoCache::CPSGBioseqInfoCache(int lifespan, size_t max_size)
    : m_Lifespan(lifespan),
      m_MaxSize(max_size)
{
}


CPSGBioseqInfoCache::~CPSGBioseqInfoCache()
{
}


void CPSGBioseqInfoCache::x_Erase(TValueIter iter)
{
    m_RemoveList.erase(iter->second.remove_list_iterator);
    m_Values.erase(iter++);
}


void CPSGBioseqInfoCache::x_PopFront()
{
    _ASSERT(!m_RemoveList.empty());
    _ASSERT(m_RemoveList.front() != m_Values.end());
    _ASSERT(m_RemoveList.front()->second.remove_list_iterator == m_RemoveList.begin());
    m_Values.erase(m_RemoveList.front());
    m_RemoveList.pop_front();
}


void CPSGBioseqInfoCache::x_Expire()
{
    while ( !m_RemoveList.empty() &&
            m_RemoveList.front()->second.deadline.IsExpired() ) {
        x_PopFront();
    }
}


void CPSGBioseqInfoCache::x_LimitSize()
{
    while ( m_Values.size() > m_MaxSize ) {
        x_PopFront();
    }
}


CPSGBioseqInfoCache::mapped_type CPSGBioseqInfoCache::Find(const key_type& key)
{
    lock_guard guard(m_Mutex);
    x_Expire();
    auto found = m_Values.find(key);
    return found == m_Values.end()? nullptr: found->second.value;
}


CPSGBioseqInfoCache::mapped_type
CPSGBioseqInfoCache::Add(const key_type& key,
                         const CPSG_BioseqInfo& info)
{
    //return make_shared<SPsgBioseqInfo>(key, info);
    SPsgBioseqInfo parsed(key, info);
    // Try to find an existing entry (though this should not be a common case).
    lock_guard guard(m_Mutex);
    x_Expire();
    auto iter = m_Values.lower_bound(key);
    if ( iter != m_Values.end() && key == iter->first ) {
        if ( !iter->second.deadline.IsExpired() ) {
            iter->second.value->Update(parsed);
            return iter->second.value;
        }
        x_Erase(iter++);
    }
    // Create new entry.
    shared_ptr<SPsgBioseqInfo> value = make_shared<SPsgBioseqInfo>(std::move(parsed));
    iter = m_Values.insert(iter,
                           typename TValues::value_type(key, SNode(value, m_Lifespan)));
    iter->second.remove_list_iterator = m_RemoveList.insert(m_RemoveList.end(), iter);
    x_LimitSize();
    return iter->second.value;
}


/////////////////////////////////////////////////////////////////////////////
// SPsgBioseqInfo
/////////////////////////////////////////////////////////////////////////////


SPsgBioseqInfo::SPsgBioseqInfo(SPsgBioseqInfo&& parsed)
    : request_id(std::move(parsed.request_id)),
      included_info(parsed.included_info.load()),
      molecule_type(parsed.molecule_type),
      length(parsed.length),
      state(parsed.state),
      chain_state(parsed.chain_state),
      tax_id(parsed.tax_id),
      hash(parsed.hash),
      gi(parsed.gi),
      canonical(std::move(parsed.canonical)),
      ids(std::move(parsed.ids)),
      psg_blob_id(std::move(parsed.psg_blob_id))
{
}


SPsgBioseqInfo::SPsgBioseqInfo(const CSeq_id_Handle& request_id,
                               const CPSG_BioseqInfo& bioseq_info)
    : request_id(request_id),
      included_info(0),
      molecule_type(CSeq_inst::eMol_not_set),
      length(0),
      state(0),
      chain_state(0),
      tax_id(INVALID_TAX_ID),
      hash(0)
{
    Update(bioseq_info);
}


SPsgBioseqInfo::TIncludedInfo SPsgBioseqInfo::Update(const CPSG_BioseqInfo& bioseq_info)
{
#ifdef NCBI_ENABLE_SAFE_FLAGS
    TIncludedInfo got_info = bioseq_info.IncludedInfo().get();
#else
    TIncludedInfo got_info = bioseq_info.IncludedInfo();
#endif
    TIncludedInfo new_info = got_info & ~included_info;
    if ( !new_info ) {
        return new_info;
    }

    DEFINE_STATIC_FAST_MUTEX(s_Mutex);
    CFastMutexGuard guard(s_Mutex);
    new_info = got_info & ~included_info;
    if (new_info & CPSG_Request_Resolve::fMoleculeType) {
        molecule_type = bioseq_info.GetMoleculeType();
    }

    if (new_info & CPSG_Request_Resolve::fLength) {
        length = bioseq_info.GetLength();
    }

    if (new_info & CPSG_Request_Resolve::fState) {
        state = bioseq_info.GetState();
    }

    if (new_info & CPSG_Request_Resolve::fChainState) {
        chain_state = bioseq_info.GetChainState();
    }

    if (new_info & CPSG_Request_Resolve::fHash) {
        hash = bioseq_info.GetHash();
    }

    if (new_info & CPSG_Request_Resolve::fCanonicalId) {
        canonical = PsgIdToHandle(bioseq_info.GetCanonicalId());
        _ASSERT(canonical);
        ids.push_back(canonical);
    }
    if (new_info & CPSG_Request_Resolve::fGi) {
        gi = bioseq_info.GetGi();
        if ( gi == INVALID_GI ) {
            gi = ZERO_GI;
        }
    }

    if (new_info & CPSG_Request_Resolve::fOtherIds) {
        vector<CPSG_BioId> other_ids = bioseq_info.GetOtherIds();
        ITERATE(vector<CPSG_BioId>, other_id, other_ids) {
            // NOTE: Bioseq-info may contain unparseable ids which should be ignored (e.g "gnl|FPAA000046" for GI 132).
            auto other_idh = PsgIdToHandle(*other_id);
            if (other_idh) ids.push_back(other_idh);
        }
    }

    if (new_info & CPSG_Request_Resolve::fTaxId) {
        tax_id = bioseq_info.GetTaxId();
        if ( tax_id <= 0 ) {
            _TRACE("bad tax_id for "<<ids.front()<<" : "<<tax_id);
        }
    }

    if (new_info & CPSG_Request_Resolve::fBlobId) {
        psg_blob_id = bioseq_info.GetBlobId().GetId();
    }

    included_info |= new_info;
    return new_info;
}


SPsgBioseqInfo::TIncludedInfo SPsgBioseqInfo::Update(SPsgBioseqInfo& parsed)
{
    TIncludedInfo got_info = parsed.included_info;
    TIncludedInfo new_info = got_info & ~included_info;
    if ( !new_info ) {
        return new_info;
    }

    DEFINE_STATIC_FAST_MUTEX(s_Mutex);
    CFastMutexGuard guard(s_Mutex);
    new_info = got_info & ~included_info;
    if (new_info & CPSG_Request_Resolve::fMoleculeType) {
        molecule_type = parsed.molecule_type;
    }

    if (new_info & CPSG_Request_Resolve::fLength) {
        length = parsed.length;
    }

    if (new_info & CPSG_Request_Resolve::fState) {
        state = parsed.state;
    }

    if (new_info & CPSG_Request_Resolve::fChainState) {
        chain_state = parsed.chain_state;
    }

    if (new_info & CPSG_Request_Resolve::fHash) {
        hash = parsed.hash;
    }

    if (new_info & CPSG_Request_Resolve::fCanonicalId) {
        canonical = std::move(parsed.canonical);
    }
    if (new_info & CPSG_Request_Resolve::fGi) {
        gi = parsed.gi;
    }

    if (new_info & CPSG_Request_Resolve::fOtherIds) {
        ids = std::move(parsed.ids);
    }

    if (new_info & CPSG_Request_Resolve::fTaxId) {
        tax_id = parsed.tax_id;
    }

    if (new_info & CPSG_Request_Resolve::fBlobId) {
        psg_blob_id = parsed.psg_blob_id;
    }

    included_info |= new_info;
    return new_info;
}


bool SPsgBioseqInfo::IsDead() const
{
    if ( included_info & CPSG_Request_Resolve::fState ) {
        if ( state != CPSG_BioseqInfo::eLive ) {
            return true;
        }
    }
    if ( included_info & CPSG_Request_Resolve::fChainState ) {
        if ( chain_state != CPSG_BioseqInfo::eLive ) {
            return true;
        }
    }
    return false;
}


bool SPsgBioseqInfo::HasBlobId() const
{
    return KnowsBlobId() && !psg_blob_id.empty();
}


CConstRef<CPsgBlobId> SPsgBioseqInfo::GetDLBlobId() const
{
    return ConstRef(new CPsgBlobId(psg_blob_id, IsDead()));
}


CBioseq_Handle::TBioseqStateFlags SPsgBioseqInfo::GetBioseqStateFlags() const
{
    if ( included_info & CPSG_Request_Resolve::fState ) {
        if ( state != CPSG_BioseqInfo::eLive ) {
            return CBioseq_Handle::fState_dead;
        }
    }
    return CBioseq_Handle::fState_none;
}


CBioseq_Handle::TBioseqStateFlags SPsgBioseqInfo::GetChainStateFlags() const
{
    if ( included_info & CPSG_Request_Resolve::fChainState ) {
        if ( chain_state != CPSG_BioseqInfo::eLive ) {
            return CBioseq_Handle::fState_dead;
        }
    }
    return CBioseq_Handle::fState_none;
}


/////////////////////////////////////////////////////////////////////////////
// SPsgBlobInfo
/////////////////////////////////////////////////////////////////////////////


SPsgBlobInfo::SPsgBlobInfo(const CPSG_BlobInfo& blob_info)
    : blob_state_flags(CBioseq_Handle::fState_none)
{
    auto blob_id = blob_info.GetId<CPSG_BlobId>();
    _ASSERT(blob_id);
    blob_id_main = blob_id->GetId();
    id2_info = blob_info.GetId2Info();

    if ( blob_info.IsDead() ) {
        blob_state_flags |= CBioseq_Handle::fState_dead;
    }
    if ( blob_info.IsSuppressed() ) {
        blob_state_flags |= CBioseq_Handle::fState_suppress_perm;
    }
    if ( blob_info.IsWithdrawn() ) {
        blob_state_flags |= CBioseq_Handle::fState_withdrawn;
    }

    auto lm = blob_id->GetLastModified(); // last_modified is in milliseconds
    last_modified = lm.IsNull()? 0: lm.GetValue();
}


SPsgBlobInfo::SPsgBlobInfo(const CTSE_Info& tse)
    : blob_state_flags(tse.GetBlobState()),
      last_modified(tse.GetBlobVersion()*60000) // minutes to ms
{
    const CPsgBlobId& blob_id = dynamic_cast<const CPsgBlobId&>(*tse.GetBlobId());
    blob_id_main = blob_id.ToPsgId();
    id2_info = blob_id.GetId2Info();
}


/////////////////////////////////////////////////////////////////////////////
// CPSGAnnotInfoCache
/////////////////////////////////////////////////////////////////////////////

SPsgAnnotInfo::SPsgAnnotInfo(const pair<string, TIds>& key,
                             const TInfos& infos)
    : name(key.first),
      ids(key.second),
      infos(infos)
{
}


CPSGAnnotInfoCache::CPSGAnnotInfoCache(int lifespan, size_t max_size)
    : m_Lifespan(lifespan),
      m_MaxSize(max_size)
{
}


CPSGAnnotInfoCache::~CPSGAnnotInfoCache()
{
}


void CPSGAnnotInfoCache::x_Erase(TValueIter iter)
{
    m_RemoveList.erase(iter->second.remove_list_iterator);
    m_Values.erase(iter++);
}


void CPSGAnnotInfoCache::x_PopFront()
{
    _ASSERT(!m_RemoveList.empty());
    _ASSERT(m_RemoveList.front() != m_Values.end());
    _ASSERT(m_RemoveList.front()->second.remove_list_iterator == m_RemoveList.begin());
    m_Values.erase(m_RemoveList.front());
    m_RemoveList.pop_front();
}


void CPSGAnnotInfoCache::x_Expire()
{
    while ( !m_RemoveList.empty() &&
            m_RemoveList.front()->second.deadline.IsExpired() ) {
        x_PopFront();
    }
}


void CPSGAnnotInfoCache::x_LimitSize()
{
    while ( m_Values.size() > m_MaxSize ) {
        x_PopFront();
    }
}


CPSGAnnotInfoCache::mapped_type CPSGAnnotInfoCache::Find(const key_type& key)
{
    lock_guard guard(m_Mutex);
    x_Expire();
    auto found = m_Values.find(key);
    return found == m_Values.end()? nullptr: found->second.value;
}


CPSGAnnotInfoCache::mapped_type CPSGAnnotInfoCache::Add(const key_type& key,
                                                const SPsgAnnotInfo::TInfos& infos)
{
    // Try to find an existing entry (though this should not be a common case).
    lock_guard guard(m_Mutex);
    x_Expire();
    auto iter = m_Values.lower_bound(key);
    if ( iter != m_Values.end() && key == iter->first ) {
        x_Erase(iter++);
    }
    // Create new entry.
    shared_ptr<SPsgAnnotInfo> value = make_shared<SPsgAnnotInfo>(key, infos);
    iter = m_Values.insert(iter,
                           typename TValues::value_type(key, SNode(value, m_Lifespan)));
    iter->second.remove_list_iterator = m_RemoveList.insert(m_RemoveList.end(), iter);
    x_LimitSize();
    return iter->second.value;
}


CPSGCaches::CPSGCaches(int data_lifespan, size_t data_max_size,
                       int no_data_lifespan, size_t no_data_max_size)
    : m_BioseqInfoCache(data_lifespan, data_max_size),
      m_BlobInfoCache(data_lifespan, data_max_size),
      m_AnnotInfoCache(data_lifespan, data_max_size),
      m_IpgTaxIdCache(data_lifespan, data_max_size),
      m_NoBioseqInfoCache(no_data_lifespan, no_data_max_size),
      m_NoBlobInfoCache(no_data_lifespan, no_data_max_size),
      m_NoAnnotInfoCache(no_data_lifespan, no_data_max_size),
      m_NoIpgTaxIdCache(no_data_lifespan, no_data_max_size),
      m_NoCDDCache(no_data_lifespan, no_data_max_size)
{
}


CPSGCaches::~CPSGCaches() = default;


END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER
