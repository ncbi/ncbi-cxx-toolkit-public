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


#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seq/Seq_descr.hpp>

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
    : m_ModifiedMembers(0),
      m_SetMembers(0)
{
}


CBioseq_Base_Info::CBioseq_Base_Info(const CBioseq_Base_Info& info)
    : m_ModifiedMembers(info.m_ModifiedMembers),
      m_SetMembers(info.m_SetMembers),
      m_Descr(info.m_Descr)
{
    ITERATE ( TAnnot, it, info.m_Annot ) {
        x_AttachAnnot(Ref(new CSeq_annot_Info(**it)));
    }
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


void CBioseq_Base_Info::x_SetDescr(const CSeq_descr& descr)
{
    m_Descr.Reset(&descr);
    x_SetSetMembers(fMember_descr);
}


void CBioseq_Base_Info::x_SetAnnot(const TObjAnnot& annot)
{
    ITERATE( TObjAnnot, it, annot ) {
        x_AttachAnnot(Ref(new CSeq_annot_Info(**it)));
    }
    x_SetSetMembers(fMember_annot);
}


void CBioseq_Base_Info::x_FillAnnot(TObjAnnot& annot) const
{
    ITERATE( TAnnot, it, m_Annot ) {
        CConstRef<CSeq_annot> add = (*it)->GetCompleteSeq_annot();
        annot.push_back(Ref(const_cast<CSeq_annot*>(&*add)));
    }
}


void CBioseq_Base_Info::x_AttachAnnot(CRef<CSeq_annot_Info> annot)
{
    _ASSERT(!annot->HaveParent_Info());
    m_Annot.push_back(annot);
    annot->x_ParentAttach(*this);
    _ASSERT(&annot->GetParentBioseq_Base_Info() == this);
    x_AttachObject(*annot);
}


void CBioseq_Base_Info::x_DetachAnnot(CRef<CSeq_annot_Info> annot)
{
    _ASSERT(&annot->GetParentBioseq_Base_Info() == this);
    x_DetachObject(*annot);
    annot->x_ParentDetach(*this);
    _ASSERT(!annot->HaveParent_Info());
}


void CBioseq_Base_Info::x_CheckSetMember(TMembers member) const
{
    if ( !x_IsSetMember(member) ) {
        NCBI_THROW(CUnassignedMember,eGet,
                   string(x_GetTypeName())+'.'+x_GetMemberName(member));
    }
}


const char* CBioseq_Base_Info::x_GetMemberName(TMembers member) const
{
    if ( member & fMember_descr ) {
        return "descr";
    }
    if ( member & fMember_annot ) {
        return "annot";
    }
    return "<unknown>";
}


void CBioseq_Base_Info::x_SetModifiedMember(TMembers members)
{
    TMembers old = m_ModifiedMembers;
    m_ModifiedMembers = old | members;
    if ( m_ModifiedMembers != old ) {
        GetParentSeq_entry_Info().x_SetModifiedContents();
    }
}


void CBioseq_Base_Info::x_ResetModifiedMembers(void)
{
    m_ModifiedMembers = 0;
}


void CBioseq_Base_Info::x_SetSetMembers(TMembers members)
{
    m_SetMembers |= members;
}


CRef<CSeq_annot_Info> CBioseq_Base_Info::x_AddAnnot(const CSeq_annot& annot)
{
    CRef<CSeq_annot_Info> info(new CSeq_annot_Info(annot));
    x_AddAnnot(info);
    return info;
}


void CBioseq_Base_Info::x_AddAnnot(CRef<CSeq_annot_Info> info)
{
    x_SetModifiedMember(fMember_annot);
    x_AttachAnnot(info);
}


void CBioseq_Base_Info::x_RemoveAnnot(CRef<CSeq_annot_Info> info)
{
    if ( &info->GetBaseParent_Info() != this ) {
        NCBI_THROW(CObjMgrException, eAddDataError,
                   "CSeq_entry_Info::x_RemoveAnnot: "
                   "not an owner");
    }
    x_SetModifiedMember(fMember_annot);
    x_DetachAnnot(info);
    NON_CONST_ITERATE ( TAnnot, it, m_Annot ) {
        if ( *it == info ) {
            m_Annot.erase(it);
            break;
        }
    }
}


void CBioseq_Base_Info::x_UpdateAnnotIndexContents(CTSE_Info& tse)
{
    TParent::x_UpdateAnnotIndexContents(tse);
    NON_CONST_ITERATE ( TAnnot, it, m_Annot ) {
        (*it)->x_UpdateAnnotIndex(tse);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.15  2003/12/11 17:02:50  grichenk
* Fixed CRef resetting in constructors.
*
* Revision 1.14  2003/11/19 22:18:02  grichenk
* All exceptions are now CException-derived. Catch "exception" rather
* than "runtime_error".
*
* Revision 1.13  2003/11/12 16:53:17  grichenk
* Modified CSeqMap to work with const objects (CBioseq, CSeq_loc etc.)
*
* Revision 1.12  2003/09/30 16:22:02  vasilche
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
* Revision 1.11  2003/06/02 16:06:37  dicuccio
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
* Revision 1.10  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.9  2003/03/14 19:10:41  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.8  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.7  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.6  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.5  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.4  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.3  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.2  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/02/07 21:25:05  grichenk
* Initial revision
*
*
* ===========================================================================
*/
