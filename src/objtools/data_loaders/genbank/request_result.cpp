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
*  Author: Eugene Vasilchenko
*
*  File Description: GenBank Data loader
*
*/

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/impl/request_result.hpp>
#include <objtools/data_loaders/genbank/impl/processors.hpp>
#include <objtools/data_loaders/genbank/impl/info_cache.hpp>
#include <objtools/data_loaders/genbank/impl/dispatcher.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_split_info.hpp>
#include <objmgr/annot_selector.hpp>
#include <corelib/ncbithr.hpp>
#include <serial/iterator.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

using namespace GBL;

static const CReaderRequestResult::TBlobVersion kBlobVersionNotSet = -1;

NCBI_PARAM_DECL(int, GENBANK, TRACE_LOAD);
NCBI_PARAM_DEF_EX(int, GENBANK, TRACE_LOAD, 0,
                  eParam_NoThread, GENBANK_TRACE_LOAD);


static
int s_GetLoadTraceLevel(void)
{
    static volatile int load_trace_level;
    static volatile bool initialized;
    if ( !initialized ) {
        load_trace_level = NCBI_PARAM_TYPE(GENBANK, TRACE_LOAD)::GetDefault();
        initialized = true;
    }
    return load_trace_level;
}


/////////////////////////////////////////////////////////////////////////////
// CFixedSeq_ids
/////////////////////////////////////////////////////////////////////////////

CFixedSeq_ids::CFixedSeq_ids(TState state)
    : m_State(state | CBioseq_Handle::fState_no_data),
      m_Ref(new TObject)
{
}


CFixedSeq_ids::CFixedSeq_ids(const TList& list, TState state)
    : m_State(state),
      m_Ref(new TObject(list))
{
    if ( list.empty() ) {
        m_State |= CBioseq_Handle::fState_no_data;
    }
}


CFixedSeq_ids::CFixedSeq_ids(ENcbiOwnership ownership, TList& list,
                             TState state)
    : m_State(state)
{
    CRef<TObject> ref(new TObject);
    if ( ownership == eTakeOwnership ) {
        swap(ref->GetData(), list);
    }
    else {
        ref->GetData() = list;
    }
    m_Ref = ref;
}


TGi CFixedSeq_ids::FindGi(void) const
{
    TGi gi = ZERO_GI;
    ITERATE ( CFixedSeq_ids, it, *this ) {
        if ( it->Which() == CSeq_id::e_Gi ) {
            gi = it->GetGi();
            break;
        }
    }
    return gi;
}


CSeq_id_Handle CFixedSeq_ids::FindAccVer(void) const
{
    CSeq_id_Handle acc;
    ITERATE ( CFixedSeq_ids, it, *this ) {
        if ( !it->IsGi() && it->GetSeqId()->GetTextseq_Id() ) {
            acc = *it;
            break;
        }
    }
    return acc;
}


string CFixedSeq_ids::FindLabel(void) const
{
    return objects::GetLabel(*this);
}


CNcbiOstream& operator<<(CNcbiOstream& out, const CFixedSeq_ids& ids)
{
    const char* sep = "( ";
    const char* end = "()";
    ITERATE ( CFixedSeq_ids, it, ids ) {
        out << sep << *it;
        sep = ", ";
        end = " )";
    }
    out << end;
    return out;
}


/////////////////////////////////////////////////////////////////////////////
// CFixedBlob_ids
/////////////////////////////////////////////////////////////////////////////

CFixedBlob_ids::CFixedBlob_ids(TState state)
    : m_State(state | CBioseq_Handle::fState_no_data),
      m_Ref(new TObject)
{
}


CFixedBlob_ids::CFixedBlob_ids(const TList& list,
                               TState state)
    : m_State(state),
      m_Ref(new TObject(list))
{
    if ( empty() ) {
        m_State |= CBioseq_Handle::fState_no_data;
    }
}


CFixedBlob_ids::CFixedBlob_ids(ENcbiOwnership ownership, TList& list,
                               TState state)
    : m_State(state)
{
    CRef<TObject> ref(new TObject);
    if ( ownership == eTakeOwnership ) {
        swap(ref->GetData(), list);
    }
    else {
        ref->GetData() = list;
    }
    m_Ref = ref;
    if ( empty() ) {
        m_State |= CBioseq_Handle::fState_no_data;
    }
}


CNcbiOstream& operator<<(CNcbiOstream& out, const CFixedBlob_ids& ids)
{
    const char* sep = "( ";
    const char* end = "()";
    ITERATE ( CFixedBlob_ids, it, ids ) {
        out << sep << it->GetBlob_id();
        sep = ", ";
        end = " )";
    }
    out << end;
    return out;
}


/////////////////////////////////////////////////////////////////////////////
// CBlob_Info
/////////////////////////////////////////////////////////////////////////////


CBlob_Info::CBlob_Info(void)
    : m_Contents(0)
{
}


CBlob_Info::CBlob_Info(CConstRef<CBlob_id> blob_id, TContentsMask contents)
    : m_Blob_id(blob_id),
      m_Contents(contents)
{
}


CBlob_Info::~CBlob_Info(void)
{
}


bool CBlob_Info::Matches(TContentsMask mask,
                         const SAnnotSelector* sel) const
{
    TContentsMask common_mask = GetContentsMask() & mask;
    if ( common_mask == 0 ) {
        return false;
    }

    if ( CProcessor_ExtAnnot::IsExtAnnot(*GetBlob_id()) ) {
        // not named accession, but external annots
        return true;
    }

    if ( (common_mask & ~(fBlobHasExtAnnot|fBlobHasNamedAnnot)) != 0 ) {
        // not only features;
        return true;
    }

    // only features

    if ( !IsSetAnnotInfo() ) {
        // no known annot info -> assume matching
        return true;
    }
    else {
        return GetAnnotInfo()->Matches(sel);
    }
}


void CBlob_Info::SetAnnotInfo(CRef<CBlob_Annot_Info>& annot_info)
{
    _ASSERT(!IsSetAnnotInfo());
    m_AnnotInfo = annot_info;
}


/////////////////////////////////////////////////////////////////////////////
// CBlob_Annot_Info
/////////////////////////////////////////////////////////////////////////////


void CBlob_Annot_Info::AddNamedAnnotName(const string& name)
{
    m_NamedAnnotNames.insert(name);
}


void CBlob_Annot_Info::AddAnnotInfo(const CID2S_Seq_annot_Info& info)
{
    m_AnnotInfo.push_back(ConstRef(&info));
}


