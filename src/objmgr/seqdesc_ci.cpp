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
* Author: Aleksey Grichenko
*
* File Description:
*   Object manager iterators
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/seq_entry_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CSeqdesc_CI::
//


// inline methods first
inline
bool CSeqdesc_CI::x_ValidDesc(void) const
{
    _ASSERT(m_Entry);
    const CSeq_entry_Info& entry = m_Entry.GetSeq_entry_Handle().x_GetInfo();
    return !entry.x_IsEndDesc(m_Desc_CI);
}


inline
bool CSeqdesc_CI::x_RequestedType(void) const
{
    _ASSERT(CSeqdesc::e_MaxChoice < 32);
    _ASSERT(x_ValidDesc());
    return m_Choice & (1<<(**m_Desc_CI).Which()) ? true : false;
}


inline
bool CSeqdesc_CI::x_Valid(void) const
{
    return !m_Entry || x_ValidDesc() && x_RequestedType();
}


CSeqdesc_CI::CSeqdesc_CI(void)
{
    _ASSERT(x_Valid());
}


CSeqdesc_CI::CSeqdesc_CI(const CSeq_descr_CI& desc_it,
                         CSeqdesc::E_Choice choice)
{
    x_SetChoice(choice);
    x_SetEntry(desc_it);
    _ASSERT(x_Valid());
}


CSeqdesc_CI::CSeqdesc_CI(const CBioseq_Handle& handle,
                         CSeqdesc::E_Choice choice,
                         size_t search_depth)
{
    x_SetChoice(choice);
    x_SetEntry(CSeq_descr_CI(handle, search_depth));
    _ASSERT(x_Valid());
}


CSeqdesc_CI::CSeqdesc_CI(const CSeq_entry_Handle& entry,
                         CSeqdesc::E_Choice choice,
                         size_t search_depth)
    : m_Entry(entry, search_depth)
{
    x_SetChoice(choice);
    x_SetEntry(CSeq_descr_CI(entry, search_depth));
    _ASSERT(x_Valid());
}


CSeqdesc_CI::CSeqdesc_CI(const CBioseq_Handle& handle,
                         const TDescChoices& choices,
                         size_t search_depth)
{
    x_SetChoices(choices);
    x_SetEntry(CSeq_descr_CI(handle, search_depth));
    _ASSERT(x_Valid());
}


CSeqdesc_CI::CSeqdesc_CI(const CSeq_entry_Handle& entry,
                         const TDescChoices& choices,
                         size_t search_depth)
{
    x_SetChoices(choices);
    x_SetEntry(CSeq_descr_CI(entry, search_depth));
    _ASSERT(x_Valid());
}


CSeqdesc_CI::CSeqdesc_CI(const CSeqdesc_CI& iter)
    : m_Choice(iter.m_Choice),
      m_Entry(iter.m_Entry),
      m_Desc_CI(iter.m_Desc_CI)
{
    _ASSERT(x_Valid());
}


CSeqdesc_CI::~CSeqdesc_CI(void)
{
}


CSeqdesc_CI& CSeqdesc_CI::operator= (const CSeqdesc_CI& iter)
{
    if (this != &iter) {
        m_Choice   = iter.m_Choice;
        m_Entry    = iter.m_Entry;
        m_Desc_CI  = iter.m_Desc_CI;
    }
    _ASSERT(x_Valid());
    return *this;
}


void CSeqdesc_CI::x_AddChoice(CSeqdesc::E_Choice choice)
{
    if ( choice != CSeqdesc::e_not_set ) {
        _ASSERT(choice < 32);
        m_Choice |= (1<<choice);
    }
    else {
        // set all bits
        m_Choice |= ~0;
    }
}


void CSeqdesc_CI::x_SetChoice(CSeqdesc::E_Choice choice)
{
    m_Choice = 0;
    x_AddChoice(choice);
}


void CSeqdesc_CI::x_SetChoices(const TDescChoices& choices)
{
    m_Choice = 0;
    ITERATE ( TDescChoices, it, choices ) {
        x_AddChoice(*it);
    }
}


void CSeqdesc_CI::x_NextDesc(void)
{
    _ASSERT(x_ValidDesc());
    const CSeq_entry_Info& entry = m_Entry.GetSeq_entry_Handle().x_GetInfo();
    m_Desc_CI = entry.x_GetNextDesc(m_Desc_CI, m_Choice);
}


void CSeqdesc_CI::x_FirstDesc(void)
{
    if ( !m_Entry ) {
        return;
    }
    const CSeq_entry_Info& entry = m_Entry.GetSeq_entry_Handle().x_GetInfo();
    m_Desc_CI = entry.x_GetFirstDesc(m_Choice);
}


void CSeqdesc_CI::x_Settle(void)
{
    while ( m_Entry && !x_ValidDesc() ) {
        ++m_Entry;
        x_FirstDesc();
    }
}


void CSeqdesc_CI::x_SetEntry(const CSeq_descr_CI& entry)
{
    m_Entry = entry;
    x_FirstDesc();
    // Advance to the first relevant Seqdesc, if any.
    x_Settle();
}


void CSeqdesc_CI::x_Next(void)
{
    x_NextDesc();
    x_Settle();
}


CSeqdesc_CI& CSeqdesc_CI::operator++(void)
{
    x_Next();
    _ASSERT(x_Valid());
    return *this;
}


const CSeqdesc& CSeqdesc_CI::operator*(void) const
{
    _ASSERT(x_ValidDesc() && x_RequestedType());
    return **m_Desc_CI;
}


const CSeqdesc* CSeqdesc_CI::operator->(void) const
{
    _ASSERT(x_ValidDesc() && x_RequestedType());
    return *m_Desc_CI;
}


CSeq_entry_Handle CSeqdesc_CI::GetSeq_entry_Handle(void) const
{
    return m_Entry.GetSeq_entry_Handle();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.17  2005/06/09 20:33:55  grichenk
* Fixed loading of split descriptors by CSeqdesc_CI
*
* Revision 1.16  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.15  2004/11/22 16:08:15  dicuccio
* Fix compiler warning in Win32
*
* Revision 1.14  2004/10/07 14:03:32  vasilche
* Use shared among TSEs CTSE_Split_Info.
* Use typedefs and methods for TSE and DataSource locking.
* Load split CSeqdesc on the fly in CSeqdesc_CI.
*
* Revision 1.13  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.12  2004/04/28 14:14:39  grichenk
* Added filtering by several seqdesc types.
*
* Revision 1.11  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.10  2004/02/09 19:18:54  grichenk
* Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
* and CSeqdesc_CI to avoid using data directly.
*
* Revision 1.9  2003/06/02 16:06:38  dicuccio
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
* Revision 1.8  2003/03/14 19:10:41  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.7  2003/03/13 13:02:36  dicuccio
* Added #include for annot_object.hpp - fixes obtuse error for Win32 builds
*
* Revision 1.6  2002/12/05 19:28:32  grichenk
* Prohibited postfix operator ++()
*
* Revision 1.5  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.4  2002/05/09 14:20:54  grichenk
* Added checking of m_Current validity
*
* Revision 1.3  2002/05/06 03:28:48  vakatov
* OM/OM1 renaming
*
* Revision 1.2  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/01/11 19:06:25  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
