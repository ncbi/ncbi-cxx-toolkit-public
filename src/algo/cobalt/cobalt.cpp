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
      m_Iterate(iterate),
      m_UseClusters(false),
      m_KmerLength(0),
      m_MaxClusterDist(0.0),
      m_ClustDistMeasure(TKMethods::eFractionCommonKmersGlobal)
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
                                -m_GapExtend, m_MatrixName) != 0) {
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
    bool is_cluster_found = false;

    if (m_UseClusters) {
        if ((is_cluster_found = FindQueryClusters())) {
            AlignInClusters();
            // No multiple alignment is done for one cluster
            if (m_Clusterer.GetClusters().size() == 1) {
                return;
            }
        }
    }

    FindDomainHits();
    FindLocalHits();
    FindPatternHits();
    x_FindConsistentHitSubset();
    ComputeTree();
    BuildAlignment();

    if (is_cluster_found) {
        MultiAlignClusters();
    }
}


bool
CMultiAligner::FindQueryClusters()
{

    vector<TKmerCounts> kmer_counts;
    TKMethods::SetParams(m_KmerLength, m_KmerAlphabet);
    TKMethods::ComputeCounts(m_tQueries, kmer_counts);
    auto_ptr<CClusterer::TDistMatrix> dmat
        = TKMethods::ComputeDistMatrix(kmer_counts, m_ClustDistMeasure);

    //-------------------------------------------------------
    if (m_Verbose) {
        printf("K-mer counts distance matrix:\n");
        printf("    ");
        for (size_t i=1;i < m_QueryData.size();i++) {
            printf("%6d", i);
        }
        printf("\n");
        for (size_t i=0;i < m_QueryData.size() - 1;i++) {
            printf("%3d:", i);
            for (size_t j=1;j < m_QueryData.size();j++) {
                if (j < i+1) {
                    printf("      ");
                }
                else {
                    printf("%6.3f", (*dmat)(i, j));
                }
            }
            printf("\n");
        }
        printf("\n");
    }
    //-------------------------------------------------------

    m_Clusterer.SetDistMatrix(dmat);
    m_Clusterer.ComputeClusters(m_MaxClusterDist);

    const CClusterer::TClusters& clusters = m_Clusterer.GetClusters();

    // If there are only single-element clusters
    // COBALT will be run without clustering informartion
    if (clusters.size() == m_QueryData.size()) {
        //-----------------------------------------------------------------
        if (m_Verbose) { // two first lines needed for output analyzer
            printf("\nNumber of queries in clusters: 0 (0%%)\n");
            printf("Number of domain searches reduced by: 0 (0%%)\n\n");
            printf("Only single-element clusters were found."
                   " No clustering information will be used.\n");
        }
        //-----------------------------------------------------------------

        return false;
    }

    // Selecting cluster prototypes as center elements
    m_Clusterer.SetPrototypes();
    blast::TSeqLocVector cluster_prototypes;
    ITERATE(CClusterer::TClusters, cluster_it, m_Clusterer.GetClusters()) {
        cluster_prototypes.push_back(m_tQueries[cluster_it->GetPrototype()]);
        m_AllQueryData.push_back(m_QueryData[cluster_it->GetPrototype()]);
    }

    // Rearenging input sequences to consider cluster information
    m_tQueries.resize(cluster_prototypes.size());
    copy(cluster_prototypes.begin(), cluster_prototypes.end(), 
         m_tQueries.begin());
    m_QueryData.swap(m_AllQueryData);

    //-------------------------------------------------------
    if (m_Verbose) {
        printf("Query clusters:\n");
        int cluster_idx = 0;
        int num_in_clusters = 0;
        ITERATE(CClusterer::TClusters, it_cl, clusters) {
            printf("Cluster %3d: ", cluster_idx++);
            printf("(prototype: %3d) ", it_cl->GetPrototype());
            
            ITERATE(CClusterer::TSingleCluster, it_el, *it_cl) {
                printf("%d, ", *it_el);
            }
            printf("\n");
            if (it_cl->size() > 1) {
                num_in_clusters += it_cl->size();
            }
        }
     
        int gain = m_AllQueryData.size() - clusters.size();
        printf("\nNumber of queries in clusters: %d (%.0f%%)\n", 
               num_in_clusters,
               (double)num_in_clusters / m_AllQueryData.size() * 100.0);
        printf("Number of domain searches reduced by: %d (%.0f%%)\n\n", gain, 
               (double) gain / m_AllQueryData.size() * 100.0); 
        
    }
    //-------------------------------------------------------

    // Distance matrix is not needed any more, release memory
    m_Clusterer.PurgeDistMatrix();

    return true;
}