bool CBlob_Annot_Info::Matches(const SAnnotSelector* sel) const
{
    if ( GetNamedAnnotNames().empty() ) {
        // no filtering by name
        return true;
    }
    
    if ( !sel || !sel->IsIncludedAnyNamedAnnotAccession() ) {
        // no names included
        return false;
    }

    if ( sel->IsIncludedNamedAnnotAccession("NA*") ) {
        // all accessions are included
        return true;
    }
    
    // annot filtering by name
    ITERATE ( TNamedAnnotNames, it, GetNamedAnnotNames() ) {
        const string& name = *it;
        if ( !NStr::StartsWith(name, "NA") ) {
            // not named accession
            return true;
        }
        if ( sel->IsIncludedNamedAnnotAccession(name) ) {
            // matches
            return true;
        }
    }
    // no match by name found
    return false;
}


/////////////////////////////////////////////////////////////////////////////
// CLoadLockBlob
/////////////////////////////////////////////////////////////////////////////


CLoadLockBlob::CLoadLockBlob(CReaderRequestResult& result,
                             const CBlob_id& blob_id,
                             TChunkId chunk_id)
    : TParent(result.GetLoadLockBlob(blob_id)),
      m_Blob_id(blob_id)
{
    x_ObtainTSE_LoadLock(result);
    if ( chunk_id != kMain_ChunkId ) {
        SelectChunk(chunk_id);
    }
}


CLoadLockBlob::~CLoadLockBlob(void)
{
}


CTSE_LoadLock& CLoadLockBlob::GetTSE_LoadLock(void)
{
    if ( !m_TSE_LoadLock ) {
        CReaderRequestResult& result =
            dynamic_cast<CReaderRequestResult&>(GetRequestor());
        x_ObtainTSE_LoadLock(result);
        _ASSERT(m_TSE_LoadLock);
        _ASSERT(m_TSE_LoadLock.IsLoaded());
    }
    return m_TSE_LoadLock;
}


CLoadLockBlob::TBlobVersion CLoadLockBlob::GetKnownBlobVersion(void) const
{
    if ( !m_TSE_LoadLock ) {
        return kBlobVersionNotSet;
    }
    return m_TSE_LoadLock->GetBlobVersion();
}


const CTSE_Split_Info& CLoadLockBlob::GetSplitInfo(void) const
{
    _ASSERT(m_TSE_LoadLock);
    return m_TSE_LoadLock->GetSplitInfo();
}


bool CLoadLockBlob::NeedsDelayedMainChunk(void) const
{
    return m_TSE_LoadLock && m_TSE_LoadLock->HasSplitInfo() &&
        m_TSE_LoadLock->GetSplitInfo().x_NeedsDelayedMainChunk();
}


void CLoadLockBlob::SelectChunk(TChunkId chunk_id)
{
    if ( chunk_id == kMain_ChunkId ) {
        m_Chunk = null;
    }
    else {
        _ASSERT(m_TSE_LoadLock.IsLoaded());
        m_Chunk = &GetSplitInfo().GetChunk(chunk_id);
        _ASSERT(m_Chunk && m_Chunk->GetChunkId() == chunk_id);
    }
}


TChunkId CLoadLockBlob::GetSelectedChunkId(void) const
{
    return m_Chunk? m_Chunk->GetChunkId(): kMain_ChunkId;
}


bool CLoadLockBlob::IsLoadedChunk(void) const
{
    if ( !m_Chunk ) {
        return IsLoadedBlob();
    }
    else {
        return m_Chunk->IsLoaded();
    }
}


bool CLoadLockBlob::IsLoadedChunk(TChunkId chunk_id) const
{
    if ( chunk_id == kMain_ChunkId ) {
        return IsLoadedBlob();
    }
    else {
        if ( m_Chunk && m_Chunk->GetChunkId() == chunk_id ) {
            return m_Chunk->IsLoaded();
        }
        _ASSERT(IsLoadedBlob());
        return GetData()->GetSplitInfo().GetChunk(chunk_id).IsLoaded();
    }
}


void CLoadLockBlob::x_ObtainTSE_LoadLock(CReaderRequestResult& result)
{
    if ( TParent::IsLoaded() ) {
        m_TSE_LoadLock = GetData();
        _ASSERT(m_TSE_LoadLock);
        _ASSERT(m_TSE_LoadLock.IsLoaded());
        result.x_AddTSE_LoadLock(m_TSE_LoadLock);
        return;
    }
    m_TSE_LoadLock = result.GetTSE_LoadLockIfLoaded(m_Blob_id);
    if ( m_TSE_LoadLock ) {
        _ASSERT(m_TSE_LoadLock.IsLoaded());
        TParent::SetLoaded(m_TSE_LoadLock);
        result.x_AddTSE_LoadLock(m_TSE_LoadLock);
        return;
    }
}


/////////////////////////////////////////////////////////////////////////////
// CLoadLockChunkSetter
/////////////////////////////////////////////////////////////////////////////


BEGIN_LOCAL_NAMESPACE;

struct SBlobId
{
    SBlobId(const CTSE_Info& tse)
        : m_TSE(tse)
        {
        }

    const CTSE_Info& m_TSE;
};

CNcbiOstream& operator<<(CNcbiOstream& out, const SBlobId& id)
{
    out << id.m_TSE.GetBlobId().ToString();
    return out;
}

struct SChunkId
{
    SChunkId(const CTSE_Chunk_Info& chunk)
        : m_Chunk(chunk)
        {
        }

    const CTSE_Chunk_Info& m_Chunk;
};

CNcbiOstream& operator<<(CNcbiOstream& out, const SChunkId& id)
{
    out << id.m_Chunk.GetSplitInfo().GetBlobId().ToString()<<"."<<id.m_Chunk.GetChunkId();
    return out;
}

struct SSeqIds
{
    SSeqIds(const CSeq_entry& entry)
        : m_Entry(entry)
        {
        }

    const CSeq_entry& m_Entry;
};

CNcbiOstream& operator<<(CNcbiOstream& out, const SSeqIds& ids)
{
    for ( CTypeConstIterator<CBioseq> it = Begin(ids.m_Entry); it; ++it ) {
        const CBioseq& seq = *it;
        const char* sep = "Bioseq( ";
        const char* end = "Bioseq()";
        ITERATE ( CBioseq::TId, it2, seq.GetId() ) {
            out << sep << (*it2)->AsFastaString();
            sep = ", ";
            end = " )";
        }
        out << end;
        break;
    }
    return out;
}

