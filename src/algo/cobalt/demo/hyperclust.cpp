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

File name: hyperclust.cpp

Author: Jason Papadopoulos

Contents: Distances and clustering of multiple alignment profiles

******************************************************************************/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <serial/iterator.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/reader_exception.hpp>
#include <algo/align/nw/nw_pssm_aligner.hpp>
#include <algo/cobalt/cobalt.hpp>
#include "cobalt_app_util.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(cobalt);

class CMultiApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int Run(void);
    virtual void Exit(void);

    CRef<CObjectManager> m_ObjMgr;
};

void CMultiApplication::Init(void)
{
    HideStdArgs(fHideLogfile | fHideConffile | fHideFullVersion | fHideXmlHelp
                | fHideDryRun);

    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(), 
                              "Distances and clustering of multiple alignment"
                              " profiles");

    arg_desc->AddKey("i", "infile", "file containing names of alignment files",
                     CArgDescriptions::eInputFile);
    arg_desc->AddOptionalKey("d", "defline_file", 
                     "file containing deflines corrsponding to alignment files",
                     CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("first", "first", 
                     "produce alignments from alignment number 'first' "
                     "to all succeeding alignments in the list",
                     CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("last", "last", 
                     "stop after aligning index 'last' to all succeeding "
                     "alignments in the list (ignored by default)",
                     CArgDescriptions::eInteger, "0");
    arg_desc->AddDefaultKey("g0", "penalty", 
                     "gap open penalty for initial/terminal gaps",
                     CArgDescriptions::eInteger, "5");
    arg_desc->AddDefaultKey("e0", "penalty", 
                     "gap extend penalty for initial/terminal gaps",
                     CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("g1", "penalty", 
                     "gap open penalty for middle gaps",
                     CArgDescriptions::eInteger, "11");
    arg_desc->AddDefaultKey("e1", "penalty", 
                     "gap extend penalty for middle gaps",
                     CArgDescriptions::eInteger, "1");
    arg_desc->AddDefaultKey("matrix", "matrix", 
                     "score matrix to use",
                     CArgDescriptions::eString, "BLOSUM62");
    arg_desc->AddDefaultKey("v", "verbose", 
                     "turn on verbose output",
                     CArgDescriptions::eBoolean, "F");
    arg_desc->AddDefaultKey("local", "local", 
                     "reduce end gap penalties in profile-profile alignment",
                     CArgDescriptions::eBoolean, "T");
    arg_desc->AddFlag("pairs", "Report pairwise distances up to a cutoff "
                               "value instead of the distance matrix");
    arg_desc->AddDefaultKey("cutoff", "distance",
                            "Maximum pairwise distance to report",
                            CArgDescriptions::eDouble, "0.75");

    SetupArgDescriptions(arg_desc.release());
}

static blast::TSeqLocVector
x_GetSeqLocFromStream(CNcbiIstream& instream, CObjectManager& objmgr)
{
    blast::TSeqLocVector retval;

    CStreamLineReader line_reader(instream);
    CFastaReader fr(line_reader, CFastaReader::fAssumeProt |
                    CFastaReader::fForceType |
                    /*CFastaReader::fOneSeq |*/
                    CFastaReader::fParseRawID | CFastaReader::fNoParseID);

    // read one query at a time, and use a separate seq_entry,
    // scope, and lowercase mask for each query. This lets different
    // query sequences have the same ID. Later code will distinguish
    // between queries by using different elements of retval[]

    while (!line_reader.AtEOF()) {
        CRef<CScope> scope(new CScope(objmgr));

        scope->AddDefaults();

        CRef<CSeq_entry> entry = fr.ReadOneSeq();


        if (entry == 0) {
            NCBI_THROW(CObjReaderException, eInvalid, 
                        "Could not retrieve seq entry");
        }

        scope->AddTopLevelSeqEntry(*entry);
        CTypeConstIterator<CBioseq> itr(ConstBegin(*entry));
        CRef<CSeq_loc> seqloc(new CSeq_loc());
        seqloc->SetWhole().Assign(*itr->GetId().front());
        blast::SSeqLoc sl(seqloc, scope);
        retval.push_back(sl);
    }
    return retval;
}

void  
x_SetScoreMatrix(const char *matrix_name,
               CPSSMAligner& aligner)
{     
   if (strcmp(matrix_name, "BLOSUM62") == 0)
       aligner.SetScoreMatrix(&NCBISM_Blosum62);
   else if (strcmp(matrix_name, "BLOSUM45") == 0)
       aligner.SetScoreMatrix(&NCBISM_Blosum45);
   else if (strcmp(matrix_name, "BLOSUM80") == 0)
       aligner.SetScoreMatrix(&NCBISM_Blosum80);
   else if (strcmp(matrix_name, "PAM30") == 0)
       aligner.SetScoreMatrix(&NCBISM_Pam30);
   else if (strcmp(matrix_name, "PAM70") == 0)
       aligner.SetScoreMatrix(&NCBISM_Pam70);
   else if (strcmp(matrix_name, "PAM250") == 0)
       aligner.SetScoreMatrix(&NCBISM_Pam250);
}   

static const int kScaleFactor = 100;

static void
x_FillResidueFrequencies(double **freq_data,
                    vector<CSequence>& query_data)
{
    int align_length = query_data[0].GetLength();
    int num_seqs = query_data.size();
    for (int i = 0; i < align_length; i++) {
        for (int j = 0; j < num_seqs; j++)
            freq_data[i][query_data[j].GetLetter(i)]++;
    }
}

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

static int 
x_ScoreFromTranscriptCore(CPSSMAligner& aligner,
                          double **freq1, int len1,
                          double **freq2, int len2,
                          int end_gap_open,
                          int end_gap_extend)
{
    const TNCBIScore (*sm) [NCBI_FSM_DIM] = aligner.GetMatrix().s;
    const CNWAligner::TTranscript& transcript = aligner.GetTranscript(false);
    int offset1 = -1;
    int offset2 = -1;
    int state1 = 0;
    int state2 = 0;

    const size_t dim = transcript.size();
    double dscore = 0.0;

    for(size_t i = 0; i < dim; ++i) {

        int wg1 = end_gap_open, ws1 = end_gap_extend;
        int wg2 = end_gap_open, ws2 = end_gap_extend;

        if (offset1 >= 0 && offset1 < len1 - 1) {
            wg1 = aligner.GetWg();
            ws1 = aligner.GetWs();
        }

        if (offset2 >= 0 && offset2 < len2 - 1) {
            wg2 = aligner.GetWg();
            ws2 = aligner.GetWs();
        }


        CNWAligner::ETranscriptSymbol ts = transcript[i];
        switch(ts) {

        case CNWAligner::eTS_Replace:
        case CNWAligner::eTS_Match: {
            state1 = state2 = 0;
            ++offset1; ++offset2;
            double accum = 0.0, sum = 0.0;
            double diff_freq1[kAlphabetSize];
            double diff_freq2[kAlphabetSize];

            for (int m = 1; m < kAlphabetSize; m++) {
                if (freq1[offset1][m] < freq2[offset2][m]) {
                    accum += freq1[offset1][m] * (double)sm[m][m];
                    diff_freq1[m] = 0.0;
                    diff_freq2[m] = freq2[offset2][m] - freq1[offset1][m];
                }
                else {
                    accum += freq2[offset2][m] * (double)sm[m][m];
                    diff_freq1[m] = freq1[offset1][m] - 
                                    freq2[offset2][m];
                    diff_freq2[m] = 0.0;
                }
            }

            if (freq1[offset1][0] <= freq2[offset2][0]) {
                for (int m = 1; m < kAlphabetSize; m++)
                    sum += diff_freq1[m];
            } else {
                for (int m = 1; m < kAlphabetSize; m++)
                    sum += diff_freq2[m];
            }

            if (sum > 0) {
                if (freq1[offset1][0] <= freq2[offset2][0]) {
                    for (int m = 1; m < kAlphabetSize; m++)
                        diff_freq1[m] /= sum;
                } else {
                    for (int m = 1; m < kAlphabetSize; m++)
                        diff_freq2[m] /= sum;
                }

                for (int m = 1; m < kAlphabetSize; m++) {
                    for (int n = 1; n < kAlphabetSize; n++) {
                        accum += diff_freq1[m] * 
                                 diff_freq2[n] * 
                                (double)sm[m][n];
                    }
                }
            }
            dscore += accum * kScaleFactor +
                   freq1[offset1][0] * aligner.GetWs() * (1-freq2[offset2][0]) +
                   freq2[offset2][0] * aligner.GetWs() * (1-freq1[offset1][0]);
        }
        break;

        case CNWAligner::eTS_Insert: {
            ++offset2;
            if(state1 != 1) dscore += wg1 * (1.0 - freq2[offset2][0]);
            state1 = 1; state2 = 0;
            dscore += ws1;
        }
        break;

        case CNWAligner::eTS_Delete: {
            ++offset1;
            if(state2 != 1) dscore += wg2 * (1.0 - freq1[offset1][0]);
            state1 = 0; state2 = 1;
            dscore += ws2;
        }
        break;
        
        default: {
             break;
        }
        }
    }

    return (int)(dscore + 0.5);
}

static double ** 
x_GetProfile(vector<CSequence>& alignment)
{
    double **freq_data;
    int freq_size = alignment[0].GetLength();

    // build a set of residue frequencies for the
    // sequences in the left subtree

    freq_data = new double* [freq_size];
    freq_data[0] = new double[kAlphabetSize * freq_size];

    for (int i = 1; i < freq_size; i++)
        freq_data[i] = freq_data[0] + kAlphabetSize * i;

    memset(freq_data[0], 0, kAlphabetSize * freq_size * sizeof(double));
    x_FillResidueFrequencies(freq_data, alignment);
    x_NormalizeResidueFrequencies(freq_data, freq_size);
    return freq_data;
}

/*
static void 
x_FreeProfile(double **freq_data)
{
    delete [] freq_data[0];
    delete [] freq_data;
}
*/

int
x_AlignProfileProfile(double **freq1_data, int freq1_size,
                      double **freq2_data, int freq2_size,
                      CPSSMAligner& aligner,
                      bool local_alignment)
{
    int score;
    int end_gap_open = aligner.GetStartWg();
    int end_gap_extend = aligner.GetStartWs();

    aligner.SetSequences((const double**)freq1_data, freq1_size, 
                         (const double**)freq2_data, freq2_size, 
                         kScaleFactor);
    aligner.Run();
    score =  x_ScoreFromTranscriptCore(aligner, 
                                       freq1_data, freq1_size,
                                       freq2_data, freq2_size,
                                       local_alignment ? 0 : end_gap_open,
                                       local_alignment ? 0 : end_gap_extend);
    return score;
}

static double
x_GetSelfScore(double **freq_data, int freq_size,
               SNCBIFullScoreMatrix& matrix)
{
    double score = 0.0;
    for (int i = 0; i < freq_size; i++) {
        for (size_t j = 1; j < kAlphabetSize; j++)
            score += freq_data[i][j] * matrix.s[j][j];
    }
    return kScaleFactor * score;
}

typedef struct {
    char name[500];
    vector<CSequence> align;
    int length;
    double self_score;
    double **profile;
} SAlignEntry;

static void 
x_PrintTree(const TPhyTreeNode *tree, int level, 
            vector<SAlignEntry>& aligns)
{
    int i, j;

    for (i = 0; i < level; i++)
        printf("    ");

    printf("node: ");
    if (tree->GetValue().GetId() >= 0)
        printf("cluster %d (%s) ", tree->GetValue().GetId(),
                                aligns[tree->GetValue().GetId()].name);
    if (tree->GetValue().IsSetDist())
        printf("distance %lf", tree->GetValue().GetDist());
    printf("\n");

    if (tree->IsLeaf())
        return;

    TPhyTreeNode::TNodeList_CI child(tree->SubNodeBegin());

    j = 0;
    while (child != tree->SubNodeEnd()) {
        for (i = 0; i < level; i++)
            printf("    ");

        printf("%d:\n", j);
        x_PrintTree(*child, level + 1, aligns);

        j++;
        child++;
    }
}

static void
x_FillNewDistanceMatrix(const TPhyTreeNode *node, 
                        CDistMethods::TMatrix& matrix)
{
    if (node->IsLeaf())
        return;

    vector<CTree::STreeLeaf> left_leaves;
    vector<CTree::STreeLeaf> right_leaves;

    TPhyTreeNode::TNodeList_CI child(node->SubNodeBegin());
    CTree::ListTreeLeaves(*child, left_leaves, 
                          (*child)->GetValue().GetDist());
    child++;
    CTree::ListTreeLeaves(*child, right_leaves, 
                          (*child)->GetValue().GetDist());

    for (size_t i = 0; i < left_leaves.size(); i++) {
        for (size_t j = 0; j < right_leaves.size(); j++) {
            int idx1 = left_leaves[i].query_idx;
            double dist1 = left_leaves[i].distance;
            int idx2 = right_leaves[j].query_idx;
            double dist2 = right_leaves[j].distance;

            if (dist1 > 0)
                dist1 = 1.0 / dist1;
            if (dist2 > 0)
                dist2 = 1.0 / dist2;
            matrix(idx1, idx2) = matrix(idx2, idx1) = dist1 + dist2;
        }
    }
    left_leaves.clear();
    right_leaves.clear();

    child = node->SubNodeBegin();
    while (child != node->SubNodeEnd()) {
        x_FillNewDistanceMatrix(*child++, matrix);
    }
}


int CMultiApplication::Run(void)
{
    SetDiagPostLevel(eDiag_Warning);

    // Process command line args
    const CArgs& args = GetArgs();
    
    bool verbose = args["v"].AsBoolean();

    // set up the aligner
    CPSSMAligner aligner;
    CNWAligner::TScore Wg = -kScaleFactor * args["g1"].AsInteger();
    CNWAligner::TScore Ws = -kScaleFactor * args["e1"].AsInteger();
    CNWAligner::TScore EndWg = -kScaleFactor * args["g0"].AsInteger();
    CNWAligner::TScore EndWs = -kScaleFactor * args["e0"].AsInteger();
    x_SetScoreMatrix(args["matrix"].AsString().c_str(), aligner);
    aligner.SetWg(Wg);
    aligner.SetWs(Ws);
    aligner.SetStartWg(EndWg);
    aligner.SetStartWs(EndWs);
    aligner.SetEndWg(EndWg);
    aligner.SetEndWs(EndWs);

    // Set up data loaders
    m_ObjMgr = CObjectManager::GetInstance();

    // read all the alignments
    CNcbiIstream& infile = args["i"].AsInputFile();
    vector<SAlignEntry> align_list;
    while (!infile.fail() && !infile.eof()) {
        char buf[128];

        buf[0] = 0;
        infile >> buf;
        if (buf[0] == 0)
            continue;

        CNcbiIfstream is((const char *)buf);
        blast::TSeqLocVector align_read = x_GetSeqLocFromStream(is, *m_ObjMgr);

        align_list.push_back(SAlignEntry());
        SAlignEntry& e = align_list.back();
        for (size_t i = 0; i < align_read.size(); i++) {
            e.align.push_back(CSequence(*align_read[i].seqloc, *align_read[i].scope));
        }
        e.length = e.align[0].GetLength();
        e.profile = x_GetProfile(e.align);
        e.self_score = x_GetSelfScore(e.profile, e.length,
                                      aligner.GetMatrix());

        char *name_ptr = buf + strlen(buf) - 1;
        while (name_ptr > buf && 
               name_ptr[-1] != '/' &&
               name_ptr[-1] != '\\') {
            name_ptr--;
        }
        strcpy(e.name, name_ptr);
    }

    int first_index = args["first"].AsInteger();
    int last_index = args["last"].AsInteger();
    if (last_index == 0)
        last_index = align_list.size() - 1;
    if (first_index != 0 || last_index != (int)align_list.size() - 1) {
        printf("error: all-against-all alignment required\n");
        return -1;
    }

    CDistMethods::TMatrix distances(align_list.size(),
                                    align_list.size());

    for (int i = first_index; i <= last_index; i++) {
        for (size_t j = i + 1; j < align_list.size(); j++) {

            int len1 = align_list[i].align[0].GetLength();
            int len2 = align_list[j].align[0].GetLength();

            aligner.SetEndSpaceFree(false, false, false, false);
            if (args["local"].AsBoolean() == true) {
                if (len1 > 1.5 * len2 || len2 > 1.5 * len1) {
                    aligner.SetEndSpaceFree(true, true, true, true);
                }
                else if (len1 > 1.2 * len2 || len2 > 1.2 * len1) {
                    aligner.SetStartWs(EndWs / 2);
                    aligner.SetEndWs(EndWs / 2);
                }
            }

            int score = x_AlignProfileProfile(align_list[i].profile,
                                              align_list[i].length,
                                              align_list[j].profile,
                                              align_list[j].length,
                                              aligner,
                                              args["local"].AsBoolean());

            if (verbose) {
                printf("%s %s %.2f\n", align_list[i].name,
                            align_list[j].name, (double)score / 100);
            }

            aligner.SetStartWs(EndWs);
            aligner.SetEndWs(EndWs);

            distances(i, j) = distances(j, i) = 1.0 - (double)score *
                        (1.0 / align_list[i].self_score +
                         1.0 / align_list[j].self_score) / 2;
        }
    }
    if (verbose) {
        printf("\n\n");
    }

    int num_clusters = align_list.size();

    if (args["d"]) {
        CNcbiIstream& dfile(args["d"].AsInputFile());

        while (!dfile.fail() && !dfile.eof()) {
            char defline[500];
            char name[128];

            dfile >> name;
            dfile >> name;
            dfile >> defline;

            defline[0] = 0;
            dfile.getline(defline, sizeof(defline), '\n');
            if (defline[0] == 0)
                continue;

            for (int i = 0; i < num_clusters; i++) {
                if (strstr(align_list[i].name, name)) {
                    snprintf(align_list[i].name, sizeof(align_list[i].name),
                             "%s %s", name, defline);
                    char *tmp = align_list[i].name;
                    while (*tmp) {
                        if (!isdigit(*tmp) && !isalpha(*tmp))
                            *tmp = '_';
                        tmp++;
                    }
                }
            }
        }
    }

    if (args["pairs"]) {
        for (int i=0;i < num_clusters - 1; i++) {
            for (int j=(i+1);j < num_clusters; j++) {

                if (distances(i, j) < args["cutoff"].AsDouble()) {
                    NcbiCout << align_list[i].name << "\t"
                             << align_list[j].name << "\t"
                             << distances(i, j) << NcbiEndl;
                }
            }
        }
    }
    else {
        printf("%d\n", num_clusters);
        for (int i = 0; i < num_clusters; i++) {
            printf("%s\n", align_list[i].name);
            for (int j = 0; j < num_clusters; j++) {
                printf("%5.4f ", distances(i, j));
            }
            printf("\n");
        }
    }

    CTree tree(distances);
    if (verbose)
        x_PrintTree(tree.GetTree(), 0, align_list);

    CDistMethods::TMatrix new_distances(align_list.size(),
                                        align_list.size());
    x_FillNewDistanceMatrix(tree.GetTree(), new_distances);

    //--------------------------------
    if (verbose) {
        printf("\n\n");
        printf("\n\nNew Distance matrix:\n    ");
        for (int i = (int)align_list.size() - 1; i > 0; i--)
            printf("%5d ", i);
        printf("\n");
    
        for (int i = 0; i < (int)align_list.size() - 1; i++) {
            printf("%2d: ", i);
            for (int j = (int)align_list.size() - 1; j > i; j--) {
                printf("%5.3f ", new_distances(i, j));
            }
            printf("\n");
        }
        printf("\n\n");

        printf("\n\nPercent relative error:\n    ");
        for (int i = (int)align_list.size() - 1; i > 0; i--)
            printf("%5d ", i);
        printf("\n");
    
        for (int i = 0; i < (int)align_list.size() - 1; i++) {
            printf("%2d: ", i);
            for (int j = (int)align_list.size() - 1; j > i; j--) {
                printf("%5.2f ", 100*fabs(new_distances(i, j) - 
                                          distances(i, j)) / distances(i, j));
            }
            printf("\n");
        }
        printf("\n\n");
    }
    //--------------------------------

    return 0;
}

void CMultiApplication::Exit(void)
{
    SetDiagStream(0);
}

int main(int argc, const char* argv[])
{
    return CMultiApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
