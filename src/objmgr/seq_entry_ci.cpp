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
*    Handle to Seq-entry object
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/seq_entry_ci.hpp>

#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/scope_impl.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>

#include <objects/seqset/Bioseq_set.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_CI
/////////////////////////////////////////////////////////////////////////////


CSeq_entry_CI::CSeq_entry_CI(const CSeq_entry_Handle& entry,
                             ERecursionMode           recursive,
                             CSeq_entry::E_Choice     type_filter)
    : m_Recursive(recursive),
      m_Filter(type_filter)
{
    x_Initialize(entry.GetSet());
}


CSeq_entry_CI::CSeq_entry_CI(const CBioseq_set_Handle& seqset,
                             ERecursionMode            recursive,
                             CSeq_entry::E_Choice      type_filter)
    : m_Recursive(recursive),
      m_Filter(type_filter)
{
    x_Initialize(seqset);
}


CSeq_entry_CI::CSeq_entry_CI(const CSeq_entry_CI& iter)
{
    *this = iter;
}


CSeq_entry_CI& CSeq_entry_CI::operator =(const CSeq_entry_CI& iter)
{
    if (this != &iter) {
        m_Parent = iter.m_Parent;
        m_Iterator = iter.m_Iterator;
        m_Current = iter.m_Current;
        m_Recursive = iter.m_Recursive;
        m_Filter = iter.m_Filter;
        if ( iter.m_SubIt.get() ) {
            m_SubIt.reset(new CSeq_entry_CI(*iter.m_SubIt));
        }
    }
    return *this;
}


void CSeq_entry_CI::x_Initialize(const CBioseq_set_Handle& seqset)
{
    if ( seqset ) {
        m_Parent = seqset;
        m_Iterator = seqset.x_GetInfo().GetSeq_set().begin();
        x_SetCurrentEntry();
        while ((*this)  &&  !x_ValidType()) {
            x_Next();
        }
    }
}


void CSeq_entry_CI::x_SetCurrentEntry(void)
{
    if ( m_Parent && m_Iterator != m_Parent.x_GetInfo().GetSeq_set().end() ) {
        m_Current = CSeq_entry_Handle(**m_Iterator,
                                      m_Parent.GetTSE_Handle());
    }
    else {
        m_Current.Reset();
    }
}


CSeq_entry_CI& CSeq_entry_CI::operator ++(void)
{
    do {
        x_Next();
    }
    while ((*this)  &&  !x_ValidType());
    return *this;
}


bool CSeq_entry_CI::x_ValidType(void) const
{
    _ASSERT(*this);
    switch ( m_Filter ) {
    case CSeq_entry::e_Seq:
        return (*this)->IsSeq();
    case CSeq_entry::e_Set:
        return (*this)->IsSet();
    default:
        break;
    }
    return true;
}


void CSeq_entry_CI::x_Next(void)
{
    if ( !(*this) ) {
        return;
    }

    if (m_Recursive == eRecursive) {
        if ( m_SubIt.get() ) {
            // Already inside sub-entry - try to move forward.
            ++(*m_SubIt);
            if ( *m_SubIt ) {
                return;
            }
            // End of sub-entry, continue to the next entry.
            m_SubIt.reset();
        }
        else if ( m_Current.IsSet() ) {
            // Current entry is a seq-set, iterate sub-entries.
            m_SubIt.reset(new CSeq_entry_CI(m_Current, m_Recursive, m_Filter));
            return;
        }
    }

    ++m_Iterator;
    x_SetCurrentEntry();
}


int CSeq_entry_CI::GetDepth(void) const
{
    return m_SubIt.get() ? m_SubIt->GetDepth() + 1 : 0;
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_I
/////////////////////////////////////////////////////////////////////////////


CSeq_entry_I::CSeq_entry_I(const CSeq_entry_EditHandle& entry,
                           ERecursionMode               recursive,
                           CSeq_entry::E_Choice         type_filter)
    : m_Recursive(recursive),
      m_Filter(type_filter)
{
    x_Initialize(entry.SetSet());
}


CSeq_entry_I::CSeq_entry_I(const CBioseq_set_EditHandle& seqset,
                           ERecursionMode                recursive,
                           CSeq_entry::E_Choice          type_filter)
    : m_Recursive(recursive),
      m_Filter(type_filter)
{
    x_Initialize(seqset);
}


CSeq_entry_I::CSeq_entry_I(const CSeq_entry_I& iter)
{
    *this = iter;
}


CSeq_entry_I& CSeq_entry_I::operator =(const CSeq_entry_I& iter)
{
    if (this != &iter) {
        m_Parent = iter.m_Parent;
        m_Iterator = iter.m_Iterator;
        m_Current = iter.m_Current;
        m_Recursive = iter.m_Recursive;
        m_Filter = iter.m_Filter;
        if ( iter.m_SubIt.get() ) {
            m_SubIt.reset(new CSeq_entry_I(*iter.m_SubIt));
        }
    }
    return *this;
}


void CSeq_entry_I::x_Initialize(const CBioseq_set_EditHandle& seqset)
{
    if ( seqset ) {
        m_Parent = seqset;
        m_Iterator = seqset.x_GetInfo().SetSeq_set().begin();
        x_SetCurrentEntry();
        while ((*this)  &&  !x_ValidType()) {
            x_Next();
        }
    }
}


void CSeq_entry_I::x_SetCurrentEntry(void)
{
    if ( m_Parent && m_Iterator != m_Parent.x_GetInfo().SetSeq_set().end() ) {
        m_Current = CSeq_entry_EditHandle(**m_Iterator,
                                          m_Parent.GetTSE_Handle());
    }
    else {
        m_Current.Reset();
    }
}


CSeq_entry_I& CSeq_entry_I::operator ++(void)
{
    do {
        x_Next();
    }
    while ((*this)  &&  !x_ValidType());
    return *this;
}


bool CSeq_entry_I::x_ValidType(void) const
{
    _ASSERT(*this);
    switch ( m_Filter ) {
    case CSeq_entry::e_Seq:
        return (*this)->IsSeq();
    case CSeq_entry::e_Set:
        return (*this)->IsSet();
    default:
        break;
    }
    return true;
}


void CSeq_entry_I::x_Next(void)
{
    if ( !(*this) ) {
        return;
    }

    if (m_Recursive == eRecursive) {
        if ( m_SubIt.get() ) {
            // Already inside sub-entry - try to move forward.
            ++(*m_SubIt);
            if ( *m_SubIt ) {
                return;
            }
            // End of sub-entry, continue to the next entry.
            m_SubIt.reset();
        }
        else if ( m_Current.IsSet() ) {
            // Current entry is a seq-set, iterate sub-entries.
            m_SubIt.reset(new CSeq_entry_I(m_Current, m_Recursive, m_Filter));
            return;
        }
    }

    ++m_Iterator;
    x_SetCurrentEntry();
}


int CSeq_entry_I::GetDepth(void) const
{
    return m_SubIt.get() ? m_SubIt->GetDepth() + 1 : 0;
}


END_SCOPE(objects)
END_NCBI_SCOPE