END_LOCAL_NAMESPACE;


CLoadLockSetter::CLoadLockSetter(CLoadLockBlob& lock)
{
    x_Init(lock, lock.GetSelectedChunkId());
}


CLoadLockSetter::CLoadLockSetter(CLoadLockBlob& lock,
                                 TChunkId chunk_id)
{
    x_Init(lock, chunk_id);
}


CLoadLockSetter::CLoadLockSetter(CReaderRequestResult& result,
                                 const CBlob_id& blob_id,
                                 TChunkId chunk_id)
    : TParent(result.GetLoadLockBlob(blob_id))
{
    x_ObtainTSE_LoadLock(result, blob_id);
    if ( chunk_id != kMain_ChunkId ) {
        x_SelectChunk(chunk_id);
    }
}


void CLoadLockSetter::x_Init(CLoadLockBlob& lock, TChunkId chunk_id)
{
    TParent::operator=(lock);
    m_TSE_LoadLock = lock.m_TSE_LoadLock;
    if ( chunk_id == kMain_ChunkId ) {
        if ( !m_TSE_LoadLock ) {
            CReaderRequestResult& result =
                dynamic_cast<CReaderRequestResult&>(GetRequestor());
            x_ObtainTSE_LoadLock(result, lock.m_Blob_id);
        }
    }
    else {
        _ASSERT(m_TSE_LoadLock);
        _ASSERT(m_TSE_LoadLock.IsLoaded());
        if ( chunk_id == lock.GetSelectedChunkId() ) {
            m_Chunk = &const_cast<CTSE_Chunk_Info&>(*lock.m_Chunk);
        }
        else {
            x_SelectChunk(chunk_id);
        }
    }
}


CLoadLockSetter::~CLoadLockSetter(void)
{
    if ( !IsLoaded() ) {
        ERR_POST("Incomplete loading");
    }
}


bool CLoadLockSetter::IsLoaded(void) const
{
    if ( !m_Chunk ) {
        return m_TSE_LoadLock.IsLoaded();
    }
    else {
        return m_Chunk->IsLoaded();
    }
}


void CLoadLockSetter::SetLoaded(void)
{
    _ASSERT(!IsLoaded());
    if ( !m_Chunk ) {
        if ( s_GetLoadTraceLevel() > 0 ) {
            LOG_POST(Info<<"GBLoader:"<<SBlobId(*m_TSE_LoadLock)<<" loaded");
        }
        m_TSE_LoadLock.SetLoaded();
        TParent::SetLoaded(m_TSE_LoadLock);
        CReaderRequestResult& result =
            dynamic_cast<CReaderRequestResult&>(GetRequestor());
        result.x_AddTSE_LoadLock(m_TSE_LoadLock);
    }
    else {
        if ( s_GetLoadTraceLevel() >= 2 ||
             (s_GetLoadTraceLevel() >= 1 &&
              m_Chunk->GetChunkId() >= kMasterWGS_ChunkId) ) {
            LOG_POST(Info<<"GBLoader:"<<SChunkId(*m_Chunk)<<" loaded");
        }
        m_Chunk->SetLoaded();
    }
}


void CLoadLockSetter::x_SelectChunk(TChunkId chunk_id)
{
    if ( chunk_id == kMain_ChunkId ) {
        m_Chunk = null;
    }
    else {
        _ASSERT(m_TSE_LoadLock.IsLoaded());
        m_Chunk = &GetSplitInfo().GetChunk(chunk_id);
        _ASSERT(m_Chunk && m_Chunk->GetChunkId() == chunk_id);
    }
}


void CLoadLockSetter::x_ObtainTSE_LoadLock(CReaderRequestResult& result,
                                           const CBlob_id& blob_id)
{
    if ( TParent::IsLoaded() ) {
        m_TSE_LoadLock = GetData();
        _ASSERT(m_TSE_LoadLock);
        _ASSERT(m_TSE_LoadLock.IsLoaded());
        result.x_AddTSE_LoadLock(m_TSE_LoadLock);
        return;
    }
    m_TSE_LoadLock = result.GetTSE_LoadLock(blob_id);
    _ASSERT(m_TSE_LoadLock);
    if ( m_TSE_LoadLock.IsLoaded() ) {
        TParent::SetLoaded(m_TSE_LoadLock);
        result.x_AddTSE_LoadLock(m_TSE_LoadLock);
        return;
    }
    CLoadLockBlobState state(result, blob_id, eAlreadyLoaded);
    if ( state ) {
        m_TSE_LoadLock->SetBlobState(state.GetBlobState());
    }
    CLoadLockBlobVersion version(result, blob_id, eAlreadyLoaded);
    if ( version ) {
        m_TSE_LoadLock->SetBlobVersion(version.GetBlobVersion());
    }
}


CTSE_Split_Info& CLoadLockSetter::GetSplitInfo(void)
{
    _ASSERT(m_TSE_LoadLock);
    return m_TSE_LoadLock->GetSplitInfo();
}


CLoadLockSetter::TBlobState CLoadLockSetter::GetBlobState(void) const
{
    _ASSERT(m_TSE_LoadLock);
    return m_TSE_LoadLock->GetBlobState();
}


void CLoadLockSetter::SetSeq_entry(CSeq_entry& entry,
                                   CTSE_SetObjectInfo* set_info)
{
    _ASSERT(!IsLoaded());
    if ( !m_Chunk ) {
        if ( s_GetLoadTraceLevel() > 0 ) {
            LOG_POST(Info<<"GBLoader:"<<SBlobId(*m_TSE_LoadLock)<<" entry = "<<SSeqIds(entry));
        }
        m_TSE_LoadLock->SetSeq_entry(entry, set_info);
    }
    else {
        if ( s_GetLoadTraceLevel() > 0 ) {
            LOG_POST(Info<<"GBLoader:"<<SChunkId(*m_Chunk)<<" entry = "<<SSeqIds(entry));
        }
        m_Chunk->x_LoadSeq_entry(entry, set_info);
    }
}


/////////////////////////////////////////////////////////////////////////////
// CReaderRequestResult
/////////////////////////////////////////////////////////////////////////////


// helper method to get system time for expiration
static inline
CReaderRequestResult::TExpirationTime sx_GetCurrentTime(void)
{
    return CReaderRequestResult::TExpirationTime(time(0));
}


