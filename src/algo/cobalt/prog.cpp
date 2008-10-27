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
#include <algorithm>

/// @file prog.cpp
/// Perform a progressive multiple alignment

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Convert one hit into a constraint usable by CPSSMAligner
/// @param constraint list of previously generated constraints [in][out]
/// @param hit Constraint to convert [in]
///
static void
x_AddConstraint(vector<size_t>& constraint,
                CHit *hit)
{
    int seq1_start = hit->m_SeqRange1.GetFrom();
    int seq1_stop = hit->m_SeqRange1.GetTo();
    int seq2_start = hit->m_SeqRange2.GetFrom();
    int seq2_stop = hit->m_SeqRange2.GetTo();

    // add the top left corner of the constraint region,
    // *unless* it is at the beginning of one or both sequences.
    // This is a workaround to accomodate the handling of
    // constraint regions within CPSSMAligner

    if (!constraint.empty()) {
        int last = constraint.size();
        if (constraint[last-4] >= (size_t)seq1_start ||
            constraint[last-2] >= (size_t)seq2_start) {
            return;
        }
    }

    constraint.push_back(seq1_start);
    constraint.push_back(seq1_start);
    constraint.push_back(seq2_start);
    constraint.push_back(seq2_start);
    
    // constraint must be consistent, and be of size > 1
    // to cause another group of sequence offsets to be added

    if (seq1_start >= seq1_stop ||
        seq2_start >= seq2_stop)
        return;

    constraint.push_back(seq1_stop);
    constraint.push_back(seq1_stop);
    constraint.push_back(seq2_stop);
    constraint.push_back(seq2_stop);
}

/// Convert a constraint into a form usable by CPSSMAligner
/// @param constraint list of previously generated constraints [in][out]
/// @param hit Constraint to convert [in]
///
static void x_HitToConstraints(vector<size_t>& constraint,
                               CHit *hit)
{
    if (!(hit->HasSubHits())) {
        x_AddConstraint(constraint, hit);
    }
    else {
        // every sub-hit gets its own constraint

        NON_CONST_ITERATE(CHit::TSubHit, subitr, hit->GetSubHit()) {
            x_AddConstraint(constraint, *subitr);
        }
    }
}


