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
* Author: Eugene Vasilchenko
*
* File Description:
*   CSeq_entry_Info info -- entry for data source information about Seq-entry
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seqdesc.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


static const int kBioseqChunkId = kMax_Int;


CSeq_entry_Info::CSeq_entry_Info(void)
    : m_Which(CSeq_entry::e_not_set)
{
}


CSeq_entry_Info::CSeq_entry_Info(CSeq_entry& entry)
    : m_Which(CSeq_entry::e_not_set)
{
    x_SetObject(entry);
}


CSeq_entry_Info::CSeq_entry_Info(const CSeq_entry_Info& info,
                                 TObjectCopyMap* copy_map)
    : TParent(info, copy_map),
      m_Which(CSeq_entry::e_not_set)
{
    x_SetObject(info, copy_map);
}


CSeq_entry_Info::~CSeq_entry_Info(void)
{
}


const CBioseq_set_Info& CSeq_entry_Info::GetParentBioseq_set_Info(void) const
{
    return static_cast<const CBioseq_set_Info&>(GetBaseParent_Info());
}


CBioseq_set_Info& CSeq_entry_Info::GetParentBioseq_set_Info(void)
{
    return static_cast<CBioseq_set_Info&>(GetBaseParent_Info());
}


const CSeq_entry_Info& CSeq_entry_Info::GetParentSeq_entry_Info(void) const
{
    return GetParentBioseq_set_Info().GetParentSeq_entry_Info();
}


CSeq_entry_Info& CSeq_entry_Info::GetParentSeq_entry_Info(void)
{
    return GetParentBioseq_set_Info().GetParentSeq_entry_Info();
}


void CSeq_entry_Info::x_CheckWhich(E_Choice which) const
{
    if ( Which() != which ) {
        switch ( which ) {
        case CSeq_entry::e_Seq:
            NCBI_THROW(CUnassignedMember,eGet,"Seq_entry.seq");
        case CSeq_entry::e_Set:
            NCBI_THROW(CUnassignedMember,eGet,"Seq_entry.set");
        default:
            NCBI_THROW(CUnassignedMember,eGet,"Seq_entry.not_set");
        }
    }
}


const CBioseq_Info& CSeq_entry_Info::GetSeq(void) const
{
    x_CheckWhich(CSeq_entry::e_Seq);
    x_Update(fNeedUpdate_bioseq);
    const CBioseq_Base_Info& base = *m_Contents;
    return dynamic_cast<const CBioseq_Info&>(base);
}


CBioseq_Info& CSeq_entry_Info::SetSeq(void)
{
    x_CheckWhich(CSeq_entry::e_Seq);
    x_Update(fNeedUpdate_bioseq);
    CBioseq_Base_Info& base = *m_Contents;
    return dynamic_cast<CBioseq_Info&>(base);
}


const CBioseq_set_Info& CSeq_entry_Info::GetSet(void) const
{
    x_CheckWhich(CSeq_entry::e_Set);
    const CBioseq_Base_Info& base = *m_Contents;
    return dynamic_cast<const CBioseq_set_Info&>(base);
}


CBioseq_set_Info& CSeq_entry_Info::SetSet(void)
{
    x_CheckWhich(CSeq_entry::e_Set);
    CBioseq_Base_Info& base = *m_Contents;
    return dynamic_cast<CBioseq_set_Info&>(base);
}


void CSeq_entry_Info::x_Select(CSeq_entry::E_Choice which,
                               CRef<CBioseq_Base_Info> contents)
{
    if ( Which() != which || m_Contents != contents ) {
        if ( m_Contents ) {
            x_DetachContents();
            m_Contents.Reset();
        }
        m_Which = which;
        m_Contents = contents;
        switch ( m_Which ) {
        case CSeq_entry::e_Seq:
            m_Object->SetSeq(SetSeq().x_GetObject());
            break;
        case CSeq_entry::e_Set:
            m_Object->SetSet(SetSet().x_GetObject());
            break;
        default:
            m_Object->Reset();
            break;
        }
        x_AttachContents();
    }
}


inline
void CSeq_entry_Info::x_Select(CSeq_entry::E_Choice which,
                               CBioseq_Base_Info* contents)
{
    x_Select(which, Ref(contents));
}


void CSeq_entry_Info::Reset(void)
{
    x_Select(CSeq_entry::e_not_set, 0);
    SetBioObjectId(GetTSE_Info().x_RegisterBioObject(*this));
}


CBioseq_set_Info& CSeq_entry_Info::SelectSet(CBioseq_set_Info& seqset)
{
    if ( Which() != CSeq_entry::e_not_set ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "Reset CSeq_entry_Handle before selecting set");
    }
    x_Select(CSeq_entry::e_Set, &seqset);
    return SetSet();
}


CBioseq_set_Info& CSeq_entry_Info::SelectSet(CBioseq_set& seqset)
{
    return SelectSet(*new CBioseq_set_Info(seqset));
}


CBioseq_set_Info& CSeq_entry_Info::SelectSet(void)
{
    if ( !IsSet() ) {
        SelectSet(*new CBioseq_set);
    }
    return SetSet();
}