void CMultiAligner::AlignInClusters(void)
{
    const CClusterer::TClusters& clusters = m_Clusterer.GetClusters();
    m_ClusterGapPositions.clear();
    m_ClusterGapPositions.resize(clusters.size());

    // For each cluster
    for (size_t cluster_idx=0;cluster_idx < clusters.size();cluster_idx++) {

        const CClusterer::TSingleCluster& cluster = clusters[cluster_idx];
        CSequence& cluster_prot = m_AllQueryData[cluster.GetPrototype()];

        // One-element clusters contain only prototype sequence hence
        // nothing to align
        if (clusters[cluster_idx].size() > 1) {

            // Iterating over cluster elements
            ITERATE(CClusterer::TSingleCluster, seq_idx, cluster) {
                ASSERT((size_t)*seq_idx < m_AllQueryData.size());

                bool is_gap_in_prototype = false;

                // Skipping prototype sequence
                if (*seq_idx == cluster.GetPrototype()) {
                    continue;
                }
                CSequence& cluster_seq = m_AllQueryData[*seq_idx];
                
                // Aligning cluster sequence to cluster prototype
                m_Aligner.SetSequences((const char*)cluster_seq.GetSequence(),
                                       cluster_seq.GetLength(), 
                                       (const char*)cluster_prot.GetSequence(), 
                                       cluster_prot.GetLength());
                m_Aligner.Run();
                CNWAligner::TTranscript t = m_Aligner.GetTranscript(false);
                cluster_seq.PropagateGaps(t, CNWAligner::eTS_Insert);

                // Saving gap positions with respect to non-gap letters only
                for (size_t j=0;j < t.size();j++) {
                    if (t[j] == CNWAligner::eTS_Delete) {
                        is_gap_in_prototype = true;
                        break;
                    }
                }

                if (!is_gap_in_prototype) {
                    continue;
                }

                cluster_prot.PropagateGaps(t, CNWAligner::eTS_Delete);

                // If gaps are added to cluster prototype they also need to
                // be added to sequences that were already aligned
                CClusterer::TSingleCluster::const_iterator it;
                for (it=clusters[cluster_idx].begin();it != seq_idx;++it) {
                    if (*it == cluster.GetPrototype()) {
                        continue;
                    }

                    m_AllQueryData[*it].PropagateGaps(t, 
                                                      CNWAligner::eTS_Delete);
                }
            }

            Uint4 pos = 0;
            for (Uint4 i=0;i < (Uint4)cluster_prot.GetLength();i++) {
                if (cluster_prot.GetLetter(i) == CSequence::kGapChar) {
                    m_ClusterGapPositions[cluster_idx].push_back(pos);
                }
                else {
                    pos++;
                }
            }

            //------------------------------------------------------------
            if (m_Verbose) {
                printf("Aligning in cluster %d:\n", cluster_idx);
                ITERATE(CClusterer::TSingleCluster, elem, cluster) {
                    const CSequence& seq = m_AllQueryData[*elem];
                    printf("%3d: ", *elem);
                    for (int i=0;i < seq.GetLength();i++) {
                        printf("%c", seq.GetPrintableLetter(i));
                    }
                    printf("\n");
                }
                printf("\n\n");
            }
            //------------------------------------------------------------
        }
    }
    //--------------------------------------------------------------------
    if (m_Verbose) {
        for (size_t i=0;i < m_ClusterGapPositions.size();i++) {
            if (m_ClusterGapPositions[i].empty()) {
                continue;
            }

            printf("Gaps in cluster %d: ", i);
            size_t j;
            for (j=0;j < m_ClusterGapPositions[i].size() - 1;j++) {
                printf("%3d, ", m_ClusterGapPositions[i][j]);
            }
            printf("%3d\n", m_ClusterGapPositions[i][j]);
        }
        printf("\n\n");
    }
    //--------------------------------------------------------------------

    // If there is only one cluster no multiple alignment is done
    if (clusters.size() == 1) {
        m_AllQueryData.swap(m_Results);
    }

}

