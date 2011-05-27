#ifndef OBJTOOLS_FORMAT___GATHER_ITER__HPP
#define OBJTOOLS_FORMAT___GATHER_ITER__HPP

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
* Author:  Michael Kornbluh
*
* File Description:
*   This returns, in order, the bioseq_handles that will be gathered for
*   formatting.
*
*/

#include <corelib/ncbiobj.hpp>

#include <objmgr/seq_entry_ci.hpp>

#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CBioseq_CI;
class CBioseq_Handle;
class CFlatFileConfig;
class CSeq_entry_CI;
class CSeq_entry_Handle;

class CGather_Iter : public CObject {
public:

    CGather_Iter( const CSeq_entry_Handle& top_seq_entry, 
        const CFlatFileConfig& config );

    // standard methods needed to act like an iterator.
    // It does a depth-first search over sub-Seq-entries and
    // bioseqs (where bioseqs are always leaves and always after
    // Seq-entries are iterated for a Seq-entry )
    operator bool(void) const;
    CGather_Iter &operator++(void);
    const CBioseq_Handle &operator*(void) const;

private:
    // We recursively dive into Seq-entries, and, at the bottom,
    // traverse through the bioseqs of a Seq-entry.
    // m_SeqEntryIterStack is used like a stack, with the top-level
    // Seq-entries iterator at the beginning.
    //
    // invariant: between public invocations, all iterators
    // on the stack or in m_BioseqIter are guaranteed to
    // point to a valid object, unless this iterator has reached its end.
    // Also, if m_SeqEntryIterStack
    // is non-empty, m_BioseqIter is guaranteed to be valid.
    std::vector< CSeq_entry_CI > m_SeqEntryIterStack;
    auto_ptr<CBioseq_CI> m_BioseqIter;

    const CFlatFileConfig& m_Config;

    // adds all CSeq_entry_CI to m_SeqEntryIterStack as well as
    // a bottommost m_BioseqIter.
    // This is assuming it's even possible, of course.
    // Returns true if it was successful.
    bool x_AddSeqEntryToStack( const CSeq_entry_Handle& seq_entry );

    // Determines: Can we use this bioseq?
    bool x_IsBioseqHandleOkay( const CBioseq_Handle &bioseq );
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* OBJTOOLS_FORMAT___GATHER_INFO__HPP */
