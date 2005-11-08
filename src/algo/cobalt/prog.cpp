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

File name: prog.cpp

Author: Jason Papadopoulos

Contents: Perform a progressive multiple alignment

******************************************************************************/

#include <ncbi_pch.hpp>
#include <algo/cobalt/cobalt.hpp>

/// @file prog.cpp
/// Perform a progressive multiple alignment

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

static void
x_AddConstraint(int seq_length1,
                int seq_length2,
                vector<size_t>& constraint,
                CHit *hit)
{
    int seq1_start = hit->m_SeqRange1.GetFrom();
    int seq1_stop = hit->m_SeqRange1.GetTo();
    int seq2_start = hit->m_SeqRange2.GetFrom();
    int seq2_stop = hit->m_SeqRange2.GetTo();

    // add the top left corner of the constraint region,
    // *unless* it is at the beginning of one or both sequences

    if (!constraint.empty()) {
        int last = constraint.size();
        if (constraint[last-4] >= seq1_start ||
            constraint[last-2] >= seq2_start) {
            return;
        }
    }

    constraint.push_back(seq1_start);
    constraint.push_back(seq1_start);
    constraint.push_back(seq2_start);
    constraint.push_back(seq2_start);
    
    if (seq1_start >= seq1_stop ||
        seq2_start >= seq2_stop)
        return;

    constraint.push_back(seq1_stop);
    constraint.push_back(seq1_stop);
    constraint.push_back(seq2_stop);
    constraint.push_back(seq2_stop);
}

static void x_HitToConstraints(vector<size_t>& constraint,
                               vector<CSequence>& query_data,
                               CHit *hit)
{
    int seq_index1 = hit->m_SeqIndex1;
    int seq_index2 = hit->m_SeqIndex2;
    int length1 = query_data[seq_index1].GetLength();
    int length2 = query_data[seq_index2].GetLength();

    if (!(hit->HasSubHits())) {
        x_AddConstraint(length1, length2, constraint, hit);
    }
    else {
        NON_CONST_ITERATE(CHit::TSubHit, subitr, hit->GetSubHit()) {
            x_AddConstraint(length1, length2, constraint, *subitr);
        }
    }
}


static void
x_FillResidueFrequencies(double **freq_data,
                    vector<CSequence>& query_data,
                    vector<CTree::STreeLeaf>& node_list)
{
    double sum = 0.0;
    for (size_t i = 0; i < node_list.size(); i++) {
        sum += node_list[i].distance;
    }
    assert(sum >= 0.0);

    for (size_t i = 0; i < node_list.size(); i++) {
        // update the residue frequencies to include the influence
        // of a new sequence at a leaf in the tree

        int index = node_list[i].query_idx;
        double weight = node_list[i].distance / sum;

        if (node_list[i].distance == 0 && sum == 0)
            weight = 1;
        else
            weight = node_list[i].distance / sum;

        int size = query_data[index].GetLength();
        CSequence::TFreqMatrix& matrix = query_data[index].GetFreqs();

        for (int j = 0; j < size; j++) {
            if (query_data[index].GetLetter(j) == CSequence::kGapChar) {
                freq_data[j][0] += weight;
            }
            else {
                for (int k = 0; k < kAlphabetSize; k++) {
                    freq_data[j][k] += weight * matrix(j, k);
                }
            }
        }
    }
}

/// Normalize the residue frequencies so that each column
/// sums to 1.0
/// @param freq_data The residue frequencies [in/modified]
/// @param freq_size Number of columns in freq_data [in]
///
static void
x_NormalizeResidueFrequencies(double **freq_data,
                              int freq_size)
{
    for (int i = 0; i < freq_size; i++) {
        double sum = 0.0;

        // Compute the total weight of each row

        for (int j = 0; j < kAlphabetSize; j++) {
            sum += freq_data[i][j];
        }

        sum = 1.0 / sum;
        for (int j = 0; j < kAlphabetSize; j++) {
            freq_data[i][j] *= sum;
        }
    }
}

