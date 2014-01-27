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
* Authors:
*           Andrei Gourianov
*           Aleksey Grichenko
*           Michael Kimelman
*           Denis Vakatov
*           Eugene Vasilchenko
*
* File Description:
*           Scope is top-level object available to a client.
*           Its purpose is to define a scope of visibility and reference
*           resolution and provide access to the bio sequence data
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/bioseq_set_handle.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/error_codes.hpp>


#define NCBI_USE_ERRCODE_X   ObjMgr_Scope

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
//
// CScope
//
/////////////////////////////////////////////////////////////////////////////


CScope::CScope(CObjectManager& objmgr)
{
    if ( CanBeDeleted() ) {
        // this CScope object is allocated in heap
        m_Impl.Reset(new CScope_Impl(objmgr));
        m_Impl->m_HeapScope = this;
    }
    else {
        // allocate heap CScope object
        m_HeapScope.Reset(new CScope(objmgr));
        _ASSERT(m_HeapScope->CanBeDeleted());
        m_Impl = m_HeapScope->m_Impl;
        _ASSERT(m_Impl);
    }
}


CScope::~CScope(void)
{
    if ( m_Impl && m_Impl->m_HeapScope == this ) {
        m_Impl->m_HeapScope = 0;
    }
}


CObjectManager& CScope::GetObjectManager(void)
{
    return m_Impl->GetObjectManager();
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id& id)
{
    return GetBioseqHandle(id, eGetBioseq_All);
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id_Handle& id)
{
    return GetBioseqHandle(id, eGetBioseq_All);
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_loc& loc)
{
    return m_Impl->GetBioseqHandle(loc, eGetBioseq_All);
}


CTSE_Handle CScope::GetTSE_Handle(const CSeq_entry& entry,
                                  EMissing action)
{
    return GetSeq_entryHandle(entry, action).GetTSE_Handle();
}


CBioseq_Handle CScope::GetBioseqHandle(const CBioseq& seq,
                                       EMissing action)
{
    //ERR_POST_X_ONCE(1, Warning<<"CScope::GetBioseqHandle(CBioseq&) is deprecated");
    return m_Impl->GetBioseqHandle(seq, action);
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id& id,
                                       EGetBioseqFlag get_flag)
{
    return GetBioseqHandle(CSeq_id_Handle::GetHandle(id), get_flag);
}


CScope::TBioseqHandles CScope::GetBioseqHandles(const TIds& ids)
{
    return m_Impl->GetBioseqHandles(ids);
}


CBioseq_Handle CScope::GetBioseqHandle(const CSeq_id_Handle& id,
                                       EGetBioseqFlag get_flag)
{
    return m_Impl->GetBioseqHandle(id, get_flag);
}


bool CScope::IsSameBioseq(const CSeq_id_Handle& id1,
                          const CSeq_id_Handle& id2,
                          EGetBioseqFlag get_flag)
{
    return m_Impl->IsSameBioseq(id1, id2, get_flag);
}


CBioseq_set_Handle CScope::GetBioseq_setHandle(const CBioseq_set& seqset,
                                               EMissing action)
{
    //ERR_POST_X_ONCE(2, Warning<<"CScope::GetBioseq_setHandle(CBioseq_set&) is deprecated.");
    return m_Impl->GetBioseq_setHandle(seqset, action);
}


CSeq_entry_Handle CScope::GetSeq_entryHandle(const CSeq_entry& entry,
                                             EMissing action)
{
    //ERR_POST_X_ONCE(3, Warning<<"CScope::GetSeq_entryHandle(CSeq_entry&) is deprecated.");
    return m_Impl->GetSeq_entryHandle(entry, action);
}


CSeq_annot_Handle CScope::GetSeq_annotHandle(const CSeq_annot& annot,
                                             EMissing action)
{
    //ERR_POST_X_ONCE(4, Warning<<"CScope::GetSeq_annotHandle(CSeq_annot&) is deprecated.");
    return m_Impl->GetSeq_annotHandle(annot, action);
}


CSeq_feat_Handle CScope::GetSeq_featHandle(const CSeq_feat& feat,
                                           EMissing action)
{
    return m_Impl->GetSeq_featHandle(feat, action);
}


