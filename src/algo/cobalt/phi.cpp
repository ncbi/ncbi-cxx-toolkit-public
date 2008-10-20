static char const rcsid[] = "$Id$";

/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: phi.cpp

Author: Jason Papadopoulos

Contents: Match PHI patterns against a list of sequences

******************************************************************************/

#include <ncbi_pch.hpp>
#include <algo/blast/core/pattern.h>
#include <algo/blast/core/phi_lookup.h>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/cobalt/cobalt.hpp>

/// @file phi.cpp
/// Match PHI patterns against a list of sequences

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Intermediate representation of a pattern hit
typedef struct SPatternHit {
    int query_idx;       ///< query sequence containing the hit
    TRange hit;          ///< range on query sequence where hit occurs

    /// constructor
    /// @param seq Sequence
    /// @param range Offset range of hit
    ///
    SPatternHit(int seq, TRange range)
        : query_idx(seq), hit(range) {}
} SPatternHit;

void
CMultiAligner::FindPatternHits()
{
    size_t num_queries = m_QueryData.size();

    const vector<CMultiAlignerOptions::CPattern>& patterns
         = m_Options->GetCddPatterns();

    if (patterns.size() == 0) {
        return;
    }

    // empty out existing list

    m_PatternHits.PurgeAllHits();

    BlastScoreBlk *sbp = BlastScoreBlkNew(BLASTAA_SEQ_CODE, 1);
    SPHIPatternSearchBlk *phi_pattern;
    Int4 hit_offsets[PHI_MAX_HIT];

    // for each pattern

    for (size_t i=0;i < patterns.size();i++) {

        vector<SPatternHit> phi_hits;

        // precompile the pattern in preparation for running
        // all sequences through it

        char* pattern = (char*)patterns[i].AsPointer();
        _ASSERT(pattern);

        SPHIPatternSearchBlkNew(pattern, FALSE, sbp, &phi_pattern, NULL);
        _ASSERT(phi_pattern != NULL);

        // for each sequence

        for (size_t j = 0; j < num_queries; j++) {

            // scan the sequence through the compiled pattern,
            // saving any hits found

            Int4 twice_num_hits = ::FindPatternHits(hit_offsets, 
                                  (const Uint1 *)(m_QueryData[j].GetSequence()),
                                  m_QueryData[j].GetLength(),
                                  FALSE, phi_pattern);
            for (size_t k = 0; k < (size_t)twice_num_hits; k += 2) {
                phi_hits.push_back(SPatternHit(j, TRange(hit_offsets[k+1],
                                                         hit_offsets[k])));
            }
        }

        // for each hit to the same pattern by different sequences,
        // create a pairwise alignment. Temporarily hijack the score
        // of the alignment to store the identity of the pattern

        for (int j = 0; j < (int)phi_hits.size() - 1; j++) {
            for (int k = j + 1; k < (int)phi_hits.size(); k++) {
                if (phi_hits[j].query_idx != phi_hits[k].query_idx) {

                    m_PatternHits.AddToHitList(new CHit(phi_hits[j].query_idx,
                                                    phi_hits[k].query_idx,
                                                    phi_hits[j].hit,
                                                    phi_hits[k].hit,
                                                    i, CEditScript()));
                }
            }
        }

        // clean up the current pattern

        phi_pattern = SPHIPatternSearchBlkFree(phi_pattern);
    }

    sbp = BlastScoreBlkFree(sbp);

    //------------------------------------------------
    if (m_Verbose) {
        printf("\n\nPHI Pattern Hits:\n");
        for (int i = 0; i < m_PatternHits.Size(); i++) {
            CHit *hit = m_PatternHits.GetHit(i);
            printf("query %3d %4d - %4d query %3d %4d - %4d pattern %d\n",
                   hit->m_SeqIndex1,
                   hit->m_SeqRange1.GetFrom(), 
                   hit->m_SeqRange1.GetTo(),
                   hit->m_SeqIndex2,
                   hit->m_SeqRange2.GetFrom(), 
                   hit->m_SeqRange2.GetTo(),
                   hit->m_Score);
        }
        printf("\n\n");
    }
    //------------------------------------------------

    for (int i = 0; i < m_PatternHits.Size(); i++) {
        m_PatternHits.GetHit(i)->m_Score = 1;
    }
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