static void 
x_ExpandRange(TRange& range,
              CSequence& seq)
{
    int len = seq.GetLength();
    int i, offset;

    offset = -1;
    for (i = 0; i < len; i++) {
        if (seq.GetLetter(i) != CSequence::kGapChar)
            offset++;
        if (offset == range.GetFrom()) {
            range.SetFrom(i);
            break;
        }
    }
    if (offset == range.GetTo()) {
        range.SetTo(i);
        return;
    }
    for (i++; i < len; i++) {
        if (seq.GetLetter(i) != CSequence::kGapChar)
            offset++;
        if (offset == range.GetTo()) {
            range.SetTo(i);
            break;
        }
    }
    assert(i < len);
}


void
CMultiAligner::x_FindConstraints(vector<size_t>& constraint,
                 vector<CSequence>& alignment,
                 vector<CTree::STreeLeaf>& node_list1,
                 int freq1_size,
                 vector<CTree::STreeLeaf>& node_list2,
                 int freq2_size,
                 CNcbiMatrix<CHitList>& pair_info,
                 int iteration)
{
    const int kMinAlignLength = 11;
    CHitList profile_hitlist;

    for (int i = 0; i < (int)node_list1.size(); i++) {

        for (int j = 0; j < (int)node_list2.size(); j++) {

            int seq1 = node_list1[i].query_idx;
            int seq2 = node_list2[j].query_idx;

            for (int k = 0; k < pair_info(seq1, seq2).Size(); k++) {
                CHit *hit = pair_info(seq1, seq2).GetHit(k);
                CHit *new_hit = new CHit(seq1, seq2);
                bool swap_ranges = (seq1 != hit->m_SeqIndex1);
                
                new_hit->m_BitScore = hit->m_BitScore;
                if (swap_ranges) {
                    new_hit->m_SeqRange1 = hit->m_SeqRange2;
                    new_hit->m_SeqRange2 = hit->m_SeqRange1;
                }
                else {
                    new_hit->m_SeqRange1 = hit->m_SeqRange1;
                    new_hit->m_SeqRange2 = hit->m_SeqRange2;
                }
                NON_CONST_ITERATE(CHit::TSubHit, subitr, hit->GetSubHit()) {
                    CHit *subhit = *subitr;
                    CHit *new_subhit = new CHit(seq1, seq2);
                    if (swap_ranges) {
                        new_subhit->m_SeqRange1 = subhit->m_SeqRange2;
                        new_subhit->m_SeqRange2 = subhit->m_SeqRange1;
                    }
                    else {
                        new_subhit->m_SeqRange1 = subhit->m_SeqRange1;
                        new_subhit->m_SeqRange2 = subhit->m_SeqRange2;
                    }
                    new_hit->InsertSubHit(new_subhit);
                }
                profile_hitlist.AddToHitList(new_hit);
            }
        }
    }

    if (profile_hitlist.Empty())
        return;

    for (int i = 0; i < profile_hitlist.Size(); i++) {
        CHit *hit = profile_hitlist.GetHit(i);
        x_ExpandRange(hit->m_SeqRange1, alignment[hit->m_SeqIndex1]);
        x_ExpandRange(hit->m_SeqRange2, alignment[hit->m_SeqIndex2]);

        NON_CONST_ITERATE(CHit::TSubHit, subitr, hit->GetSubHit()) {
            CHit *subhit = *subitr;
            x_ExpandRange(subhit->m_SeqRange1, alignment[subhit->m_SeqIndex1]);
            x_ExpandRange(subhit->m_SeqRange2, alignment[subhit->m_SeqIndex2]);
        }
    }

    profile_hitlist.SortByStartOffset();

    //--------------------------------------
    if (m_Verbose) {
        printf("possible constraints (offsets wrt input profiles):\n");
        for (int i = 0; i < profile_hitlist.Size(); i++) {
            CHit *hit = profile_hitlist.GetHit(i);
            if (hit->m_BitScore != 1.0)
                printf("query %3d %4d - %4d query %3d %4d - %4d score %f %c\n",
                   hit->m_SeqIndex1, 
                   hit->m_SeqRange1.GetFrom(), hit->m_SeqRange1.GetTo(),
                   hit->m_SeqIndex2, 
                   hit->m_SeqRange2.GetFrom(), hit->m_SeqRange2.GetTo(),
                   hit->m_BitScore,
                   hit->HasSubHits() ? 'd' : 'f');
        }
    }
    //--------------------------------------

    vector<SGraphNode> graph;
    for (int j = 0; j < profile_hitlist.Size(); j++) {
        CHit *hit = profile_hitlist.GetHit(j);
        if (iteration == 0 &&
            (hit->m_SeqRange1.GetLength() < kMinAlignLength ||
             hit->m_SeqRange2.GetLength() < kMinAlignLength) )
            continue;

        int i;
        for (i = 0; i < (int)graph.size(); i++) {
            CHit *ghit = graph[i].hit;
            if ((abs(ghit->m_SeqRange1.GetFrom() - 
                     hit->m_SeqRange1.GetFrom()) <= kMinAlignLength / 2 ||
                 abs(ghit->m_SeqRange1.GetTo() - 
                     hit->m_SeqRange1.GetTo()) <= kMinAlignLength / 2 ||
                 abs(ghit->m_SeqRange2.GetFrom() - 
                     hit->m_SeqRange2.GetFrom()) <= kMinAlignLength / 2 ||
                 abs(ghit->m_SeqRange2.GetTo() - 
                     hit->m_SeqRange2.GetTo()) <= kMinAlignLength / 2) &&
                (ghit->m_SeqRange1.GetFrom() - ghit->m_SeqRange2.GetFrom() ==
                 hit->m_SeqRange1.GetFrom() - hit->m_SeqRange2.GetFrom()) &&
                (ghit->m_SeqRange1.GetTo() - ghit->m_SeqRange2.GetTo() ==
                 hit->m_SeqRange1.GetTo() - hit->m_SeqRange2.GetTo()) ) {

                if (ghit->m_BitScore < hit->m_BitScore)
                    graph[i].hit = hit;
                break;
            }
        }
        if (i == graph.size())
            graph.push_back(SGraphNode(hit, j));
    }

    if (graph.empty())
        return;

    if (iteration == 0) {
        int num_hits = profile_hitlist.Size();
        vector<int> list1, list2;
        list1.reserve(num_hits);
        list2.reserve(num_hits);

        for (int i = 0; i < (int)graph.size(); i++) {
            CHit *ghit = graph[i].hit;
            list1.clear();
            list2.clear();
    
            for (int j = 0; j < num_hits; j++) {
                CHit *hit = profile_hitlist.GetHit(j);

                if ((abs(ghit->m_SeqRange1.GetFrom() - 
                         hit->m_SeqRange1.GetFrom()) <= kMinAlignLength / 2 ||
                     abs(ghit->m_SeqRange1.GetTo() - 
                         hit->m_SeqRange1.GetTo()) <= kMinAlignLength / 2 ||
                     abs(ghit->m_SeqRange2.GetFrom() - 
                         hit->m_SeqRange2.GetFrom()) <= kMinAlignLength / 2 ||
                     abs(ghit->m_SeqRange2.GetTo() - 
                         hit->m_SeqRange2.GetTo()) <= kMinAlignLength / 2) &&
                    (ghit->m_SeqRange1.GetFrom() - ghit->m_SeqRange2.GetFrom()==
                     hit->m_SeqRange1.GetFrom() - hit->m_SeqRange2.GetFrom()) &&
                    (ghit->m_SeqRange1.GetTo() - ghit->m_SeqRange2.GetTo() ==
                     hit->m_SeqRange1.GetTo() - hit->m_SeqRange2.GetTo()) ) {

                    int k;
                    for (k = 0; k < (int)list1.size(); k++) {
                        if (list1[k] == hit->m_SeqIndex1)
                            break;
                    }
                    if (k == (int)list1.size())
                        list1.push_back(hit->m_SeqIndex1);

                    for (k = 0; k < (int)list2.size(); k++) {
                        if (list2[k] == hit->m_SeqIndex2)
                            break;
                    }
                    if (k == (int)list2.size())
                        list2.push_back(hit->m_SeqIndex2);
                }
            }
            graph[i].best_score = graph[i].hit->m_BitScore * 
                                  list1.size() * list2.size();
        }
    }
    else {
        NON_CONST_ITERATE(vector<SGraphNode>, itr, graph) {
            itr->best_score = itr->hit->m_BitScore;
        }
    }

    SGraphNode *best_path = x_FindBestPath(graph);
    while (best_path != 0) {
        CHit *hit = best_path->hit;

        //--------------------------------------
        if (m_Verbose) {
            printf("pick query %3d %4d - %4d query %3d %4d - %4d\n",
                        hit->m_SeqIndex1, 
                    hit->m_SeqRange1.GetFrom(), hit->m_SeqRange1.GetTo(),
                    hit->m_SeqIndex2, 
                    hit->m_SeqRange2.GetFrom(), hit->m_SeqRange2.GetTo());
        }
        //--------------------------------------

        x_HitToConstraints(constraint, alignment, hit);
        best_path = best_path->path_next;
    }
    assert(!constraint.empty());
}