CReaderRequestResult::CReaderRequestResult(const CSeq_id_Handle& requested_id,
                                           CReadDispatcher& dispatcher,
                                           CGBInfoManager& manager)
    : GBL::CInfoRequestor(manager),
      m_ReadDispatcher(dispatcher),
      m_Level(0),
      m_RequestedId(requested_id),
      m_RecursionLevel(0),
      m_RecursiveTime(0),
      m_AllocatedConnection(0),
      m_RetryDelay(0),
      m_StartTime(sx_GetCurrentTime())
{
}


CWriter* CReaderRequestResult::GetIdWriter(void) const
{
    return m_ReadDispatcher.GetWriter(*this, CWriter::eIdWriter);
}


CWriter* CReaderRequestResult::GetBlobWriter(void) const
{
    return m_ReadDispatcher.GetWriter(*this, CWriter::eBlobWriter);
}


CReaderRequestResult::TExpirationTime
CReaderRequestResult::GetNewIdExpirationTime(void) const
{
    return GetStartTime()+GetIdExpirationTimeout();
}


CReaderRequestResult::TExpirationTime
CReaderRequestResult::GetIdExpirationTimeout(void) const
{
    return 2*3600;
}


bool CReaderRequestResult::GetAddWGSMasterDescr(void) const
{
    return false;
}


CReaderRequestResult::~CReaderRequestResult(void)
{
    ReleaseLocks();
    _ASSERT(!m_AllocatedConnection);
}


CGBDataLoader* CReaderRequestResult::GetLoaderPtr(void)
{
    return 0;
}


void CReaderRequestResult::SetRequestedId(const CSeq_id_Handle& requested_id)
{
    if ( !m_RequestedId ) {
        m_RequestedId = requested_id;
    }
}


CTSE_LoadLock CReaderRequestResult::GetBlobLoadLock(const CBlob_id& blob_id)
{
    return CTSE_LoadLock();
}


bool CReaderRequestResult::SetNoBlob(const CBlob_id& blob_id,
                                     TBlobState blob_state)
{
    SetLoadedBlobState(blob_id, blob_state);
    CLoadLockBlob blob(*this, blob_id);
    if ( !blob.IsLoadedBlob() ) {
        CLoadLockSetter setter(blob);
        setter.SetLoaded();
        return true;
    }
    return false;
}


void CReaderRequestResult::ReleaseNotLoadedBlobs(void)
{
}


void CReaderRequestResult::GetLoadedBlob_ids(const CSeq_id_Handle& /*idh*/,
                                             TLoadedBlob_ids& /*blob_ids*/) const
{
    return;
}


void CReaderRequestResult::x_AddTSE_LoadLock(const CTSE_LoadLock& lock)
{
    m_TSE_LockSet.insert(lock);
}


void CReaderRequestResult::SaveLocksTo(TTSE_LockSet& locks)
{
    ITERATE ( TTSE_LockSet, it, m_TSE_LockSet ) {
        locks.insert(*it);
    }
}


void CReaderRequestResult::ReleaseLocks(void)
{
    m_TSE_LockSet.clear();
}


CReaderRequestResult::TExpirationTime
CReaderRequestResult::GetRequestTime(void) const
{
    return GetStartTime();
}


CReaderRequestResult::TExpirationTime
CReaderRequestResult::GetNewExpirationTime(void) const
{
    return GetNewIdExpirationTime();
}


/////////////////////////////////////////////////////////////////////////////
// CReaderRequestResultRecursion
/////////////////////////////////////////////////////////////////////////////


CReaderRequestResultRecursion::CReaderRequestResultRecursion(
    CReaderRequestResult& result)
    : CStopWatch(eStart),
      m_Result(result)
{
    m_SaveTime = result.m_RecursiveTime;
    result.m_RecursiveTime = 0;
    ++result.m_RecursionLevel;
}


CReaderRequestResultRecursion::~CReaderRequestResultRecursion(void)
{
    _ASSERT(m_Result.m_RecursionLevel>0);
    m_Result.m_RecursiveTime += m_SaveTime;
    --m_Result.m_RecursionLevel;
}


double CReaderRequestResultRecursion::GetCurrentRequestTime(void) const
{
    double time = Elapsed();
    double rec_time = m_Result.m_RecursiveTime;
    if ( rec_time > time ) {
        return 0;
    }
    else {
        m_Result.m_RecursiveTime = time;
        return time - rec_time;
    }
}


/////////////////////////////////////////////////////////////////////////////
// CGBLoaderManager
/////////////////////////////////////////////////////////////////////////////


CGBInfoManager::CGBInfoManager(size_t gc_size)
    : m_CacheAcc(GetMainMutex(), gc_size),
      m_CacheSeqIds(GetMainMutex(), gc_size),
      m_CacheGi(GetMainMutex(), gc_size),
      m_CacheStrSeqIds(GetMainMutex(), gc_size),
      m_CacheStrGi(GetMainMutex(), gc_size),
      m_CacheLabel(GetMainMutex(), gc_size),
      m_CacheTaxId(GetMainMutex(), gc_size),
      m_CacheHash(GetMainMutex(), gc_size),
      m_CacheBlobIds(GetMainMutex(), gc_size),
      m_CacheBlobState(GetMainMutex(), gc_size),
      m_CacheBlobVersion(GetMainMutex(), gc_size),
      m_CacheBlob(GetMainMutex(), 0)
{
}


CGBInfoManager::~CGBInfoManager(void)
{
}


CLoadLockSeqIds::CLoadLockSeqIds(CReaderRequestResult& result,
                                 const CSeq_id_Handle& id)
    : TParent(result.GetLoadLockSeqIds(id))
{
}


CLoadLockSeqIds::CLoadLockSeqIds(CReaderRequestResult& result,
                                 const string& id)
    : TParent(result.GetLoadLockSeqIds(id))
{
}

CLoadLockSeqIds::CLoadLockSeqIds(CReaderRequestResult& result,
                                 const CSeq_id_Handle& id,
                                 EAlreadyLoaded)
    : TParent(result.GetLoadedSeqIds(id))
{
}


CLoadLockSeqIds::CLoadLockSeqIds(CReaderRequestResult& result,
                                 const string& id,
                                 EAlreadyLoaded)
    : TParent(result.GetLoadedSeqIds(id))
{
}


CLoadLockAcc::CLoadLockAcc(CReaderRequestResult& result,
                           const CSeq_id_Handle& id)
    : TParent(result.GetLoadLockAcc(id))
{
}


CLoadLockGi::CLoadLockGi(CReaderRequestResult& result,
                         const CSeq_id_Handle& id)
    : TParent(result.GetLoadLockGi(id))
{
}