CSeq_entry_EditHandle CScope::GetSeq_entryEditHandle(const CSeq_entry& entry)
{
    CSeq_entry_Handle h = m_Impl->GetSeq_entryHandle(entry, eMissing_Throw);
    if ( !h.GetTSE_Handle().CanBeEdited() ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::GetSeq_entryEditHandle: entry cannot be edited");
    }
    return m_Impl->GetEditHandle(h);
}


CSeq_annot_EditHandle CScope::GetSeq_annotEditHandle(const CSeq_annot& annot)
{
    CSeq_annot_Handle h = m_Impl->GetSeq_annotHandle(annot, eMissing_Throw);
    if ( !h.GetTSE_Handle().CanBeEdited() ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::GetSeq_annotEditHandle: annot cannot be edited");
    }
    return m_Impl->GetEditHandle(h);
}


CBioseq_EditHandle CScope::GetBioseqEditHandle(const CBioseq& bioseq)
{
    CBioseq_Handle h = m_Impl->GetBioseqHandle(bioseq, eMissing_Throw);
    if ( !h.GetTSE_Handle().CanBeEdited() ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::GetBioseqEditHandle: bioseq cannot be edited");
    }
    return m_Impl->GetEditHandle(h);
}


CBioseq_set_EditHandle
CScope::GetBioseq_setEditHandle(const CBioseq_set& seqset)
{
    CBioseq_set_Handle h = m_Impl->GetBioseq_setHandle(seqset, eMissing_Throw);
    if ( !h.GetTSE_Handle().CanBeEdited() ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::GetBioseq_setEditHandle: "
                   "bioseq set cannot be edited");
    }
    return m_Impl->GetEditHandle(h);
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id& id,
                                              const CTSE_Handle& tse)
{
    return GetBioseqHandleFromTSE(CSeq_id_Handle::GetHandle(id), tse);
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id& id,
                                              const CBioseq_Handle& bh)
{
    return GetBioseqHandleFromTSE(id, bh.GetTSE_Handle());
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id& id,
                                              const CSeq_entry_Handle& seh)
{
    return GetBioseqHandleFromTSE(id, seh.GetTSE_Handle());
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                              const CBioseq_Handle& bh)
{
    return GetBioseqHandleFromTSE(id, bh.GetTSE_Handle());
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                              const CSeq_entry_Handle& seh)
{
    return GetBioseqHandleFromTSE(id, seh.GetTSE_Handle());
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                              const CTSE_Handle& tse)
{
    return m_Impl->GetBioseqHandleFromTSE(id, tse);
}


void CScope::GetAllTSEs(TTSE_Handles& tses, enum ETSEKind kind)
{
    m_Impl->GetAllTSEs(tses, int(kind));
}


CBioseq_EditHandle CScope::GetEditHandle(const CBioseq_Handle& seq)
{
    return m_Impl->GetEditHandle(seq);
}


CSeq_entry_EditHandle CScope::GetEditHandle(const CSeq_entry_Handle& entry)
{
    return m_Impl->GetEditHandle(entry);
}


CSeq_annot_EditHandle CScope::GetEditHandle(const CSeq_annot_Handle& annot)
{
    return m_Impl->GetEditHandle(annot);
}


CBioseq_set_EditHandle CScope::GetEditHandle(const CBioseq_set_Handle& seqset)
{
    return m_Impl->GetEditHandle(seqset);
}


void CScope::ResetHistory(EActionIfLocked action)
{
    m_Impl->ResetHistory(action);
}


void CScope::ResetDataAndHistory(void)
{
    m_Impl->ResetDataAndHistory();
}


void CScope::ResetDataAndHistory(ERemoveDataLoaders)
{
    m_Impl->ResetScope();
}


void CScope::RemoveFromHistory(const CBioseq_Handle& bioseq)
{
    m_Impl->RemoveFromHistory(bioseq.GetTSE_Handle());
}


void CScope::RemoveFromHistory(const CTSE_Handle& tse)
{
    m_Impl->RemoveFromHistory(tse);
}


void CScope::RemoveDataLoader(const string& loader_name,
                              EActionIfLocked action)
{
    m_Impl->RemoveDataLoader(loader_name, action);
}


void CScope::RemoveTopLevelSeqEntry(const CTSE_Handle& entry)
{
    m_Impl->RemoveTopLevelSeqEntry(entry);
}