void
CMultiAligner::x_AlignProfileProfile(
                 vector<CTree::STreeLeaf>& node_list1,
                 vector<CTree::STreeLeaf>& node_list2,
                 vector<CSequence>& alignment,
                 CNcbiMatrix<CHitList>& pair_info,
                 int iteration)
{
    double **freq1_data;
    double **freq2_data;
    const int kScale = CMultiAligner::kRpsScaleFactor;

    int freq1_size = alignment[node_list1[0].query_idx].GetLength();
    int freq2_size = alignment[node_list2[0].query_idx].GetLength();

    //---------------------------
    if (m_Verbose) {
        printf("\nalign profile (size %d) with profile (size %d)\n",
                        freq1_size, freq2_size);
    }
    //---------------------------

    // build a set of residue frequencies for the
    // sequences in the left subtree

    freq1_data = new double* [freq1_size];
    freq1_data[0] = new double[kAlphabetSize * freq1_size];

    for (int i = 1; i < freq1_size; i++)
        freq1_data[i] = freq1_data[0] + kAlphabetSize * i;

    memset(freq1_data[0], 0, kAlphabetSize * freq1_size * sizeof(double));
    x_FillResidueFrequencies(freq1_data, alignment, node_list1);
    x_NormalizeResidueFrequencies(freq1_data, freq1_size);
    
    // build a set of residue frequencies for the
    // sequences in the right subtree

    freq2_data = new double* [freq2_size];
    freq2_data[0] = new double[kAlphabetSize * freq2_size];

    for (int i = 1; i < freq2_size; i++)
        freq2_data[i] = freq2_data[0] + kAlphabetSize * i;

    memset(freq2_data[0], 0, kAlphabetSize * freq2_size * sizeof(double));
    x_FillResidueFrequencies(freq2_data, alignment, node_list2);
    x_NormalizeResidueFrequencies(freq2_data, freq2_size);
    
    // Perform dynamic programming global alignment

    m_Aligner.SetSequences((const double**)freq1_data, freq1_size, 
                         (const double**)freq2_data, freq2_size, kScale);
    m_Aligner.SetEndSpaceFree(false, false, false, false); 

    // List the query sequences that participate in
    // each profile

    vector<size_t> constraint;

    x_FindConstraints(constraint, alignment, node_list1,
                      freq1_size, node_list2, freq2_size,
                      pair_info, iteration);

    //-------------------------------
    if (m_Verbose) {
    printf("constraints: ");
    for (int i = 0; i < (int)constraint.size(); i+=4) {
        printf("(seq1 %d seq2 %d)->", constraint[i], constraint[i+2]);
    }
    printf("\n");
    }
    //-------------------------------
    m_Aligner.SetPattern(constraint);

    // if constraints are present, remove end gap penalties

    // scale up the gap penalties, run the aligner, scale
    // the penalties back down

    m_Aligner.SetWg(m_GapOpen * kScale);
    m_Aligner.SetStartWg(m_EndGapOpen * kScale);
    m_Aligner.SetEndWg(m_EndGapOpen * kScale);
    m_Aligner.SetWs(m_GapExtend * kScale);
    m_Aligner.SetStartWs(m_EndGapExtend * kScale);
    m_Aligner.SetEndWs(m_EndGapExtend * kScale);

    if ((freq1_size > 1.5 * freq2_size ||
         freq2_size > 1.5 * freq1_size) &&
         !constraint.empty()) {
        m_Aligner.SetEndSpaceFree(true, true, true, true);
    }

    m_Aligner.Run();
    m_Aligner.SetWg(m_GapOpen);
    m_Aligner.SetStartWg(m_EndGapOpen);
    m_Aligner.SetEndWg(m_EndGapOpen);
    m_Aligner.SetWs(m_GapExtend);
    m_Aligner.SetStartWs(m_EndGapExtend);
    m_Aligner.SetEndWs(m_EndGapExtend);

    delete [] freq1_data[0];
    delete [] freq1_data;
    delete [] freq2_data[0];
    delete [] freq2_data;

    // Retrieve the traceback information from the alignment

    const CNWAligner::TTranscript t(m_Aligner.GetTranscript(false));
    for (int i = 0; i < (int)node_list1.size(); i++) {
        alignment[node_list1[i].query_idx].PropagateGaps(t,
                                              CNWAligner::eTS_Insert);
    }
    for (int i = 0; i < (int)node_list2.size(); i++) {
        alignment[node_list2[i].query_idx].PropagateGaps(t,
                                              CNWAligner::eTS_Delete);
    }

    //-------------------------------------------
    if (m_Verbose) {
        printf("      ");
        for (int i = 0; i < (int)t.size() / 10; i++)
            printf("%10d", i + 1);
        printf("\n     ");
        for (int i = 0; i < (int)t.size(); i++)
            printf("%d", i % 10);
        printf("\n\n");

        for (int i = 0; i < (int)node_list1.size(); i++) {
            int index = node_list1[i].query_idx;
            CSequence& query = alignment[index];
            printf("%3d: ", index);
            for (int j = 0; j < query.GetLength(); j++) {
                printf("%c", query.GetPrintableLetter(j));
            }
            printf("\n");
        }
        printf("\n");
        for (int i = 0; i < (int)node_list2.size(); i++) {
            int index = node_list2[i].query_idx;
            CSequence& query = alignment[index];
            printf("%3d: ", index);
            for (int j = 0; j < query.GetLength(); j++) {
                printf("%c", query.GetPrintableLetter(j));
            }
            printf("\n");
        }
    }
    //-------------------------------------------
}

