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

File name: cobalt.cpp

Author: Jason Papadopoulos

Contents: Implementation of CMultiAligner class

******************************************************************************/

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/cobalt/cobalt.hpp>

/// @file cobalt.cpp
/// Implementation of the CMultiAligner class

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

CMultiAligner::CMultiAligner(const char *matrix_name,
                             CNWAligner::TScore gap_open,
                             CNWAligner::TScore gap_extend,
                             CNWAligner::TScore end_gap_open,
                             CNWAligner::TScore end_gap_extend,
                             bool iterate,
                             double blastp_evalue,
                             double conserved_cutoff,
                             double filler_res_boost,
                             double pseudocount)
    : m_BlastpEvalue(blastp_evalue),
      m_LocalResFreqBoost(filler_res_boost),
      m_MatrixName(matrix_name),
      m_ConservedCutoff(conserved_cutoff),
      m_Pseudocount(pseudocount),
      m_GapOpen(gap_open),
      m_GapExtend(gap_extend),
      m_EndGapOpen(end_gap_open),
      m_EndGapExtend(end_gap_extend),
      m_Verbose(false),
      m_Iterate(iterate)
{
    SetScoreMatrix(matrix_name);
    m_Aligner.SetWg(m_GapOpen);
    m_Aligner.SetWs(m_GapExtend);
    m_Aligner.SetStartWg(m_EndGapOpen);
    m_Aligner.SetStartWs(m_EndGapExtend);
    m_Aligner.SetEndWg(m_EndGapOpen);
    m_Aligner.SetEndWs(m_EndGapExtend);
}


CMultiAligner::~CMultiAligner()
{
}


void 
CMultiAligner::SetQueries(const blast::TSeqLocVector& queries)
{
    if (queries.size() < 2) {
        NCBI_THROW(CMultiAlignerException, eInvalidInput,
                   "Aligner requires at least two input sequences");
    }

    Reset();
    m_tQueries = queries;
    m_QueryData.clear();
    ITERATE(blast::TSeqLocVector, itr, queries) {
        m_QueryData.push_back(CSequence(*itr));
    }
}


void
CMultiAligner::SetScoreMatrix(const char *matrix_name)
{
    if (strcmp(matrix_name, "BLOSUM62") == 0)
        m_Aligner.SetScoreMatrix(&NCBISM_Blosum62);
    else if (strcmp(matrix_name, "BLOSUM45") == 0)
        m_Aligner.SetScoreMatrix(&NCBISM_Blosum45);
    else if (strcmp(matrix_name, "BLOSUM80") == 0)
        m_Aligner.SetScoreMatrix(&NCBISM_Blosum80);
    else if (strcmp(matrix_name, "PAM30") == 0)
        m_Aligner.SetScoreMatrix(&NCBISM_Pam30);
    else if (strcmp(matrix_name, "PAM70") == 0)
        m_Aligner.SetScoreMatrix(&NCBISM_Pam70);
    else if (strcmp(matrix_name, "PAM250") == 0)
        m_Aligner.SetScoreMatrix(&NCBISM_Pam250);
    else
        NCBI_THROW(CMultiAlignerException, eInvalidScoreMatrix,
                   "Unsupported score matrix");
    
    m_MatrixName = matrix_name;
}

void 
CMultiAligner::SetUserHits(CHitList& hits)
{
    for (int i = 0; i < hits.Size(); i++) {
        CHit *hit = hits.GetHit(i);
        if (hit->m_SeqIndex1 < 0 || hit->m_SeqIndex2 < 0 ||
            hit->m_SeqIndex1 >= (int)m_QueryData.size() ||
            hit->m_SeqIndex2 >= (int)m_QueryData.size()) {
            NCBI_THROW(CMultiAlignerException, eInvalidInput,
                        "Sequence specified by constraint is out of range");
        }
        int index1 = hit->m_SeqIndex1;
        int index2 = hit->m_SeqIndex2;

        if ((hit->m_SeqRange1.GetLength() == 1 &&
             hit->m_SeqRange2.GetLength() != 1) ||
            (hit->m_SeqRange1.GetLength() != 1 &&
             hit->m_SeqRange2.GetLength() == 1)) {
            NCBI_THROW(CMultiAlignerException, eInvalidInput,
                        "Range specified by constraint is degenerate");
        }
        int from1 = hit->m_SeqRange1.GetFrom();
        int from2 = hit->m_SeqRange2.GetFrom();
        int to1 = hit->m_SeqRange1.GetTo();
        int to2 = hit->m_SeqRange2.GetTo();

        if (from1 > to1 || from2 > to2) {
            NCBI_THROW(CMultiAlignerException, eInvalidInput,
                        "Range specified by constraint is invalid");
        }
        if (from1 >= m_QueryData[index1].GetLength() ||
            to1 >= m_QueryData[index1].GetLength() ||
            from2 >= m_QueryData[index2].GetLength() ||
            to2 >= m_QueryData[index2].GetLength()) {
            NCBI_THROW(CMultiAlignerException, eInvalidInput,
                        "Constraint is out of range");
        }
    }

    m_UserHits.PurgeAllHits();
    m_UserHits.Append(hits);
    for (int i = 0; i < m_UserHits.Size(); i++)
        m_UserHits.GetHit(i)->m_Score = 1000000;
}

void
CMultiAligner::Reset()
{
    m_Results.clear();
    m_DomainHits.PurgeAllHits();
    m_LocalHits.PurgeAllHits();
    m_PatternHits.PurgeAllHits();
    m_CombinedHits.PurgeAllHits();
    m_UserHits.PurgeAllHits();
}


void 
CMultiAligner::ComputeTree()
{
    // Convert the current collection of pairwise
    // hits into a distance matrix. This is the only
    // nonlinear operation that involves the scores of
    // alignments, so the raw scores should be converted
    // into bit scores before becoming distances.
    //
    // To do that, we need Karlin parameters for the 
    // score matrix and gap penalties chosen. Since both
    // of those can change independently, calculate the
    // Karlin parameters right now

    Blast_KarlinBlk karlin_blk;
    if (Blast_KarlinBlkGappedLoadFromTables(&karlin_blk, -m_GapOpen,
                                -m_GapExtend, 32768, m_MatrixName) != 0) {
        NCBI_THROW(blast::CBlastException, eInvalidArgument,
                     "Cannot generate Karlin block");
    }

    CDistances distances(m_QueryData, 
                         m_CombinedHits, 
                         m_Aligner.GetMatrix(),
                         karlin_blk);

    //--------------------------------
    if (m_Verbose) {
        const CDistMethods::TMatrix& matrix = distances.GetMatrix();
        printf("distance matrix:\n");
        printf("    ");
        for (int i = matrix.GetCols() - 1; i > 0; i--)
            printf("%5d ", i);
        printf("\n");
    
        for (int i = 0; i < matrix.GetRows() - 1; i++) {
            printf("%2d: ", i);
            for (int j = matrix.GetCols() - 1; j > i; j--) {
                printf("%5.3f ", matrix(i, j));
            }
            printf("\n");
        }
        printf("\n\n");
    }
    //--------------------------------

    // build the phylo tree associated with the matrix

    m_Tree.ComputeTree(distances.GetMatrix(), m_FastMeTree);

    //--------------------------------
    if (m_Verbose) {
        CTree::PrintTree(m_Tree.GetTree());
    }
    //--------------------------------
}

void 
CMultiAligner::Run()
{
    FindDomainHits();
    FindLocalHits();
    FindPatternHits();
    x_FindConsistentHitSubset();
    ComputeTree();
    BuildAlignment();
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