/// Compute the residue frequencies of a collection of sequences
/// @param freq_data The computed frequencies [out]
/// @param query_data List of all sequences [in]
/// @param node_list List of suubset of sequences in query_data 
///                 that will contribute to the final residue 
///                 frequencies [in]
///
static void
x_FillResidueFrequencies(double **freq_data,
                    vector<CSequence>& query_data,
                    vector<CTree::STreeLeaf>& node_list)
{
    double sum = 0.0;

    // sum all the distances from the (implicit) tree root
    // to all sequences in node_list

    for (size_t i = 0; i < node_list.size(); i++) {
        sum += node_list[i].distance;
    }
    _ASSERT(sum >= 0.0);

    for (size_t i = 0; i < node_list.size(); i++) {

        // update the residue frequencies to include the influence
        // of a new sequence at a leaf in the tree

        int index = node_list[i].query_idx;
        double weight = node_list[i].distance / sum;

        // the residue frequencies of the sequence are scaled by
        // the fraction of 'sum' taken up by sequence i in the list.
        // Since node_list stores *reciprocal* distances, this penalizes
        // sequences further away from the tree root

        if (node_list[i].distance == 0 && sum == 0)
            weight = 1;
        else
            weight = node_list[i].distance / sum;

        int size = query_data[index].GetLength();
        CSequence::TFreqMatrix& matrix = query_data[index].GetFreqs();

        // add in the residue frequencies

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

/// Convert a sequence range to reflect gaps added to the
/// underlying sequence
/// @param range [in][out]
/// @param seq The sequence [in]
///
static void 
x_ExpandRange(TRange& range,
              CSequence& seq)
{
    int len = seq.GetLength();
    int i, offset;

    // convert range.From

    offset = -1;
    for (i = 0; i < len; i++) {
        if (seq.GetLetter(i) != CSequence::kGapChar)
            offset++;
        if (offset == range.GetFrom()) {
            range.SetFrom(i);
            break;
        }
    }

    // convert range.To

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
    _ASSERT(i < len);
}


/// Find the set of constraints to use for a profile-profile
/// alignment. This routine considers only constraints between
/// one collection of equal-size sequences and another collection
/// of equal-size sequences
/// @param constraint List of compute constraints, in the format
///               expected by CPSSMAligner [out]
/// @param alignment Current multiple alignment of sequences [in]
/// @param node_list1 List of sequences in first collection [in]
/// @param node_list2 List of sequences in second collection [in]
/// @param pair_info List of pairwise constraints (between sequences) [in]
/// @param iteration The iteration number [in]
///
void
CMultiAligner::x_FindConstraints(vector<size_t>& constraint,
                 vector<CSequence>& alignment,
                 vector<CTree::STreeLeaf>& node_list1,
                 vector<CTree::STreeLeaf>& node_list2,
                 CNcbiMatrix<CHitList>& pair_info,
                 int iteration)
{
    const int kMinAlignLength = 11;
    CHitList profile_hitlist;

    // first collect all of the constraints that pass from
    // one sequence collection to the other

    for (int i = 0; i < (int)node_list1.size(); i++) {

        for (int j = 0; j < (int)node_list2.size(); j++) {

            int seq1 = node_list1[i].query_idx;
            int seq2 = node_list2[j].query_idx;

            // for all constraints between seqience i in collection 1
            // and sequence j in collection 2

            for (int k = 0; k < pair_info(seq1, seq2).Size(); k++) {
                CHit *hit = pair_info(seq1, seq2).GetHit(k);
                CHit *new_hit = new CHit(seq1, seq2);
                bool swap_ranges = (seq1 != hit->m_SeqIndex1);
                
                // make a temporary hit to represent the constraint

                new_hit->m_Score = hit->m_Score;
                if (swap_ranges) {
                    new_hit->m_SeqRange1 = hit->m_SeqRange2;
                    new_hit->m_SeqRange2 = hit->m_SeqRange1;
                }
                else {
                    new_hit->m_SeqRange1 = hit->m_SeqRange1;
                    new_hit->m_SeqRange2 = hit->m_SeqRange2;
                }

                // add in any sub-hits if this is a domain hit (i.e.
                // all of the individual blocks of the constraint are
                // treated together)

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

            // check for interrupt
            if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
                NCBI_THROW(CMultiAlignerException, eInterrupt,
                           "Alignment interrupted");
            }
        }
    }

    if (profile_hitlist.Empty())
        return;

    // convert the range of each constraint into a range on
    // the aligned sequences, i.e. change each set of bounds
    // to account for gaps that have since been added to the
    // sequences

    for (int i = 0; i < profile_hitlist.Size(); i++) {
        CHit *hit = profile_hitlist.GetHit(i);
        x_ExpandRange(hit->m_SeqRange1, alignment[hit->m_SeqIndex1]);
        x_ExpandRange(hit->m_SeqRange2, alignment[hit->m_SeqIndex2]);

        NON_CONST_ITERATE(CHit::TSubHit, subitr, hit->GetSubHit()) {
            CHit *subhit = *subitr;
            x_ExpandRange(subhit->m_SeqRange1, alignment[subhit->m_SeqIndex1]);
            x_ExpandRange(subhit->m_SeqRange2, alignment[subhit->m_SeqIndex2]);
        }

        // check for interrupt
        if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
            NCBI_THROW(CMultiAlignerException, eInterrupt,
                       "Alignment interrupted");
        }
    }

    // put the constraints in canonical order

    profile_hitlist.SortByStartOffset();

    //--------------------------------------
    if (m_Verbose) {
        printf("possible constraints (offsets wrt input profiles):\n");
        for (int i = 0; i < profile_hitlist.Size(); i++) {
            CHit *hit = profile_hitlist.GetHit(i);
            if (hit->m_Score != 1)
                printf("query %3d %4d - %4d query %3d %4d - %4d score %d %c\n",
                   hit->m_SeqIndex1, 
                   hit->m_SeqRange1.GetFrom(), hit->m_SeqRange1.GetTo(),
                   hit->m_SeqIndex2, 
                   hit->m_SeqRange2.GetFrom(), hit->m_SeqRange2.GetTo(),
                   hit->m_Score,
                   hit->HasSubHits() ? 'd' : 'f');
        }
    }
    //--------------------------------------

    // build up a graph of constraints that is derived from
    // profile_hitlist

    vector<SGraphNode> graph;
    for (int j = 0; j < profile_hitlist.Size(); j++) {
        CHit *hit = profile_hitlist.GetHit(j);

        // for the first progressive alignment only, ignore 
        // constraints that are too small. We do not do this 
        // in later iterations because the constraints will include
        // low-scoring pattern hits and conserved columns, which
        // must be included

        if (iteration == 0 && hit->m_Score < 1000 &&
            (hit->m_SeqRange1.GetLength() < kMinAlignLength ||
             hit->m_SeqRange2.GetLength() < kMinAlignLength) )
            continue;

        // find out whether constraint j must override a previous
        // constraint that was added to the graph

        int i;
        for (i = 0; i < (int)graph.size(); i++) {
            CHit *ghit = graph[i].hit;

            // if constraint i in the graph is already consistent with
            // constraint j, then no overriding is possible

            if ((ghit->m_SeqRange1.StrictlyBelow(hit->m_SeqRange1) &&
                 ghit->m_SeqRange2.StrictlyBelow(hit->m_SeqRange2)) ||
                (hit->m_SeqRange1.StrictlyBelow(ghit->m_SeqRange1) && 
                 hit->m_SeqRange2.StrictlyBelow(ghit->m_SeqRange2)) ) {
                 continue;
            }

            // constraint j overwrites constraint i in the graph if
            // both of its ranges are 'near' constraint i, 
            // the two constraints lie on the same diagonal, and 
            // constraint j has the higher score

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

                if (ghit->m_Score < hit->m_Score)
                    graph[i].hit = hit;
                break;
            }
        }

        // constraint j is sufficiently different from the others
        // in the graph that the graph should expand to include 
        // constraint j

        if (i == (int)graph.size())
            graph.push_back(SGraphNode(hit, j));

        // check for interrupt
        if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
            NCBI_THROW(CMultiAlignerException, eInterrupt,
                       "Alignment interrupted");
        }
    }

    if (graph.empty())
        return;

    // we now have a list of constraints from which a highest-
    // scoring subset can be selected. First, though, we have to
    // assign a score to each constraint in the graph

    if (iteration == 0) {

        // for the first iteration, the constraints in the graph 
        // have scores assigned to them based on sequence similarity, 
        // and it makes sense to reward constraints that are similar
        // across many different sequences

        int num_hits = profile_hitlist.Size();
        vector<int> list1, list2;
        list1.reserve(num_hits);
        list2.reserve(num_hits);

        // for constraint i

        for (int i = 0; i < (int)graph.size(); i++) {
            CHit *ghit = graph[i].hit;
            list1.clear();
            list2.clear();
    
            // the score in the graph for constraint i is the score
            // for constraint i, scaled by the product of the number of
            // sequences in each collection that contain constraints
            // compatible with constraint i. Two constraints are compatible
            // if the start and end points of both constraints are 'close' 
            // to each other and lie on the same diagonals

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

                    // constraint j is compatible with constraint
                    // i. If constraint j is between a different pair
                    // of sequences than we've seen so far, it
                    // constributes to the score of constraint i

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

            // scale the score of constraint i to reward the situation
            // when multiple different constraints between the two
            // sequence collection agree with constraint i

            graph[i].best_score = graph[i].hit->m_Score * 
                                  list1.size() * list2.size();
            // check for interrupt
            if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
                NCBI_THROW(CMultiAlignerException, eInterrupt,
                           "Alignment interrupted");
            }
        }
    }
    else {

        // iterations beyond the first contain constraints whose
        // scores were assigned arbitraily. In this case, just
        // propagate those scores directly to the graph

        NON_CONST_ITERATE(vector<SGraphNode>, itr, graph) {
            itr->best_score = itr->hit->m_Score;
        }
    }

    // find the highest-scoring consistent subset of constraints

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

        // convert the constraint into a form the aligner can use

        x_HitToConstraints(constraint, hit);
        best_path = best_path->path_next;
    }
    _ASSERT(!constraint.empty());
}


