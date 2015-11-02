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
                             TFlags flags,
                             CSeq_entry::E_Choice     type_filter )
    : m_Flags(flags),
      m_Filter(type_filter)
{
    if ( m_Flags & fIncludeGivenEntry ) {
        m_Current = entry;
        while ((*this)  &&  !x_ValidType()) {
            x_Next();
        }
    } else {
        if ( entry.IsSet() ) {
            x_Initialize( entry.GetSet() );
        }
    }
}


CSeq_entry_CI::CSeq_entry_CI(const CBioseq_set_Handle& seqset,
                             TFlags flags,
                             CSeq_entry::E_Choice      type_filter )
    : m_Flags(flags),
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
        m_Index = iter.m_Index;
        m_Current = iter.m_Current;
        m_Flags    = iter.m_Flags;
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
        m_Index = 0;
        x_SetCurrentEntry();
        while ((*this)  &&  !x_ValidType()) {
            x_Next();
        }
    }
}


void CSeq_entry_CI::x_SetCurrentEntry(void)
{
    if ( m_Parent ) {
        const vector< CRef<CSeq_entry_Info> >& entries =
            m_Parent.x_GetInfo().GetSeq_set();
        if ( m_Index < entries.size() ) {
            m_Current = CSeq_entry_Handle(*entries[m_Index],
                                          m_Parent.GetTSE_Handle());
            return;
        }
    }
    m_Current.Reset();
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

    if ( m_SubIt.get() ) {
        // There should not even be a m_SubIt unless we're
        // in recursive mode
        _ASSERT(m_Flags & fRecursive);
        // m_SubIt implies m_Parent
        _ASSERT( m_Parent );
        // Already inside sub-entry - try to move forward.
        ++(*m_SubIt);
        if ( *m_SubIt ) {
            return;
        }
        // End of sub-entry, continue to the next entry.
        m_SubIt.reset();
    }
    else if ( m_Current.IsSet() ) {
        // don't set up m_SubIt unless parent is already set up
        if( ! m_Parent ) {
            x_Initialize(m_Current.GetSet());
            return;
        } else if( m_Flags & fRecursive ) {
            // Current entry is a seq-set and we're in recursive mode,
            // so iterate sub-entries.
            m_SubIt.reset(new CSeq_entry_CI(
                m_Current, 
                (m_Flags & ~fIncludeGivenEntry), // some flags don't get passed down recursively
                m_Filter ));
            if ( *m_SubIt ) {
                return;
            }
            // Empty sub-entry, continue to the next entry.
            m_SubIt.reset();
        }
    }

    // "if m_Parent" is really just a proxy for seeing if m_Index is valid,
    // otherwise incrementing is undefined
    if( m_Parent ) {
        ++m_Index;
    }
    x_SetCurrentEntry();
}


int CSeq_entry_CI::GetDepth(void) const
{
    // first, calculate depth assuming the "given" entry has
    // a depth of zero
    const int iDepthFromGiven = 
        ( m_SubIt.get() ?
            2 + m_SubIt->GetDepth() :
            (  m_Parent ? 1 : 0 )
        );

    if( (m_Flags & fIncludeGivenEntry) ) {
        return iDepthFromGiven;
    } else {
        // if given entry is not supposed to be included in the iteration,
        // it's children get a depth of zero, so we subtract by 1.
        _ASSERT( iDepthFromGiven > 0 );
        return (iDepthFromGiven - 1);
    }
}


/////////////////////////////////////////////////////////////////////////////
// CSeq_entry_I
/////////////////////////////////////////////////////////////////////////////


CSeq_entry_I::CSeq_entry_I(const CSeq_entry_EditHandle& entry,
                           TFlags flags,
                           CSeq_entry::E_Choice         type_filter)
    : CSeq_entry_CI(entry, flags, type_filter)
{
}


CSeq_entry_I::CSeq_entry_I(const CBioseq_set_EditHandle& seqset,
                           TFlags flags,
                           CSeq_entry::E_Choice          type_filter)
    : CSeq_entry_CI(seqset, flags, type_filter)
{
}


CSeq_entry_I::CSeq_entry_I(const CSeq_entry_I& iter)
{
    *this = iter;
}


CSeq_entry_I& CSeq_entry_I::operator =(const CSeq_entry_I& iter)
{
    CSeq_entry_CI::operator=(iter);
    return *this;
}


END_SCOPE(objects)
END_NCBI_SCOPE
