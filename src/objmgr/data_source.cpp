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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   DataSource for object manager
*
*/


#include <objmgr/impl/data_source.hpp>

#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/seqmatch_info.hpp>
#include <objmgr/data_loader.hpp>
#include <objmgr/seq_map.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <objects/seqloc/Seq_loc_equiv.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/seqport_util.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CTSE_LockingSet::CTSE_LockingSet(void)
    : m_LockCount(0)
{
}


CTSE_LockingSet::CTSE_LockingSet(const CTSE_LockingSet& tse_set)
    : m_LockCount(0), m_TSE_set(tse_set.m_TSE_set)
{
}


CTSE_LockingSet::~CTSE_LockingSet(void)
{
    _ASSERT(!x_Locked());
}


DEFINE_CLASS_STATIC_FAST_MUTEX(CTSE_LockingSet::sm_Mutex);

CTSE_LockingSet& CTSE_LockingSet::operator=(const CTSE_LockingSet& tse_set)
{
    CFastMutexGuard guard(sm_Mutex);
    _ASSERT(!x_Locked());
    m_TSE_set = tse_set.m_TSE_set;
    return *this;
}


bool CTSE_LockingSet::insert(CTSE_Info* tse)
{
    CFastMutexGuard guard(sm_Mutex);
    bool ins = m_TSE_set.insert(tse).second;
    if ( ins && x_Locked() ) {
        tse->AddReference();
    }
    return ins;
}


void CTSE_LockingSet::erase(CTSE_Info* tse)
{
    CFastMutexGuard guard(sm_Mutex);
    if ( m_TSE_set.erase(tse) && x_Locked() ) {
        tse->RemoveReference();
    }
}


void CTSE_LockingSet::x_Lock(void)
{
    CFastMutexGuard guard(sm_Mutex);
    if ( m_LockCount++ == 0 ) {
        NON_CONST_ITERATE( TTSESet, it, m_TSE_set ) {
            (*it)->AddReference();
        }
    }
    _ASSERT(m_LockCount > 0);
}


void CTSE_LockingSet::x_Unlock(void)
{
    CFastMutexGuard guard(sm_Mutex);
    _ASSERT(m_LockCount > 0);
    if ( --m_LockCount == 0 ) {
        NON_CONST_ITERATE( TTSESet, it, m_TSE_set ) {
            (*it)->RemoveReference();
        }
    }
}


CDataSource::CDataSource(CDataLoader& loader, CObjectManager& objmgr)
    : m_Loader(&loader),
      m_pTopEntry(0),
      m_ObjMgr(&objmgr),
      m_DirtyAnnotIndexCount(0),
      m_DefaultPriority(99)
{
    m_Loader->SetTargetDataSource(*this);
}


CDataSource::CDataSource(CSeq_entry& entry, CObjectManager& objmgr)
    : m_Loader(0),
      m_pTopEntry(&entry),
      m_ObjMgr(&objmgr),
      m_DirtyAnnotIndexCount(0),
      m_DefaultPriority(9)
{
    AddTSE(entry, false);
}


CDataSource::~CDataSource(void)
{
    DropAllTSEs();
    delete m_Loader;
}


void CDataSource::DropAllTSEs(void)
{
    // Find and drop each TSE
    while ( !m_TSE_InfoMap.empty() ) {
        _VERIFY(DropTSE(m_TSE_InfoMap.begin()->second->GetTSE()));
    }
}


TTSE_Lock CDataSource::x_FindBestTSE(const CSeq_id_Handle& handle) const
{
    TTSE_LockSet all_tse;
    size_t all_count = 0;
    {{
        TReadLockGuard ds_guard(m_DataSource_Mtx);
        TTSEMap::const_iterator tse_set = m_TSE_seq.find(handle);
        if ( tse_set == m_TSE_seq.end() ) {
            return TTSE_Lock();
        }
        ITERATE ( CTSE_LockingSet, it, tse_set->second ) {
            if ( all_tse.insert(TTSE_Lock(*it)).second )
                ++all_count;
        }
        if ( all_count == 0 ) {
            return TTSE_Lock();
        }
    }}
    if ( all_count == 1 ) {
        // There is only one TSE, no matter live or dead
        return TTSE_Lock(*all_tse.begin());
    }
    // The map should not contain empty entries
    _ASSERT(!all_tse.empty());
    TTSE_LockSet live_tse;
    size_t live_count = 0;
    ITERATE ( TTSE_LockSet, tse, all_tse ) {
        // Find live TSEs
        if ( !(*tse)->IsDead() ) {
            live_tse.insert(*tse);
            ++live_count;
        }
    }

    // Check live
    if ( live_count == 1 ) {
        // There is only one live TSE -- ok to use it
        return *live_tse.begin();
    }
    else if ( live_count == 0 ) {
        if ( m_Loader ) {
            CConstRef<CTSE_Info> best
                (m_Loader->ResolveConflict(handle, all_tse));
            if ( best ) {
                return best;
            }
        }
        // No live TSEs -- try to select the best dead TSE
        THROW1_TRACE(runtime_error,
                     "CDataSource::x_FindBestTSE() -- "
                     "Multiple seq-id matches found");
    }
    if ( m_Loader ) {
        // Multiple live TSEs - try to resolve the conflict (the status of some
        // TSEs may change)
        CConstRef<CTSE_Info> best
            (m_Loader->ResolveConflict(handle, live_tse));
        if ( best ) {
            return best;
        }
    }
    THROW1_TRACE(runtime_error,
                 "CDataSource::x_FindBestTSE() -- "
                 "Multiple live entries found");
}


TTSE_Lock CDataSource::GetBlobById(const CSeq_id_Handle& idh)
{
    TReadLockGuard guard(m_DataSource_Mtx);
    TTSEMap::iterator tse_set = m_TSE_seq.find(idh);
    if (tse_set == m_TSE_seq.end()) {
        // Request TSE-info from loader if any
        if ( m_Loader ) {
            //
        }
        else {
            // No such blob, no loader to call
            return TTSE_Lock();
        }
//###
    }
//###
    return TTSE_Lock();
}


const CSeq_entry& CDataSource::GetTSEFromInfo(const TTSE_Lock& tse)
{
    if ( tse->NullSeq_entry() ) {
        //### Force loader to load the TSE by blob id
    }
    return tse->GetSeq_entry();
}


CConstRef<CBioseq_Info> CDataSource::GetBioseq_Info(const CSeqMatch_Info& info)
{
    CRef<CBioseq_Info> ret;
    // The TSE is locked by the scope, so, it can not be deleted.
    CTSE_Info::TBioseqs::const_iterator found =
        info.GetTSE_Info().m_Bioseqs.find(info.GetIdHandle());
    if ( found != info.GetTSE_Info().m_Bioseqs.end() ) {
        ret = found->second;
    }
    return ret;
}


void CDataSource::FilterSeqid(TSeq_id_HandleSet& setResult,
                              const TSeq_id_HandleSet& setSource) const
{
    _ASSERT(&setResult != &setSource);
    TReadLockGuard guard(m_DataSource_Mtx);
    ITERATE ( TSeq_id_HandleSet, it, setSource ) {
        // if it is in my map
        if ( m_TSE_seq.find(*it) != m_TSE_seq.end() ) {
            //### The id handle is reported to be good, but it can be deleted
            //### by the next request!
            setResult.insert(*it);
        }
    }
}