int
CMultiAligner::x_RealignSequences(
                   const TPhyTreeNode *input_cluster,
                   vector<CSequence>& alignment,
                   CNcbiMatrix<CHitList>& pair_info,
                   int sp_score,
                   int iteration)
{
    int num_queries = m_QueryData.size();

    vector<CTree::STreeLeaf> full_seq_list;
    CTree::ListTreeLeaves(GetTree(), full_seq_list, 0.0);

    vector<CTree::STreeLeaf> cluster_seq_list;
    CTree::ListTreeLeaves(input_cluster, cluster_seq_list, 0.0);

    //--------------------------------
    if (m_Verbose) {
        printf("cluster: ");
        for (int i = 0; i < (int)cluster_seq_list.size(); i++) {
            printf("%d ", cluster_seq_list[i].query_idx);
        }
        printf("\n");
    }
    //--------------------------------

    vector<int> cluster_idx;
    vector<int> other_idx;
    vector<CTree::STreeLeaf> other_seq_list;
    int cluster_size = cluster_seq_list.size();

    for (int i = 0; i < num_queries; i++) {
        int j;
        for (j = 0; j < cluster_size; j++) {
            if (full_seq_list[i].query_idx == 
                cluster_seq_list[j].query_idx) {
                break;
            }
        }
        if (j == cluster_size) {
            other_seq_list.push_back(full_seq_list[i]);
            other_idx.push_back(full_seq_list[i].query_idx);
        }
    }
    for (int i = 0; i < cluster_size; i++) {
        cluster_idx.push_back(cluster_seq_list[i].query_idx);
    }

    vector<CSequence> tmp_align = alignment;

    CSequence::CompressSequences(tmp_align, cluster_idx);
    CSequence::CompressSequences(tmp_align, other_idx);
    x_AlignProfileProfile(cluster_seq_list, other_seq_list,
                          tmp_align, pair_info, iteration);

    int new_sp_score = 0;
    for (int i = 0; i < num_queries - 1; i++) {
        for (int j = i + 1; j < num_queries; j++) {
            new_sp_score += CSequence::GetPairwiseScore(
                                    tmp_align[i],
                                    tmp_align[j],
                                    m_Aligner.GetMatrix(),
                                    m_Aligner.GetWg(),
                                    m_Aligner.GetWs());
        }
    }
    if (m_Verbose)
        printf("realigned SP score = %d\n\n", new_sp_score);

    if (new_sp_score > sp_score) {
        sp_score = new_sp_score;
        alignment.swap(tmp_align);
    }

    return sp_score;
}

