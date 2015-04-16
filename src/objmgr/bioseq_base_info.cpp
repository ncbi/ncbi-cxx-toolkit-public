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
*   Bioseq info for data source
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/impl/bioseq_base_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CBioseq_Info::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


CBioseq_Base_Info::CBioseq_Base_Info(void)
    : m_ObjAnnot(0)
{
}


CBioseq_Base_Info::CBioseq_Base_Info(const CBioseq_Base_Info& src,
                                     TObjectCopyMap* copy_map)
    : TParent(src, copy_map),
      m_ObjAnnot(0),
      m_DescrChunks(src.m_DescrChunks),
      m_DescrTypeMasks(src.m_DescrTypeMasks),
      m_AnnotChunks(src.m_AnnotChunks)
{
}



CBioseq_Base_Info::~CBioseq_Base_Info(void)
{
}


const CSeq_entry_Info& CBioseq_Base_Info::GetParentSeq_entry_Info(void) const
{
    return static_cast<const CSeq_entry_Info&>(GetBaseParent_Info());
}


CSeq_entry_Info& CBioseq_Base_Info::GetParentSeq_entry_Info(void)
{
    return static_cast<CSeq_entry_Info&>(GetBaseParent_Info());
}


void CBioseq_Base_Info::x_DSAttachContents(CDataSource& ds)
{
    TParent::x_DSAttachContents(ds);
    // members
    NON_CONST_ITERATE ( TAnnot, it, m_Annot ) {
        (*it)->x_DSAttach(ds);
    }
}


void CBioseq_Base_Info::x_DSDetachContents(CDataSource& ds)
{
    // members
    NON_CONST_ITERATE ( TAnnot, it, m_Annot ) {
        (*it)->x_DSDetach(ds);
    }
    TParent::x_DSDetachContents(ds);
}


void CBioseq_Base_Info::x_TSEAttachContents(CTSE_Info& tse)
{
    TParent::x_TSEAttachContents(tse);
    // members
    NON_CONST_ITERATE ( TAnnot, it, m_Annot ) {
        (*it)->x_TSEAttach(tse);
    }
}


void CBioseq_Base_Info::x_TSEDetachContents(CTSE_Info& tse)
{
    // members
    NON_CONST_ITERATE ( TAnnot, it, m_Annot ) {
        (*it)->x_TSEDetach(tse);
    }
    TParent::x_TSEDetachContents(tse);
}


void CBioseq_Base_Info::x_ParentAttach(CSeq_entry_Info& parent)
{
    x_BaseParentAttach(parent);
}


void CBioseq_Base_Info::x_ParentDetach(CSeq_entry_Info& parent)
{
    x_BaseParentDetach(parent);
}


void CBioseq_Base_Info::x_SetAnnot(void)
{
    _ASSERT(m_ObjAnnot == 0);
    m_ObjAnnot = &x_SetObjAnnot();
    NON_CONST_ITERATE( TObjAnnot, it, *m_ObjAnnot ) {
        CRef<CSeq_annot_Info> info(new CSeq_annot_Info(**it));
        m_Annot.push_back(info);
        x_AttachAnnot(info);
    }
}


void CBioseq_Base_Info::x_SetAnnot(const CBioseq_Base_Info& annot,
                                   TObjectCopyMap* copy_map)
{
    m_ObjAnnot = &x_SetObjAnnot();
    _ASSERT(m_ObjAnnot->size() == annot.m_Annot.size());
    m_ObjAnnot->clear();
    ITERATE ( TAnnot, it, annot.m_Annot ) {
        AddAnnot(Ref(new CSeq_annot_Info(**it, copy_map)));
    }
}


void CBioseq_Base_Info::x_UpdateAnnotIndexContents(CTSE_Info& tse)
{
    tse.GetMasterSeqSegments();
    TParent::x_UpdateAnnotIndexContents(tse);
    NON_CONST_ITERATE ( TAnnot, it, m_Annot ) {
        (*it)->x_UpdateAnnotIndex(tse);
    }
}


void CBioseq_Base_Info::x_AddDescrChunkId(const TDescTypeMask& types,
                                          TChunkId id)
{
    m_DescrChunks.push_back(id);
    m_DescrTypeMasks.push_back(types);
    x_SetNeedUpdate(fNeedUpdate_descr);
}


void CBioseq_Base_Info::x_AddAnnotChunkId(TChunkId id)
{
    m_AnnotChunks.push_back(id);
    x_SetNeedUpdate(fNeedUpdate_annot);
}