const CBioseq& CDataSource::GetBioseq(const CBioseq_Info& info)
{
    // the handle must be resolved to this data source
    _ASSERT(&info.GetDataSource() == this);
    // Bioseq core and TSE must be loaded if there exists a handle
    // Loader may be called to load descriptions (not included in core)
    if ( m_Loader ) {
        // Send request to the loader
        /*
        CHandleRangeMap hrm;
        hrm.AddRange(handle.GetSeq_id_Handle(),
                     CHandleRange::TRange::GetWhole(), eNa_strand_unknown);
        m_Loader->GetRecords(hrm, CDataLoader::eBioseq);
        */
    }
    return info.GetBioseq();
}


const CSeq_entry& CDataSource::GetTSE(const CTSE_Info& info)
{
    // the handle must be resolved to this data source
    _ASSERT(&info.GetDataSource() == this);
    // Bioseq and TSE must be loaded if there exists a handle
    return info.GetTSE();
}


CDataSource::TBioseqCore CDataSource::GetBioseqCore(const CBioseq_Info& info)
{
    // the handle must be resolved to this data source
    _ASSERT(&info.GetDataSource() == this);
    // Bioseq core and TSE must be loaded if there exists a handle --
    // just return the bioseq as-is.
    return TBioseqCore(&info.GetBioseq());
}


const CSeqMap& CDataSource::GetSeqMap(const CBioseq_Info& info)
{
    // the handle must be resolved to this data source
    _ASSERT(&info.GetDataSource() == this);
    return x_GetSeqMap(info);
}


const CSeqMap& CDataSource::x_GetSeqMap(const CBioseq_Info& info)
{
    if ( !info.m_SeqMap ) {
        // No need to lock anything as TSE should be locked by the handle
        //### Lock seq-maps to prevent duplicate seq-map creation
        //CMutexGuard guard(m_DataSource_Mtx);    
        if ( !info.m_SeqMap ) {
            // Call loader first
            if ( m_Loader ) {
                // Send request to the loader
                ITERATE ( CBioseq::TId, it, info.GetBioseq().GetId() ) {
                    CHandleRangeMap hrm;
                    hrm.AddRange(GetSeq_id_Mapper().GetHandle(**it),
                                 CHandleRange::TRange::GetWhole(),
                                 eNa_strand_unknown);
                    m_Loader->GetRecords(hrm, CDataLoader::eBioseq);
                    break;
                }
            }
            if ( !info.m_SeqMap ) {
                THROW1_TRACE(runtime_error,
                             "CDataSource::x_GetSeqMap: "
                             "sequence doesn't have CSeqMap object");
            }
            _ASSERT(info.m_SeqMap);
        }
    }
    return *info.m_SeqMap;
}


CRef<CTSE_Info> CDataSource::AddTSE(CSeq_entry& tse, bool dead,
                                    const CObject* blob_id)
{
    TWriteLockGuard guard(m_DataSource_Mtx);

    CRef<CSeq_entry_Info> entry_info = x_FindSeq_entry_Info(tse);
    if ( entry_info ) {
        THROW1_TRACE(runtime_error,
                     "CDataSource::AddTSE(): tse already added");
    }

    if ( tse.GetParentEntry() ) {
        THROW1_TRACE(runtime_error,
                     "CDataSource::AddTSE(): tse is child of another entry");
    }

    tse.Parentize();

    return x_AttachTSE(tse, dead, blob_id);
}


bool CDataSource::DropTSE(CSeq_entry& tse)
{
    // Allow to drop top-level seq-entries only
    if ( tse.GetParentEntry() ) {
        _TRACE("DropTSE: DS="<<this<<" TSE="<<&tse<<" - non-top");
        return false;
    }

    CRef<CSeq_entry> ref(&tse);

    // Lock indexes
    TWriteLockGuard guard(m_DataSource_Mtx);    

    CRef<CTSE_Info> tse_info_lock = x_FindTSE_Info(tse);
    if ( !tse_info_lock ) {
        _TRACE("DropTSE: DS="<<this<<" TSE="<<&tse<<" - not mine");
        return false;
    }

    CTSE_Info* tse_info = &*tse_info_lock;
    _ASSERT(tse_info->Locked());
    tse_info_lock.Reset();
    if ( tse_info->Locked() ) {
        _TRACE("DropTSE: DS="<<this<<" TSE="<<&tse<<" - locked");
        return false; // Not really dropped, although found
    }

    x_DetachTSE(*tse_info);
    return true;
}


CRef<CTSE_Info> CDataSource::x_AttachTSE(CSeq_entry& tse, bool dead,
                                         const CObject* blob_id)
{
    CRef<CTSE_Info> tse_info = x_CreateTSE_Info(tse, dead, blob_id);

    x_AttachSeq_entry_Contents(*tse_info);

    return tse_info;
}


void CDataSource::x_DetachTSE(CTSE_Info& tse_info)
{
    if ( m_Loader ) {
        m_Loader->DropTSE(tse_info);
    }

    x_DetachSeq_entry_Contents(tse_info);
    x_DetachSeq_annots(tse_info);

    x_DeleteTSE_Info(tse_info);
}


/////////////////////////////////////////////////////////////////////////////
//   mapping of various XXX_Info
// CDataSource must be guarded by mutex
/////////////////////////////////////////////////////////////////////////////

CConstRef<CTSE_Info>
CDataSource::GetTSEInfo(const CSeq_entry& tse)
{
    TReadLockGuard guard(m_DataSource_Mtx);
    return x_FindTSE_Info(tse);
}


CConstRef<CSeq_entry_Info>
CDataSource::GetSeq_entry_Info(const CSeq_entry& entry)
{
    TReadLockGuard guard(m_DataSource_Mtx);
    return x_FindSeq_entry_Info(entry);
}


CConstRef<CSeq_annot_Info>
CDataSource::GetSeq_annot_Info(const CSeq_annot& annot)
{
    TReadLockGuard guard(m_DataSource_Mtx);
    return x_FindSeq_annot_Info(annot);
}


CRef<CTSE_Info> CDataSource::x_FindTSE_Info(const CSeq_entry& obj)
{
    CRef<CTSE_Info> ret;
    TTSE_InfoMap::iterator found = m_TSE_InfoMap.find(&obj);
    if ( found != m_TSE_InfoMap.end() ) {
        ret.Reset(found->second);
    }
    return ret;
}


CRef<CSeq_entry_Info> CDataSource::x_FindSeq_entry_Info(const CSeq_entry& obj)
{
    CRef<CSeq_entry_Info> ret;
    TSeq_entry_InfoMap::iterator found = m_Seq_entry_InfoMap.find(&obj);
    if ( found != m_Seq_entry_InfoMap.end() ) {
        ret.Reset(found->second);
    }
    return ret;
}