CLoadLockGi::CLoadLockGi(CReaderRequestResult& result,
                         const string& id)
    : TParent(result.GetLoadLockGi(id))
{
}


CLoadLockLabel::CLoadLockLabel(CReaderRequestResult& result,
                               const CSeq_id_Handle& id)
    : TParent(result.GetLoadLockLabel(id))
{
}


CLoadLockTaxId::CLoadLockTaxId(CReaderRequestResult& result,
                               const CSeq_id_Handle& id)
    : TParent(result.GetLoadLockTaxId(id))
{
}


CLoadLockHash::CLoadLockHash(CReaderRequestResult& result,
                             const CSeq_id_Handle& id)
    : TParent(result.GetLoadLockHash(id))
{
}


CLoadLockBlobIds::CLoadLockBlobIds(CReaderRequestResult& result,
                                   const CSeq_id_Handle& id,
                                   const SAnnotSelector* sel)
    : TParent(result.GetLoadLockBlobIds(id, sel)),
      m_Seq_id(id)
{
}
CLoadLockBlobIds::CLoadLockBlobIds(CReaderRequestResult& result,
                                   const CSeq_id_Handle& id,
                                   const SAnnotSelector* sel,
                                   EAlreadyLoaded)
    : TParent(result.GetLoadedBlobIds(id, sel)),
      m_Seq_id(id)
{
}


CLoadLockBlobState::CLoadLockBlobState(CReaderRequestResult& result,
                                       const CBlob_id& id)
    : TParent(result.GetLoadLockBlobState(id))
{
}


CLoadLockBlobState::CLoadLockBlobState(CReaderRequestResult& result,
                                       const CBlob_id& id,
                                       EAlreadyLoaded)
    : TParent(result.GetLoadedBlobState(id))
{
}


CLoadLockBlobVersion::CLoadLockBlobVersion(CReaderRequestResult& result,
                                           const CBlob_id& id)
    : TParent(result.GetLoadLockBlobVersion(id))
{
}


CLoadLockBlobVersion::CLoadLockBlobVersion(CReaderRequestResult& result,
                                           const CBlob_id& id,
                                           EAlreadyLoaded)
    : TParent(result.GetLoadedBlobVersion(id))
{
}


/////////////////////////////////////////////////////////////////////////////
// Seq-id -> Seq-ids

bool
CReaderRequestResult::IsLoadedSeqIds(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheSeqIds.IsLoaded(*this, id);
}


bool
CReaderRequestResult::MarkLoadingSeqIds(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheSeqIds.MarkLoading(*this, id);
}


CReaderRequestResult::TInfoLockIds
CReaderRequestResult::GetLoadLockSeqIds(const CSeq_id_Handle& id)
{
    // if connection is allocated we cannot wait for another lock because
    // of possible deadlock.
    EDoNotWait do_not_wait = m_AllocatedConnection? eDoNotWait: eAllowWaiting;
    return GetGBInfoManager().m_CacheSeqIds.GetLoadLock(*this, id, do_not_wait);
}


CReaderRequestResult::TInfoLockIds
CReaderRequestResult::GetLoadedSeqIds(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheSeqIds.GetLoaded(*this, id);
}


bool
CReaderRequestResult::SetLoadedSeqIds(const CSeq_id_Handle& id,
                                      const CFixedSeq_ids& value)
{
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") seq_ids = "<<value);
    }
    return GetGBInfoManager().m_CacheSeqIds.SetLoaded(*this, id, value);
}


/////////////////////////////////////////////////////////////////////////////
// string -> Seq-ids

bool
CReaderRequestResult::IsLoadedSeqIds(const string& id)
{
    return GetGBInfoManager().m_CacheStrSeqIds.IsLoaded(*this, id);
}


bool
CReaderRequestResult::MarkLoadingSeqIds(const string& id)
{
    return GetGBInfoManager().m_CacheStrSeqIds.MarkLoading(*this, id);
}


CReaderRequestResult::TInfoLockIds
CReaderRequestResult::GetLoadLockSeqIds(const string& id)
{
    // if connection is allocated we cannot wait for another lock because
    // of possible deadlock.
    EDoNotWait do_not_wait = m_AllocatedConnection? eDoNotWait: eAllowWaiting;
    return GetGBInfoManager().m_CacheStrSeqIds.GetLoadLock(*this, id, do_not_wait);
}


CReaderRequestResult::TInfoLockIds
CReaderRequestResult::GetLoadedSeqIds(const string& id)
{
    return GetGBInfoManager().m_CacheStrSeqIds.GetLoaded(*this, id);
}


bool
CReaderRequestResult::SetLoadedSeqIds(const string& id,
                                      const CFixedSeq_ids& value)
{
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:StrId("<<id<<") seq_ids = "<<value);
    }
    return GetGBInfoManager().m_CacheStrSeqIds.SetLoaded(*this, id, value);
}


/////////////////////////////////////////////////////////////////////////////
// Copy info


bool
CReaderRequestResult::SetLoadedSeqIds(const CSeq_id_Handle& id,
                                      const CLoadLockSeqIds& ids)
{
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") seq_ids = "<<ids.GetData());
    }
    CLoadLockSeqIds lock(*this, id);
    return lock.SetLoadedSeq_ids(ids);
}


bool
CReaderRequestResult::SetLoadedBlobIds(const CSeq_id_Handle& id,
                                       const SAnnotSelector* sel,
                                       const CLoadLockBlobIds& ids)
{
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") blob_ids = "<<ids.GetData());
    }
    CLoadLockBlobIds lock(*this, id, sel);
    return lock.SetLoadedBlob_ids(ids);
}


/////////////////////////////////////////////////////////////////////////////
// Seq-id -> Acc.Ver

bool
CReaderRequestResult::IsLoadedAcc(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheAcc.IsLoaded(*this, id) ||
        IsLoadedSeqIds(id);
}


bool
CReaderRequestResult::MarkLoadingAcc(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheAcc.MarkLoading(*this, id);
}


CReaderRequestResult::TInfoLockAcc
CReaderRequestResult::GetLoadLockAcc(const CSeq_id_Handle& id)
{
    // if connection is allocated we cannot wait for another lock because
    // of possible deadlock.
    EDoNotWait do_not_wait = m_AllocatedConnection? eDoNotWait: eAllowWaiting;
    TInfoLockAcc lock =
        GetGBInfoManager().m_CacheAcc.GetLoadLock(*this, id, do_not_wait);

    if ( !lock.IsLoaded() ) {
        TInfoLockIds ids_lock = GetLoadedSeqIds(id);
        if ( ids_lock ) {
            UpdateAccFromSeqIds(lock, ids_lock);
        }
    }
    return lock;
}