void CScope::RemoveBioseq(const CBioseq_Handle& seq)
{
    m_Impl->RemoveTopLevelBioseq(seq);
}


void CScope::RemoveSeq_annot(const CSeq_annot_Handle& annot)
{
    m_Impl->RemoveTopLevelAnnot(annot);
}


CScope::TIds CScope::GetIds(const CSeq_id& id)
{
    return GetIds(CSeq_id_Handle::GetHandle(id));
}


CScope::TIds CScope::GetIds(const CSeq_id_Handle& idh)
{
    return m_Impl->GetIds(idh);
}


CSeq_id_Handle CScope::GetAccVer(const CSeq_id_Handle& idh)
{
    return m_Impl->GetAccVer(idh, false);
}


TGi CScope::GetGi(const CSeq_id_Handle& idh)
{
    return m_Impl->GetGi(idh, false);
}


CSeq_id_Handle CScope::x_GetAccVer(const TIds& ids)
{
    CSeq_id_Handle ret;
    ITERATE ( TIds, iter, ids ) {
        if ( !iter->IsGi() && iter->GetSeqId()->GetTextseq_Id() ) {
            ret = *iter;
            break;
        }
    }
    return ret;
}


TGi CScope::x_GetGi(const TIds& ids)
{
    ITERATE ( TIds, iter, ids ) {
        if ( iter->IsGi() ) {
            return iter->GetGi();
        }
        if ( iter->Which() == CSeq_id::e_Gi ) {
            return iter->GetSeqId()->GetGi();
        }
    }
    return ZERO_GI;
}


string CScope::GetLabel(const CSeq_id& id, EForceLabelLoad force_load)
{
    return GetLabel(CSeq_id_Handle::GetHandle(id), force_load);
}


string CScope::GetLabel(const CSeq_id_Handle& idh, EForceLabelLoad force_load)
{
    return m_Impl->GetLabel(idh, force_load == eForceLabelLoad);
}


int CScope::GetTaxId(const CSeq_id& id, EForceLoad force_load)
{
    return GetTaxId(CSeq_id_Handle::GetHandle(id), force_load);
}


int CScope::GetTaxId(const CSeq_id_Handle& idh, EForceLoad force_load)
{
    return m_Impl->GetTaxId(idh, force_load == eForceLoad);
}


CConstRef<CSynonymsSet> CScope::GetSynonyms(const CSeq_id& id)
{
    return GetSynonyms(CSeq_id_Handle::GetHandle(id));
}


CConstRef<CSynonymsSet> CScope::GetSynonyms(const CSeq_id_Handle& id)
{
    return m_Impl->GetSynonyms(id, eGetBioseq_All);
}


CConstRef<CSynonymsSet> CScope::GetSynonyms(const CBioseq_Handle& bh)
{
    return m_Impl->GetSynonyms(bh);
}


void CScope::AddDefaults(TPriority priority)
{
    m_Impl->AddDefaults(priority);
}


void CScope::AddDataLoader(const string& loader_name, TPriority priority)
{
    m_Impl->AddDataLoader(loader_name, priority);
}


void CScope::AddScope(CScope& scope, TPriority priority)
{
    m_Impl->AddScope(*scope.m_Impl, priority);
}


CSeq_entry_Handle CScope::AddTopLevelSeqEntry(CSeq_entry& entry,
                                              TPriority priority,
                                              EExist action)
{
    return m_Impl->AddSeq_entry(entry, priority, action);
}


CBioseq_Handle CScope::AddBioseq(CBioseq& bioseq,
                                 TPriority priority,
                                 EExist action)
{
    return m_Impl->AddBioseq(bioseq, priority, action);
}


CSeq_annot_Handle CScope::AddSeq_annot(CSeq_annot& annot,
                                       TPriority priority,
                                       EExist action)
{
    return m_Impl->AddSeq_annot(annot, priority, action);
}


CSeq_entry_Handle CScope::AddTopLevelSeqEntry(const CSeq_entry& entry,
                                              TPriority priority,
                                              EExist action)
{
    return m_Impl->AddSharedSeq_entry(entry, priority, action);
}