void
CMultiAligner::x_AlignProgressive(
                 const TPhyTreeNode *tree,
                 vector<CSequence>& query_data,
                 CNcbiMatrix<CHitList>& pair_info,
                 int iteration)
{
    TPhyTreeNode::TNodeList_CI child(tree->SubNodeBegin());

    const TPhyTreeNode *left_child = *child++;
    if (!left_child->IsLeaf())
        x_AlignProgressive(left_child, query_data, 
                           pair_info, iteration);

    const TPhyTreeNode *right_child = *child;
    if (!right_child->IsLeaf())
        x_AlignProgressive(right_child, query_data, 
                           pair_info, iteration);

    // align the two children

    vector<CTree::STreeLeaf> node_list1, node_list2;
    CTree::ListTreeLeaves(left_child, node_list1, 
                         left_child->GetValue().GetDist());
    CTree::ListTreeLeaves(right_child, node_list2, 
                         right_child->GetValue().GetDist());

    x_AlignProfileProfile(node_list1, node_list2,
                          query_data, pair_info, iteration);
}

void 
CMultiAligner::x_FindConservedColumns(
                            vector<CSequence>& new_alignment,
                            CHitList& conserved)
{
    int num_queries = new_alignment.size();
    int align_length = new_alignment[0].GetLength();
    vector<double> avg_sp(align_length);
    vector<TRange> chosen_cols;
    const TNCBIScore (*matrix)[NCBI_FSM_DIM] = m_Aligner.GetMatrix().s;

    for (int j = 0; j < align_length; j++) {

        int k;

        // Columns containing gaps cannot be conserved

        for (k = 0; k < num_queries; k++) {
            if (new_alignment[k].GetLetter(j) == CSequence::kGapChar)
                break;
        }
        if (k < num_queries) {
            avg_sp[j] = -999;
            continue;
        }

        // calculate the sum-of-pairs score for the column

        double sp_score = 0;
        for (k = 0; k < num_queries - 1; k++) {
            for (int m = k + 1; m < num_queries; m++) {
                double accum = 0;
                CSequence::TFreqMatrix& matrix1 = new_alignment[k].GetFreqs();
                CSequence::TFreqMatrix& matrix2 = new_alignment[m].GetFreqs();

                double diff_freq1[kAlphabetSize];
                double diff_freq2[kAlphabetSize];

                for (int n = 0; n < kAlphabetSize; n++) {
                    if (matrix1(j, n) < matrix2(j, n)) {
                        accum += matrix1(j, n) * matrix[n][n];
                        diff_freq1[n] = 0.0;
                        diff_freq2[n] = matrix2(j, n) - matrix1(j, n);
                    }
                    else {
                        accum += matrix2(j, n) * matrix[n][n];
                        diff_freq1[n] = matrix1(j, n) - matrix2(j, n);
                        diff_freq2[n] = 0.0;
                     }
                }

                double sum = 0.0; 
                for (int n = 0; n < kAlphabetSize; n++) {
                    sum += diff_freq1[n];
                }
                if (sum > 0) {
                    for (int n = 0; n < kAlphabetSize; n++)
                        diff_freq1[n] /= sum;

                    for (int n = 0; n < kAlphabetSize; n++) {
                        for (int p = 0; p < kAlphabetSize; p++) {
                            accum += diff_freq1[n] * diff_freq2[p] *
                                     matrix[n][p];
                        }
                    }
                }

                sp_score += accum;
            }
        }

        // divide by the number of pairs in the column, to
        // get an 'average' sum of pairs score

        sp_score = sp_score / (num_queries * (num_queries - 1)/2);

        avg_sp[j] = sp_score;

        // if this average is high enough, this column is 
        // considered conserved. Find the offset into each 
        // unaligned sequence that corresponds to column j 
        // in the alignment, and save it

        if (sp_score >= m_ConservedCutoff) {
            if (!chosen_cols.empty() &&
                chosen_cols.back().GetTo() == j - 1) {
                chosen_cols.back().SetTo(j);
            }
            else {
                chosen_cols.push_back(TRange(j, j));
            }
        }
    }

    int i = 0;
    for (int j = 0; j < (int)chosen_cols.size(); j++) {
        if (chosen_cols[j].GetLength() > 1) {
            if (m_Verbose) {
                printf("constraint at position %3d - %3d\n",
                        chosen_cols[j].GetFrom(), chosen_cols[j].GetTo());
            }
            chosen_cols[i++] = chosen_cols[j];
        }
    }
    chosen_cols.resize(i);

    vector<TRange> range_nogap(num_queries);

    for (int j = 0; j < (int)chosen_cols.size(); j++) {

        TRange& curr_range = chosen_cols[j];

        for (int k = 0; k < num_queries; k++) {

            int offset1 = -1;
            int m;
            for (m = 0; m <= curr_range.GetFrom(); m++) {
                if (new_alignment[k].GetLetter(m) != CSequence::kGapChar)
                    offset1++;
            }

            int offset2 = offset1;
            for (; m <= curr_range.GetTo(); m++) {
                if (new_alignment[k].GetLetter(m) != CSequence::kGapChar)
                    offset2++;
            }
            range_nogap[k].Set(offset1, offset2);
        }

        for (int k = 0; k < num_queries - 1; k++) {
            for (int m = k + 1; m < num_queries; m++) {
                conserved.AddToHitList(new CHit(k, m, range_nogap[k],
                                 range_nogap[m], 1, CEditScript()));
            }
        }
    }

    for (int i = 0; i < conserved.Size(); i++)
        conserved.GetHit(i)->m_BitScore = 1.0;

    //------------------------------
    if (m_Verbose) {
        printf("Per-column average sum of pairs:\n");
        for (int j = 0; j < align_length; j++) {
            if (avg_sp[j] == -999)
                printf("     . ");
            else
                printf("%6.2f ", avg_sp[j]);
            if ((j+1)%10 == 0)
                printf("\n");
        }
        printf("\n");
    }
    //------------------------------
}