CRef<CSeq_annot_Info> CDataSource::x_FindSeq_annot_Info(const CSeq_annot& obj)
{
    CRef<CSeq_annot_Info> ret;
    TSeq_annot_InfoMap::iterator found = m_Seq_annot_InfoMap.find(&obj);
    if ( found != m_Seq_annot_InfoMap.end() ) {
        ret.Reset(found->second);
    }
    return ret;
}


CRef<CBioseq_Info> CDataSource::x_FindBioseq_Info(const CBioseq& obj)
{
    CRef<CBioseq_Info> ret;
    TBioseq_InfoMap::iterator found = m_Bioseq_InfoMap.find(&obj);
    if ( found != m_Bioseq_InfoMap.end() ) {
        ret.Reset(found->second);
    }
    return ret;
}


CRef<CTSE_Info> CDataSource::x_CreateTSE_Info(CSeq_entry& entry,
                                              bool dead,
                                              const CObject* blob_id)
{
    CRef<CTSE_Info> info(new CTSE_Info(this, entry, dead, blob_id));
    _TRACE("x_CreateTSE_Info: DS="<<this<<" TSE="<<&info->GetTSE()<<" TSE_Info="<<&*info);
    _VERIFY(m_TSE_InfoMap.insert
            (TTSE_InfoMap::value_type(&entry, info)).second);
    _VERIFY(m_Seq_entry_InfoMap.insert
            (TSeq_entry_InfoMap::value_type(&entry,
                                            &*info)).second);
    return info;
}


CRef<CSeq_entry_Info> CDataSource::x_CreateSeq_entry_Info(CSeq_entry& entry,
                                                          CSeq_entry_Info& par)
{
    CRef<CSeq_entry_Info> info(new CSeq_entry_Info(entry, par));
    _VERIFY(m_Seq_entry_InfoMap.insert
            (TSeq_entry_InfoMap::value_type(&entry, &*info)).second);
    return info;
}


CRef<CSeq_annot_Info> CDataSource::x_CreateSeq_annot_Info(CSeq_annot& annot,
                                                          CSeq_entry_Info& par)
{
    CRef<CSeq_annot_Info> info(new CSeq_annot_Info(annot, par));
    _VERIFY(m_Seq_annot_InfoMap.insert
            (TSeq_annot_InfoMap::value_type(&annot, &*info)).second);
    _ASSERT(!info->m_Indexed);
    ++m_DirtyAnnotIndexCount;
    return info;
}


CRef<CBioseq_Info> CDataSource::x_CreateBioseq_Info(CBioseq& seq,
                                                    CSeq_entry_Info& par)
{
    CRef<CBioseq_Info> info(new CBioseq_Info(seq, par));
    _VERIFY(m_Bioseq_InfoMap.insert
            (TBioseq_InfoMap::value_type(&seq, &*info)).second);
    return info;
}


void CDataSource::x_DeleteTSE_Info(CTSE_Info& info)
{
    _TRACE("x_DeleteTSE_Info: DS="<<this<<" TSE="<<&info.GetTSE()<<" TSE_Info="<<&info);
    _VERIFY(m_Seq_entry_InfoMap.erase(&info.GetSeq_entry()));
    _VERIFY(m_TSE_InfoMap.erase(&info.GetSeq_entry()));
}


void CDataSource::x_DeleteSeq_entry_Info(CSeq_entry_Info& info)
{
    _VERIFY(m_Seq_entry_InfoMap.erase(&info.GetSeq_entry()));
}


void CDataSource::x_DeleteSeq_annot_Info(CSeq_annot_Info& info)
{
    _ASSERT(!info.m_Indexed);
    --m_DirtyAnnotIndexCount;
    _VERIFY(m_Seq_annot_InfoMap.erase(&info.GetSeq_annot()));
}


void CDataSource::x_DeleteBioseq_Info(CBioseq_Info& info)
{
    _VERIFY(m_Bioseq_InfoMap.erase(&info.GetBioseq()));
}


void CDataSource::x_RegisterAnnotObject(const CObject* /*object*/,
                                        CAnnotObject_Info* /*info*/)
{
    //_VERIFY(m_AnnotObject_InfoMap.
    //        insert(TAnnotObject_InfoMap::value_type(object, info)).second);
}


void CDataSource::x_UnregisterAnnotObject(const CObject* /*object*/)
{
    //_VERIFY(m_AnnotObject_InfoMap.erase(object));
}


/////////////////////////////////////////////////////////////////////////////


bool CDataSource::AttachEntry(CSeq_entry& parent, CSeq_entry& entry)
{
    TWriteLockGuard guard(m_DataSource_Mtx);

    CRef<CSeq_entry_Info> parent_info = x_FindSeq_entry_Info(parent);
    if ( !parent_info ) {
        return false;
    }
    if ( parent.IsSeq() ) {
        return false;
    }

    CRef<CSeq_entry_Info> entry_info = x_FindSeq_entry_Info(entry);
    if ( entry_info ) {
        return false;
    }
    if ( entry.GetParentEntry() ) {
        return false;
    }

    // insert it to parent entry
    parent_info->GetSeq_entry().SetSet().SetSeq_set()
        .push_back(CRef<CSeq_entry>(&entry));
    // parentize
    parent_info->GetSeq_entry().Parentize();

    entry_info = x_CreateSeq_entry_Info(entry, *parent_info);
    x_AttachSeq_entry_Contents(*entry_info);
    return true;
}


void CDataSource::x_AttachSeq_entry_Contents(CSeq_entry_Info& entry_info)
{
    switch ( entry_info.GetSeq_entry().Which() ) {
    case CSeq_entry::e_Seq:
        x_AttachBioseq(entry_info.GetSeq_entry().SetSeq(), entry_info);
        break;
    case CSeq_entry::e_Set:
        x_AttachBioseqSet(entry_info.GetSeq_entry().SetSet(), entry_info);
        break;
    }
}


void CDataSource::x_DetachSeq_entry_Contents(CSeq_entry_Info& entry)
{
    NON_CONST_ITERATE( CSeq_entry_Info::TChildren, it, entry.m_Children ) {
        if ( *it ) {
            x_DetachSeq_entry_Contents(**it);
            x_DeleteSeq_entry_Info(**it);
            it->Reset();
        }
    }
    entry.m_Children.clear();

    NON_CONST_ITERATE( CSeq_entry_Info::TAnnots, it, entry.m_Annots ) {
        if ( *it ) {
            x_DetachSeq_annot(**it);
            x_DeleteSeq_annot_Info(**it);
            it->Reset();
        }
    }
    entry.m_Annots.clear();

    if ( entry.m_Bioseq ) {
        x_DetachBioseq(*entry.m_Bioseq);
        x_DeleteBioseq_Info(*entry.m_Bioseq);
        entry.m_Bioseq.Reset();
    }
}


void CDataSource::x_AttachBioseqSet(CBioseq_set& seq_set,
                                    CSeq_entry_Info& parent_info)
{
    NON_CONST_ITERATE ( CBioseq_set::TSeq_set, it, seq_set.SetSeq_set() ) {
        CRef<CSeq_entry_Info> info = x_CreateSeq_entry_Info(**it, parent_info);
        x_AttachSeq_entry_Contents(*info);
    }
    
    if ( seq_set.IsSetAnnot() ) {
        x_AttachSeq_annots(seq_set.SetAnnot(), parent_info);
    }
}


