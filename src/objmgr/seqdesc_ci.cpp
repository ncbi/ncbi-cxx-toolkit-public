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
//#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/bioseq_base_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CSeqdesc_CI::
//


// inline methods first
inline
const CBioseq_Base_Info& CSeqdesc_CI::x_GetBaseInfo(void) const
{
    return m_Entry.x_GetBaseInfo();
}


inline
bool CSeqdesc_CI::x_ValidDesc(void) const
{
    _ASSERT(m_Entry);
    return !x_GetBaseInfo().x_IsEndDesc(m_Desc_CI);
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
    m_Desc_CI = x_GetBaseInfo().x_GetNextDesc(m_Desc_CI, m_Choice);
}


void CSeqdesc_CI::x_FirstDesc(void)
{
    if ( !m_Entry ) {
        return;
    }
    m_Desc_CI = x_GetBaseInfo().x_GetFirstDesc(m_Choice);
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
