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
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Ref_ext.hpp>
#include <objmgr/impl/annot_object.hpp>
//#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/bioseq_base_info.hpp>
#include <objmgr/scope.hpp>

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
    TDescTypeMask maxval = 1 << CSeqdesc::e_MaxChoice;
    _ASSERT(maxval);
    _ASSERT(x_ValidDesc());
    return m_Choice & (1<<(**m_Desc_CI).Which()) ? true : false;
}


inline
bool CSeqdesc_CI::x_Valid(void) const
{
    return !m_Entry || (x_ValidDesc() && x_RequestedType());
}


void CSeqdesc_CI::x_CheckRef(const CBioseq_Handle& handle)
{
    m_Ref.Reset();
    if (!handle  ||
        !handle.IsSetInst_Repr()  ||
        handle.GetInst_Repr() != CSeq_inst::eRepr_ref  ||
        !handle.IsSetInst_Ext()  ||
        !handle.GetInst_Ext().IsRef()) {
        return;
    }
    const CRef_ext& ref = handle.GetInst_Ext().GetRef();
    CConstRef<CSeq_id> ref_id(ref.GetId());
    if ( !ref_id ) return; // Bad reference location or multiple ids.
    m_Ref = handle.GetScope().GetBioseqHandle(*ref_id);
}


CSeqdesc_CI::CSeqdesc_CI(void)
    : m_HaveTitle(false),
      m_Depth(0)
{
    _ASSERT(x_Valid());
}


CSeqdesc_CI::CSeqdesc_CI(const CSeq_descr_CI& desc_it,
                         CSeqdesc::E_Choice choice)
    : m_HaveTitle(false),
      m_Depth(0)
{
    x_SetChoice(choice);
    x_SetEntry(desc_it);
    _ASSERT(x_Valid());
}


CSeqdesc_CI::CSeqdesc_CI(const CBioseq_Handle& handle,
                         CSeqdesc::E_Choice choice,
                         size_t search_depth)
    : m_HaveTitle(false),
      m_Depth(search_depth)
{
    x_SetChoice(choice);
    x_CheckRef(handle);
    x_SetEntry(CSeq_descr_CI(handle, search_depth));
    _ASSERT(x_Valid());
}


CSeqdesc_CI::CSeqdesc_CI(const CSeq_entry_Handle& entry,
                         CSeqdesc::E_Choice choice,
                         size_t search_depth)
    : m_Entry(entry, search_depth),
      m_HaveTitle(false),
      m_Depth(search_depth)
{
    x_SetChoice(choice);
    x_SetEntry(CSeq_descr_CI(entry, search_depth));
    _ASSERT(x_Valid());
}


CSeqdesc_CI::CSeqdesc_CI(const CBioseq_Handle& handle,
                         const TDescChoices& choices,
                         size_t search_depth)
    : m_HaveTitle(false),
      m_Depth(search_depth)
{
    x_SetChoices(choices);
    x_CheckRef(handle);
    x_SetEntry(CSeq_descr_CI(handle, search_depth));
    _ASSERT(x_Valid());
}


CSeqdesc_CI::CSeqdesc_CI(const CSeq_entry_Handle& entry,
                         const TDescChoices& choices,
                         size_t search_depth)
    : m_HaveTitle(false),
      m_Depth(search_depth)
{
    x_SetChoices(choices);
    x_SetEntry(CSeq_descr_CI(entry, search_depth));
    _ASSERT(x_Valid());
}


CSeqdesc_CI::CSeqdesc_CI(const CSeqdesc_CI& iter)
    : m_Choice(iter.m_Choice),
      m_Entry(iter.m_Entry),
      m_Desc_CI(iter.m_Desc_CI),
      m_Ref(iter.m_Ref),
      m_HaveTitle(iter.m_HaveTitle),
      m_Depth(iter.m_Depth)
{
    _ASSERT(x_Valid());
}


CSeqdesc_CI::~CSeqdesc_CI(void)
{
}


CSeqdesc_CI& CSeqdesc_CI::operator= (const CSeqdesc_CI& iter)
{
    if (this != &iter) {
        m_Choice    = iter.m_Choice;
        m_Entry     = iter.m_Entry;
        m_Desc_CI   = iter.m_Desc_CI;
        m_Ref       = iter.m_Ref;
        m_HaveTitle = iter.m_HaveTitle;
        m_Depth     = iter.m_Depth;
    }
    _ASSERT(x_Valid());
    return *this;
}


void CSeqdesc_CI::x_AddChoice(CSeqdesc::E_Choice choice)
{
    if ( choice != CSeqdesc::e_not_set ) {
        TDescTypeMask mask = 1 << choice;
        _ASSERT(mask);
        m_Choice |= (mask);
    }
    else {
        // set all bits
        m_Choice |= ~0;
    }
}


void CSeqdesc_CI::x_RemoveChoice(CSeqdesc::E_Choice choice)
{
    if ( choice != CSeqdesc::e_not_set ) {
        TDescTypeMask mask = 1 << choice;
        _ASSERT(mask);
        m_Choice &= ~(mask);
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
    if ( m_Ref ) {
        if ( m_Entry  &&  x_Valid() ) {
            if ((*m_Desc_CI)->Which() == CSeqdesc::e_Title) {
                m_HaveTitle = true;
            }
        }
        if ( !m_Entry ) {
            // Ignore pointed-to title if the ref contains one.
            if ( m_HaveTitle ) {
                x_RemoveChoice(CSeqdesc::e_Title);
            }
            m_HaveTitle = false;
            // Ignore pointed-to source.
            x_RemoveChoice(CSeqdesc::e_Source);
            CBioseq_Handle ref_handle = m_Ref;
            x_CheckRef(ref_handle);
            x_SetEntry(CSeq_descr_CI(ref_handle, m_Depth));
        }
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