CBioseq_Handle CScope::AddBioseq(const CBioseq& bioseq,
                                 TPriority priority,
                                 EExist action)
{
    return m_Impl->AddSharedBioseq(bioseq, priority, action);
}


CSeq_entry_Handle CScope::AddGC_Assembly(const CGC_Assembly& gc_assembly,
                                         CGC_Assembly_Parser::FParserFlags flags,
                                         TPriority pri,
                                         EExist action)
{
    CGC_Assembly_Parser parser(gc_assembly, flags);
    return AddTopLevelSeqEntry(*parser.GetTSE(), pri, action);
}


CSeq_annot_Handle CScope::AddSeq_annot(const CSeq_annot& annot,
                                       TPriority priority,
                                       EExist action)
{
    return m_Impl->AddSharedSeq_annot(annot, priority, action);
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id& id,
                                              const CSeq_entry& tse)
{
    //ERR_POST_X_ONCE(5, Warning<<"GetBioseqHandleFromTSE(CSeq_entry) is deprecated: use handles.");
    return GetBioseqHandleFromTSE(id, GetSeq_entryHandle(tse));
}


CBioseq_Handle CScope::GetBioseqHandleFromTSE(const CSeq_id_Handle& id,
                                              const CSeq_entry& tse)
{
    //ERR_POST_X_ONCE(6, Warning<<"GetBioseqHandleFromTSE(CSeq_entry) is deprecated: use handles.");
    return GetBioseqHandleFromTSE(id, GetSeq_entryHandle(tse));
}


void CScope::AttachEntry(CSeq_entry& parent, CSeq_entry& entry)
{
    //ERR_POST_X_ONCE(7, Warning<<"CScope::AttachEntry() is deprecated: use class CSeq_entry_EditHandle.");
    GetSeq_entryEditHandle(parent).SetSet().AttachEntry(entry);
}


void CScope::RemoveEntry(CSeq_entry& entry)
{
    //ERR_POST_X_ONCE(8, Warning<<"CScope::RemoveEntry() is deprecated: use class CSeq_entry_EditHandle.");
    GetSeq_entryEditHandle(entry).Remove();
}


void CScope::AttachAnnot(CSeq_entry& parent, CSeq_annot& annot)
{
    //ERR_POST_X_ONCE(9, Warning<<"CScope::AttachAnnot() is deprecated: use class CSeq_annot_EditHandle.");
    GetSeq_entryEditHandle(parent).AttachAnnot(annot);
}


void CScope::RemoveAnnot(CSeq_entry& parent, CSeq_annot& annot)
{
    //ERR_POST_X_ONCE(10, Warning<<"CScope::RemoveAnnot() is deprecated: use class CSeq_annot_EditHandle.");
    CSeq_entry_EditHandle eh = GetSeq_entryEditHandle(parent);
    CSeq_annot_EditHandle ah = GetSeq_annotEditHandle(annot);
    if ( ah.GetParentEntry() != eh ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::RemoveAnnot: parent doesn't contain annot");
    }
    ah.Remove();
}


void CScope::ReplaceAnnot(CSeq_entry& parent,
                          CSeq_annot& old_annot, CSeq_annot& new_annot)
{
    //ERR_POST_X_ONCE(11, Warning<<"CScope::RemoveAnnot() is deprecated: use class CSeq_annot_EditHandle.");
    CSeq_entry_EditHandle eh = GetSeq_entryEditHandle(parent);
    CSeq_annot_EditHandle ah = GetSeq_annotEditHandle(old_annot);
    if ( ah.GetParentEntry() != eh ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CScope::ReplaceAnnot: parent doesn't contain old_annot");
    }
    ah.Remove();
    eh.AttachAnnot(new_annot);
}

CScopeTransaction CScope::GetTransaction()
{
    return CScopeTransaction(*this);
}


void CScope::UpdateAnnotIndex(void)
{
    m_Impl->x_ClearAnnotCache();
}


/// Bulk retrieval methods
CScope::TSeq_id_Handles CScope::GetAccVers(const TSeq_id_Handles& idhs,
                                           EForceLoad force_load)
{
    TSeq_id_Handles results;
    GetAccVers(&results, idhs, force_load);
    return results;
}