void CDataSource::x_AttachBioseq(CBioseq& seq, CSeq_entry_Info& parent_info)
{
    CRef<CBioseq_Info> seq_info = x_CreateBioseq_Info(seq, parent_info);

    x_IndexBioseq(*seq_info);

    if ( seq.IsSetAnnot() ) {
        x_AttachSeq_annots(seq.SetAnnot(), parent_info);
    }
}


void CDataSource::x_IndexBioseq(CBioseq_Info& seq_info)
{
    CRef<CTSE_Info> tse_info(&seq_info.GetTSE_Info());
    ITERATE ( CBioseq::TId, id, seq_info.GetBioseq().GetId() ) {
        // Find the bioseq index
        CSeq_id_Handle key = GetSeq_id_Mapper().GetHandle(**id);
        m_TSE_seq[key].insert(&*tse_info);
        CTSE_Info::TBioseqs::iterator found = tse_info->m_Bioseqs.find(key);
        if ( found != tse_info->m_Bioseqs.end() ) {
            // No duplicate bioseqs in the same TSE
            string sid1, sid2, si;
            {{
                CNcbiOstrstream os;
                ITERATE ( CBioseq::TId, it,
                          found->second->GetBioseq().GetId() ) {
                    os << (*it)->DumpAsFasta() << " | ";
                }
                sid1 = CNcbiOstrstreamToString(os);
            }}
            {{
                CNcbiOstrstream os;
                ITERATE (CBioseq::TId, it, seq_info.GetBioseq().GetId()) {
                    os << (*it)->DumpAsFasta() << " | ";
                }
                sid2 = CNcbiOstrstreamToString(os);
            }}
            {{
                CNcbiOstrstream os;
                os << (*id)->DumpAsFasta();
                si = CNcbiOstrstreamToString(os);
            }}
            THROW1_TRACE(runtime_error,
                         " duplicate Bioseq id '" + si + "' present in" +
                         "\n  seq1: " + sid1 +
                         "\n  seq2: " + sid2);
        }
        else {
            // Add new seq-id synonym
            seq_info.m_Synonyms.insert(key);
        }
    }

    ITERATE ( CBioseq_Info::TSynonyms, syn, seq_info.m_Synonyms ) {
        tse_info->m_Bioseqs[*syn] = &seq_info;
    }

    x_CreateSeqMap(seq_info);
}


void CDataSource::x_DetachBioseq(CBioseq_Info& seq_info)
{
    CRef<CTSE_Info> tse_info(&seq_info.GetTSE_Info());
    ITERATE ( CBioseq_Info::TSynonyms, syn, seq_info.m_Synonyms ) {
        _ASSERT(tse_info->m_Bioseqs[*syn] == &seq_info);
        _VERIFY(tse_info->m_Bioseqs.erase(*syn));
        m_TSE_seq[*syn].erase(&*tse_info);
    }
}


void CDataSource::x_AttachSeq_annots(TAnnots& annot,
                                     CSeq_entry_Info& parent_info)
{
    NON_CONST_ITERATE( TAnnots, it, annot ) {
        x_CreateSeq_annot_Info(**it, parent_info);
    }
}


bool CDataSource::AttachMap(CSeq_entry& seq, CSeqMap& seqmap)
{
    TWriteLockGuard guard(m_DataSource_Mtx);
    //### Lock the entry to prevent destruction or modification
    //### May need to lock the TSE instead.
    if ( !seq.IsSeq() ) {
        return false;
    }
    CRef<CBioseq_Info> seq_info = x_FindBioseq_Info(seq.GetSeq());
    if ( !seq_info ) {
        return false;
    }
    // Lock the TSE
    seq_info->m_SeqMap.Reset(&seqmap);
    return true;
}


bool CDataSource::AttachAnnot(CSeq_entry& entry,
                              CSeq_annot& annot)
{
    TWriteLockGuard guard(m_DataSource_Mtx);
    //### Lock the entry to prevent destruction or modification
    //### May need to lock the TSE instead. In this case also lock
    //### the entries list for a while.
    CRef<CSeq_entry_Info> entry_info = x_FindSeq_entry_Info(entry);
    if ( !entry_info ) {
        return false;
    }

    CRef<CSeq_annot_Info> annot_info = x_FindSeq_annot_Info(annot);
    if ( annot_info ) {
        return false;
    }
    
    switch ( entry.Which() ) {
    case CSeq_entry::e_Set:
        entry.SetSet().SetAnnot().push_back(CRef<CSeq_annot>(&annot));
        break;
    case CSeq_entry::e_Seq:
        entry.SetSeq().SetAnnot().push_back(CRef<CSeq_annot>(&annot));
        break;
    default:
        return false;
    }

    x_CreateSeq_annot_Info(annot, *entry_info);

    return true;
}


void CDataSource::x_DetachSeq_annots(CTSE_Info& tse_info)
{
    ITERATE ( CTSE_Info::TAnnotObjs, it, tse_info.m_AnnotObjs ) {
        x_DropTSE_ref(it->first, &tse_info);
    }
    tse_info.m_AnnotObjs.clear();
}


void CDataSource::x_IndexAllAnnots(CSeq_entry_Info& entry_info)
{
    NON_CONST_ITERATE ( CSeq_entry_Info::TAnnots, it, entry_info.m_Annots ) {
        x_IndexSeq_annot(**it);
    }
    NON_CONST_ITERATE(CSeq_entry_Info::TChildren, it, entry_info.m_Children) {
        x_IndexAllAnnots(**it);
    }
}


bool CDataSource::x_RemoveSeq_annot(TAnnots& annot_set,
                                    CSeq_annot& annot)
{
    annot_set.remove(CRef<CSeq_annot>(&annot));
    return annot_set.empty();
}


bool CDataSource::RemoveAnnot(CSeq_entry& entry, CSeq_annot& annot)
{
    if ( m_Loader ) {
        THROW1_TRACE(runtime_error,
            "CDataSource::RemoveAnnot() -- can not modify a loaded entry");
    }
    CRef<CSeq_entry_Info> entry_info = x_FindSeq_entry_Info(entry);
    if ( !entry_info ) {
        return false;
    }

    CRef<CSeq_annot_Info> annot_info = x_FindSeq_annot_Info(annot);
    if ( !annot_info ) {
        return false;
    }

    x_UnindexSeq_annot(*annot_info);

    x_DeleteSeq_annot_Info(*annot_info);
    entry_info->m_Annots.remove(annot_info);

    switch ( entry.Which() ) {
    case CSeq_entry::e_Set:
        if ( x_RemoveSeq_annot(entry.SetSet().SetAnnot(), annot) ) {
            entry.SetSet().ResetAnnot();
        }
        break;
    case CSeq_entry::e_Seq:
        if ( x_RemoveSeq_annot(entry.SetSeq().SetAnnot(), annot) ) {
            entry.SetSeq().ResetAnnot();
        }
        break;
    default:
        _ASSERT(0);
        break;
    }
    return true;
}