/// Align two collections of sequences. All sequences within
/// a single collection begin with the same size
/// @param node_list1 List of sequence number in first collection [in]
/// @param node_list2 List of sequence number in second collection [in]
/// @param alignment Complete list of aligned sequences (may contain 
///               sequences that will not be aligned). On output, new gaps
///               will be propagated to all affected sequences [in][out]
/// @param pair_info Constraints that may be used in the alignment [in]
/// @param iteration The iteration number [in]
///
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

    // determine the list of constraints to use

    x_FindConstraints(constraint, alignment, node_list1,
                      node_list2, pair_info, iteration);

    //-------------------------------
    if (m_Verbose) {
        printf("constraints: ");
        for (int i = 0; i < (int)constraint.size(); i+=4) {
            printf("(seq1 %d seq2 %d)->", (int)constraint[i], 
                                        (int)constraint[i+2]);
        }
        printf("\n");
    }
    //-------------------------------
    m_Aligner.SetPattern(constraint);

    // if there is a large length disparity between the two
    // profiles, reduce or eliminate end gap penalties. Also 
    // scale up the gap penalties to match those of the score matrix
    
    m_Aligner.SetWg(m_GapOpen * kScale);
    m_Aligner.SetStartWg(m_EndGapOpen * kScale);
    m_Aligner.SetEndWg(m_EndGapOpen * kScale);
    m_Aligner.SetWs(m_GapExtend * kScale);
    m_Aligner.SetStartWs(m_EndGapExtend * kScale);
    m_Aligner.SetEndWs(m_EndGapExtend * kScale);

    if (freq1_size > 1.2 * freq2_size ||
        freq2_size > 1.2 * freq1_size) {
       m_Aligner.SetStartWs(m_EndGapExtend * kScale / 2);
       m_Aligner.SetEndWs(m_EndGapExtend * kScale / 2); 
    }
    if ((freq1_size > 1.5 * freq2_size ||
         freq2_size > 1.5 * freq1_size) &&
         !constraint.empty()) {
        m_Aligner.SetEndSpaceFree(true, true, true, true);
    }

    // run the aligner, scale the penalties back down

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
    // and propagate new gaps to the affected sequences

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


