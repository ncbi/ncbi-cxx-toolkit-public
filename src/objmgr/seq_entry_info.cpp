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


CSeq_entry_Info::CSeq_entry_Info(void)
    : m_Which(CSeq_entry::e_not_set)
{
}


CSeq_entry_Info::CSeq_entry_Info(CSeq_entry& entry)
    : m_Which(CSeq_entry::e_not_set)
{
    x_SetObject(entry);
}


CSeq_entry_Info::CSeq_entry_Info(const CSeq_entry_Info& info)
    : m_Which(CSeq_entry::e_not_set)
{
    x_SetObject(info);
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
    const CBioseq_Base_Info& base = *m_Contents;
    return dynamic_cast<const CBioseq_Info&>(base);
}


CBioseq_Info& CSeq_entry_Info::SetSeq(void)
{
    x_CheckWhich(CSeq_entry::e_Seq);
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
            x_AttachObjectVariant(SetSeq().x_GetObject());
            break;
        case CSeq_entry::e_Set:
            x_AttachObjectVariant(SetSet().x_GetObject());
            break;
        default:
            x_DetachObjectVariant();
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
    if ( m_Contents ) {
        m_Contents->x_Update(flags);
        _ASSERT(Which()==m_Object->Which());
        _ASSERT(!IsSet()||GetSet().GetBioseq_setCore() == &m_Object->GetSet());
        _ASSERT(!IsSeq()||GetSeq().GetBioseqCore() == &m_Object->GetSeq());
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
            entry.ParentizeOneLevel();
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
    x_CheckWhich(CSeq_entry::e_not_set);
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


void CSeq_entry_Info::x_SetObject(const CSeq_entry_Info& info)
{
    x_CheckWhich(CSeq_entry::e_not_set);
    _ASSERT(!m_Object);
    _ASSERT(!m_Contents);

    m_Which = info.m_Which;
    m_Object.Reset(new CSeq_entry);
    if ( HasDataSource() ) {
        x_DSMapObject(m_Object, GetDataSource());
    }
    switch ( m_Which ) {
    case CSeq_entry::e_Seq:
    {
        CRef<CBioseq_Info> seq(new CBioseq_Info(info.GetSeq()));
        m_Contents.Reset(seq);
        x_AttachObjectVariant(seq->x_GetObject());
        break;
    }
    case CSeq_entry::e_Set:
    {
        CRef<CBioseq_set_Info> seqset(new CBioseq_set_Info(info.GetSet()));
        m_Contents.Reset(seqset);
        x_AttachObjectVariant(seqset->x_GetObject());
        break;
    }
    default:
        break;
    }
    x_AttachContents();
}


void CSeq_entry_Info::x_DetachObjectVariant(void)
{
    m_Object->Reset();
}


void CSeq_entry_Info::x_AttachObjectVariant(CBioseq_set& seqset)
{
    x_DetachObjectVariant();
    m_Object->SetSet(seqset);
}


void CSeq_entry_Info::x_AttachObjectVariant(CBioseq& seq)
{
    x_DetachObjectVariant();
    m_Object->SetSeq(seq);
}

/*
CRef<CSeq_entry> CSeq_entry_Info::x_CreateObject(void) const
{        
    CRef<TObject> obj(new TObject);
    switch ( Which() ) {
    case CSeq_entry::e_Set:
        obj->SetSet(const_cast<CBioseq_set&>
                    (*GetSet().GetCompleteBioseq_set()));
        break;
    case CSeq_entry::e_Seq:
        obj->SetSeq(const_cast<CBioseq&>
                    (*GetSeq().GetCompleteBioseq()));
        break;
    default:
        break;
    }
    return obj;
}
*/


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
        _ASSERT(!x_DirtyAnnotIndex());
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
    x_Update(fNeedUpdate_descr);
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


void CSeq_entry_Info::AddDescr(CSeq_entry_Info& src)
{
    x_Update(fNeedUpdate_descr);
    if ( src.IsSetDescr() ) {
        m_Contents->AddSeq_descr(src.m_Contents->SetDescr());
    }
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


CRef<CSeq_annot_Info> CSeq_entry_Info::AddAnnot(const CSeq_annot& annot)
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


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.22  2005/02/28 15:23:05  grichenk
 * RemoveDesc() returns CRef<CSeqdesc>
 *
 * Revision 1.21  2005/01/12 17:16:14  vasilche
 * Avoid performance warning on MSVC.
 *
 * Revision 1.20  2004/10/07 14:03:32  vasilche
 * Use shared among TSEs CTSE_Split_Info.
 * Use typedefs and methods for TSE and DataSource locking.
 * Load split CSeqdesc on the fly in CSeqdesc_CI.
 *
 * Revision 1.19  2004/08/17 15:56:22  vasilche
 * Added mapping and unmapping CSeq_entry -> CSeq_entry_Info in x_SetObject().
 *
 * Revision 1.18  2004/08/13 15:55:16  vasilche
 * Do not map null Seq-entry.
 *
 * Revision 1.17  2004/08/04 14:53:26  vasilche
 * Revamped object manager:
 * 1. Changed TSE locking scheme
 * 2. TSE cache is maintained by CDataSource.
 * 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
 * 4. Fixed processing of split data.
 *
 * Revision 1.16  2004/07/12 16:57:32  vasilche
 * Fixed loading of split Seq-descr and Seq-data objects.
 * They are loaded correctly now when GetCompleteXxx() method is called.
 *
 * Revision 1.15  2004/05/21 21:42:13  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.14  2004/03/31 17:08:07  vasilche
 * Implemented ConvertSeqToSet and ConvertSetToSeq.
 *
 * Revision 1.13  2004/03/25 19:27:44  vasilche
 * Implemented MoveTo and CopyTo methods of handles.
 *
 * Revision 1.12  2004/03/24 18:30:30  vasilche
 * Fixed edit API.
 * Every *_Info object has its own shallow copy of original object.
 *
 * Revision 1.11  2004/03/16 15:47:28  vasilche
 * Added CBioseq_set_Handle and set of EditHandles
 *
 * Revision 1.10  2004/02/03 19:02:18  vasilche
 * Fixed broken 'dirty annot index' state after RemoveEntry().
 *
 * Revision 1.9  2004/02/02 14:46:44  vasilche
 * Several performance fixed - do not iterate whole tse set in CDataSource.
 *
 * Revision 1.8  2004/01/29 19:33:07  vasilche
 * Fixed coredump on WorkShop when invalid Seq-entry is added to CScope.
 *
 * Revision 1.7  2004/01/22 20:10:40  vasilche
 * 1. Splitted ID2 specs to two parts.
 * ID2 now specifies only protocol.
 * Specification of ID2 split data is moved to seqsplit ASN module.
 * For now they are still reside in one resulting library as before - libid2.
 * As the result split specific headers are now in objects/seqsplit.
 * 2. Moved ID2 and ID1 specific code out of object manager.
 * Protocol is processed by corresponding readers.
 * ID2 split parsing is processed by ncbi_xreader library - used by all readers.
 * 3. Updated OBJMGR_LIBS correspondingly.
 *
 * Revision 1.6  2003/12/18 16:38:07  grichenk
 * Added CScope::RemoveEntry()
 *
 * Revision 1.5  2003/12/11 17:02:50  grichenk
 * Fixed CRef resetting in constructors.
 *
 * Revision 1.4  2003/11/19 22:18:03  grichenk
 * All exceptions are now CException-derived. Catch "exception" rather
 * than "runtime_error".
 *
 * Revision 1.3  2003/09/30 16:22:03  vasilche
 * Updated internal object manager classes to be able to load ID2 data.
 * SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
 * Scope caches results of requests for data to data loaders.
 * Optimized CSeq_id_Handle for gis.
 * Optimized bioseq lookup in scope.
 * Reduced object allocations in annotation iterators.
 * CScope is allowed to be destroyed before other objects using this scope are
 * deleted (feature iterators, bioseq handles etc).
 * Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
 * Added 'adaptive' option to objmgr_demo application.
 *
 * Revision 1.2  2003/06/02 16:06:38  dicuccio
 * Rearranged src/objects/ subtree.  This includes the following shifts:
 *     - src/objects/asn2asn --> arc/app/asn2asn
 *     - src/objects/testmedline --> src/objects/ncbimime/test
 *     - src/objects/objmgr --> src/objmgr
 *     - src/objects/util --> src/objmgr/util
 *     - src/objects/alnmgr --> src/objtools/alnmgr
 *     - src/objects/flat --> src/objtools/flat
 *     - src/objects/validator --> src/objtools/validator
 *     - src/objects/cddalignview --> src/objtools/cddalignview
 * In addition, libseq now includes six of the objects/seq... libs, and libmmdb
 * replaces the three libmmdb? libs.
 *
 * Revision 1.1  2003/04/24 16:12:38  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 *
 * ===========================================================================
 */