void
CMultiAligner::x_BuildAlignmentIterative(
                         vector<CTree::STreeEdge>& edges,
                         double cluster_cutoff)
{
    int num_queries = m_QueryData.size();
    int iteration = 0;
    int conserved_cols;
    int new_conserved_cols;
    int new_sp_score;
    int best_sp_score = INT4_MIN;

    CNcbiMatrix<CHitList> pair_info(num_queries, num_queries, CHitList());
    for (int i = 0; i < m_CombinedHits.Size(); i++) {
        CHit *hit = m_CombinedHits.GetHit(i);
        pair_info(hit->m_SeqIndex1, hit->m_SeqIndex2).AddToHitList(hit);
        pair_info(hit->m_SeqIndex2, hit->m_SeqIndex1).AddToHitList(hit);
    }
    CHitList conserved_regions;

    conserved_cols = 0;
    vector<CSequence> tmp_aligned = m_QueryData;

    x_AlignProgressive(GetTree(), tmp_aligned, 
                       pair_info, iteration);

    while (1) {

        int realign_sp_score = 0;
        for (int i = 0; i < num_queries - 1; i++) {
            for (int j = i + 1; j < num_queries; j++) {
                realign_sp_score += CSequence::GetPairwiseScore(
                                        tmp_aligned[i],
                                        tmp_aligned[j],
                                        m_Aligner.GetMatrix(),
                                        m_Aligner.GetWg(),
                                        m_Aligner.GetWs());
            }
        }
        for (int i = 0; i < 5; i++) {
            new_sp_score = realign_sp_score;
            for (int j = 0; j < (int)edges.size() &&
                           edges[j].distance >= cluster_cutoff; j++) {

                new_sp_score = x_RealignSequences(edges[j].node,
                                                  tmp_aligned,
                                                  pair_info, 
                                                  new_sp_score, 
                                                  iteration);
            }

            if (new_sp_score - realign_sp_score <= 
                             0.02 * abs(realign_sp_score))
                break;
            realign_sp_score = new_sp_score;
        }
        realign_sp_score = max(realign_sp_score, new_sp_score);

        //-------------------------------------------------
        if (m_Verbose) {
            for (int i = 0; i < num_queries; i++) {
                for (int j = 0; j < (int)tmp_aligned[i].GetLength(); j++) {
                    printf("%c", tmp_aligned[i].GetPrintableLetter(j));
                }
                printf("\n");
            }
            printf("sum of pairs = %d\n", realign_sp_score);
        }
        //-------------------------------------------------

        if (realign_sp_score > best_sp_score) {
            if (m_Verbose) {
                printf("REPLACE ALIGNMENT\n\n");
            }
            best_sp_score = realign_sp_score;
            m_Results = tmp_aligned;
        }

        // recompute the conserved columns based on the new alignment
        // first remove the last batch of conserved regions

        for (int i = 0; i < num_queries; i++) {
            for (int j = 0; j < num_queries; j++) {
                pair_info(i, j).ResetList();
            }
        }
        conserved_regions.PurgeAllHits();

        x_FindConservedColumns(tmp_aligned, conserved_regions);

        new_conserved_cols = 0;
        for (int i = 0; i < m_PatternHits.Size(); i++) {
            CHit *hit = m_PatternHits.GetHit(i);
            pair_info(hit->m_SeqIndex1, hit->m_SeqIndex2).AddToHitList(hit);
            pair_info(hit->m_SeqIndex2, hit->m_SeqIndex1).AddToHitList(hit);
            new_conserved_cols += hit->m_SeqRange1.GetLength();
        }
        for (int i = 0; i < conserved_regions.Size(); i++) {
            CHit *hit = conserved_regions.GetHit(i);
            pair_info(hit->m_SeqIndex1, hit->m_SeqIndex2).AddToHitList(hit);
            pair_info(hit->m_SeqIndex2, hit->m_SeqIndex1).AddToHitList(hit);
            new_conserved_cols += hit->m_SeqRange1.GetLength();
        }

        if (new_conserved_cols <= conserved_cols)
            break;

        iteration++;
        conserved_cols = new_conserved_cols;
        tmp_aligned = m_QueryData;

        x_AlignProgressive(GetTree(), tmp_aligned, 
                           pair_info, iteration);
    }

    for (int i = 0; i < num_queries; i++) {
        for (int j = 0; j < num_queries; j++) {
            pair_info(i, j).ResetList();
        }
    }
}