// Translates position of a gap with respect to non-gap characters to simple
// position in the sequence
static Uint4 s_CorrectLocation(Uint4 pos, const CSequence& seq)
{    
    Uint4 letters = 0;
    int i;
    for (i=0; letters < pos && i < seq.GetLength();i++) {
        if (seq.GetLetter(i) != CSequence::kGapChar) {
            letters++;
        }
    }

    _ASSERT(letters == pos || letters + 1 == pos);

    return i;
}

// Translates gap positions with respect to non-gap letters to simple position
// in the sequence. Each new position for a series of gaps is independent
// of others in the vector.
// pos [in|out]
static void s_CorrectLocations(vector<Uint4>& pos, const CSequence& seq)
{
    // Translate 'before letter' coordinate to position in the sequence
    int j = 0;
    Uint4 letters = 0;
    size_t i = 0;
    for (i=0;i < pos.size();i++) {
        while (j < seq.GetLength() && letters < pos[i] + 1) {
            if (seq.GetLetter(j) != CSequence::kGapChar) {
                letters++;
            }
            j++;
        }
	// For a single gap at the end the above loop exits too early
	// position needs to be shited by one
	int len = seq.GetLength();
	if (seq.GetLetter(len - 1) == CSequence::kGapChar 
	    && seq.GetLetter(len - 2) != CSequence::kGapChar) {
	    j++;
	}

        pos[i] = j - 2;
    }

    // For a series of gaps before the same letter find position of each gap
    j = 1;
    while (j < (int)pos.size()) {
        while (j < (int)pos.size() && pos[j] != pos[j - 1]) {
            j++;
        }
        Uint4 offset = 0;
        while (j < (int)pos.size() && pos[j] == pos[j - 1]) {
            j++;
            offset++;
        }
        for (Uint4 i=0;i <= offset;i++) {
            pos[j - i - 1] -= i;
        }
    }

    // Make positions independent (as if previous gaps were not there)
    for (size_t i=0;i < pos.size();i++) {
        pos[i] -= i;
    }
}


