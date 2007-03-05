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

File name: dist.cpp

Author: Jason Papadopoulos

Contents: Implementation of CDistances

******************************************************************************/

#include <ncbi_pch.hpp>
#include <algo/cobalt/dist.hpp>

/// @file dist.cpp
/// Implementation of CDistances

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Update the extent of a sequence that figures into its self
/// score, based upon the sequence ranges of a pairwise alignment
/// @param seq1 The current sequence range [in/out]
/// @param align1 The range of the current alignment on sequence 1
/// @param seq1_length Length of sequence 1
/// @param align2 The range of the current alignment on sequence 2
/// @param seq2_length Length of sequence 2
///
static void
x_UpdateRanges(TRange& seq1,
               TRange align1,
               int seq1_length,
               TRange align2,
               int seq2_length)
{
    // extend the alignment bounds to the left and right
    // until one sequence runs out.

    TOffset first = min(seq1.GetFrom(), align1.GetFrom() - align2.GetFrom());
    first = max(first, 0);

    TOffset second = max(seq1.GetTo(), 
                      align1.GetTo() + (seq2_length - 1 - align2.GetTo()));
    second = min(second, seq1_length - 1);

    seq1.Set(first, second);
}

void
CDistances::x_GetSelfScores(vector<CSequence>& query_data, 
                            CHitList& hitlist,
                            SNCBIFullScoreMatrix& score_matrix,
                            vector<double>& self_score,
                            Blast_KarlinBlk& karlin_blk)
{
    // Fill in the self scores for each sequence

    int num_queries = query_data.size();
    vector<TRange> seq_ranges(num_queries);

    // initialize the range of each sequence to 
    // an impossible value

    for (int i = 0; i < num_queries; i++) {
        self_score[i] = 0.0;
        seq_ranges[i].SetEmpty();
    }

    // for each alignment between two sequences, extend
    // the range of the alignment to left and right until
    // one sequence or the other runs out

    for (int i = 0; i < hitlist.Size(); i++) {
        CHit *hit = hitlist.GetHit(i);
        int seq1 = hit->m_SeqIndex1;
        int seq2 = hit->m_SeqIndex2;
        TRange& range1 = seq_ranges[seq1];
        TRange& range2 = seq_ranges[seq2];

        x_UpdateRanges(range1, 
                    hit->m_SeqRange1, query_data[seq1].GetLength(),
                    hit->m_SeqRange2, query_data[seq2].GetLength());
        x_UpdateRanges(range2, 
                    hit->m_SeqRange2, query_data[seq2].GetLength(),
                    hit->m_SeqRange1, query_data[seq1].GetLength());
    }

    // for each sequence

    for (int i = 0; i < num_queries; i++) {

        int score = 0;
        TRange& range = seq_ranges[i];

        // If there are no hits to limit the self-score computation,
        // sum the entire query sequence

        if (range.Empty())
            range.Set(0, query_data[i].GetLength() - 1);

        // otherwise, sum the portion of the sequence needed to
        // envelop all other sequences that align to it. If all
        // sequences are about the same size then this is likely to
        // just sum all sequences in their entirety. However, if there
        // is a big disparity in sequence lengths, only the sequence
        // ranges that an aligner would concentrate on will contribute
        // to the self score for that sequence

        for (int j = range.GetFrom(); j <= range.GetTo(); j++) {
            unsigned char c = query_data[i].GetLetter(j);
            score += score_matrix.s[c][c];
        }
        self_score[i] = karlin_blk.Lambda * score - karlin_blk.logK;
    }

#if 0
    //---------------------
    printf("Self scores:\n");
    for (int i = 0; i < num_queries; i++) {
        printf("query %d(%d): range %d-%d score %lf\n", i, 
               query_data[i].GetLength(),
               seq_ranges[i].GetFrom(), seq_ranges[i].GetTo(), 
               self_score[i]);
    }
    printf("\n\n");
    //---------------------
#endif
}


void
CDistances::ComputeMatrix(vector<CSequence>& query_data,
                          CHitList& hitlist,
                          SNCBIFullScoreMatrix& score_matrix,
                          Blast_KarlinBlk& karlin_blk)
{
    int num_queries = query_data.size();
    vector<double> self_score(num_queries);

    // Get the self score of each sequence

    x_GetSelfScores(query_data, hitlist, score_matrix, 
                    self_score, karlin_blk);

    // All sequences start out equally far from each other

    m_Matrix.Resize(num_queries, num_queries);
    m_Matrix.Set(1.0);

    // Every pairwise alignment pulls the two sequences involved
    // in the alignment closer together. This formulation of
    // pairwise distances amounts to calculating the average
    // per-letter distance between the two sequences. See 
    //
    // Clarke et al, "Inferring Genome Trees by Using a Filter 
    // to Eliminate Phylogenetically Discordant Sequences and 
    // a Distance Matrix Based on Mean Normalized BLASTP Scores", 
    // Jour. Bacteriology Apr. 2002 pp 2072-2080

    for (int i = 0; i < hitlist.Size(); i++) {
        CHit *hit = hitlist.GetHit(i);
        int j = hit->m_SeqIndex1;
        int k = hit->m_SeqIndex2;
        double align_score = karlin_blk.Lambda * 
                                hit->m_Score - karlin_blk.logK;

        _ASSERT(j < k);
        m_Matrix(j,k) -= 0.5 * align_score * 
                     (1.0 / self_score[j] + 1.0 / self_score[k]);
    }

    // Force distance matrix to be symmetric

    for (int i = 0; i < num_queries; i++) {
        m_Matrix(i, i) = 0.0;
        for (int j = 0; j < i; j++) {
            // clamp to zero any distances that are too small
            if (fabs(m_Matrix(j, i)) < 1e-6)
                m_Matrix(j, i) = 0;

            m_Matrix(i, j) = m_Matrix(j, i);
        }
    }
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