/// Callback for sorting tree edges in order of
/// decreasing edge weight
class compare_tree_edge_descending {
public:
    bool operator()(const CTree::STreeEdge& a, 
                    const CTree::STreeEdge& b) const {
        return a.distance > b.distance;
    }
};


void 
CMultiAligner::BuildAlignment()
{
    // write down all the tree edges, along with their weight

    vector<CTree::STreeEdge> edges;
    CTree::ListTreeEdges(GetTree(), edges);
    sort(edges.begin(), edges.end(), compare_tree_edge_descending());
    int num_edges = edges.size();

    int num_internal_edges = min(11, (int)(0.3 * num_edges + 0.5));
    double cluster_cutoff = INT4_MAX;
    int i;
    for (i = 0; i < num_internal_edges - 1; i++) {
        if (edges[i].distance > 2 * edges[i+1].distance) {
            cluster_cutoff = edges[i].distance;
            break;
        }
    }
    if (i == num_internal_edges - 1) {
        cluster_cutoff = edges[i].distance;
    }

    //---------------------
    if (m_Verbose) {
        for (int i = 0; i < num_edges; i++)
            printf("%f ", edges[i].distance);
        printf("cutoff = %f\n", cluster_cutoff);
    }
    //---------------------

    // Progressively align all sequences

    x_BuildAlignmentIterative(edges, cluster_cutoff);
}

END_SCOPE(cobalt)
END_NCBI_SCOPE

/*--------------------------------------------------------------------
  $Log$
  Revision 1.2  2005/11/08 17:54:19  papadopo
  ASSERT -> assert

  Revision 1.1  2005/11/07 18:14:00  papadopo
  Initial revision

--------------------------------------------------------------------*/
