#ifndef OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
#define OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
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
*   Alignment converters
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/seq_loc_mapper_base.hpp>

#include <objtools/alnmgr/alnexception.hpp>
#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_user_options.hpp>
#include <objtools/alnmgr/aln_stats.hpp>


BEGIN_NCBI_SCOPE


NCBI_XALNMGR_EXPORT
void
ConvertSeqAlignToPairwiseAln(CPairwiseAln& pairwise_aln,      ///< output
                             const objects::CSeq_align& sa,   ///< input Seq-align
                             objects::CSeq_align::TDim row_1, ///< which pair of rows
                             objects::CSeq_align::TDim row_2,
                             CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


NCBI_XALNMGR_EXPORT
void
ConvertDensegToPairwiseAln(CPairwiseAln& pairwise_aln,       ///< output
                           const objects::CDense_seg& ds,    ///< input Dense-seg
                           objects::CSeq_align::TDim row_1,  ///< which pair of rows
                           objects::CSeq_align::TDim row_2,
                           CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


NCBI_XALNMGR_EXPORT
void
ConvertStdsegToPairwiseAln(CPairwiseAln& pairwise_aln,                   ///< output
                           const objects::CSeq_align::TSegs::TStd& stds, ///< input Stds
                           objects::CSeq_align::TDim row_1,              ///< which pair of rows 
                           objects::CSeq_align::TDim row_2,
                           CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


NCBI_XALNMGR_EXPORT
void
ConvertDendiagToPairwiseAln(CPairwiseAln& pairwise_aln,                           ///< output
                            const objects::CSeq_align::TSegs::TDendiag& dendiags, ///< input Dendiags
                            objects::CSeq_align::TDim row_1,                      ///< which pair of rows 
                            objects::CSeq_align::TDim row_2,
                            CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


NCBI_XALNMGR_EXPORT
void
ConvertSparseToPairwiseAln(CPairwiseAln& pairwise_aln,             ///< output
                           const objects::CSparse_seg& sparse_seg, ///< input Sparse-seg
                           objects::CSeq_align::TDim row_1,        ///< which pair of rows 
                           objects::CSeq_align::TDim row_2,
                           CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


NCBI_XALNMGR_EXPORT
void
ConvertSplicedToPairwiseAln(CPairwiseAln& pairwise_aln,               ///< output
                            const objects::CSpliced_seg& spliced_seg, ///< input Splice-seg
                            objects::CSeq_align::TDim row_1,          ///< which pair of rows 
                            objects::CSeq_align::TDim row_2,
                            CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


/// Build a pairwise alignment from a pair of seq-locs.
NCBI_XALNMGR_EXPORT
void
ConvertSeqLocsToPairwiseAln(CPairwiseAln&            aln,      ///< output
                            const objects::CSeq_loc& loc_1,    ///< first seq-loc
                            const objects::CSeq_loc& loc_2,    ///< second seq-loc
                            CAlnUserOptions::EDirection direction = CAlnUserOptions::eBothDirections); ///< which direction


typedef list< CRef<CPairwiseAln> > TPairwiseAlnList;

NCBI_XALNMGR_EXPORT
void SeqLocMapperToPairwiseAligns(const objects::CSeq_loc_Mapper_Base& mapper,
                                  TPairwiseAlnList&                    aligns);


/// Create an anchored alignment from Seq-align using hints
/// Optionally, choose the anchor row explicitly (this overrides options.GetAnchorId())
/// NB: Potentially, this "shrinks" the alignment vertically in case some row was not aligned to the anchor.
NCBI_XALNMGR_EXPORT
CRef<CAnchoredAln> 
CreateAnchoredAlnFromAln(const TAlnStats& aln_stats,      ///< input
                         size_t aln_idx,                  ///< which input alignment
                         const CAlnUserOptions& options,  ///< user options
                         objects::CSeq_align::TDim explicit_anchor_row = -1); ///< optional anchor row


NCBI_XALNMGR_EXPORT
void 
CreateAnchoredAlnVec(TAlnStats& aln_stats,            ///< input
                     TAnchoredAlnVec& out_vec,        ///< output
                     const CAlnUserOptions& options); ///< user options


/// A simple API that assumes that the seq_align has exactly two rows
/// and you want to create a pairwise with the default policy
NCBI_XALNMGR_EXPORT
CRef<CPairwiseAln>
CreatePairwiseAlnFromSeqAlign(const objects::CSeq_align& seq_align);
                                 

END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_CONVERTERS__HPP