void CBioseq_Base_Info::x_DoUpdate(TNeedUpdateFlags flags)
{
    if ( flags & fNeedUpdate_descr ) {
        x_LoadChunks(m_DescrChunks);
    }
    if ( flags & (fNeedUpdate_annot|fNeedUpdate_children) ) {
        x_LoadChunks(m_AnnotChunks);
        if ( IsSetAnnot() ) {
            _ASSERT(m_ObjAnnot && m_ObjAnnot->size() == m_Annot.size());
            TObjAnnot::iterator it2 = m_ObjAnnot->begin();
            NON_CONST_ITERATE ( TAnnot, it, m_Annot ) {
                (*it)->x_UpdateComplete();
                it2->Reset(const_cast<CSeq_annot*>(&(*it)->x_GetObject()));
                ++it2;
            }
        }
    }
    TParent::x_DoUpdate(flags);
}


void CBioseq_Base_Info::x_SetNeedUpdateParent(TNeedUpdateFlags flags)
{
    TParent::x_SetNeedUpdateParent(flags);
    //GetParentSeq_entry_Info().x_SetNeedUpdateContents(flags);
}


const CBioseq_Base_Info::TDescr& CBioseq_Base_Info::GetDescr(void) const
{
    x_Update(fNeedUpdate_descr);
    return x_GetDescr();
}


CBioseq_Base_Info::TDescr& CBioseq_Base_Info::SetDescr(void)
{
    x_Update(fNeedUpdate_descr);
    return x_SetDescr();
}


void CBioseq_Base_Info::SetDescr(CBioseq_Base_Info::TDescr& v)
{
    x_Update(fNeedUpdate_descr);
    x_SetDescr(v);
}


void CBioseq_Base_Info::ResetDescr(void)
{
    x_Update(fNeedUpdate_descr);
    x_ResetDescr();
}


bool CBioseq_Base_Info::AddSeqdesc(CSeqdesc& d)
{
    x_Update(fNeedUpdate_descr);
    TDescr::Tdata& s = x_SetDescr().Set();
    ITERATE ( TDescr::Tdata, it, s ) {
        if ( it->GetPointer() == &d ) {
            return false;
        }
    }
    s.push_back(Ref(&d));
    return true;
}


CRef<CSeqdesc> CBioseq_Base_Info::RemoveSeqdesc(const CSeqdesc& d)
{
    x_Update(fNeedUpdate_descr);
    if ( !IsSetDescr() ) {
        return CRef<CSeqdesc>(0);
    }
    TDescr::Tdata& s = x_SetDescr().Set();
    NON_CONST_ITERATE ( TDescr::Tdata, it, s ) {
        if ( it->GetPointer() == &d ) {
            // Lock the object to prevent destruction
            CRef<CSeqdesc> desc_nc = *it;
            s.erase(it);
            if ( s.empty() ) {
                ResetDescr();
            }
            return desc_nc;
        }
    }
    return CRef<CSeqdesc>(0);
}


void CBioseq_Base_Info::AddSeq_descr(const TDescr& v)
{
    TDescr::Tdata& s = x_SetDescr().Set();
    const TDescr::Tdata& src = v.Get();
    ITERATE ( TDescr::Tdata, it, src ) {
        s.push_back(*it);
    }
}


const CSeq_descr::Tdata& CBioseq_Base_Info::x_GetDescList(void) const
{
    try {
        return x_GetDescr().Get();
    }
    catch ( exception& ) {
        if ( !x_IsSetDescr() && IsSetDescr() ) {
            return const_cast<CBioseq_Base_Info*>(this)->x_SetDescr().Get();
        }
        throw;
    }
}


bool CBioseq_Base_Info::x_IsEndDesc(TDesc_CI iter) const
{
    return iter == x_GetDescList().end();
}


inline
bool CBioseq_Base_Info::x_IsEndNextDesc(TDesc_CI iter) const
{
    _ASSERT(!x_IsEndDesc(iter));
    return x_IsEndDesc(++iter);
}


CBioseq_Base_Info::TDesc_CI
CBioseq_Base_Info::x_GetFirstDesc(TDescTypeMask types) const
{
    x_PrefetchDesc(x_GetDescList().begin(), types);
    return x_FindDesc(x_GetDescList().begin(), types);
}


CBioseq_Base_Info::TDesc_CI
CBioseq_Base_Info::x_GetNextDesc(TDesc_CI iter, TDescTypeMask types) const
{
    _ASSERT(!x_IsEndDesc(iter));
    if ( x_IsEndNextDesc(iter) ) {
        x_PrefetchDesc(iter, types);
    }
    return x_FindDesc(++iter, types);
}


CBioseq_Base_Info::TDesc_CI
CBioseq_Base_Info::x_FindDesc(TDesc_CI iter, TDescTypeMask types) const
{
    _ASSERT(CSeqdesc::e_MaxChoice < 32);
    while ( !x_IsEndDesc(iter) ) {
        if ( types & (1<<(**iter).Which()) ) {
            break;
        }
        if ( x_IsEndNextDesc(iter) ) {
            x_PrefetchDesc(iter, types);
        }
        ++iter;
    }
    return iter;
}