CReaderRequestResult::TInfoLockAcc
CReaderRequestResult::GetLoadedAcc(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheAcc.GetLoaded(*this, id);
}


bool
CReaderRequestResult::SetLoadedAcc(const CSeq_id_Handle& id,
                                   const CSeq_id_Handle& value)
{
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") acc = "<<value);
    }
    return GetGBInfoManager().m_CacheAcc.SetLoaded(*this, id, value);
}


/////////////////////////////////////////////////////////////////////////////
// Seq-ids -> Acc.Ver

bool CReaderRequestResult::UpdateAccFromSeqIds(TInfoLockAcc& acc_lock,
                                               const TInfoLockIds& ids_lock)
{
    if ( acc_lock.IsLoaded() ) {
        return false;
    }
    return acc_lock.SetLoaded(ids_lock.GetData().FindAccVer(),
                              ids_lock.GetExpirationTime());
}


bool CReaderRequestResult::SetLoadedAccFromSeqIds(const CSeq_id_Handle& id,
                                                  const CLoadLockSeqIds& ids)
{
    CSeq_id_Handle acc = ids.GetSeq_ids().FindAccVer();
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") acc = "<<acc);
    }
    TExpirationTime exp_time = ids.GetExpirationTime();
    return GetGBInfoManager().m_CacheAcc.SetLoaded(*this, id, acc, exp_time);
}


/////////////////////////////////////////////////////////////////////////////
// Seq-ids <-> GI

bool
CReaderRequestResult::SetLoadedSeqIdsFromZeroGi(const CSeq_id_Handle& id,
                                                const CLoadLockGi& gi_lock)
{
    _ASSERT(gi_lock.IsLoadedGi() && gi_lock.GetGi() == ZERO_GI);
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") seq_ids = null");
    }
    CLoadLockSeqIds lock(*this, id);
    TBlobState state = CBioseq_Handle::fState_no_data;
    return lock.SetLoadedSeq_ids(CFixedSeq_ids(state),
                                 gi_lock.GetExpirationTimeGi());
}


bool
CReaderRequestResult::SetLoadedBlobIdsFromZeroGi(const CSeq_id_Handle& id,
                                                 const SAnnotSelector* sel,
                                                 const CLoadLockGi& gi_lock)
{
    _ASSERT(gi_lock.IsLoadedGi() && gi_lock.GetGi() == ZERO_GI);
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") blob_ids = null");
    }
    CLoadLockBlobIds lock(*this, id, sel);
    TBlobState state = CBioseq_Handle::fState_no_data;
    return lock.SetNoBlob_ids(state,
                              gi_lock.GetExpirationTimeGi());
}


bool CReaderRequestResult::UpdateGiFromSeqIds(TInfoLockGi& gi_lock,
                                              const TInfoLockIds& ids_lock)
{
    if ( gi_lock.IsLoaded() ) {
        return false;
    }
    return gi_lock.SetLoaded(ids_lock.GetData().FindGi(),
                             ids_lock.GetExpirationTime());
}


bool CReaderRequestResult::SetLoadedGiFromSeqIds(const CSeq_id_Handle& id,
                                                 const CLoadLockSeqIds& ids)
{
    TGi gi = ids.GetSeq_ids().FindGi();
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") gi = "<<gi);
    }
    TExpirationTime exp_time = ids.GetExpirationTime();
    return GetGBInfoManager().m_CacheGi.SetLoaded(*this, id, gi, exp_time);
}


/////////////////////////////////////////////////////////////////////////////
// Seq-id -> GI

bool
CReaderRequestResult::IsLoadedGi(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheGi.IsLoaded(*this, id) ||
        IsLoadedSeqIds(id);
}


bool
CReaderRequestResult::MarkLoadingGi(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheGi.MarkLoading(*this, id);
}


CReaderRequestResult::TInfoLockGi
CReaderRequestResult::GetLoadLockGi(const CSeq_id_Handle& id)
{
    // if connection is allocated we cannot wait for another lock because
    // of possible deadlock.
    EDoNotWait do_not_wait = m_AllocatedConnection? eDoNotWait: eAllowWaiting;
    TInfoLockGi lock =
        GetGBInfoManager().m_CacheGi.GetLoadLock(*this, id, do_not_wait);
    if ( !lock.IsLoaded() ) {
        TInfoLockIds ids_lock = GetLoadedSeqIds(id);
        if ( ids_lock ) {
            UpdateGiFromSeqIds(lock, ids_lock);
        }
    }
    return lock;
}


CReaderRequestResult::TInfoLockGi
CReaderRequestResult::GetLoadedGi(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheGi.GetLoaded(*this, id);
}


bool
CReaderRequestResult::SetLoadedGi(const CSeq_id_Handle& id,
                                  const TGi& value)
{
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") gi = "<<value);
    }
    return GetGBInfoManager().m_CacheGi.SetLoaded(*this, id, value);
}


/////////////////////////////////////////////////////////////////////////////
// string -> GI

bool
CReaderRequestResult::IsLoadedGi(const string& id)
{
    return GetGBInfoManager().m_CacheStrGi.IsLoaded(*this, id) ||
        IsLoadedSeqIds(id);
}


bool
CReaderRequestResult::MarkLoadingGi(const string& id)
{
    return GetGBInfoManager().m_CacheStrGi.MarkLoading(*this, id);
}


CReaderRequestResult::TInfoLockGi
CReaderRequestResult::GetLoadLockGi(const string& id)
{
    // if connection is allocated we cannot wait for another lock because
    // of possible deadlock.
    EDoNotWait do_not_wait = m_AllocatedConnection? eDoNotWait: eAllowWaiting;
    TInfoLockGi lock =
        GetGBInfoManager().m_CacheStrGi.GetLoadLock(*this, id, do_not_wait);
    if ( !lock.IsLoaded() ) {
        TInfoLockIds ids_lock = GetLoadedSeqIds(id);
        if ( ids_lock ) {
            UpdateGiFromSeqIds(lock, ids_lock);
        }
    }
    return lock;
}


CReaderRequestResult::TInfoLockGi
CReaderRequestResult::GetLoadedGi(const string& id)
{
    return GetGBInfoManager().m_CacheStrGi.GetLoaded(*this, id);
}