/// Number of 'letters' in the reduced alphabet
static const int kNumClasses = 10;

/// Background frequencies of the letters in the reduced
/// alphabet (each is the sum of the Robinson-Robinson
/// frequencies of the underlying protein letters that
/// appear in that letter class)
///
static const double kDerivedFreqs[kNumClasses] = {
        0.000000, 0.019250, 0.073770, 0.052030, 0.228450,
        0.084020, 0.021990, 0.207660, 0.108730, 0.204100
};

/// Mapping from protein letters to reduced alphabet letters
static const int kRes2Class[kAlphabetSize] = {
        0, 7, 9, 1, 9, 9, 5, 2, 6, 4, 8, 4, 4,
        9, 3, 9, 8, 7, 7, 4, 5, 0, 5, 9, 0, 0
};

/// Calculate the entropy score of one column of a multiple
/// alignment (see the COBALT papaer for details)
/// @param align The alignment [in]
/// @param col the column number to score
/// @return the computed score
double
CMultiAligner::x_GetScoreOneCol(vector<CSequence>& align, int col)
{
    int count[kNumClasses];
    double freq[kNumClasses];
    int num_queries = align.size();

    double r = 1.0 / num_queries;

    for (int j = 0; j < kNumClasses; j++)
        count[j] = 0;

    for (int j = 0; j < num_queries; j++)
        count[kRes2Class[align[j].GetLetter(col)]]++;

    for (int j = 1; j < kNumClasses; j++)
        freq[j] = count[j] * r;

    double H1 = 0;
    double H2 = 0;
    double H3 = 0;
    for (int j = 1; j < kNumClasses; j++) {
        if (count[j] > 0 && kDerivedFreqs[j] > 0) {
            H1 += freq[j] * log((num_queries - 1)/m_Pseudocount + 
                                (count[j] - 1)/kDerivedFreqs[j]);

            H2 += freq[j];
            H3 += kDerivedFreqs[j];
        }
    }

    return H1 - H2 * log((num_queries - 1) * 
                         (m_Pseudocount+H3)/m_Pseudocount);
}


/// Compute the entropy score of a multiple alignment
/// @param align complete multiple alignment [in]
/// @return the alignment score
///
double
CMultiAligner::x_GetScore(vector<CSequence>& align)
{
    int align_len = align[0].GetLength();
    double H = 0;

    for (int i = 0; i < align_len; i++)
        H += x_GetScoreOneCol(align, i);

    return H;
}