bool CDataSource::ReplaceAnnot(CSeq_entry& entry,
                               CSeq_annot& old_annot,
                               CSeq_annot& new_annot)
{
    if ( !RemoveAnnot(entry, old_annot) )
        return false;
    return AttachAnnot(entry, new_annot);
}


void PrintSeqMap(const string& /*id*/, const CSeqMap& /*smap*/)
{
#if _DEBUG && 0
    _TRACE("CSeqMap("<<id<<"):");
    ITERATE ( CSeqMap, it, smap ) {
        switch ( it.GetType() ) {
        case CSeqMap::eSeqGap:
            _TRACE("    gap: "<<it.GetLength());
            break;
        case CSeqMap::eSeqData:
            _TRACE("    data: "<<it.GetLength());
            break;
        case CSeqMap::eSeqRef:
            _TRACE("    ref: "<<it.GetRefSeqid().AsString()<<' '<<
                   it.GetRefPosition()<<' '<<it.GetLength()<<' '<<
                   it.GetRefMinusStrand());
            break;
        default:
            _TRACE("    bad: "<<it.GetType()<<' '<<it.GetLength());
            break;
        }
    }
    _TRACE("end of CSeqMap "<<id);
#endif
}


void CDataSource::x_CreateSeqMap(CBioseq_Info& seq_info)
{
    seq_info.m_SeqMap = 
        CSeqMap::CreateSeqMapForBioseq(seq_info.GetBioseq(), this);
}



void CDataSource::x_GetAnnotData(const CHandleRangeMap& loc,
                                 const SAnnotSelector& sel)
{
    if ( m_Loader ) {
        // Send request to the loader
        switch ( sel.GetAnnotChoice() ) {
        case CSeq_annot::C_Data::e_Ftable:
            if ( !sel.IsSetDataSources() ) {
                m_Loader->GetRecords(loc, CDataLoader::eFeatures);
                m_Loader->GetRecords(loc, CDataLoader::eExternal);
            }
            else {
                if ( sel.HasDataSource("") ) {
                    m_Loader->GetRecords(loc, CDataLoader::eFeatures);
                }
                if ( sel.HasDataSource("SNP") ) {
                    m_Loader->GetRecords(loc, CDataLoader::eExternal);
                }
            }
            break;
        case CSeq_annot::C_Data::e_Align:
            //### Need special flag for alignments
            m_Loader->GetRecords(loc, CDataLoader::eAlign);
            break;
        case CSeq_annot::C_Data::e_Graph:
            m_Loader->GetRecords(loc, CDataLoader::eGraph);
            break;
        }
    }
}


void CDataSource::UpdateAnnotIndex(const CHandleRangeMap& loc,
                                   const SAnnotSelector& sel)
{
    x_GetAnnotData(loc, sel);
    if ( m_DirtyAnnotIndexCount ) {
        TWriteLockGuard ds_guard(m_DataSource_Mtx);
        // Index all annotations if not indexed yet
        if ( m_DirtyAnnotIndexCount ) {
            NON_CONST_ITERATE ( TTSE_InfoMap, it, m_TSE_InfoMap ) {
                //### Lock TSE so that another thread can not index it too
                x_IndexAllAnnots(*it->second);
            }
        }
        _ASSERT(!m_DirtyAnnotIndexCount);
    }
}


void CDataSource::UpdateAnnotIndex(const CHandleRangeMap& loc,
                                   const SAnnotSelector& sel,
                                   const CSeq_entry_Info& entry_info)
{
    x_GetAnnotData(loc, sel);
    TWriteLockGuard ds_guard(m_DataSource_Mtx);
    x_IndexAllAnnots(const_cast<CSeq_entry_Info&>(entry_info));
}


void CDataSource::UpdateAnnotIndex(const CSeq_annot_Info& annot_info)
{
    TWriteLockGuard ds_guard(m_DataSource_Mtx);
    x_IndexSeq_annot(const_cast<CSeq_annot_Info&>(annot_info));
}


void CDataSource::GetSynonyms(const CSeq_id_Handle& main_idh,
                              set<CSeq_id_Handle>& syns)
{
    //### The TSE returns locked, unlock it
    CSeq_id_Handle idh = main_idh;
    TTSE_Lock tse_info(x_FindBestTSE(main_idh));
    if ( !tse_info ) {
        // Try to find the best matching id (not exactly equal)
        TSeq_id_HandleSet hset;
        GetSeq_id_Mapper().GetMatchingHandles(idh, hset);
        ITERATE(TSeq_id_HandleSet, hit, hset) {
            if ( bool(tse_info)  &&  idh.IsBetter(*hit) )
                continue;
            TTSE_Lock tmp_tse(x_FindBestTSE(*hit));
            if ( tmp_tse ) {
                tse_info = tmp_tse;
                idh = *hit;
            }
        }
    }
    // At this point the tse_info (if not null) should be locked
    if ( tse_info ) {
        CTSE_Info::TBioseqs::const_iterator info =
            tse_info->m_Bioseqs.find(idh);
        if (info == tse_info->m_Bioseqs.end()) {
            // Just copy the existing id
            syns.insert(main_idh);
        }
        else {
            // Create range list for each synonym of a seq_id
            const CBioseq_Info::TSynonyms& syn = info->second->m_Synonyms;
            ITERATE ( CBioseq_Info::TSynonyms, syn_it, syn ) {
                syns.insert(*syn_it);
            }
        }
    }
    else {
        // Just copy the existing range map
        syns.insert(main_idh);
    }
}


void CDataSource::GetTSESetWithAnnots(const CSeq_id_Handle& idh,
                                      TTSE_LockSet& with_ref)
{
    TReadLockGuard guard(m_DataSource_Mtx);    

    TTSEMap::const_iterator rtse_it = m_TSE_ref.find(idh);
    if (rtse_it != m_TSE_ref.end()  &&  !rtse_it->second.empty()) {
        ITERATE(CTSE_LockingSet::TTSESet, tse, rtse_it->second) {
            if ( (*tse)->m_Bioseqs.find(idh) == (*tse)->m_Bioseqs.end() ) {
                _ASSERT((*tse)->m_Bioseqs.find(idh)==(*tse)->m_Bioseqs.end());
                with_ref.insert(TTSE_Lock(*tse)); // with reference only
            }
        }
    }
}


void CDataSource::x_IndexSeq_annot(CSeq_annot_Info& annot_info)
{
    if ( annot_info.m_Indexed ) {
        return;
    }

    _ASSERT(m_DirtyAnnotIndexCount > 0);

    CSeq_annot::C_Data& data = annot_info.GetSeq_annot().SetData();
    switch ( data.Which() ) {
    case CSeq_annot::C_Data::e_Ftable:
        annot_info.x_MapAnnotObjects(data.SetFtable());
        break;
    case CSeq_annot::C_Data::e_Align:
        annot_info.x_MapAnnotObjects(data.SetAlign());
        break;
    case CSeq_annot::C_Data::e_Graph:
        annot_info.x_MapAnnotObjects(data.SetGraph());
        break;
    }

    annot_info.m_Indexed = true;
    --m_DirtyAnnotIndexCount;
    _ASSERT(m_DirtyAnnotIndexCount >= 0);
}