bool
CReaderRequestResult::SetLoadedGi(const string& id,
                                  const TGi& value)
{
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:StrId("<<id<<") gi = "<<value);
    }
    return GetGBInfoManager().m_CacheStrGi.SetLoaded(*this, id, value);
}


/////////////////////////////////////////////////////////////////////////////
// Seq-ids -> Label

bool CReaderRequestResult::UpdateLabelFromSeqIds(TInfoLockLabel& label_lock,
                                                 const TInfoLockIds& ids_lock)
{
    if ( label_lock.IsLoaded() ) {
        return false;
    }
    return label_lock.SetLoaded(ids_lock.GetData().FindLabel(),
                                ids_lock.GetExpirationTime());
}


bool CReaderRequestResult::SetLoadedLabelFromSeqIds(const CSeq_id_Handle& id,
                                                    const CLoadLockSeqIds& ids)
{
    string label = ids.GetSeq_ids().FindLabel();
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") label = "<<label);
    }
    TExpirationTime exp_time = ids.GetExpirationTime();
    return GetGBInfoManager().m_CacheLabel.SetLoaded(*this, id, label, exp_time);
}


/////////////////////////////////////////////////////////////////////////////
// Seq-id -> Label

bool
CReaderRequestResult::IsLoadedLabel(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheLabel.IsLoaded(*this, id);
}


bool
CReaderRequestResult::MarkLoadingLabel(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheLabel.MarkLoading(*this, id);
}


CReaderRequestResult::TInfoLockLabel
CReaderRequestResult::GetLoadLockLabel(const CSeq_id_Handle& id)
{
    // if connection is allocated we cannot wait for another lock because
    // of possible deadlock.
    EDoNotWait do_not_wait = m_AllocatedConnection? eDoNotWait: eAllowWaiting;
    return GetGBInfoManager().m_CacheLabel.GetLoadLock(*this, id, do_not_wait);
}


CReaderRequestResult::TInfoLockLabel
CReaderRequestResult::GetLoadedLabel(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheLabel.GetLoaded(*this, id);
}


bool
CReaderRequestResult::SetLoadedLabel(const CSeq_id_Handle& id,
                                     const string& value)
{
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") label = "<<value);
    }
    return GetGBInfoManager().m_CacheLabel.SetLoaded(*this, id, value);
}


/////////////////////////////////////////////////////////////////////////////
// Seq-id -> TaxID

bool
CReaderRequestResult::IsLoadedTaxId(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheTaxId.IsLoaded(*this, id);
}


bool
CReaderRequestResult::MarkLoadingTaxId(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheTaxId.MarkLoading(*this, id);
}


CReaderRequestResult::TInfoLockTaxId
CReaderRequestResult::GetLoadLockTaxId(const CSeq_id_Handle& id)
{
    // if connection is allocated we cannot wait for another lock because
    // of possible deadlock.
    EDoNotWait do_not_wait = m_AllocatedConnection? eDoNotWait: eAllowWaiting;
    return GetGBInfoManager().m_CacheTaxId.GetLoadLock(*this, id, do_not_wait);
}


CReaderRequestResult::TInfoLockTaxId
CReaderRequestResult::GetLoadedTaxId(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheTaxId.GetLoaded(*this, id);
}


bool
CReaderRequestResult::SetLoadedTaxId(const CSeq_id_Handle& id,
                                     const TTaxId& value)
{
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") tax_id = "<<value);
    }
    return GetGBInfoManager().m_CacheTaxId.SetLoaded(*this, id, value);
}


/////////////////////////////////////////////////////////////////////////////
// Seq-id -> Hash

bool
CReaderRequestResult::IsLoadedHash(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheHash.IsLoaded(*this, id);
}


bool
CReaderRequestResult::MarkLoadingHash(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheHash.MarkLoading(*this, id);
}


CReaderRequestResult::TInfoLockHash
CReaderRequestResult::GetLoadLockHash(const CSeq_id_Handle& id)
{
    // if connection is allocated we cannot wait for another lock because
    // of possible deadlock.
    EDoNotWait do_not_wait = m_AllocatedConnection? eDoNotWait: eAllowWaiting;
    return GetGBInfoManager().m_CacheHash.GetLoadLock(*this, id, do_not_wait);
}


CReaderRequestResult::TInfoLockHash
CReaderRequestResult::GetLoadedHash(const CSeq_id_Handle& id)
{
    return GetGBInfoManager().m_CacheHash.GetLoaded(*this, id);
}


bool
CReaderRequestResult::SetLoadedHash(const CSeq_id_Handle& id,
                                    const TSequenceHash& value)
{
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") hash = "<<value);
    }
    return GetGBInfoManager().m_CacheHash.SetLoaded(*this, id, value);
}


/////////////////////////////////////////////////////////////////////////////
// Seq-id -> BlobIds

bool
CReaderRequestResult::IsLoadedBlobIds(const CSeq_id_Handle& id,
                                      const SAnnotSelector* sel)
{
    TKeyBlob_ids key = s_KeyBlobIds(id, sel);
    return GetGBInfoManager().m_CacheBlobIds.IsLoaded(*this, key);
}


bool
CReaderRequestResult::MarkLoadingBlobIds(const CSeq_id_Handle& id,
                                         const SAnnotSelector* sel)
{
    TKeyBlob_ids key = s_KeyBlobIds(id, sel);
    return GetGBInfoManager().m_CacheBlobIds.MarkLoading(*this, key);
}


CReaderRequestResult::TInfoLockBlobIds
CReaderRequestResult::GetLoadLockBlobIds(const CSeq_id_Handle& id,
                                         const SAnnotSelector* sel)
{
    // if connection is allocated we cannot wait for another lock because
    // of possible deadlock.
    EDoNotWait do_not_wait = m_AllocatedConnection? eDoNotWait: eAllowWaiting;
    TKeyBlob_ids key = s_KeyBlobIds(id, sel);
    return GetGBInfoManager().m_CacheBlobIds.GetLoadLock(*this, key, do_not_wait);
}


CReaderRequestResult::TInfoLockBlobIds
CReaderRequestResult::GetLoadedBlobIds(const CSeq_id_Handle& id,
                                       const SAnnotSelector* sel)
{
    TKeyBlob_ids key = s_KeyBlobIds(id, sel);
    return GetGBInfoManager().m_CacheBlobIds.GetLoaded(*this, key);
}