void CScope::GetAccVers(TSeq_id_Handles* results,
                        const TSeq_id_Handles& idhs,
                        EForceLoad force_load)
{
    if ( !results ) {
        NCBI_THROW(CCoreException, eNullPtr,
                   "CScope::GetAccVers: null results pointer");
    }
    return m_Impl->GetAccVers(*results, idhs, force_load == eForceLoad);
}


CScope::TGIs CScope::GetGis(const TSeq_id_Handles& idhs,
                            EForceLoad force_load)
{
    TGIs results;
    GetGis(&results, idhs, force_load);
    return results;
}


void CScope::GetGis(TGIs* results,
                    const TSeq_id_Handles& idhs,
                    EForceLoad force_load)
{
    if ( !results ) {
        NCBI_THROW(CCoreException, eNullPtr,
                   "CScope::GetGis: null results pointer");
    }
    return m_Impl->GetGis(*results, idhs, force_load == eForceLoad);
}


CScope::TLabels CScope::GetLabels(const TSeq_id_Handles& idhs,
                                  EForceLoad force_load)
{
    TLabels results;
    GetLabels(&results, idhs, force_load);
    return results;
}


void CScope::GetLabels(TLabels* results,
                       const TSeq_id_Handles& idhs,
                       EForceLoad force_load)
{
    if ( !results ) {
        NCBI_THROW(CCoreException, eNullPtr,
                   "CScope::GetLabels: null results pointer");
    }
    return m_Impl->GetLabels(*results, idhs, force_load == eForceLoad);
}


CScope::TTaxIds CScope::GetTaxIds(const TSeq_id_Handles& idhs,
                                  EForceLoad force_load)
{
    TTaxIds results;
    GetTaxIds(&results, idhs, force_load);
    return results;
}


void CScope::GetTaxIds(TTaxIds* results,
                       const TSeq_id_Handles& idhs,
                       EForceLoad force_load)
{
    if ( !results ) {
        NCBI_THROW(CCoreException, eNullPtr,
                   "CScope::GetTaxIds: null results pointer");
    }
    return m_Impl->GetTaxIds(*results, idhs, force_load == eForceLoad);
}


TSeqPos CScope::GetSequenceLength(const CSeq_id& id,
                                  EForceLoad force_load)
{
    return GetSequenceLength(CSeq_id_Handle::GetHandle(id), force_load);
}


TSeqPos CScope::GetSequenceLength(const CSeq_id_Handle& id,
                                  EForceLoad force_load)
{
    return m_Impl->GetSequenceLength(id, force_load == eForceLoad);
}


void CScope::GetSequenceLengths(TSequenceLengths* results,
                                const TSeq_id_Handles& ids,
                                EForceLoad force_load)
{
    if ( !results ) {
        NCBI_THROW(CCoreException, eNullPtr,
                   "CScope::GetSequenceLengths: null results pointer");
    }
    return m_Impl->GetSequenceLengths(*results, ids, force_load == eForceLoad);
}


CScope::TSequenceLengths CScope::GetSequenceLengths(const TSeq_id_Handles& ids,
                                                    EForceLoad force_load)
{
    TSequenceLengths results;
    GetSequenceLengths(&results, ids, force_load);
    return results;
}


CSeq_inst::TMol CScope::GetSequenceType(const CSeq_id& id,
                                        EForceLoad force_load)
{
    return GetSequenceType(CSeq_id_Handle::GetHandle(id), force_load);
}


CSeq_inst::TMol CScope::GetSequenceType(const CSeq_id_Handle& id,
                                        EForceLoad force_load)
{
    return m_Impl->GetSequenceType(id, force_load == eForceLoad);
}


void CScope::GetSequenceTypes(TSequenceTypes* results,
                              const TSeq_id_Handles& ids,
                              EForceLoad force_load)
{
    if ( !results ) {
        NCBI_THROW(CCoreException, eNullPtr,
                   "CScope::GetSequenceTypes: null results pointer");
    }
    return m_Impl->GetSequenceTypes(*results, ids, force_load == eForceLoad);
}


CScope::TSequenceTypes CScope::GetSequenceTypes(const TSeq_id_Handles& ids,
                                                EForceLoad force_load)
{
    TSequenceTypes results;
    GetSequenceTypes(&results, ids, force_load);
    return results;
}


END_SCOPE(objects)
END_NCBI_SCOPE
