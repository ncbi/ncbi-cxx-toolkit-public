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
                             double blastp_evalue,
                             double conserved_cutoff,
                             double filler_res_boost)
    : m_BlastpEvalue(blastp_evalue),
      m_LocalResFreqBoost(filler_res_boost),
      m_ConservedCutoff(conserved_cutoff),
      m_GapOpen(gap_open),
      m_GapExtend(gap_extend),
      m_EndGapOpen(end_gap_open),
      m_EndGapExtend(end_gap_extend),
      m_Verbose(false)
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
    Reset();
    m_tQueries = queries;
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
    // convert the current collection of pairwise
    // hits into a distance matrix

    CDistances distances(m_QueryData, 
                         m_CombinedHits, 
                         m_Aligner.GetMatrix());

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

    m_Tree.ComputeTree(distances.GetMatrix());

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

/*-----------------------------------------------------------------------
  $Log$
  Revision 1.4  2005/11/18 20:17:34  papadopo
  remove handling of unneeded Karlin block

  Revision 1.3  2005/11/17 22:30:35  papadopo
  add a few comments

  Revision 1.2  2005/11/08 17:52:17  papadopo
  1. do not assume blast namespace
  2. move seqalign output to another file
  3. minor cleanup

  Revision 1.1  2005/11/07 18:14:00  papadopo
  Initial revision

-----------------------------------------------------------------------*/