void CDataSource::x_UnindexSeq_annot(CSeq_annot_Info& annot_info)
{
    TWriteLockGuard ds_guard(m_DataSource_Mtx);
    if ( !annot_info.m_Indexed ) {
        return;
    }

    annot_info.x_UnmapAnnotObjects();

    annot_info.m_Indexed = false;
    ++m_DirtyAnnotIndexCount;
}


void CDataSource::x_DetachSeq_annot(CSeq_annot_Info& annot_info)
{
    TWriteLockGuard ds_guard(m_DataSource_Mtx);
    if ( !annot_info.m_Indexed ) {
        return;
    }

    annot_info.x_DropAnnotObjects();

    annot_info.m_Indexed = false;
    ++m_DirtyAnnotIndexCount;
}


void CDataSource::x_AddTSE_ref(const CSeq_id_Handle& idh, CTSE_Info* tse_info)
{
    _TRACE("x_AddTSE_ref("<<idh.AsString()<<","<<&tse_info->GetTSE()<<")");
    TTSEMap::iterator it = m_TSE_ref.lower_bound(idh);
    if ( it == m_TSE_ref.end() || it->first != idh ) {
        it = m_TSE_ref.insert(it, TTSEMap::value_type(idh,
                                                      CTSE_LockingSet()));
    }
    _ASSERT(it != m_TSE_ref.end() && it->first == idh);
    _VERIFY(it->second.insert(tse_info));
}


void CDataSource::x_DropTSE_ref(const CSeq_id_Handle& idh, CTSE_Info* tse_info)
{
    _TRACE("x_DropTSE_ref("<<idh.AsString()<<","<<&tse_info->GetTSE()<<")");
    TTSEMap::iterator it = m_TSE_ref.find(idh);
    _ASSERT(it != m_TSE_ref.end() && it->first == idh);
    it->second.erase(tse_info);
    if ( it->second.empty() ) {
        m_TSE_ref.erase(it);
    }
}


void CDataSource::x_CleanupUnusedEntries(void)
{
    // Lock indexes
    TWriteLockGuard guard(m_DataSource_Mtx);    

    bool broken = true;
    while ( broken ) {
        broken = false;
        NON_CONST_ITERATE( TTSE_InfoMap, it, m_TSE_InfoMap ) {
            if ( !it->second->Locked() ) {
                //### Lock the entry and check again
                x_DetachTSE(*it->second);
                broken = true;
                break;
            }
        }
    }
}


void CDataSource::x_UpdateTSEStatus(CSeq_entry& tse, bool dead)
{
    CRef<CTSE_Info> tse_info = x_FindTSE_Info(tse);
    _ASSERT(tse_info);
    tse_info->m_Dead = dead;
}


CSeqMatch_Info CDataSource::BestResolve(CSeq_id_Handle idh)
{
    //### Lock all TSEs found, unlock all filtered out in the end.
    CTSE_LockingSetLock lock;
    {{
        TWriteLockGuard guard(m_DataSource_Mtx);
        lock.Lock(m_TSE_seq[idh]);
    }}
    if ( m_Loader ) {
        // Send request to the loader
        CHandleRangeMap hrm;
        hrm.AddRange(idh,
                     CHandleRange::TRange::GetWhole(), eNa_strand_unknown);
        m_Loader->GetRecords(hrm, CDataLoader::eBioseqCore);
    }
    CSeqMatch_Info match;
    TTSE_Lock tse(x_FindBestTSE(idh));
    if ( !tse ) {
        // Try to find the best matching id (not exactly equal)
        TSeq_id_HandleSet hset;
        GetSeq_id_Mapper().GetMatchingHandles(idh, hset);
        ITERATE(TSeq_id_HandleSet, hit, hset) {
            if ( bool(tse)  &&  idh.IsBetter(*hit) )
                continue;
            TTSE_Lock tmp_tse(x_FindBestTSE(*hit));
            if ( tmp_tse ) {
                tse = tmp_tse;
                idh = *hit;
            }
        }
    }
    if ( tse ) {
        match = CSeqMatch_Info(idh, *tse);
    }
    return match;
}


CSeqMatch_Info* CDataSource::ResolveConflict(const CSeq_id_Handle& id,
                                             CSeqMatch_Info& info1,
                                             CSeqMatch_Info& info2)
{
    if (&info1.GetDataSource() != this  ||
        &info2.GetDataSource() != this) {
        // Can not compare TSEs from different data sources or
        // without a loader.
        return 0;
    }
    if (!m_Loader) {
        if ( info1.GetIdHandle().IsBetter(info2.GetIdHandle()) ) {
            return &info1;
        }
        if ( info2.GetIdHandle().IsBetter(info1.GetIdHandle()) ) {
            return &info2;
        }
        return 0;
    }
    TTSE_LockSet tse_set;
    tse_set.insert(TTSE_Lock(&info1.GetTSE_Info()));
    tse_set.insert(TTSE_Lock(&info2.GetTSE_Info()));
    CConstRef<CTSE_Info> tse = m_Loader->ResolveConflict(id, tse_set);
    if (tse == &info1.GetTSE_Info()) {
        return &info1;
    }
    if (tse == &info2.GetTSE_Info()) {
        return &info2;
    }
    return 0;
}


string CDataSource::GetName(void) const
{
    if ( m_Loader )
        return m_Loader->GetName();
    else
        return kEmptyStr;
}

#if 0
bool CDataSource::IsSynonym(const CSeq_id_Handle& h1,
                            const CSeq_id_Handle& h2) const
{
    //CSeq_id_Handle h1 = GetSeq_id_Mapper().GetHandle(id1);
    //CSeq_id_Handle h2 = GetSeq_id_Mapper().GetHandle(id2);

    TReadLockGuards guard(m_DataSource_Mtx);    
    TTSEMap::const_iterator tse_set = m_TSE_seq.find(h1);
    if (tse_set == m_TSE_seq.end())
        return false; // Could not find id1 in the datasource
    NON_CONST_ITERATE (TTSESet, tse_it,
                       const_cast<TTSESet&>(tse_set->second) ) {
        const CBioseq_Info& bioseq =
            *const_cast<CTSE_Info::TBioseqs&>((*tse_it)->m_Bioseqs)[h1];
        if (bioseq.m_Synonyms.find(h2) != bioseq.m_Synonyms.end())
            return true;
    }
    return false;
}
#endif

TTSE_Lock CDataSource::GetTSEHandles(const CSeq_entry& entry,
                                     set< CConstRef<CBioseq_Info> >& bioseqs,
                                     CSeq_inst::EMol filter)
{
    // Find TSE_Info
    CRef<CTSE_Info> tse_info;
    {{
        TReadLockGuard  guard(m_DataSource_Mtx);
        tse_info = x_FindTSE_Info(entry);
    }}
    if ( tse_info ) {
        // Populate the map
        ITERATE( CTSE_Info::TBioseqs, it, tse_info->m_Bioseqs ) {
            if ( filter != CSeq_inst::eMol_not_set ) {
                // Filter sequences
                if (it->second->GetBioseq().GetInst().GetMol() != filter) {
                    continue;
                }
            }
            bioseqs.insert(it->second);
        }
    }
    return tse_info;
}