// TODO: gaps are represented as single positions, those should be changed
// to ranges
void CMultiAligner::MultiAlignClusters(void)
{
    vector<CSequence> results(m_AllQueryData.size());
    const CClusterer::TClusters& clusters = m_Clusterer.GetClusters();
    vector< vector<Uint4> > multi_aln_gaps(clusters.size());

    // Copy sequences to results list
    // and get positions of multiple alignment gaps
    for (size_t cluster_idx=0;cluster_idx < clusters.size();cluster_idx++) {
        const CClusterer::TSingleCluster& cluster = clusters[cluster_idx];
        ITERATE(CClusterer::TSingleCluster, elem, cluster) {
            if (*elem == cluster.GetPrototype()) {
                results[*elem] = m_Results[cluster_idx];

                // Get positions of multiple alignment gaps
                const CSequence& prototype = results[*elem];
                for (int i=0;i < prototype.GetLength();i++) {
                    if (prototype.GetLetter(i) == CSequence::kGapChar) {
                        multi_aln_gaps[cluster_idx].push_back((Uint4)i);
                    }
                }
            }
            else {
                results[*elem] = m_AllQueryData[*elem];
            }
        }
        //--------------------------------------------------------------------
        if (m_Verbose) {
            if (cluster_idx == 0) {
                printf("Multiple alignment gaps for clusters:\n");
            }
            if (!multi_aln_gaps[cluster_idx].empty()) {
                printf("%3d: ", cluster_idx);
                size_t i;
                for (i=0;i < multi_aln_gaps[cluster_idx].size() - 1;i++) {
                    printf("%4d-", multi_aln_gaps[cluster_idx][i]);
                    while (i < multi_aln_gaps[cluster_idx].size()
                           && multi_aln_gaps[cluster_idx][i] + 1 
                           == multi_aln_gaps[cluster_idx][i + 1]) {
                        i++;
                    }                     
                    if (i == multi_aln_gaps[cluster_idx].size() - 1
                        && multi_aln_gaps[cluster_idx][i - 1] + 1
                        == multi_aln_gaps[cluster_idx][i]) {
                        i++;
                    }
                    printf("%4d", multi_aln_gaps[cluster_idx][i]);
                }
                printf("\n");
            }
        }
        //--------------------------------------------------------------------
    }

    // Calculate positions for multiple alignmnent gaps considering
    // in-cluster alignmenet gaps
    vector< vector<Uint4> > multi_aln_gaps_corrected(clusters.size());
    for (size_t cluster_idx=0;cluster_idx < clusters.size();cluster_idx++) {

        // Nothing to do for if no gaps were added in multiple alignment
        if (multi_aln_gaps[cluster_idx].empty()) {
            continue;
        }

        const CClusterer::TSingleCluster& cluster = clusters[cluster_idx];
        const CSequence& prototype = results[cluster.GetPrototype()];
        multi_aln_gaps_corrected[cluster_idx].resize(
                                       multi_aln_gaps[cluster_idx].size());

        copy(multi_aln_gaps[cluster_idx].begin(),
             multi_aln_gaps[cluster_idx].end(),
             multi_aln_gaps_corrected[cluster_idx].begin());


        if (m_ClusterGapPositions[cluster_idx].empty()) {
            continue;
        }

        // For each multiple alignment gap
        Uint4 offset = 0, i = 0;
        for (Uint4 j=0;j < multi_aln_gaps[cluster_idx].size();j++) {
            Uint4 pos;

            // Translate in-cluster alignment gap
            if (i < m_ClusterGapPositions[cluster_idx].size()) {
                pos = s_CorrectLocation(m_ClusterGapPositions[cluster_idx][i],
                                        prototype);
            }

            // For each in-cluster alignment gap with smaller position
            // shift multiple alignemnt gap
            while (pos < multi_aln_gaps[cluster_idx][j] 
                   && i < m_ClusterGapPositions[cluster_idx].size()) {
                i++;
                offset++;

                if (i < m_ClusterGapPositions[cluster_idx].size()) {
                    pos = s_CorrectLocation(
                                        m_ClusterGapPositions[cluster_idx][i],
                                        prototype);
                }
            }
            multi_aln_gaps_corrected[cluster_idx][j] += offset;


            // Keep the same shift size for a series of gaps
            while (j < multi_aln_gaps[cluster_idx].size() - 1 
                     && multi_aln_gaps[cluster_idx][j] + 1 
                      == multi_aln_gaps[cluster_idx][j + 1]) {
                j++;
                multi_aln_gaps_corrected[cluster_idx][j] += offset;
            }
            if (j > 0 && j == multi_aln_gaps[cluster_idx].size() - 1
                && multi_aln_gaps[cluster_idx][j - 1] + 1
                == multi_aln_gaps[cluster_idx][j]) {
                multi_aln_gaps_corrected[cluster_idx][j] += offset;
            }

        
        }
        //--------------------------------------------------------------------
        if (m_Verbose) {
            printf("\nCorrected multiple alignment gaps for clusters:\n"); 
            if (!multi_aln_gaps_corrected[cluster_idx].empty()) {
                printf("%3d: ", cluster_idx);
                 size_t i;
                for (i=0;i < multi_aln_gaps_corrected[cluster_idx].size() - 1
                         ;i++) {
                    printf("%4d-", multi_aln_gaps_corrected[cluster_idx][i]);
                    while (i < multi_aln_gaps_corrected[cluster_idx].size() - 1
                           && multi_aln_gaps_corrected[cluster_idx][i] + 1
                           == multi_aln_gaps_corrected[cluster_idx][i + 1]) {
                        i++;
                    }
                    if (i == multi_aln_gaps_corrected[cluster_idx].size() - 1
                        && multi_aln_gaps_corrected[cluster_idx][i - 1] + 1
                        == multi_aln_gaps_corrected[cluster_idx][i]) {
                        i++;
                    }
                    printf("%4d ", multi_aln_gaps_corrected[cluster_idx][i]);
                }
                printf("\n");
            }
        }
        //--------------------------------------------------------------------
    }

    // Insert multiple alignment gaps into cluster sequences
    // and in-cluster alignment gaps into cluster prototypes
    for (size_t cluster_idx=0;cluster_idx < clusters.size();cluster_idx++) {
        const CClusterer::TSingleCluster& cluster = clusters[cluster_idx];
        if (cluster.size() == 1) {
            continue;
        }
        ITERATE(CClusterer::TSingleCluster, elem, cluster) {
            if (*elem == cluster.GetPrototype()) {
                results[*elem].InsertGaps(m_ClusterGapPositions[cluster_idx],
                                          false);
            }
            else {
                results[*elem].InsertGaps(multi_aln_gaps_corrected[cluster_idx],
                                          true);
            }
        }
    }

    //----------------------------------------------------------------------
    if (m_Verbose) {
        for (size_t cluster_idx=0;cluster_idx < clusters.size();cluster_idx++) {
            const CClusterer::TSingleCluster& cluster = clusters[cluster_idx];
            if (cluster.size() == 1) {
                continue;
            }
            printf("\nCluster %d with multiple alingnment and in-cluster"
                   " gaps:\n", cluster_idx);
            const CSequence& prototype = results[cluster.GetPrototype()];
            printf("%4d: ", cluster.GetPrototype());
            for (int i=0;i < prototype.GetLength();i++) {
                printf("%c", prototype.GetPrintableLetter(i));
            }
            printf("\n");
            ITERATE(CClusterer::TSingleCluster, elem, cluster) {
                if (*elem == cluster.GetPrototype()) {
                    continue;
                }
                printf("%4d: ", *elem);
                for (int i=0;i < results[*elem].GetLength();i++) {
                    printf("%c", results[*elem].GetPrintableLetter(i));
                }
                printf("\n");
            }
        }
    }
    //----------------------------------------------------------------------

    // Calculate positions of in-cluster alignements
    // (translate from before letter to positions in a sequence)
    for (size_t cluster_idx=0;cluster_idx < clusters.size();cluster_idx++) {
        const CClusterer::TSingleCluster& cluster = clusters[cluster_idx];
        const CSequence& prototype = results[cluster.GetPrototype()];

        s_CorrectLocations(m_ClusterGapPositions[cluster_idx], prototype);

        //--------------------------------------------------------------------
        if (m_Verbose) {
            if (cluster_idx == 0) {
                printf("\nCorrected in-cluster gaps for clusters:\n");
            }
            if (!m_ClusterGapPositions[cluster_idx].empty()) {
                printf("%3d:", cluster_idx);
                size_t i;
                for (i=0;i < m_ClusterGapPositions[cluster_idx].size() - 1
                         ;i++) {
                    printf("%4d,", m_ClusterGapPositions[cluster_idx][i]);
                }
                printf("%4d\n", m_ClusterGapPositions[cluster_idx][i]);
            }
        }
        //--------------------------------------------------------------------
    }

    // Insert in-cluster gaps from other clusters to each cluster sequence
    // (thery are already there)
    for (size_t cluster_idx=0;cluster_idx < clusters.size();cluster_idx++) {
        
        size_t size = 0;
        for (size_t i=0;i < m_ClusterGapPositions.size();i++) {
            if (i != cluster_idx) {
                size += m_ClusterGapPositions[i].size();
            }
        }

        // Combine all in-cluster gaps in one vector
        vector<Uint4> gaps(size);
        size_t ind = 0;
         for (size_t i=0;i < m_ClusterGapPositions.size();i++) {
            if (i != cluster_idx && !m_ClusterGapPositions[i].empty()) {
                ITERATE(vector<Uint4>, it, m_ClusterGapPositions[i]) {
                    gaps[ind++] = *it;
                }
            }
        }
        sort(gaps.begin(), gaps.end());

        // Correct gap positions by gaps that are being added
        // shift each larger position by number of gaps with smaller positions
        for (size_t i=0;i < gaps.size();i++) {
            gaps[i] += i;
        }

        // Correct gap positions by considering in-cluster gaps from 
        // the cluster that the sequence belongs to
        Uint4 offset = 0;
        size_t j = 0;
        for (size_t i=0;i < gaps.size();i++) {
            while (j < m_ClusterGapPositions[cluster_idx].size() 
                   && m_ClusterGapPositions[cluster_idx][j] < gaps[i]) {
                j++;
                offset++;
            }
            gaps[i] += offset;
        }

        const CClusterer::TSingleCluster& cluster = clusters[cluster_idx];
        _ASSERT(gaps[gaps.size() - 1] 
              <= results[cluster.GetPrototype()].GetLength() + gaps.size());

        // Insert gaps
        ITERATE(CClusterer::TSingleCluster, elem, cluster) {
            results[*elem].InsertGaps(gaps, true);
        }

    }

    //----------------------------------------------------------------------
    if (m_Verbose) {
        printf("\n\nCluster prototypes only:\n");
        for (size_t i=0;i < clusters.size();i++) {
            const CClusterer::TSingleCluster& cluster = clusters[i];
            for (int j=0;j < results[cluster.GetPrototype()].GetLength();j++) {
                printf("%c",
                       results[cluster.GetPrototype()].GetPrintableLetter(j));
            }
            printf("\n");
        }
        printf("\n\n");

        printf("All queries:\n");
        for (size_t i=0;i < results.size();i++) {
            for (int j=0;j < results[i].GetLength();j++) {
                printf("%c", results[i].GetPrintableLetter(j));
            }
            printf("\n");
        }
        printf("\n");
    }
    //----------------------------------------------------------------------
    
    m_Results.swap(results);
}


