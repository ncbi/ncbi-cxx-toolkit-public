#ifndef OBJTOOLS_ALNMGR___ALN_GENERATORS__HPP
#define OBJTOOLS_ALNMGR___ALN_GENERATORS__HPP
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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment generators
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqalign/Seq_align.hpp>

#include <objtools/alnmgr/pairwise_aln.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/// Convert CAnchoredAln to seq-align of the selected type.
NCBI_XALNMGR_EXPORT
CRef<CSeq_align>
CreateSeqAlignFromAnchoredAln(const CAnchoredAln& anchored_aln,
                              CSeq_align::TSegs::E_Choice choice,
                              CScope* scope = NULL);


/// Convert CPairwiseAln to seq-align of the selected type.
NCBI_XALNMGR_EXPORT
CRef<CSeq_align>
CreateSeqAlignFromPairwiseAln(const CPairwiseAln& pairwise_aln,
                              CSeq_align::TSegs::E_Choice choice,
                              CScope* scope = NULL);


NCBI_XALNMGR_EXPORT
void
CreateDense_diagFromAnchoredAln(CSeq_align::TSegs::TDendiag& dd,
                                const CAnchoredAln& anchored_aln,
                                CScope* scope = NULL);

NCBI_XALNMGR_EXPORT
CRef<CDense_seg>
CreateDensegFromAnchoredAln(const CAnchoredAln& anchored_aln,
                            CScope* scope = NULL);

NCBI_XALNMGR_EXPORT
CRef<CSeq_align_set>
CreateAlignSetFromAnchoredAln(const CAnchoredAln& anchored_aln,
                              CScope* scope = NULL);

NCBI_XALNMGR_EXPORT
CRef<CPacked_seg>
CreatePackedsegFromAnchoredAln(const CAnchoredAln& anchored_aln,
                               CScope* scope = NULL);

NCBI_XALNMGR_EXPORT
CRef<CSpliced_seg>
CreateSplicedsegFromAnchoredAln(const CAnchoredAln& anchored_aln,
                                CScope* scope = NULL);

NCBI_XALNMGR_EXPORT
CRef<CDense_seg>
CreateDensegFromPairwiseAln(const CPairwiseAln& pairwise_aln,
                            CScope* scope = NULL);

NCBI_XALNMGR_EXPORT
CRef<CSeq_align_set>
CreateAlignSetFromPairwiseAln(const CPairwiseAln& pairwise_aln,
                              CScope* scope = NULL);

NCBI_XALNMGR_EXPORT
CRef<CPacked_seg>
CreatePackedsegFromPairwiseAln(const CPairwiseAln& pairwise_aln,
                               CScope* scope = NULL);

NCBI_XALNMGR_EXPORT
CRef<CSpliced_seg>
CreateSplicedsegFromPairwiseAln(const CPairwiseAln& pairwise_aln,
                                CScope* scope = NULL);


/// Create seq-align from each of the pairwise alignments vs the selected
/// anchor row. Each pairwise alignment's second sequence is aligned to the
/// anchor pairwise alignment's second sequence. The output alignments contain
/// only aligned segments, no gaps are included.
/// @param pariwises
///   Input vector of CPairwiseAln.
/// @param anchor
///   Index of the pairwise alignment to be used as the anchor.
/// @param out_seqaligns
///   Output vector of seq-aligns. Number of objects put in the vector
///   is size of the input vector less one (the anchor is not aligned to
///   itself).
/// @param choice
///   Type of the output seq-aligns.
/// @param scope
///   Optional scope (not required to build most seq-align types).
NCBI_XALNMGR_EXPORT
void CreateSeqAlignFromEachPairwiseAln(
    const CAnchoredAln::TPairwiseAlnVector pairwises,
    CAnchoredAln::TDim                     anchor,
    vector<CRef<CSeq_align> >&             out_seqaligns,
    CSeq_align::TSegs::E_Choice            choice,
    CScope*                                scope = NULL);


/// Convert source alignment to a new type. For spliced-segs the anchor_row
/// is used as product row.
NCBI_XALNMGR_EXPORT
CRef<CSeq_align>
ConvertSeq_align(const CSeq_align& src,
                 CSeq_align::TSegs::E_Choice dst_choice,
                 CSeq_align::TDim anchor_row = -1,
                 CScope* scope = NULL);


END_NCBI_SCOPE


#endif  // OBJTOOLS_ALNMGR___ALN_GENERATORS__HPP
