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
// CPSGBioseqCache
/////////////////////////////////////////////////////////////////////////////


shared_ptr<SPsgBioseqInfo> CPSGBioseqCache::Get(const CSeq_id_Handle& idh)
{
    CFastMutexGuard guard(m_Mutex);
    auto found = m_Ids.find(idh);
    if (found == m_Ids.end()) {
        return nullptr;
    }
    shared_ptr<SPsgBioseqInfo> ret = found->second;
    m_Infos.remove(ret);
    if (ret->deadline.IsExpired()) {
        m_Ids.erase(found);
        return nullptr;
    }
    ret->deadline = CDeadline(m_Lifespan);
    m_Infos.push_back(ret);
    return ret;
}


shared_ptr<SPsgBioseqInfo> CPSGBioseqCache::Add(const CPSG_BioseqInfo& info, CSeq_id_Handle req_idh)
{
    _ASSERT(req_idh);
    // Try to find an existing entry (though this should not be a common case).
    CFastMutexGuard guard(m_Mutex);
    auto found = m_Ids.find(req_idh);
    if (found != m_Ids.end()) {
        if (!found->second->deadline.IsExpired()) {
            found->second->Update(info);
            return found->second;
        }
        m_Infos.remove(found->second);
        m_Ids.erase(found);
    }
    while (!m_Infos.empty() && (m_Infos.size() > m_MaxSize || m_Infos.front()->deadline.IsExpired())) {
        m_Ids.erase(m_Infos.front()->request_id);
        m_Infos.pop_front();
    }
    // Create new entry.
    shared_ptr<SPsgBioseqInfo> ret = make_shared<SPsgBioseqInfo>(req_idh, info, m_Lifespan);
    if ( ret->tax_id <= 0 ) {
        _TRACE("bad tax_id for "<<req_idh<<" : "<<ret->tax_id);
    }
    m_Infos.push_back(ret);
    m_Ids[req_idh] = ret;
    return ret;
}


/////////////////////////////////////////////////////////////////////////////
// SPsgBioseqInfo
/////////////////////////////////////////////////////////////////////////////


SPsgBioseqInfo::SPsgBioseqInfo(const CSeq_id_Handle& request_id,
                               const CPSG_BioseqInfo& bioseq_info,
                               int lifespan)
    : request_id(request_id),
      included_info(0),
      molecule_type(CSeq_inst::eMol_not_set),
      length(0),
      state(0),
      chain_state(0),
      tax_id(INVALID_TAX_ID),
      hash(0),
      deadline(lifespan)
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

    if (new_info & CPSG_Request_Resolve::fTaxId) {
        tax_id = bioseq_info.GetTaxId();
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
    if (new_info & CPSG_Request_Resolve::fBlobId) {
        psg_blob_id = bioseq_info.GetBlobId().GetId();
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

    if ( 1 ) { // for testing possible 'dead' inconsistency during DB replication 
        blob_state_flags |= CBioseq_Handle::fState_dead;
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
// CPSGAnnotCache
/////////////////////////////////////////////////////////////////////////////

SPsgAnnotInfo::SPsgAnnotInfo(
    const string& _name,
    const TIds& _ids,
    const TInfos& _infos,
    int lifespan)
    : name(_name), ids(_ids), infos(_infos), deadline(lifespan)
{
}


shared_ptr<SPsgAnnotInfo> CPSGAnnotCache::Get(const string& name, const CSeq_id_Handle& idh)
{
    CFastMutexGuard guard(m_Mutex);
    TNameMap::iterator found_name = m_NameMap.find(name);
    if (found_name == m_NameMap.end()) return nullptr;
    TIdMap& ids = found_name->second;
    TIdMap::iterator found_id = ids.find(idh);
    if (found_id == ids.end()) return nullptr;
    shared_ptr<SPsgAnnotInfo> ret = found_id->second;
    m_Infos.remove(ret);
    if (ret->deadline.IsExpired()) {
        for (auto& id : ret->ids) {
            ids.erase(id);
        }
        if (ids.empty()) {
            m_NameMap.erase(found_name);
        }
        return nullptr;
    }
    ret->deadline = CDeadline(m_Lifespan);
    m_Infos.push_back(ret);
    return ret;
}


shared_ptr<SPsgAnnotInfo> CPSGAnnotCache::Add(
    const SPsgAnnotInfo::TInfos& infos,
    const string& name,
    const TIds& ids)
{
    if (name.empty() || ids.empty()) return nullptr;
    CFastMutexGuard guard(m_Mutex);
    // Try to find an existing entry (though this should not be a common case).
    TNameMap::iterator found_name = m_NameMap.find(name);
    if (found_name != m_NameMap.end()) {
        TIdMap& idmap = found_name->second;
        TIdMap::iterator found = idmap.find(ids.front());
        if (found != idmap.end()) {
            if (!found->second->deadline.IsExpired()) {
                return found->second;
            }
            for (auto& id : found->second->ids) {
                idmap.erase(id);
            }
            if (idmap.empty()) {
                m_NameMap.erase(found_name);
            }
        }
    }
    while (!m_Infos.empty() && (m_Infos.size() > m_MaxSize || m_Infos.front()->deadline.IsExpired())) {
        auto rm = m_Infos.front();
        m_Infos.pop_front();
        TNameMap::iterator found_name = m_NameMap.find(rm->name);
        _ASSERT(found_name != m_NameMap.end());
        for (auto& id : rm->ids) {
            found_name->second.erase(id);
        }
        if (found_name->second.empty() && found_name->first != name) {
            m_NameMap.erase(found_name);
        }
    }
    // Create new entry.
    shared_ptr<SPsgAnnotInfo> ret = make_shared<SPsgAnnotInfo>(name, ids, infos, m_Lifespan);
    m_Infos.push_back(ret);
    TIdMap& idmap = m_NameMap[name];
    for (auto& id : ids) {
        idmap[id] = ret;
    }
    return ret;
}


END_NAMESPACE(psgl);
END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // HAVE_PSG_LOADER
