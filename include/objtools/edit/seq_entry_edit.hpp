#ifndef OBJTOOLS_EDIT___SEQ_ENTRY_EDIT__HPP
#define OBJTOOLS_EDIT___SEQ_ENTRY_EDIT__HPP

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
* Author: Mati Shomrat, NCBI
*
* File Description:
*   High level Seq-entry edit, for meaningful combination of Seq-entries.
*/
#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/Seq_feat.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry_Handle;
class CBioseq_Handle;
class CBioseq_set_Handle;
class CSeq_annot_Handle;


BEGIN_SCOPE(edit)


/// Attach one Seq-entry to another
///
/// @param to
///   Seq-entry to change
/// @param add
///   Seq-entry to add
/// @sa
///   Other forms of adding the content of one Seq-entry to another.
NCBI_XOBJEDIT_EXPORT
void AddSeqEntryToSeqEntry(const CSeq_entry_Handle& to, const CSeq_entry_Handle& add);

/// Attach one Bioseq to another
///
/// This function will add one Bioseq to another if:
///   1. 'to' is a nucleotide and 'add' is a protein. The result
///      is a nuc-prot set contating both elements.
///   2. both Bioseqs have the same molecular type. The result
///      is a segmented bioseq of which the two bioseqs serve as parts.
///   3. 'to' is a segmented bioseq and 'add' has the same molecular type
///      as 'to'. 'add' will be added as a new part of 'to'.
/// @param to
///   Bioseq to change
/// @param add
///   Bioseq to add
/// @sa
///   AddSeqEntryToSeqEntry()
NCBI_XOBJEDIT_EXPORT
void AddBioseqToBioseq(const CBioseq_Handle& to, const CBioseq_Handle& add);

/// Add a Bioseq to a Bioseq-set
///
/// This function will add the Bioseq to the set if:
///   1. The set is of class 'parts' and the Bioseq has the same
///      molecular type as the other parts.
///   2. 
/// @param bsst
///   Bioseq to change
/// @param seq
///   Bioseq to add
/// @sa
///   AddSeqEntryToSeqEntry()
NCBI_XOBJEDIT_EXPORT
void AddBioseqToBioseqSet(const CBioseq_set_Handle& bsst, const CBioseq_Handle& seq);


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2004/06/16 15:19:08  shomrat
* Initial Revision
*
*
* ===========================================================================
*/

#endif  /* OBJTOOLS_EDIT___SEQ_ENTRY_EDIT__HPP */