CBioseq_Info& CSeq_entry_Info::SelectSeq(CBioseq_Info& seq)
{
    if ( Which() != CSeq_entry::e_not_set ) {
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "Reset CSeq_entry_Handle before selecting seq");
    }
    x_Select(CSeq_entry::e_Seq, &seq);
    return SetSeq();
}


CBioseq_Info& CSeq_entry_Info::SelectSeq(CBioseq& seq)
{
    return SelectSeq(*new CBioseq_Info(seq));
}


void CSeq_entry_Info::x_DoUpdate(TNeedUpdateFlags flags)
{
    if ( flags & fNeedUpdate_bioseq ) {
        x_LoadChunk(kBioseqChunkId);
    }
    if ( (flags & fNeedUpdate_children) && m_Contents ) {
        m_Contents->x_Update((flags & fNeedUpdate_children) |
                             (flags >> kNeedUpdate_bits));
        _ASSERT(Which()==m_Object->Which());
    }
    TParent::x_DoUpdate(flags);
}


void CSeq_entry_Info::x_SetNeedUpdateContents(TNeedUpdateFlags flags)
{
    x_SetNeedUpdate(flags);
}


CConstRef<CSeq_entry> CSeq_entry_Info::GetCompleteSeq_entry(void) const
{
    x_UpdateComplete();
    return m_Object;
}


CConstRef<CSeq_entry> CSeq_entry_Info::GetSeq_entryCore(void) const
{
    x_UpdateCore();
    return m_Object;
}

void CSeq_entry_Info::x_ParentAttach(CBioseq_set_Info& parent)
{
    x_BaseParentAttach(parent);
    if ( parent.HasParent_Info() ) {
        CSeq_entry& entry = parent.GetParentSeq_entry_Info().x_GetObject();
        if ( m_Object->GetParentEntry() != &entry ) {
            m_Object->SetParentEntry(&entry);
            //entry.ParentizeOneLevel(); this call is too slow
        }
        _ASSERT(m_Object->GetParentEntry() == &entry);
    }
}


void CSeq_entry_Info::x_ParentDetach(CBioseq_set_Info& parent)
{
    m_Object->ResetParentEntry();
    x_BaseParentDetach(parent);
}


void CSeq_entry_Info::x_TSEAttachContents(CTSE_Info& tse)
{
    TParent::x_TSEAttachContents(tse);
    if ( m_Contents ) {
        m_Contents->x_TSEAttach(tse);
    }
}


void CSeq_entry_Info::x_TSEDetachContents(CTSE_Info& tse)
{
    if ( m_Contents ) {
        m_Contents->x_TSEDetach(tse);
    }
    TParent::x_TSEDetachContents(tse);
}


void CSeq_entry_Info::x_DSAttachContents(CDataSource& ds)
{
    TParent::x_DSAttachContents(ds);
    if ( m_Object ) {
        x_DSMapObject(m_Object, ds);
    }
    if ( m_Contents ) {
        m_Contents->x_DSAttach(ds);
    }
}


void CSeq_entry_Info::x_DSDetachContents(CDataSource& ds)
{
    if ( m_Contents ) {
        m_Contents->x_DSDetach(ds);
    }
    if ( m_Object ) {
        x_DSUnmapObject(m_Object, ds);
    }
    TParent::x_DSDetachContents(ds);
}


void CSeq_entry_Info::x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    ds.x_Map(obj, this);
}


void CSeq_entry_Info::x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    ds.x_Unmap(obj, this);
}


void CSeq_entry_Info::x_SetObject(TObject& obj)
{
    // x_CheckWhich(CSeq_entry::e_not_set);
    _ASSERT(!m_Object);
    _ASSERT(!m_Contents);

    m_Object.Reset(&obj);
    if ( HasDataSource() ) {
        x_DSMapObject(m_Object, GetDataSource());
    }
    switch ( (m_Which = obj.Which()) ) {
    case CSeq_entry::e_Seq:
        m_Contents.Reset(new CBioseq_Info(obj.SetSeq()));
        break;
    case CSeq_entry::e_Set:
        m_Contents.Reset(new CBioseq_set_Info(obj.SetSet()));
        break;
    default:
        break;
    }
    x_AttachContents();
}


void CSeq_entry_Info::x_SetObject(const CSeq_entry_Info& info,
                                  TObjectCopyMap* copy_map)
{
    //x_CheckWhich(CSeq_entry::e_not_set);
    _ASSERT(!m_Object);
    _ASSERT(!m_Contents);

    m_Object.Reset(new CSeq_entry);
    if ( HasDataSource() ) {
        x_DSMapObject(m_Object, GetDataSource());
    }
    CRef<CBioseq_Base_Info> cinfo;
    switch ( info.Which() ) {
    case CSeq_entry::e_Seq:
        cinfo.Reset(new CBioseq_Info(info.GetSeq(), copy_map));
        break;
    case CSeq_entry::e_Set:
        cinfo.Reset(new CBioseq_set_Info(info.GetSet(), copy_map));
        break;
    default:
        break;
    }
    x_Select(info.Which(), cinfo);
}