/// Perform a single bipartition on a multiple alignment
/// @param input_cluster A tree describing sequences that form one half
///                 of the bipartition [in]
/// @param alignment A complete multiple alignment, updated with the
///               the bipartition results if they have a higher score [in][out]
/// @param pair_info Pairwise constraints on the alignment [in]
/// @param score Starting alignment score [in]
/// @param iteration The iteration number [in]
/// @return The score of the new multiple alignment
///
double
CMultiAligner::x_RealignSequences(
                   const TPhyTreeNode *input_cluster,
                   vector<CSequence>& alignment,
                   CNcbiMatrix<CHitList>& pair_info,
                   double score,
                   int iteration)
{
    int num_queries = m_QueryData.size();

    // list all the sequences in the tree

    vector<CTree::STreeLeaf> full_seq_list;
    CTree::ListTreeLeaves(GetTree(), full_seq_list, 0.0);

    // list the sequence that form one of the clusters to align

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

    // other_seq_list get the OIDs of sequences that do
    // not participate in cluster_seq_list

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

    // squeeze out all-gap columns from each cluster, then
    // realign the two clusters

    CSequence::CompressSequences(tmp_align, cluster_idx);
    CSequence::CompressSequences(tmp_align, other_idx);
    x_AlignProfileProfile(cluster_seq_list, other_seq_list,
                          tmp_align, pair_info, iteration);

    // replace the input alignment with the alignment result
    // if the latter has a higher score

    double new_score = x_GetScore(tmp_align);
    if (m_Verbose)
        printf("realigned score = %f\n\n", new_score);

    if (new_score > score) {
        score = new_score;
        alignment.swap(tmp_align);
    }

    return score;
}

/// Main driver for progressive alignment
/// @param tree Alignment guide tree [in]
/// @param query_data The sequences to align. The first call assumes
///               this list contains the unaligned sequences, and 
///               intermediate alignment stages will update the list
///               as alignment progresses. On output query_data contains
///               the aligned version of all sequences [in][out]
/// @param pair_info List of alignment constraints [in]
/// @param iteration The iteration number [in]
void
CMultiAligner::x_AlignProgressive(
                 const TPhyTreeNode *tree,
                 vector<CSequence>& query_data,
                 CNcbiMatrix<CHitList>& pair_info,
                 int iteration)
{
    TPhyTreeNode::TNodeList_CI child(tree->SubNodeBegin());

    // recursively convert each subtree into a multiple alignment

    const TPhyTreeNode *left_child = *child++;
    if (!left_child->IsLeaf())
        x_AlignProgressive(left_child, query_data, 
                           pair_info, iteration);

    const TPhyTreeNode *right_child = *child;
    if (!right_child->IsLeaf())
        x_AlignProgressive(right_child, query_data, 
                           pair_info, iteration);

    // align the two subtrees

    vector<CTree::STreeLeaf> node_list1, node_list2;
    CTree::ListTreeLeaves(left_child, node_list1, 
                         left_child->GetValue().GetDist());
    CTree::ListTreeLeaves(right_child, node_list2, 
                         right_child->GetValue().GetDist());

    x_AlignProfileProfile(node_list1, node_list2,
                          query_data, pair_info, iteration);

    // check for interrupt
    if (m_Interrupt && (*m_Interrupt)(&m_ProgressMonitor)) {
        NCBI_THROW(CMultiAlignerException, eInterrupt,
                   "Alignment interrupted");
    }
}