bool
CReaderRequestResult::SetLoadedBlobIds(const CSeq_id_Handle& id,
                                       const SAnnotSelector* sel,
                                       const CFixedBlob_ids& value)
{
    TKeyBlob_ids key = s_KeyBlobIds(id, sel);
    if ( s_GetLoadTraceLevel() > 0 ) {
        LOG_POST(Info<<"GBLoader:SeqId("<<id<<") blob_ids("<<key.second<<") = "<<value);
    }
    return GetGBInfoManager().m_CacheBlobIds.SetLoaded(*this, key, value);
}


CReaderRequestResult::TKeyBlob_ids
CReaderRequestResult::s_KeyBlobIds(const CSeq_id_Handle& seq_id,
                                   const SAnnotSelector* sel)
{
    TKeyBlob_ids key;
    key.first = seq_id;
    if ( sel && sel->IsIncludedAnyNamedAnnotAccession() ) {
        ITERATE ( SAnnotSelector::TNamedAnnotAccessions, it,
                  sel->GetNamedAnnotAccessions() ) {
            key.second += it->first;
            key.second += ',';
        }
    }
    return key;
}


/////////////////////////////////////////////////////////////////////////////
// Blob state

bool CReaderRequestResult::SetLoadedBlobState(const CBlob_id& blob_id,
                                              TBlobState state)
{
    bool changed = GetGBInfoManager().m_CacheBlobState.SetLoaded(*this,
                                                                 blob_id,
                                                                 state);
    if ( changed ) {
        if ( s_GetLoadTraceLevel() > 0 ) {
            LOG_POST(Info<<"GBLoader:"<<blob_id<<" state = "<<state);
        }
        // set to TSE_Info
        CLoadLockBlob blob(*this, blob_id);
        if ( blob.IsLoadedBlob() ) {
            blob.GetTSE_LoadLock()->SetBlobState(state);
        }
    }
    return changed;
}


bool CReaderRequestResult::IsLoadedBlobState(const CBlob_id& blob_id)
{
    return GetGBInfoManager().m_CacheBlobState.IsLoaded(*this, blob_id);
}


CReaderRequestResult::TInfoLockBlobState
CReaderRequestResult::GetLoadLockBlobState(const CBlob_id& id)
{
    // if connection is allocated we cannot wait for another lock because
    // of possible deadlock.
    EDoNotWait do_not_wait = m_AllocatedConnection? eDoNotWait: eAllowWaiting;
    return GetGBInfoManager().m_CacheBlobState.GetLoadLock(*this, id, do_not_wait);
}


CReaderRequestResult::TInfoLockBlobState
CReaderRequestResult::GetLoadedBlobState(const CBlob_id& id)
{
    return GetGBInfoManager().m_CacheBlobState.GetLoaded(*this, id);
}


void CReaderRequestResult::SetAndSaveBlobState(const TKeyBlob& blob_id,
                                               TBlobVersion blob_state)
{
    if ( !SetLoadedBlobState(blob_id, blob_state) ) {
        return;
    }
    if ( CWriter* writer = GetIdWriter() ) {
        writer->SaveBlobState(*this, blob_id, blob_state);
    }
}


/////////////////////////////////////////////////////////////////////////////
// Blob version

bool CReaderRequestResult::SetLoadedBlobVersion(const CBlob_id& blob_id,
                                                TBlobVersion version)
{
    bool changed = GetGBInfoManager().m_CacheBlobVersion.SetLoaded(*this,
                                                                   blob_id,
                                                                   version);
    if ( changed ) {
        if ( s_GetLoadTraceLevel() > 0 ) {
            LOG_POST(Info<<"GBLoader:"<<blob_id<<" version = "<<version);
        }
        // set to TSE_Info
        CLoadLockBlob blob(*this, blob_id);
        if ( blob.IsLoadedBlob() ) {
            TBlobVersion old_version = blob.GetKnownBlobVersion();
            if ( old_version < 0 ) {
                blob.GetTSE_LoadLock()->SetBlobVersion(version);
            }
            _ASSERT(blob.GetKnownBlobVersion() == version);
        }
    }
    return changed;
}


bool CReaderRequestResult::IsLoadedBlobVersion(const CBlob_id& blob_id)
{
    return GetGBInfoManager().m_CacheBlobVersion.IsLoaded(*this, blob_id);
}


CReaderRequestResult::TInfoLockBlobVersion
CReaderRequestResult::GetLoadLockBlobVersion(const CBlob_id& id)
{
    // if connection is allocated we cannot wait for another lock because
    // of possible deadlock.
    EDoNotWait do_not_wait = m_AllocatedConnection? eDoNotWait: eAllowWaiting;
    return GetGBInfoManager().m_CacheBlobVersion.GetLoadLock(*this, id, do_not_wait);
}


CReaderRequestResult::TInfoLockBlobVersion
CReaderRequestResult::GetLoadedBlobVersion(const CBlob_id& id)
{
    return GetGBInfoManager().m_CacheBlobVersion.GetLoaded(*this, id);
}


void CReaderRequestResult::SetAndSaveBlobVersion(const TKeyBlob& blob_id,
                                                 TBlobVersion version)
{
    if ( !SetLoadedBlobVersion(blob_id, version) ) {
        return;
    }
    if ( CWriter* writer = GetIdWriter() ) {
        writer->SaveBlobVersion(*this, blob_id, version);
    }
}

/////////////////////////////////////////////////////////////////////////////
// Blob

CReaderRequestResult::TInfoLockBlob
CReaderRequestResult::GetLoadLockBlob(const CBlob_id& id)
{
    // if connection is allocated we cannot wait for another lock because
    // of possible deadlock.
    EDoNotWait do_not_wait = m_AllocatedConnection? eDoNotWait: eAllowWaiting;
    return GetGBInfoManager().m_CacheBlob.GetLoadLock(*this, id, do_not_wait);
}


CReaderRequestResult::TInfoLockBlob
CReaderRequestResult::GetLoadedBlob(const CBlob_id& id)
{
    return GetGBInfoManager().m_CacheBlob.GetLoaded(*this, id);
}


/*
void CReaderRequestResult::SetSeq_entry(const TKeyBlob& blob_id,
                                        CLoadLockBlob& blob,
                                        TChunkId chunk_id,
                                        CRef<CSeq_entry> entry,
                                        CTSE_SetObjectInfo* set_info)
{
    if ( !entry ) {
        return;
    }
    if ( chunk_id == kMain_ChunkId ) {
        blob->SetSeq_entry(*entry, set_info);
    }
    else {
        blob->GetSplitInfo().GetChunk(chunk_id).
            x_LoadSeq_entry(*entry, set_info);
    }
}
*/


END_SCOPE(objects)
END_NCBI_SCOPE