void CSeq_entry_Info::x_AttachContents(void)
{
    if ( m_Contents ) {
        m_Contents->x_ParentAttach(*this);
        x_AttachObject(*m_Contents);
    }
}


void CSeq_entry_Info::x_DetachContents(void)
{
    if ( m_Contents ) {
        x_DetachObject(*m_Contents);
        m_Contents->x_ParentDetach(*this);
    }
}


void CSeq_entry_Info::UpdateAnnotIndex(void) const
{
    if ( x_DirtyAnnotIndex() ) {
        GetTSE_Info().UpdateAnnotIndex(*this);
    }
}


void CSeq_entry_Info::x_UpdateAnnotIndexContents(CTSE_Info& tse)
{
    if ( m_Contents ) {
        m_Contents->x_UpdateAnnotIndex(tse);
    }
    TParent::x_UpdateAnnotIndexContents(tse);
}


bool CSeq_entry_Info::IsSetDescr(void) const
{
    // x_Update(fNeedUpdate_descr);
    return m_Contents && m_Contents->IsSetDescr();
}


const CSeq_descr& CSeq_entry_Info::GetDescr(void) const
{
    x_Update(fNeedUpdate_descr);
    return m_Contents->GetDescr();
}


void CSeq_entry_Info::SetDescr(TDescr& v)
{
    x_Update(fNeedUpdate_descr);
    m_Contents->SetDescr(v);
}


void CSeq_entry_Info::ResetDescr(void)
{
    x_Update(fNeedUpdate_descr);
    m_Contents->ResetDescr();
}


bool CSeq_entry_Info::AddSeqdesc(CSeqdesc& d)
{
    x_Update(fNeedUpdate_descr);
    return m_Contents->AddSeqdesc(d);
}


CRef<CSeqdesc> CSeq_entry_Info::RemoveSeqdesc(const CSeqdesc& d)
{
    x_Update(fNeedUpdate_descr);
    return m_Contents->RemoveSeqdesc(d);
}

/*
void CSeq_entry_Info::AddDescr(CSeq_entry_Info& src)
{
    x_Update(fNeedUpdate_descr);
    if ( src.IsSetDescr() ) {
        m_Contents->AddSeq_descr(src.m_Contents->SetDescr());
    }
}
*/
void CSeq_entry_Info::AddSeq_descr(const TDescr& v)
{
    x_Update(fNeedUpdate_descr);
    m_Contents->AddSeq_descr(v);
}



bool CSeq_entry_Info::x_IsEndDesc(TDesc_CI iter) const
{
    return m_Contents->x_IsEndDesc(iter);
}


CSeq_entry_Info::TDesc_CI
CSeq_entry_Info::x_GetFirstDesc(TDescTypeMask types) const
{
    return m_Contents->x_GetFirstDesc(types);
}


CSeq_entry_Info::TDesc_CI
CSeq_entry_Info::x_GetNextDesc(TDesc_CI iter, TDescTypeMask types) const
{
    return m_Contents->x_GetNextDesc(iter, types);
}


CRef<CSeq_annot_Info> CSeq_entry_Info::AddAnnot(CSeq_annot& annot)
{
    return m_Contents->AddAnnot(annot);
}


void CSeq_entry_Info::AddAnnot(CRef<CSeq_annot_Info> annot)
{
    m_Contents->AddAnnot(annot);
}


void CSeq_entry_Info::RemoveAnnot(CRef<CSeq_annot_Info> annot)
{
    m_Contents->RemoveAnnot(annot);
}


void CSeq_entry_Info::x_SetBioseqChunkId(TChunkId _DEBUG_ARG(chunk_id))
{
    _ASSERT(chunk_id == kBioseqChunkId);
    x_CheckWhich(CSeq_entry::e_not_set);
    x_SetNeedUpdate(CTSE_Info::fNeedUpdate_bioseq);
    m_Which = CSeq_entry::e_Seq;
}


CRef<CSeq_entry_Info> CSeq_entry_Info::AddEntry(CSeq_entry& entry,
                                                  int index)
{
    x_CheckWhich(CSeq_entry::e_Set);
    return SetSet().AddEntry(entry, index);
}


void CSeq_entry_Info::AddEntry(CRef<CSeq_entry_Info> entry, int index)
{
    x_CheckWhich(CSeq_entry::e_Set);
    SetSet().AddEntry(entry, index);
}


void CSeq_entry_Info::RemoveEntry(CRef<CSeq_entry_Info> entry)
{
    x_CheckWhich(CSeq_entry::e_Set);
    SetSet().RemoveEntry(entry);
}

const CBioObjectId& CSeq_entry_Info::GetBioObjectId(void) const
{
    if (m_Contents)
        return m_Contents->GetBioObjectId();
    return TParent::GetBioObjectId();
}

const CSeq_entry_Info::TAnnot& CSeq_entry_Info::GetLoadedAnnot(void) const
{
    _ASSERT(m_Contents);
    if (!m_Contents)
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "The CSeq_entry_Handle must be selected first.");
    return m_Contents->GetLoadedAnnot();
}


END_SCOPE(objects)
END_NCBI_SCOPE