/// Create a list of constraints that reflect conserved columns
/// in a multiple alignment
/// @param new_alignment The multiple alignment to analyze [in]
/// @param conserved The list of pairwise constraints [out]
///
void 
CMultiAligner::x_FindConservedColumns(
                            vector<CSequence>& new_alignment,
                            CHitList& conserved)
{
    int num_queries = new_alignment.size();
    int align_length = new_alignment[0].GetLength();
    vector<double> hvec(align_length);
    vector<TRange> chosen_cols;
    int i, j, k, m;

    // find and save the score of each column

    for (i = 0; i < align_length; i++) {
        hvec[i] = x_GetScoreOneCol(new_alignment, i);
    }

    // use the above to find all the ranges of consecutive
    // columns whose score all exceed the cutoff for being conserved.

    for (i = 0; i < align_length; i++) {
        if (hvec[i] >= m_ConservedCutoff) {
            if (!chosen_cols.empty() &&
                 chosen_cols.back().GetTo() == i-1) {
                chosen_cols.back().SetTo(i);
            }
            else {
                chosen_cols.push_back(TRange(i, i));
            }
        }
    }

    // A conserved range must be at least 2 columns

    for (i = j = 0; j < (int)chosen_cols.size(); j++) {
        if (chosen_cols[j].GetLength() > 1) {
            chosen_cols[i++] = chosen_cols[j];
        }
    }
    chosen_cols.resize(i);

    //-------------------------------------
    if (m_Verbose) {
        for (i = 0; i < (int)chosen_cols.size(); i++) {
            printf("constraint at position %3d - %3d\n",
                chosen_cols[i].GetFrom(), chosen_cols[i].GetTo());
        }
    }
    //-------------------------------------

    vector<TRange> range_nogap(num_queries);

    for (i = 0; i < (int)chosen_cols.size(); i++) {

        TRange& curr_range = chosen_cols[i];
        int start = curr_range.GetFrom();
        int length = curr_range.GetLength();

        // find the coordinates of range i on each of the
        // unaligned sequences

        for (j = 0; j < num_queries; j++) {

            // a sequence that participates in the constraints
            // cannot have a gap within range i

            for (k = 0; k < length; k++) {
                if (new_alignment[j].GetLetter(start+k) == CSequence::kGapChar)
                    break;
            }
            if (k < length) {
                range_nogap[j].SetEmpty();
                continue;
            }

            // locate the start and end range on sequence j

            int offset1 = -1;
            for (m = 0; m <= curr_range.GetFrom(); m++) {
                if (new_alignment[j].GetLetter(m) != CSequence::kGapChar)
                    offset1++;
            }

            int offset2 = offset1;
            for (; m <= curr_range.GetTo(); m++) {
                if (new_alignment[j].GetLetter(m) != CSequence::kGapChar)
                    offset2++;
            }
            range_nogap[j].Set(offset1, offset2);
        }

        // convert range i of conserved columns into an
        // all-against-all collection of pairwise constraints

        for (k = 0; k < num_queries - 1; k++) {
            for (m = k + 1; m < num_queries; m++) {
                if (!range_nogap[k].Empty() && !range_nogap[m].Empty()) {
                    conserved.AddToHitList(new CHit(k, m, range_nogap[k],
                                                    range_nogap[m], 1000, 
                                                    CEditScript()));
                }
            }
        }
    }

    //------------------------------
    if (m_Verbose) {
        printf("Per-column score\n");
        for (i = 0; i < align_length; i++) {
            printf("%6.2f ", hvec[i]);
            if ((i+1)%10 == 0)
                printf("\n");
        }
        printf("\n");
    }
    //------------------------------
}