void CBioseq_Base_Info::x_PrefetchDesc(TDesc_CI last,
                                       TDescTypeMask types) const
{
    // there might be a chunk with this descriptors
    for ( size_t i = 0, count = m_DescrTypeMasks.size(); i < count; ++i ) {
        if ( m_DescrTypeMasks[i] & types ) {
            x_LoadChunk(m_DescrChunks[i]);
            // check if new data appeared
            if ( x_IsEndDesc(last) ) {
                // first one
                if ( !x_GetDescList().empty() )
                    return;
            }
            else {
                // next one
                if ( !x_IsEndNextDesc(last) ) {
                    return;
                }
            }
        }
    }
}


void CBioseq_Base_Info::x_AttachAnnot(CRef<CSeq_annot_Info> annot)
{
    _ASSERT(!annot->HasParent_Info());
    annot->x_ParentAttach(*this);
    _ASSERT(&annot->GetParentBioseq_Base_Info() == this);
    x_AttachObject(*annot);
}


void CBioseq_Base_Info::x_DetachAnnot(CRef<CSeq_annot_Info> annot)
{
    _ASSERT(&annot->GetParentBioseq_Base_Info() == this);
    x_DetachObject(*annot);
    annot->x_ParentDetach(*this);
    _ASSERT(!annot->HasParent_Info());
}


void CBioseq_Base_Info::ResetAnnot(void)
{
    if ( !IsSetAnnot() ) {
        return;
    }
    x_Update(fNeedUpdate_annot);
    _ASSERT(IsSetAnnot());
    _ASSERT(m_ObjAnnot->size() == m_Annot.size());
    ITERATE ( TAnnot, it, m_Annot ) {
        x_DetachAnnot(*it);
    }
    m_Annot.clear();
    x_ResetObjAnnot();
    m_ObjAnnot = 0;
}


CRef<CSeq_annot_Info> CBioseq_Base_Info::AddAnnot(CSeq_annot& annot,
                                                  int chunk_id)
{
    CRef<CSeq_annot_Info> info(new CSeq_annot_Info(annot, chunk_id));
    AddAnnot(info);
    return info;
}


void CBioseq_Base_Info::AddAnnot(CRef<CSeq_annot_Info> info)
{
    _ASSERT(!info->HasParent_Info());
    if ( !m_ObjAnnot ) {
        m_ObjAnnot = &x_SetObjAnnot();
    }
    _ASSERT(m_ObjAnnot->size() == m_Annot.size());
    CRef<CSeq_annot> obj(const_cast<CSeq_annot*>(&info->x_GetObject()));
    m_ObjAnnot->push_back(obj);
    m_Annot.push_back(info);
    x_AttachAnnot(info);
}


void CBioseq_Base_Info::RemoveAnnot(CRef<CSeq_annot_Info> info)
{
    if ( &info->GetBaseParent_Info() != this ) {
        NCBI_THROW(CObjMgrException, eAddDataError,
                   "CSeq_entry_Info::x_RemoveAnnot: "
                   "not an owner");
    }
    _ASSERT(IsSetAnnot());
    _ASSERT(m_ObjAnnot->size() == m_Annot.size());

    CRef<CSeq_annot> obj(const_cast<CSeq_annot*>(&info->x_GetObject()));
    TAnnot::iterator info_it = find(m_Annot.begin(), m_Annot.end(), info);
    TObjAnnot::iterator obj_it = find(m_ObjAnnot->begin(), m_ObjAnnot->end(),
                                      obj);

    _ASSERT(info_it != m_Annot.end());
    _ASSERT(obj_it != m_ObjAnnot->end());

    x_DetachAnnot(info);

    m_Annot.erase(info_it);
    if ( m_Annot.empty() ) {
        x_ResetObjAnnot();
        m_ObjAnnot = 0;
    }
    else {
        m_ObjAnnot->erase(obj_it);
    }
}


const CSeqdesc* CBioseq_Base_Info::x_SearchFirstDesc(TDescTypeMask mask) const
{
    if ( IsSetDescr() ) {
        TDesc_CI it = x_GetFirstDesc(mask);
        if ( !x_IsEndDesc(it) ) {
            return *it;
        }
    }
    const CSeq_entry_Info& entry = GetParentSeq_entry_Info();
    if ( !entry.HasParent_Info() ) {
        return 0;
    }
    return entry.GetParentBioseq_set_Info().x_SearchFirstDesc(mask);
}


END_SCOPE(objects)
END_NCBI_SCOPE
