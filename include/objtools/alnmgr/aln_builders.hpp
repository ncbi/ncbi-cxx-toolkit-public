#ifndef OBJTOOLS_ALNMGR___ALN_BUILDERS__HPP
#define OBJTOOLS_ALNMGR___ALN_BUILDERS__HPP
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
* Authors:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment builders
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objtools/alnmgr/pairwise_aln.hpp>
#include <objtools/alnmgr/aln_stats.hpp>
#include <objtools/alnmgr/aln_user_options.hpp>


BEGIN_NCBI_SCOPE


/// Merge two pariwise alignments.
/// @param existing
///   Source and destination pairwise alignment.
/// @param addition
///   Alignment to be added to to first one.
/// @param flags
///   Merge options.
NCBI_XALNMGR_EXPORT
void MergePairwiseAlns(CPairwiseAln& existing,
                       const CPairwiseAln& addition,
                       const CAlnUserOptions::TMergeFlags& flags);


/// Sort anchored alignments by score.
/// @sa PScoreGreater
NCBI_XALNMGR_EXPORT
void SortAnchoredAlnVecByScore(TAnchoredAlnVec& anchored_aln_vec);


/// Build anchored alignment from a set of alignmnets. The input alignments
/// are merged (according to the options), a new pseudo-sequence is used to
/// provide alignment coordinates.
/// @param in_alns
///   Input Alignments (will be sorted by score unless fSkipSortByScore
///   is raised).
/// @param out_aln
///   Ouput anchored alignment.
/// @param options
///   Options for merging input alignments.
/// @param pseudo_seqid
///   User-created seq-id of the pseudo alignment sequence. If null, a new local,
///   time-stamped seq-id will be automatically generated.
NCBI_XALNMGR_EXPORT
void BuildAln(TAnchoredAlnVec& in_alns,
              CAnchoredAln& out_aln,
              const CAlnUserOptions& options,
              TAlnSeqIdIRef pseudo_seqid = TAlnSeqIdIRef());


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_BUILDERS__HPP