/// Main driver for the progressive alignment process
/// @param edges List of tree edges sorted by decreasing edge length [in]
/// @param cluster_cutoff The minimum distance a tree edge must have
///                      to be the subject of a bipartition [in]
///
void
CMultiAligner::x_BuildAlignmentIterative(
                         vector<CTree::STreeEdge>& edges,
                         double cluster_cutoff)
{
    int num_queries = m_QueryData.size();
    int iteration = 0;
    int conserved_cols;
    int new_conserved_cols;
    double new_score;
    double best_score = INT4_MIN;

    // the initial list of constraints consists of the
    // filtered list of blast alignments plus any user-defined
    // constraints

    CNcbiMatrix<CHitList> pair_info(num_queries, num_queries, CHitList());
    for (int i = 0; i < m_CombinedHits.Size(); i++) {
        CHit *hit = m_CombinedHits.GetHit(i);
        pair_info(hit->m_SeqIndex1, hit->m_SeqIndex2).AddToHitList(hit);
        pair_info(hit->m_SeqIndex2, hit->m_SeqIndex1).AddToHitList(hit);
    }
    for (int i = 0; i < m_UserHits.Size(); i++) {
        CHit *hit = m_UserHits.GetHit(i);
        pair_info(hit->m_SeqIndex1, hit->m_SeqIndex2).AddToHitList(hit);
        pair_info(hit->m_SeqIndex2, hit->m_SeqIndex1).AddToHitList(hit);
    }
    CHitList conserved_regions;

    conserved_cols = 0;
    vector<CSequence> tmp_aligned = m_QueryData;

    // perform the initial progressive alignment

    x_AlignProgressive(GetTree(), tmp_aligned, 
                       pair_info, iteration);

    while (1) {

        // compute the previous alignment score

        double realign_score = x_GetScore(tmp_aligned);
        if (m_Verbose)
            printf("start score: %f\n", realign_score);

        // repeat the complete bipartition process until
        // either the best score stops improving or we've done
        // too many bipartitions

        for (int i = 0; i < 5; i++) {
            new_score = realign_score;

            // a single bipartition consists of systematically
            // breaking the largest edges in the tree and realigning
            // the subtrees to either side of that edge. Repeat 
            // until the edges get too small

            for (int j = 0; j < (int)edges.size() &&
                           edges[j].distance >= cluster_cutoff; j++) {

                new_score = x_RealignSequences(edges[j].node,
                                               tmp_aligned,
                                               pair_info, 
                                               new_score, 
                                               iteration);
            }

            // quit if the best score from bipartition i improved 
            // the score of realignment i-1 by less than 2% 

            if (new_score - realign_score <= 0.02 * fabs(realign_score)) {
                break;
            }
            realign_score = new_score;
        }
        realign_score = max(realign_score, new_score);

        //-------------------------------------------------
        if (m_Verbose) {
            for (int i = 0; i < num_queries; i++) {
                for (int j = 0; j < (int)tmp_aligned[i].GetLength(); j++) {
                    printf("%c", tmp_aligned[i].GetPrintableLetter(j));
                }
                printf("\n");
            }
            printf("score = %f\n", realign_score);
        }
        //-------------------------------------------------

        if (realign_score > best_score) {

            // the current iteration has improved the alignment

            if (m_Verbose) {
                printf("REPLACE ALIGNMENT\n\n");
            }
            best_score = realign_score;
            m_Results = tmp_aligned;    // will always happen at least once

            if (!m_Iterate)
                break;

            m_ProgressMonitor.stage = eIterativeAlignment;
        }

        // if iteration is allowed: recompute the conserved 
        // columns based on the new alignment first remove 
        // the last batch of conserved regions

        for (int i = 0; i < num_queries; i++) {
            for (int j = 0; j < num_queries; j++) {
                pair_info(i, j).ResetList();
            }
        }
        conserved_regions.PurgeAllHits();

        x_FindConservedColumns(tmp_aligned, conserved_regions);

        // build up the list of conserved columns again, using
        // phi-pattern constraints, user-defined constraints and
        // the current collection of columns marked as conserved

        new_conserved_cols = 0;
        for (int i = 0; i < m_PatternHits.Size(); i++) {
            CHit *hit = m_PatternHits.GetHit(i);
            pair_info(hit->m_SeqIndex1, hit->m_SeqIndex2).AddToHitList(hit);
            pair_info(hit->m_SeqIndex2, hit->m_SeqIndex1).AddToHitList(hit);
            new_conserved_cols += hit->m_SeqRange1.GetLength();
        }
        for (int i = 0; i < m_UserHits.Size(); i++) {
            CHit *hit = m_UserHits.GetHit(i);
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

        // only perform another iteration of the number of
        // columns participating in constraints has increased

        if (new_conserved_cols <= conserved_cols)
            break;

        iteration++;
        conserved_cols = new_conserved_cols;
        tmp_aligned = m_QueryData;

        // do the next progressive alignment

        x_AlignProgressive(GetTree(), tmp_aligned, 
                           pair_info, iteration);
    }

    // clean up the constraints

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
CMultiAligner::x_BuildAlignment()
{
    m_ProgressMonitor.stage = eProgressiveAlignment;

    // write down all the tree edges, along with their weight

    vector<CTree::STreeEdge> edges;
    CTree::ListTreeEdges(GetTree(), edges);
    sort(edges.begin(), edges.end(), compare_tree_edge_descending());
    int num_edges = edges.size();

    // choose the maximum number of edges that the bipartition
    // phase will use; each edge will be split and the subtrees on
    // either side of the edge will be realigned

    int num_internal_edges = min(11, (int)(0.3 * num_edges + 0.5));


    // choose the cluster cutoff; this is the first big jump in
    // the length of tree edges, and marks the limit beyond which
    // we won't try to bipartition

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
        for (i = 0; i < num_edges; i++)
            printf("%f ", edges[i].distance);
        printf("cutoff = %f\n", cluster_cutoff);
    }
    //---------------------

    // progressively align all sequences

    x_BuildAlignmentIterative(edges, cluster_cutoff);
}

END_SCOPE(cobalt)
END_NCBI_SCOPE
