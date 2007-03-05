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
*   Seq-annot iterator
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_annot_CI::CSeq_annot_CI(void)
    : m_UpTree(false)
{
}


CSeq_annot_CI::~CSeq_annot_CI(void)
{
}


CSeq_annot_CI::CSeq_annot_CI(const CSeq_annot_CI& iter)
    : m_UpTree(false)
{
    *this = iter;
}


CSeq_annot_CI& CSeq_annot_CI::operator=(const CSeq_annot_CI& iter)
{
    if (this != &iter) {
        m_CurrentEntry = iter.m_CurrentEntry;
        m_AnnotIter = iter.m_AnnotIter;
        m_CurrentAnnot = iter.m_CurrentAnnot;
        m_EntryStack = iter.m_EntryStack;
        m_UpTree = iter.m_UpTree;
    }
    return *this;
}


CSeq_annot_CI::CSeq_annot_CI(CScope& scope, const CSeq_entry& entry,
                             EFlags flags)
    : m_UpTree(false)
{
    x_Initialize(scope.GetSeq_entryHandle(entry), flags);
}


CSeq_annot_CI::CSeq_annot_CI(const CSeq_entry_Handle& entry, EFlags flags)
    : m_UpTree(false)
{
    x_Initialize(entry, flags);
}


CSeq_annot_CI::CSeq_annot_CI(const CBioseq_Handle& bioseq)
    : m_UpTree(true)
{
    x_Initialize(bioseq.GetParentEntry(), eSearch_entry);
}


CSeq_annot_CI::CSeq_annot_CI(const CBioseq_set_Handle& bioseq_set,
                             EFlags flags)
    : m_UpTree(false)
{
    x_Initialize(bioseq_set.GetParentEntry(), flags);
}


inline
void CSeq_annot_CI::x_Push(void)
{
    if ( m_CurrentEntry.IsSet() ) {
        m_EntryStack.push(CSeq_entry_CI(m_CurrentEntry));
    }
}


inline
const CSeq_annot_CI::TAnnots& CSeq_annot_CI::x_GetAnnots(void) const
{
    return m_CurrentEntry.x_GetInfo().m_Contents->GetAnnot();
}


inline
void CSeq_annot_CI::x_SetEntry(const CSeq_entry_Handle& entry)
{
    m_CurrentEntry = entry;
    if ( !m_CurrentEntry ) {
        m_CurrentAnnot.Reset();
        return;
    }
    m_AnnotIter = x_GetAnnots().begin();
    if ( !m_EntryStack.empty() ) {
        x_Push();
    }
}


void CSeq_annot_CI::x_Initialize(const CSeq_entry_Handle& entry, EFlags flags)
{
    if ( !entry ) {
        NCBI_THROW(CAnnotException, eFindFailed,
                   "Can not find seq-entry in the scope");
    }

    x_SetEntry(entry);
    _ASSERT(m_CurrentEntry);
    if ( flags == eSearch_recursive ) {
        x_Push();
    }
    
    x_Settle();
}


CSeq_annot_CI& CSeq_annot_CI::operator++(void)
{
    _ASSERT(m_CurrentEntry);
    _ASSERT(m_AnnotIter != x_GetAnnots().end());
    ++m_AnnotIter;
    x_Settle();
    return *this;
}


void CSeq_annot_CI::x_Settle(void)
{
    _ASSERT(m_CurrentEntry);
    if ( m_AnnotIter == x_GetAnnots().end() ) {
        if ( m_UpTree ) {
            // Iterating from a bioseq up to its TSE
            do {
                x_SetEntry(m_CurrentEntry.GetParentEntry());
            } while ( m_CurrentEntry && m_AnnotIter == x_GetAnnots().end() );
        }
        else {
            for (;;) {
                if ( m_EntryStack.empty() ) {
                    m_CurrentEntry.Reset();
                    break;
                }
                CSeq_entry_CI& entry_iter = m_EntryStack.top();
                if ( entry_iter ) {
                    CSeq_entry_Handle sub_entry = *entry_iter;
                    ++entry_iter;
                    x_SetEntry(sub_entry);
                    _ASSERT(m_CurrentEntry);
                    if ( m_AnnotIter != x_GetAnnots().end() ) {
                        break;
                    }
                }
                else {
                    m_EntryStack.pop();
                }
            }
        }
    }
    if ( m_CurrentEntry ) {
        _ASSERT(m_AnnotIter != x_GetAnnots().end());
        m_CurrentAnnot = CSeq_annot_Handle(**m_AnnotIter,
                                           m_CurrentEntry.GetTSE_Handle());
    }
    else {
        m_CurrentAnnot.Reset();
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
