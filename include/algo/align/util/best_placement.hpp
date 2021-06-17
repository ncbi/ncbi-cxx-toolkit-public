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
 * Authors:  Alex Astashyn
 *
 * File Description:
 *
 */

#pragma once
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <functional>

BEGIN_NCBI_SCOPE

class NCBI_XALGOALIGN_EXPORT NBestPlacement
{
public:
    NBestPlacement() = delete;
   ~NBestPlacement() = delete;

    using score_fn_t = std::function<int(const objects::CSeq_align&)>;

    /// Adds the following scores:
    ///    `best_placement_score` as computed by score_fn
    ///    `rank` - all top scoring alignments will have rank=1
    ///    `rank1_index`, `rank1_count` if more than one top-scoring alignment.
    ///
    /// Input seq-align-set shall contain alignments for the same query
    static void Rank(
        objects::CSeq_align_set& sas,
                      score_fn_t score_fn = &NBestPlacement::GetScore);

    static int GetScore(const objects::CSeq_align& aln);
};

END_NCBI_SCOPE