void CDataSource::DebugDump(CDebugDumpContext /*ddc*/,
                            unsigned int /*depth*/) const
{
/*
    ddc.SetFrame("CDataSource");
    CObject::DebugDump( ddc, depth);

    ddc.Log("m_Loader", m_Loader,0);
    ddc.Log("m_pTopEntry", m_pTopEntry.GetPointer(),0);
    ddc.Log("m_ObjMgr", m_ObjMgr,0);

    if (depth == 0) {
        DebugDumpValue(ddc, "m_Entries.size()", m_Entries.size());
        DebugDumpValue(ddc, "m_TSE_seq.size()", m_TSE_seq.size());
        DebugDumpValue(ddc, "m_TSE_ref.size()", m_TSE_ref.size());
        DebugDumpValue(ddc, "m_SeqMaps.size()", m_SeqMaps.size());
    } else {
        DebugDumpValue(ddc, "m_Entries.type",
            "map<CRef<CSeq_entry>,CRef<CTSE_Info>>");
        DebugDumpPairsCRef(ddc, "m_Entries",
            m_Entries.begin(), m_Entries.end(), depth);

        { //---  m_TSE_seq
            unsigned int depth2 = depth-1;
            DebugDumpValue(ddc, "m_TSE_seq.type",
                "map<CSeq_id_Handle,set<CRef<CTSE_Info>>>");
            CDebugDumpContext ddc2(ddc,"m_TSE_seq");
            TTSEMap::const_iterator it;
            for ( it = m_TSE_seq.begin(); it != m_TSE_seq.end(); ++it) {
                string member_name = "m_TSE_seq[ " +
                    (it->first).AsString() +" ]";
                if (depth2 == 0) {
                    member_name += ".size()";
                    DebugDumpValue(ddc2, member_name, (it->second).size());
                } else {
                    DebugDumpRangeCRef(ddc2, member_name,
                        (it->second).begin(), (it->second).end(), depth2);
            }
        }
    }

        { //---  m_TSE_ref
            unsigned int depth2 = depth-1;
            DebugDumpValue(ddc, "m_TSE_ref.type",
                "map<CSeq_id_Handle,set<CRef<CTSE_Info>>>");
            CDebugDumpContext ddc2(ddc,"m_TSE_ref");
            TTSEMap::const_iterator it;
            for ( it = m_TSE_ref.begin(); it != m_TSE_ref.end(); ++it) {
                string member_name = "m_TSE_ref[ " +
                    (it->first).AsString() +" ]";
                if (depth2 == 0) {
                    member_name += ".size()";
                    DebugDumpValue(ddc2, member_name, (it->second).size());
                } else {
                    DebugDumpRangeCRef(ddc2, member_name,
                        (it->second).begin(), (it->second).end(), depth2);
                }
            }
        }

        DebugDumpValue(ddc, "m_SeqMaps.type",
            "map<const CBioseq*,CRef<CSeqMap>>");
        DebugDumpPairsCRefCRef(ddc, "m_SeqMaps",
            m_SeqMaps.begin(), m_SeqMaps.end(), depth);
    }
*/
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.116  2003/08/04 17:04:31  grichenk
* Added default data-source priority assignment.
* Added support for iterating all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.115  2003/07/17 22:51:31  vasilche
* Fixed unused variables warnings.
*
* Revision 1.114  2003/07/17 20:07:56  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.113  2003/07/09 17:54:29  dicuccio
* Fixed uninitialized variables in CDataSource and CSeq_annot_Info
*
* Revision 1.112  2003/07/01 18:01:08  vasilche
* Added check for null m_SeqMap pointer.
*
* Revision 1.111  2003/06/24 14:35:01  vasilche
* Fixed compilation in debug mode.
*
* Revision 1.110  2003/06/24 14:25:18  vasilche
* Removed obsolete CTSE_Guard class.
* Used separate mutexes for bioseq and annot maps.
*
* Revision 1.109  2003/06/19 18:23:45  vasilche
* Added several CXxx_ScopeInfo classes for CScope related information.
* CBioseq_Handle now uses reference to CBioseq_ScopeInfo.
* Some fine tuning of locking in CScope.
*
* Revision 1.104  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.103  2003/05/14 18:39:28  grichenk
* Simplified TSE caching and filtering in CScope, removed
* some obsolete members and functions.
*
* Revision 1.102  2003/05/12 19:18:29  vasilche
* Fixed locking of object manager classes in multi-threaded application.
*
* Revision 1.101  2003/05/06 18:54:09  grichenk
* Moved TSE filtering from CDataSource to CScope, changed
* some filtering rules (e.g. priority is now more important
* than scope history). Added more caches to CScope.
*
* Revision 1.100  2003/05/05 20:59:47  vasilche
* Use one static mutex for all instances of CTSE_LockingSet.
*
* Revision 1.99  2003/04/29 19:51:13  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.98  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.97  2003/04/14 21:32:18  grichenk
* Avoid passing CScope as an argument to CDataSource methods
*
* Revision 1.96  2003/03/24 21:26:45  grichenk
* Added support for CTSE_CI
*
* Revision 1.95  2003/03/21 21:08:53  grichenk
* Fixed TTSE_Lock initialization
*
* Revision 1.93  2003/03/21 19:22:51  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.92  2003/03/18 14:52:59  grichenk
* Removed obsolete methods, replaced seq-id with seq-id handle
* where possible. Added argument to limit annotations update to
* a single seq-entry.
*
* Revision 1.91  2003/03/12 20:09:34  grichenk
* Redistributed members between CBioseq_Handle, CBioseq_Info and CTSE_Info
*
* Revision 1.90  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.89  2003/03/11 14:15:52  grichenk
* +Data-source priority
*
* Revision 1.88  2003/03/10 16:55:17  vasilche
* Cleaned SAnnotSelector structure.
* Added shortcut when features are limited to one TSE.
*
* Revision 1.87  2003/03/05 20:56:43  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.86  2003/03/03 20:31:08  vasilche
* Removed obsolete method PopulateTSESet().
*
* Revision 1.85  2003/02/27 14:35:31  vasilche
* Splitted PopulateTSESet() by logically independent parts.
*
* Revision 1.84  2003/02/25 20:10:40  grichenk
* Reverted to single total-range index for annotations
*
* Revision 1.83  2003/02/24 18:57:22  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.82  2003/02/24 14:51:11  grichenk
* Minor improvements in annot indexing
*
* Revision 1.81  2003/02/13 14:34:34  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.80  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.79  2003/02/04 22:01:26  grichenk
* Fixed annotations loading/indexing order
*
* Revision 1.78  2003/02/04 21:46:32  grichenk
* Added map of annotations by intervals (the old one was
* by total ranges)
*
* Revision 1.77  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.76  2003/01/29 17:45:02  vasilche
* Annotaions index is split by annotation/feature type.
*
* Revision 1.75  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.74  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.73  2002/12/20 20:54:24  grichenk
* Added optional location/product switch to CFeat_CI
*
* Revision 1.72  2002/12/19 20:16:39  grichenk
* Fixed locations on minus strand
*
* Revision 1.71  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.70  2002/11/08 22:15:51  grichenk
* Added methods for removing/replacing annotations
*
* Revision 1.69  2002/11/08 21:03:30  ucko
* CConstRef<> now requires an explicit constructor.
*
* Revision 1.68  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.67  2002/10/18 19:12:40  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.66  2002/10/16 20:44:16  ucko
* MIPSpro: use #pragma instantiate rather than template<>, as the latter
* ended up giving rld errors for CMutexPool_Base<*>::sm_Pool.  (Sigh.)
*
* Revision 1.65  2002/10/02 17:58:23  grichenk
* Added sequence type filter to CBioseq_CI
*
* Revision 1.64  2002/09/30 20:01:19  grichenk
* Added methods to support CBioseq_CI
*
* Revision 1.63  2002/09/16 19:59:36  grichenk
* Fixed getting reference to a gap on minus strand
*
* Revision 1.62  2002/09/10 19:55:49  grichenk
* Throw exception when unable to create resolved seq-map
*
* Revision 1.61  2002/08/09 14:58:50  ucko
* Restrict template <> to MIPSpro for now, as it also leads to link
* errors with Compaq's compiler.  (Sigh.)
*
* Revision 1.60  2002/08/08 19:51:16  ucko
* Omit EMPTY_TEMPLATE for GCC and KCC, as it evidently leads to link errors(!)
*
* Revision 1.59  2002/08/08 14:28:00  ucko
* Add EMPTY_TEMPLATE to explicit instantiations.
*
* Revision 1.58  2002/08/07 18:22:48  ucko
* Explicitly instantiate CMutexPool_Base<{CDataSource,TTSESet}>::sm_Pool
*
* Revision 1.57  2002/07/25 15:01:51  grichenk
* Replaced non-const GetXXX() with SetXXX()
*
* Revision 1.56  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.55  2002/07/01 15:44:51  grichenk
* Fixed typos
*
* Revision 1.54  2002/07/01 15:40:58  grichenk
* Fixed 'tse_set' warning.
* Removed duplicate call to tse_set.begin().
* Fixed version resolving for annotation iterators.
* Fixed strstream bug in KCC.
*
* Revision 1.53  2002/06/28 17:30:42  grichenk
* Duplicate seq-id: ERR_POST() -> THROW1_TRACE() with the seq-id
* list.
* Fixed x_CleanupUnusedEntries().
* Fixed a bug with not found sequences.
* Do not copy bioseq data in GetBioseqCore().
*
* Revision 1.52  2002/06/21 15:12:15  grichenk
* Added resolving seq-id to the best version
*
* Revision 1.51  2002/06/12 14:39:53  grichenk
* Performance improvements
*
* Revision 1.50  2002/06/04 17:18:33  kimelman
* memory cleanup :  new/delete/Cref rearrangements
*
* Revision 1.49  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.48  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.47  2002/05/28 18:00:43  gouriano
* DebugDump added
*
* Revision 1.46  2002/05/24 14:57:12  grichenk
* SerialAssign<>() -> CSerialObject::Assign()
*
* Revision 1.45  2002/05/21 18:57:25  grichenk
* Fixed annotations dropping
*
* Revision 1.44  2002/05/21 18:40:50  grichenk
* Fixed annotations droppping
*
* Revision 1.43  2002/05/14 20:06:25  grichenk
* Improved CTSE_Info locking by CDataSource and CDataLoader
*
* Revision 1.42  2002/05/13 15:28:27  grichenk
* Fixed seqmap for virtual sequences
*
* Revision 1.41  2002/05/09 14:18:15  grichenk
* More TSE conflict resolving rules for annotations
*
* Revision 1.40  2002/05/06 03:28:46  vakatov
* OM/OM1 renaming
*
* Revision 1.39  2002/05/03 21:28:09  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.38  2002/05/02 20:42:37  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.37  2002/04/22 20:05:08  grichenk
* Fixed minor bug in GetSequence()
*
* Revision 1.36  2002/04/19 18:02:47  kimelman
* add verify to catch coredump
*
* Revision 1.35  2002/04/17 21:09:14  grichenk
* Fixed annotations loading
* +IsSynonym()
*
* Revision 1.34  2002/04/11 18:45:39  ucko
* Pull in extra headers to make KCC happy.
*
* Revision 1.33  2002/04/11 12:08:21  grichenk
* Fixed GetResolvedSeqMap() implementation
*
* Revision 1.32  2002/04/05 21:23:08  grichenk
* More duplicate id warnings fixed
*
* Revision 1.31  2002/04/05 20:27:52  grichenk
* Fixed duplicate identifier warning
*
* Revision 1.30  2002/04/04 21:33:13  grichenk
* Fixed GetSequence() for sequences with unresolved segments
*
* Revision 1.29  2002/04/03 18:06:47  grichenk
* Fixed segmented sequence bugs (invalid positioning of literals
* and gaps). Improved CSeqVector performance.
*
* Revision 1.27  2002/03/28 14:02:31  grichenk
* Added scope history checks to CDataSource::x_FindBestTSE()
*
* Revision 1.26  2002/03/22 17:24:12  gouriano
* loader-related fix in DropTSE()
*
* Revision 1.25  2002/03/21 21:39:48  grichenk
* garbage collector bugfix
*
* Revision 1.24  2002/03/20 21:24:59  gouriano
* *** empty log message ***
*
* Revision 1.23  2002/03/15 18:10:08  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.22  2002/03/11 21:10:13  grichenk
* +CDataLoader::ResolveConflict()
*
* Revision 1.21  2002/03/07 21:25:33  grichenk
* +GetSeq_annot() in annotation iterators
*
* Revision 1.20  2002/03/06 19:37:19  grichenk
* Fixed minor bugs and comments
*
* Revision 1.19  2002/03/05 18:44:55  grichenk
* +x_UpdateTSEStatus()
*
* Revision 1.18  2002/03/05 16:09:10  grichenk
* Added x_CleanupUnusedEntries()
*
* Revision 1.17  2002/03/04 15:09:27  grichenk
* Improved MT-safety. Added live/dead flag to CDataSource methods.
*
* Revision 1.16  2002/02/28 20:53:31  grichenk
* Implemented attaching segmented sequence data. Fixed minor bugs.
*
* Revision 1.15  2002/02/25 21:05:29  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.14  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.13  2002/02/20 20:23:27  gouriano
* corrected FilterSeqid()
*
* Revision 1.12  2002/02/15 20:35:38  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.11  2002/02/12 19:41:42  grichenk
* Seq-id handles lock/unlock moved to CSeq_id_Handle 'ctors.
*
* Revision 1.10  2002/02/07 21:27:35  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.9  2002/02/06 21:46:11  gouriano
* *** empty log message ***
*
* Revision 1.8  2002/02/05 21:46:28  gouriano
* added FindSeqid function, minor tuneup in CSeq_id_mapper
*
* Revision 1.7  2002/01/30 22:09:28  gouriano
* changed CSeqMap interface
*
* Revision 1.6  2002/01/29 17:45:00  grichenk
* Added seq-id handles locking
*
* Revision 1.5  2002/01/28 19:44:49  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.4  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.3  2002/01/18 15:56:23  gouriano
* changed TSeqMaps definition
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:18  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
