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

#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/objmgr_exception.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_annot_CI::CSeq_annot_CI(CScope& scope,
                             const CSeq_entry& entry,
                             EFlags flags)
    : m_Scope(&scope),
      m_Flags(flags)
{
    m_Level.m_Seq_entry = m_Scope->m_Impl->x_GetSeq_entry_Info(entry);
    if ( !m_Level.m_Seq_entry ) {
        NCBI_THROW(CAnnotException, eFindFailed,
                   "Can not find seq-entry in the scope");
    }
    if (m_Flags == eSearch_recursive) {
        m_Level.m_Child = m_Level.m_Seq_entry->m_Entries.begin();
    }
    else {
        m_Level.m_Child = m_Level.m_Seq_entry->m_Entries.end();
    }
    m_Annot = m_Level.m_Seq_entry->m_Annots.begin();
    while ( !x_Found() ) {
        x_Next();
    }
}


CSeq_annot_CI& CSeq_annot_CI::operator=(const CSeq_annot_CI& iter)
{
    if (this != &iter) {
        m_Scope = iter.m_Scope;
        m_Flags = iter.m_Flags;
        m_Level_Stack = iter.m_Level_Stack;
        m_Level = iter.m_Level;
        m_Annot = iter.m_Annot;
        m_Value.x_Reset();
    }
    return *this;
}


bool CSeq_annot_CI::x_Found(void) const
{
    // Iterator must be valid or no more annotations should be
    // available
    return (m_Annot != m_Level.m_Seq_entry->m_Annots.end())  ||
        (m_Level_Stack.empty()  &&
        m_Level.m_Child == m_Level.m_Seq_entry->m_Entries.end());
}


void CSeq_annot_CI::x_Next(void)
{
    if (m_Annot != m_Level.m_Seq_entry->m_Annots.end()) {
        ++m_Annot;
        return;
    }
    // No more annots on this level, search the children
    if (m_Level.m_Child == m_Level.m_Seq_entry->m_Entries.end()) {
        if ( !m_Level_Stack.empty() ) {
            m_Level = m_Level_Stack.top();
            m_Level_Stack.pop();
            ++m_Level.m_Child;
            // update annot iterator so that bool() works fine
            m_Annot = m_Level.m_Seq_entry->m_Annots.end();
        }
        return;
    }
    m_Level_Stack.push(m_Level);
    m_Level.m_Seq_entry = *m_Level.m_Child;
    m_Level.m_Child = m_Level.m_Seq_entry->m_Entries.begin();
    m_Annot = m_Level.m_Seq_entry->m_Annots.begin();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2003/09/30 16:22:03  vasilche
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
* Revision 1.3  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.2  2003/07/25 21:41:30  grichenk
* Implemented non-recursive mode for CSeq_annot_CI,
* fixed friend declaration in CSeq_entry_Info.
*
* Revision 1.1  2003/07/25 15:23:42  grichenk
* Initial revision
*
*
* ===========================================================================
*/
