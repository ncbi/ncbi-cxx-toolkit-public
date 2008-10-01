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
    
        for (int i = 0; i < (int)matrix.GetRows() - 1; i++) {
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
                m_tQueries.swap(m_AllQueries);
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
        for (size_t i=dmat->GetCols() - 1;i > 0;i--) {
            printf("%6d", (int)i);
        }
        printf("\n");
        for (size_t i=0;i < dmat->GetRows() - 1;i++) {
            printf("%3d:", (int)i);
            for (size_t j=dmat->GetCols() - 1;j > i;j--) {
                printf("%6.3f", (*dmat)(i, j));
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

    // Select cluster prototypes
    NON_CONST_ITERATE(CClusterer::TClusters, it, m_Clusterer.SetClusters()) {

        // For one-element clusters same element
        if (it->size() == 1) {
            it->SetPrototype(*it->begin());

        } else if (it->size() == 2) {

            // For two-element clusters - the longer sequence
            int len1 = m_QueryData[(*it)[0]].GetLength();
            int len2 = m_QueryData[(*it)[1]].GetLength();
            int prot = (len1 > len2) ? (*it)[0] : (*it)[1];
            it->SetPrototype(prot);

        } else {

            // For more than two elements - cluster center
            it->SetPrototype(it->FindCenterElement(m_Clusterer.GetDistMatrix()));
        }
    }

    blast::TSeqLocVector cluster_prototypes;
    ITERATE(CClusterer::TClusters, cluster_it, m_Clusterer.GetClusters()) {
        cluster_prototypes.push_back(m_tQueries[cluster_it->GetPrototype()]);
        m_AllQueryData.push_back(m_QueryData[cluster_it->GetPrototype()]);
    }

    // Rearrenging input sequences to consider cluster information
    m_tQueries.swap(cluster_prototypes);
    cluster_prototypes.swap(m_AllQueries);
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

        const CClusterer::TDistMatrix& d = m_Clusterer.GetDistMatrix();
        printf("Distances in clusters:\n");
        for (size_t cluster_idx=0;cluster_idx < clusters.size();
             cluster_idx++) {

            const CClusterer::TSingleCluster& cluster = clusters[cluster_idx];
            if (cluster.size() == 1) {
                continue;
            }

            printf("Cluster %d:\n", (int)cluster_idx);
            if (cluster.size() == 2) {
                printf("   %6.3f\n\n", d(cluster[0], cluster[1]));
                continue;
            }

            printf("    ");
            for (size_t i= cluster.size() - 1;i > 0;i--) {
                printf("%6d", (int)cluster[i]);
            }
            printf("\n");
            for (size_t i=0;i < cluster.size() - 1;i++) {
                printf("%3d:", (int)cluster[i]);
                for (size_t j=cluster.size() - 1;j > i;j--) {
                    printf("%6.3f", d(cluster[i], cluster[j]));
                }
                printf("\n");
            }
            printf("\n\n");
        }

        printf("Sequences that belong to different clusters with distance"
               " smaller than threshold (exludes prototypes):\n");
        ITERATE(CClusterer::TClusters, it, clusters) {
            if (it->size() == 1) {
                continue;
            }
            ITERATE(CClusterer::TSingleCluster, elem, *it) {
                ITERATE(CClusterer::TClusters, cl, clusters) {
                    if (it == cl) {
                        continue;
                    }

                    ITERATE(CClusterer::TSingleCluster, el, *cl) {

                        if (*el == cl->GetPrototype()) {
                            continue;
                        }

                        if (d(*elem, *el) < m_MaxClusterDist) {
                            printf("%3d, %3d: %f\n", *elem, *el, d(*elem, *el));
                        }
                    }
                }
            }
        }
        printf("\n\n");
        
        
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

    m_Aligner.SetWg(m_GapOpen);
    m_Aligner.SetStartWg(m_EndGapOpen);
    m_Aligner.SetEndWg(m_EndGapOpen);
    m_Aligner.SetWs(m_GapExtend);
    m_Aligner.SetStartWs(m_EndGapExtend);
    m_Aligner.SetEndWs(m_EndGapExtend);

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

                m_Aligner.SetEndSpaceFree(false, false, false, false);

                // If there is a large length disparity between the two
                // sequences, reduce or eliminate gap penalties.
                int len1 = cluster_seq.GetLength();
                int len2 = cluster_prot.GetLength();
                if (len1 > 1.2 * len2 || len2 > 1.2 * len1) {
                    m_Aligner.SetStartWs(m_EndGapExtend / 2);
                    m_Aligner.SetEndWs(m_EndGapExtend / 2); 
                }
                if (len1 > 1.5 * len2 || len2 > 1.5 * len1) {
                    m_Aligner.SetEndSpaceFree(true, true, true, true);
                }

                // Run aligner
                m_Aligner.Run();

                // Reset gap penalties
                m_Aligner.SetWg(m_GapOpen);
                m_Aligner.SetStartWg(m_EndGapOpen);
                m_Aligner.SetEndWg(m_EndGapOpen);
                m_Aligner.SetWs(m_GapExtend);
                m_Aligner.SetStartWs(m_EndGapExtend);
                m_Aligner.SetEndWs(m_EndGapExtend);

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
                printf("Aligning in cluster %d:\n", (int)cluster_idx);
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

            printf("Gaps in cluster %d: ", (int)i);
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

// Initiate regular column of multiple alignment
static void s_InitColumn(vector<CMultiAligner::SColumn>::iterator& it,
                         size_t len)
{
    it->insert = false;
    it->letters.resize(len);
    for (size_t i=0;i < len;i++) {
        it->letters[i] = -1;
    }
    it->number = 1;
    it->cluster = -1;
}


// Initiate a range from in-cluster alignment for insertion into multiple
// alignment
static void s_InitInsertColumn(vector<CMultiAligner::SColumn>::iterator& it,
                               size_t len, int num, int cluster)
{
    it->insert = true;
    it->letters.resize(len);
    for (size_t i=0;i < len;i++) {
        it->letters[i] = -1;
    }
    it->number = num;
    it->cluster = cluster;
}

void CMultiAligner::MultiAlignClusters(void)
{
    const CClusterer::TClusters& clusters = m_Clusterer.GetClusters();

    int seq_length = m_Results[0].GetLength();
    size_t num_seqs = m_AllQueryData.size();

    vector<int> letter_inds(clusters.size());
    vector<SColumn> columns(seq_length);

    // Represent multiple alignment as list of columns of positions in input
    // sequences (n-th letter)
    int col = 0;
    NON_CONST_ITERATE(vector<SColumn>, it, columns) {
        s_InitColumn(it, num_seqs);
        for (size_t j=0;j < clusters.size();j++) {
            if (m_Results[j].GetLetter(col) != CSequence::kGapChar) {
                it->letters[clusters[j].GetPrototype()] = letter_inds[j]++;
            }
        }
        col++;
    }

    // Insert in-cluster ranges to columns
    size_t new_length = seq_length;

    // for each cluster
    for (size_t cluster_idx=0;cluster_idx < clusters.size();cluster_idx++) {

        // for each gap position
        for (size_t i=0;i < m_ClusterGapPositions[cluster_idx].size();i++) {

            // get letter before which the gap needs to be placed
            size_t letter = m_ClusterGapPositions[cluster_idx][i];
            size_t offset = i;
            int num = 1;

            // combine all gaps before the same letter as one range
            while (i < m_ClusterGapPositions[cluster_idx].size()
                   && m_ClusterGapPositions[cluster_idx][i + 1] == letter) {
                i++;
                num++;
            }
            
            // find that column that contains this letter
            vector<SColumn>::iterator it = columns.begin();
            size_t prototype_idx =  clusters[cluster_idx].GetPrototype();
            while (it != columns.end() 
                 && (it->insert || it->letters[prototype_idx] < (int)letter)) {
                ++it;
            }

            // insert the range in all cluster sequences
            it = columns.insert(it, SColumn());
            s_InitInsertColumn(it, num_seqs, num, cluster_idx);
            ITERATE(CClusterer::TSingleCluster, elem, clusters[cluster_idx]) {

                // for insert ranges leter index is absolute index in 
                // in-cluster alignment
                it->letters[*elem] = letter + offset;
            }

            // extend the length of the sequences by added ranges
            new_length += num;
        }
    }


    // Convert columns to array of CSequence
    vector<CSequence> results(m_AllQueryData.size());

    // Initialize all sequences to gaps
    NON_CONST_ITERATE(vector<CSequence>, it, results) {
        it->Reset(new_length);
    }


    // offsets caused by in-cluster gaps in cluster prototypes
    vector<int> gap_offsets(clusters.size());
    col = 0;

    // for each column
    ITERATE(vector<SColumn>, it, columns) {
        if (!it->insert) {

            // for regular column
            // for each cluster
            for (size_t i=0;i < clusters.size();i++) {
                
                // find letter index in input sequnece (n-th letter)
                size_t prototype_idx = clusters[i].GetPrototype();
                int letter = it->letters[prototype_idx];
                
                // if gap in this position, do nothing
                if (letter < 0) {
                    continue;
                }

                // find correct location by skipping gaps in in-cluster
                // alingment
                // NOTE: This index juggling could be simplified if we
                // kept an array of unmodified input sequences
                while (m_AllQueryData[prototype_idx].GetLetter(letter 
                       + gap_offsets[i]) == CSequence::kGapChar) {
                    gap_offsets[i]++;
                }

                // insert letter in all cluster sequences
                ITERATE(CClusterer::TSingleCluster, elem, clusters[i]) {
                    results[*elem].SetLetter(col,
                     m_AllQueryData[*elem].GetLetter(letter + gap_offsets[i]));
                }
            }
        }
        else {
            
            // for insetr columns
            // for each cluster sequence copy letters from in-cluster alignment
            ITERATE(CClusterer::TSingleCluster, elem, clusters[it->cluster]) {
                for (int i=0; i < it->number;i++) {
                    results[*elem].SetLetter(col + i,
                      m_AllQueryData[*elem].GetLetter(it->letters[*elem] + i));
                }
            }

        }
        col += it->number;
    }


    //----------------------------------------------------------------------
    if (m_Verbose) {
        printf("Cluster prototypes:\n");
        ITERATE(CClusterer::TClusters, it, clusters) {
            const CSequence& seq = results[it->GetPrototype()];
            for (int i=0;i < seq.GetLength();i++) {
                printf("%c", (char)seq.GetPrintableLetter(i));
            }
            printf("\n");
        }
        printf("\n\n");

        printf("Individual clusters:\n");
        for (int i=0;i < (int)clusters.size();i++) {
            if (clusters[i].size() > 1) {
                printf("Cluster %d:\n", i);
                ITERATE(CClusterer::TSingleCluster, elem, clusters[i]) {
                    const CSequence& seq = results[*elem];
                    for (int j=0;j < seq.GetLength();j++) {
                        printf("%c", (char)seq.GetPrintableLetter(j));
                    }
                    printf("\n");
                }
                printf("\n");
            }
        }
        printf("\n\n");


        printf("All queries:\n");
        ITERATE(vector<CSequence>, it, results) {
            const CSequence& seq = *it;
            for (int i=0;i < seq.GetLength();i++) {
                printf("%c", (char)seq.GetPrintableLetter(i));
            }
            printf("\n");
        }
        printf("\n\n");
    }
    //----------------------------------------------------------------------

    m_Results.swap(results);
    m_tQueries.swap(m_AllQueries);
}


// Frequencies are not normalized
void CMultiAligner::MakeClusterResidueFrequencies(void) 
{
    // Iterate over all clusters
    const CClusterer::TClusters& clusters = m_Clusterer.GetClusters();
    for (size_t cluster_idx=0;cluster_idx < clusters.size();cluster_idx++) {

        const CClusterer::TSingleCluster& cluster = clusters[cluster_idx];
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