// Frequencies are not normalized
void CMultiAligner::MakeClusterResidueFrequencies(void) 
{
    // Iterate over all clusters
    CClusterer::TClusters& clusters = m_Clusterer.GetClusters();
    for (size_t cluster_idx=0;cluster_idx < clusters.size();cluster_idx++) {

        CClusterer::TSingleCluster& cluster = clusters[cluster_idx];
        _ASSERT(cluster.size() >= 1);
        
        // Skip one-element clusters
        if (cluster.size() == 1) {
            continue;
        }

        CSequence& prototype = m_QueryData[cluster_idx];
        CSequence::TFreqMatrix& freqs = prototype.GetFreqs();
        int size = prototype.GetLength();

        // For all cluster elements
        ITERATE(CClusterer::TSingleCluster, elem, cluster) {

            // Values from the prototype are already in the matrix
            if (*elem == cluster.GetPrototype()) {
                continue;
            }

            // Add frequencies from current sequence
            CSequence::TFreqMatrix& matrix = m_AllQueryData[*elem].GetFreqs();
            _ASSERT(matrix.GetRows() == freqs.GetRows() 
                    + m_ClusterGapPositions[cluster_idx].size());
            Uint4 gap_idx = 0, offset = 0;
            for (Uint4 i=0;i < (Uint4)size;i++) {
                while (gap_idx < m_ClusterGapPositions[cluster_idx].size()
                  && i == m_ClusterGapPositions[cluster_idx][gap_idx]) {
                    offset++;
                    gap_idx++;
                }
                for (int k=0;k < kAlphabetSize;k++) {
                    freqs(i, k) += matrix(i + offset, k);
                }
            }
            _ASSERT(offset == m_ClusterGapPositions[cluster_idx].size()
                    || m_ClusterGapPositions[cluster_idx][gap_idx] 
                    == (Uint4)prototype.GetLength());
        }

    }

}


END_SCOPE(cobalt)
END_NCBI_SCOPE
