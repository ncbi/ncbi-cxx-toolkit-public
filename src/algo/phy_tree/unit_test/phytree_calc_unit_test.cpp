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
 * Author:  Greg Boratyn
 *
 * File Description:
 *   Unit tests for the Phylogenetic tree computation API
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <objtools/readers/fasta.hpp>
#include <objtools/readers/reader_exception.hpp>

#include <serial/iterator.hpp>
#include <serial/serial.hpp>    
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <algo/phy_tree/phytree_calc.hpp>
#include <math.h>

#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

#ifdef NCBI_COMPILER_MSVC
#  define isfinite _finite
#elif defined(NCBI_COMPILER_WORKSHOP)  &&  !defined(__builtin_isfinite)
#  undef isfinite
#  define isfinite finite
#endif

USING_NCBI_SCOPE;
USING_SCOPE(objects);


// Create scope with local sequences read from fasta file
static CRef<CScope> s_CreateScope(const string& filename);

// Traverse tree and check that each node has distance, number of leaves is
// correct and leaves have correct labels.
static void s_TraverseTree(const TPhyTreeNode* node, vector<bool>& leaves);

// Check tree
static bool s_TestTree(int num_sequences, const TPhyTreeNode* tree);

// Traverse BioTreeDynamic
static void s_TraverseDynTree(const CBioTreeDynamic::CBioNode* node,
                              vector<bool>& leaves);

// Check BioTreeContainer
static bool s_TestTreeContainer(int num_sequences,
                                const CBioTreeContainer& tree);

// Check tree computation from Seq-align
static bool s_TestCalc(const CSeq_align& seq_align,
                       const CPhyTreeCalc& calc);

// Check tree computation from Seq-align-set
static bool s_TestCalc(const CSeq_align_set& seq_align_set,
                       const CPhyTreeCalc& calc);

// Generate tree in Newick-like format
static string s_GetNewickLike(const TPhyTreeNode* tree);

/// Test class for accessing CPhyTreeCalc private methods and attributes
class CTestPhyTreeCalc
{
public:
    static bool CalcDivergenceMatrix(CPhyTreeCalc& calc, vector<int>& included)
    {
        return calc.x_CalcDivergenceMatrix(included);
    }
};


BOOST_AUTO_TEST_SUITE(guide_tree_calc)

// Test tree computation with protein Seq-align as input
BOOST_AUTO_TEST_CASE(TestProteinSeqAlign)
{
    CRef<CScope> scope = s_CreateScope("data/protein.fa");
    CNcbiIfstream istr("data/seqalign_protein_local.asn");
    BOOST_REQUIRE(istr);
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    const int kNumSeqs = 12;

    CPhyTreeCalc calc(seq_align, scope);
    vector<string> labels = calc.SetLabels();
    for (int i=1;i <= kNumSeqs;i++) {
        labels.push_back(NStr::IntToString(i));
    }
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestCalc(seq_align, calc);

    // test for specific result
    BOOST_REQUIRE_EQUAL(s_GetNewickLike(calc.GetTree()),
                        "((4:0.474, (1:0.814, ((2:0.092, 3:0.060):0.586, "
                        "(((9:0.044, ((7:0.012, 8:0.020):0.045, 6:0.044)"
                        ":0.018):0.070, 10:0.000):0.390, (0:0.000, 5:0.034)"
                        ":0.393):0.078):0.072):0.474):x)");
}


// Test tree computation with protein Seq-align-set as input
BOOST_AUTO_TEST_CASE(TestProteinSeqAlignSet)
{
    CRef<CScope> scope = s_CreateScope("data/protein.fa");
    CNcbiIfstream istr("data/seqannot_protein_local.asn");
    BOOST_REQUIRE(istr);
    CSeq_annot seq_annot;
    istr >> MSerial_AsnText >> seq_annot;

    CSeq_align_set seq_align_set;
    seq_align_set.Set() = seq_annot.GetData().GetAlign();

    const int kNumSeqs = 12;
    
    CPhyTreeCalc calc(seq_align_set, scope);
    vector<string> labels = calc.SetLabels();
    for (int i=1;i <= kNumSeqs;i++) {
        labels.push_back(NStr::IntToString(i));
    }
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestCalc(seq_align_set, calc);

    // test for specific result
    BOOST_REQUIRE_EQUAL(s_GetNewickLike(calc.GetTree()),
                        "((4:0.449, ((1:0.699, (2:0.061, 3:0.062):0.440)"
                        ":0.070, (((9:0.038, ((7:0.005, 8:0.017):0.044, "
                        "6:0.037):0.017):0.050, 10:0.000):0.403, (0:0.000, "
                        "5:0.034):0.360):0.104):0.449):x)");
}


// Test tree computation with nucleotide Seq-align as input
BOOST_AUTO_TEST_CASE(TestNucleotideSeqAlign)
{
    CRef<CScope> scope = s_CreateScope("data/nucleotide.fa");
    CNcbiIfstream istr("data/seqalign_nucleotide_local.asn");
    BOOST_REQUIRE(istr);
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CPhyTreeCalc calc(seq_align, scope);
    calc.SetDistMethod(CPhyTreeCalc::eJukesCantor);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestCalc(seq_align, calc);

    // test for specific result
    BOOST_REQUIRE_EQUAL(s_GetNewickLike(calc.GetTree()),
                        "(((5:0.000, 9:0.000):0.002, (7:0.000, (3:0.000, "
                        "(4:0.001, (1:0.000, (0:0.000, ((6:0.000, 8:0.000)"
                        ":0.000, 2:0.000):0.000):0.000):0.001):0.000):0.000)"
                        ":0.002):x)");
}

// Test tree computation with nucleotide Seq-align-set as input
BOOST_AUTO_TEST_CASE(TestNucleotideSeqAlignSet)
{
    CRef<CScope> scope = s_CreateScope("data/nucleotide.fa");
    CNcbiIfstream istr("data/seqannot_nucleotide_local.asn");
    BOOST_REQUIRE(istr);
    CSeq_annot seq_annot;    
    istr >> MSerial_AsnText >> seq_annot;

    CSeq_align_set seq_align_set;
    seq_align_set.Set() = seq_annot.GetData().GetAlign();

    CPhyTreeCalc calc(seq_align_set, scope);
    calc.SetDistMethod(CPhyTreeCalc::eJukesCantor);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestCalc(seq_align_set, calc);

    // test for specific result
    BOOST_REQUIRE_EQUAL(s_GetNewickLike(calc.GetTree()),
                        "(((5:0.000, 9:0.000):0.002, (7:0.000, (3:0.000, "
                        "(4:0.001, (1:0.000, (0:0.000, ((6:0.000, 8:0.000)"
                        ":0.000, 2:0.000):0.000):0.000):0.001):0.000):0.000)"
                        ":0.002):x)");
}


// Test that exceptions are thrown for invalid input
BOOST_AUTO_TEST_CASE(TestInvalidInput)
{
    CRef<CScope> scope = s_CreateScope("data/protein.fa");
    CNcbiIfstream istr("data/seqalign_protein_local.asn");
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CPhyTreeCalc calc(seq_align, scope);

    // If set, number of labels must be the same as number of input sequences
    vector<string>& labels = calc.SetLabels();
    labels.push_back("label1");
    labels.push_back("label2");
    BOOST_CHECK_THROW(calc.CalcBioTree(), CPhyTreeCalcException);
    calc.SetLabels().clear();

    // Max divergence must be a positive number
    calc.SetMaxDivergence(-0.1);
    BOOST_CHECK_THROW(calc.CalcBioTree(), CPhyTreeCalcException);

    // If Kimura distance is selected, then max divergence must be smaller
    // than 0.85
    calc.SetDistMethod(CPhyTreeCalc::eKimura);
    calc.SetMaxDivergence(0.851);
    BOOST_CHECK_THROW(calc.CalcBioTree(), CPhyTreeCalcException);

    // If no subset of input sequences satisfies max divergence condition
    calc.SetMaxDivergence(0.001);
    BOOST_CHECK(!calc.CalcBioTree());
    BOOST_CHECK(calc.GetMessages().size() > 0);
}


// Test tree computation using Grishin distance
BOOST_AUTO_TEST_CASE(TestGrishinDistance)
{
    CRef<CScope> scope = s_CreateScope("data/protein.fa");
    CNcbiIfstream istr("data/seqalign_protein_local.asn");
    CSeq_align seq_align;
    BOOST_REQUIRE(istr);
    istr >> MSerial_AsnText >> seq_align;

    CPhyTreeCalc calc(seq_align, scope);
    calc.SetDistMethod(CPhyTreeCalc::eGrishin);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestTree(calc.GetSeqIds().size(), calc.GetTree());
}


// Test tree computation using Kimura distance
BOOST_AUTO_TEST_CASE(TestKimuraDistance)
{
    CRef<CScope> scope = s_CreateScope("data/protein.fa");
    CNcbiIfstream istr("data/seqannot_protein_local.asn");
    BOOST_REQUIRE(istr);
    CSeq_annot seq_annot;
    istr >> MSerial_AsnText >> seq_annot;

    CSeq_align_set seq_align_set;
    seq_align_set.Set() = seq_annot.GetData().GetAlign();
    CPhyTreeCalc calc(seq_align_set, scope);

    calc.SetDistMethod(CPhyTreeCalc::eKimura);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestTree(calc.GetSeqIds().size(), calc.GetTree());
}


// Test tree computation using Grishin General distance
BOOST_AUTO_TEST_CASE(TestGrishinGeneralDistance)
{
    CRef<CScope> scope = s_CreateScope("data/protein.fa");
    CNcbiIfstream istr("data/seqalign_protein_local.asn");
    BOOST_REQUIRE(istr);
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CPhyTreeCalc calc(seq_align, scope);
    calc.SetDistMethod(CPhyTreeCalc::eGrishinGeneral);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestTree(calc.GetSeqIds().size(), calc.GetTree());
}


// Test tree computation using Jukes-Cantor distance
BOOST_AUTO_TEST_CASE(TestJukesCantorDistance)
{
    CRef<CScope> scope = s_CreateScope("data/nucleotide.fa");
    CNcbiIfstream istr("data/seqalign_nucleotide_local.asn");
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CPhyTreeCalc calc(seq_align, scope);
    calc.SetDistMethod(CPhyTreeCalc::eJukesCantor);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestTree(calc.GetSeqIds().size(), calc.GetTree());
}


// Test tree computation using Poisson distance
BOOST_AUTO_TEST_CASE(TestPoissonDistance)
{
    CRef<CScope> scope = s_CreateScope("data/nucleotide.fa");
    CNcbiIfstream istr("data/seqalign_nucleotide_local.asn");
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CPhyTreeCalc calc(seq_align, scope);
    calc.SetDistMethod(CPhyTreeCalc::ePoisson);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestTree(calc.GetSeqIds().size(), calc.GetTree());
}


// Test tree computation using Neighbor-Joining tree
BOOST_AUTO_TEST_CASE(TestNJTree)
{
    CRef<CScope> scope = s_CreateScope("data/protein.fa");
    CNcbiIfstream istr("data/seqalign_protein_local.asn");
    BOOST_REQUIRE(istr);
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CPhyTreeCalc calc(seq_align, scope);
    calc.SetTreeMethod(CPhyTreeCalc::eNJ);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestTree(calc.GetSeqIds().size(), calc.GetTree());
}


// Test tree computation using Fast Minimum Evolution tree
BOOST_AUTO_TEST_CASE(TestFastMETree)
{
    CRef<CScope> scope = s_CreateScope("data/protein.fa");
    CNcbiIfstream istr("data/seqalign_protein_local.asn");
    BOOST_REQUIRE(istr);
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CPhyTreeCalc calc(seq_align, scope);
    calc.SetTreeMethod(CPhyTreeCalc::eFastME);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestTree(calc.GetSeqIds().size(), calc.GetTree());
}

// Verify that CDistMethods::Divergence() does not return a finite number for
// a pair of sequences with a gap in each position. Make sure that the function
// isfinite() works.
// Checking for Nan and Inf does not always work for optimized 32-bit
// builds.
#if NCBI_PLATFORM_BITS == 64
BOOST_AUTO_TEST_CASE(TestNanDivergence)
{
    const string kSeq1 = "A--";
    const string kSeq2 = "-A-";

    // sequence lengths must be the same
    BOOST_REQUIRE_EQUAL(kSeq1.length(), kSeq2.length());
    // there must not be a column with a residue in both sequences
    for (size_t i=0;i < kSeq1.length();i++) {
        BOOST_REQUIRE(kSeq1[i] == '-' || kSeq2[i] == '-');
    }

    // for a pair of sequences with a gap in each column divergence must not
    // be finite
    BOOST_REQUIRE(!isfinite(CDistMethods::Divergence(kSeq1, kSeq2)));
}
#endif

// Verify that tree computing functions throw when non-finite number is present
// in the distance matrix
BOOST_AUTO_TEST_CASE(TestRejectIncorrectDistance)
{
    // create a distance matrix
    CDistMethods::TMatrix dmat(2, 2, 0.0);

    // test for a negative number
    dmat(0, 1) = dmat(1, 0) = -1.0;
    BOOST_REQUIRE_THROW(CDistMethods::NjTree(dmat), invalid_argument);
    BOOST_REQUIRE_THROW(CDistMethods::FastMeTree(dmat), invalid_argument);


#if NCBI_PLATFORM_BITS == 64
    // checking for Nan and Inf does not always work for optimized 32-bit
    // builds

    // test for infinity
    dmat(0, 1) = dmat(1, 0) = numeric_limits<double>::infinity();
    BOOST_REQUIRE_THROW(CDistMethods::NjTree(dmat), invalid_argument);
    BOOST_REQUIRE_THROW(CDistMethods::FastMeTree(dmat), invalid_argument);

    // test for quiet NaN
    dmat(0, 1) = dmat(1, 0) = numeric_limits<double>::quiet_NaN();
    BOOST_REQUIRE_THROW(CDistMethods::NjTree(dmat), invalid_argument);
    BOOST_REQUIRE_THROW(CDistMethods::FastMeTree(dmat), invalid_argument);

    // test for signaling NaN
    dmat(0, 1) = dmat(1, 0) = numeric_limits<double>::signaling_NaN();
    BOOST_REQUIRE_THROW(CDistMethods::NjTree(dmat), invalid_argument);
    BOOST_REQUIRE_THROW(CDistMethods::FastMeTree(dmat), invalid_argument);
#endif
}

// Verify that alignment with a gap in each column is rejected during
// divergence computation
BOOST_AUTO_TEST_CASE(TestRejectPairWithGapInEachPosition)
{
    CRef<CScope> scope = s_CreateScope("data/protein.fa");
    const int num_seqs = 2;

    // create alignment with a gap in each position
    CSeq_align align;
    align.SetDim(num_seqs);
    CRef<CSeq_id> id(new CSeq_id("lcl|1"));
    CDense_seg& denseg = align.SetSegs().SetDenseg();
    denseg.SetDim(num_seqs);
    denseg.SetNumseg(2);
    denseg.SetIds().push_back(id);
    denseg.SetIds().push_back(id);
    // starts = {0, -1, -1, 0}
    denseg.SetStarts().push_back(0);
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(-1);
    denseg.SetStarts().push_back(0);
    // lens = {10, 10}
    denseg.SetLens().push_back(10);
    denseg.SetLens().push_back(10);
    denseg.Validate();
        
    // make sure that there is a gap in each position for a pair of sequences
    // in the alignment
    const vector<int>& starts = denseg.GetStarts();
    for (size_t i=0;i < starts.size();i+=num_seqs) {
        BOOST_REQUIRE(starts[i] < 0 || starts[i+1] < 0);
    }

    CPhyTreeCalc calc(align, scope);
    vector<int> included_inds;
    bool valid = CTestPhyTreeCalc::CalcDivergenceMatrix(calc, included_inds);

    // for alignment with 2 sequences, one was rejected, and the function must
    // return false
    BOOST_REQUIRE_EQUAL(valid, false);

    // the first sequence is always included, and the second myst be rejected
    // because the divergence in infinite
    BOOST_REQUIRE_EQUAL(included_inds.size(), 1u);

    // the first sequence is always included
    BOOST_REQUIRE_EQUAL(included_inds[0], 0);
}

// Test creation of the alignment segments summary
BOOST_AUTO_TEST_CASE(TestCalcAlnSegInfo)
{
    CRef<CScope> scope = s_CreateScope("data/protein.fa");
    CNcbiIfstream istr("data/seqalign_protein_local.asn");
    BOOST_REQUIRE(istr);
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CPhyTreeCalc calc(seq_align, scope);
    calc.SetCalcAlnSegInfo(true);
    calc.CalcBioTree();
    const CPhyTreeCalc::TSegInfo& seg_info = calc.GetAlnSegInfo();

    BOOST_REQUIRE_EQUAL((int)seg_info.size(), seq_align.GetDim());
    ITERATE (CPhyTreeCalc::TSegInfo, seq, seg_info) {
        BOOST_REQUIRE(!seq->empty());
        ITERATE (vector<CPhyTreeCalc::TRange>, it, *seq) {
            BOOST_REQUIRE(it->NotEmpty());
        }
    }
}


BOOST_AUTO_TEST_SUITE_END()


static CRef<CScope> s_CreateScope(const string& filename)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*objmgr));
    scope->AddDefaults();

    CNcbiIfstream instream(filename.c_str());
    BOOST_REQUIRE(instream);

    CStreamLineReader line_reader(instream);
    CFastaReader fasta_reader(line_reader, 
                              CFastaReader::fAssumeProt |
                              CFastaReader::fForceType |
                              CFastaReader::fNoParseID);

    while (!line_reader.AtEOF()) {

        CRef<CSeq_entry> entry = fasta_reader.ReadOneSeq();

        if (entry == 0) {
            NCBI_THROW(CObjReaderException, eInvalid, 
                        "Could not retrieve seq entry");
        }
        scope->AddTopLevelSeqEntry(*entry);
        CTypeConstIterator<CBioseq> itr(ConstBegin(*entry));
    }

    return scope;
}


// Traverse tree and check that each node has distance, number of leaves is
// correct and leaves have correct labels.
static void s_TraverseTree(const TPhyTreeNode* node, vector<bool>& leaves)
{
    // each node except for root must have distance set
    BOOST_REQUIRE(!node->GetParent() || node->GetValue().IsSetDist());

    if (node->IsLeaf()) {

        string label = node->GetValue().GetLabel();

        // each leaf node must have a label
        BOOST_REQUIRE(!label.empty());

        // label must be a number between zero and number of leaves - 1
        int id = NStr::StringToInt(label);
        BOOST_REQUIRE(id >= 0 && id < (int)leaves.size());

        // each label must not appear more than once
        BOOST_REQUIRE(!leaves[id]);
        leaves[id] = true;

        return;
    }

    TPhyTreeNode::TNodeList_CI child(node->SubNodeBegin());
    while (child != node->SubNodeEnd()) {
        s_TraverseTree(*child, leaves);
        child++;
    }
}

// Check tree
static bool s_TestTree(int num_sequences, const TPhyTreeNode* tree)
{
    vector<bool> leaves(num_sequences, false);

    // check that all num_sequences leaves are present
    s_TraverseTree(tree, leaves);
    ITERATE (vector<bool>, it, leaves) {
        BOOST_REQUIRE(*it);
    }

    return true;
}

// This is not really Newick format, because it prints root's edge length
// and doean not put ';' at the end.
static void s_GetNewick(const TPhyTreeNode* node, string& result)
{
    if (!node->IsLeaf()) {
        result += "(";

        TPhyTreeNode::TNodeList_CI child(node->SubNodeBegin());
        while (child != node->SubNodeEnd()) {
            if (child != node->SubNodeBegin()) {
                result += ", ";
            }
            s_GetNewick(*child, result);
            child++;
        }
        result += ")";
    }

    result += node->GetValue().GetLabel();
    result += ":";    
    result += node->GetValue().IsSetDist()
        ? NStr::DoubleToString(node->GetValue().GetDist(), 3) : "x";    
}

static string s_GetNewickLike(const TPhyTreeNode* tree)
{
    string result;
    result += "(";
    s_GetNewick(tree, result);
    result += ")";

    return result;
}


// Traverse BioTreeDynamic
static void s_TraverseDynTree(const CBioTreeDynamic::CBioNode* node,
                              vector<bool>& leaves)
{
    if (node->IsLeaf()) {

        string label = node->GetFeature("label");

        // each leaf node must have a label
        BOOST_REQUIRE(!label.empty());

        // label must be a number between zero and number of leaves - 1
        int id = NStr::StringToInt(label);
        BOOST_REQUIRE(id >= 0 && id < (int)leaves.size());

        // each label must not appear more than once
        BOOST_REQUIRE(!leaves[id]);
        leaves[id] = true;

        return;
    }

    CBioTreeDynamic::CBioNode::TParent::TNodeList_CI child
        = node->SubNodeBegin();

    while (child != node->SubNodeEnd()) {
        s_TraverseDynTree((CBioTreeDynamic::CBioNode*)*child, leaves);
        child++;
    }
}


// Check BioTreeContainer
static bool s_TestTreeContainer(int num_sequences, const CBioTreeContainer& tree)
{
    // Make container 2 dynamic and traverse the tree
    CBioTreeDynamic dyntree;
    BioTreeConvertContainer2Dynamic(dyntree, tree);

    vector<bool> leaves(num_sequences, false);

    // check that all num_sequences leaves are present
    s_TraverseDynTree(dyntree.GetTreeNode(), leaves);
    ITERATE (vector<bool>, it, leaves) {
        BOOST_REQUIRE(*it);
    }

    return true;
}


// Check tree computation from Seq_align
static bool s_TestCalc(const CSeq_align& seq_align,
                       const CPhyTreeCalc& calc)
{
    const int kNumInputSequences = seq_align.GetDim();
    const int kNumUsedSequences = calc.GetSeqIds().size();
    
    // divergence matrix must have the same number of rows/columns as number
    // sequences included in tree computation
    const CPhyTreeCalc::CDistMatrix& diverg = calc.GetDivergenceMatrix();
    BOOST_REQUIRE_EQUAL(kNumUsedSequences, diverg.GetNumElements());
    for (int i=0; i < diverg.GetNumElements();i++) {
        for (int j=0;j < diverg.GetNumElements();j++) {
            BOOST_REQUIRE(diverg(i, j) >= 0.0 && diverg(i, j) <= 1.0);
        }
    }

    // distance matrix must have the same number of rows/columns as number
    // sequences included in tree computation
    const CPhyTreeCalc::CDistMatrix& dist = calc.GetDistanceMatrix();
    BOOST_REQUIRE_EQUAL(kNumUsedSequences, dist.GetNumElements());
    BOOST_REQUIRE_EQUAL(kNumUsedSequences, diverg.GetNumElements());
    for (int i=0; i < diverg.GetNumElements();i++) {
        for (int j=0;j < diverg.GetNumElements();j++) {
            BOOST_REQUIRE(dist(i, j) >= 0.0);
        }
    }
   
    // GetSeqAlign() function must return alignment of sequences used in tree
    // computation
    BOOST_REQUIRE_EQUAL(kNumUsedSequences, (int)calc.GetSeqAlign()->GetDim());


    BOOST_REQUIRE_EQUAL(kNumInputSequences,
                        kNumUsedSequences + (int)calc.GetRemovedSeqIds().size());

    // Make sure that each Seq-id in calc's Seq-align is appears in input
    // Seq-align
    const vector< CRef<CSeq_id> > input_seq_ids
        = seq_align.GetSegs().GetDenseg().GetIds();

    const vector< CRef<CSeq_id> > used_seq_ids
        = calc.GetSeqAlign()->GetSegs().GetDenseg().GetIds();

    vector<bool> found(used_seq_ids.size(), false);
    for (size_t i=0;i < used_seq_ids.size();i++) {
        for (size_t j=0;j < input_seq_ids.size();j++) {
            if (used_seq_ids[i]->Match(*input_seq_ids[j])) {
                found[i] = true;
                break;
            }
        }
    }
    ITERATE (vector<bool>, it, found) {
        BOOST_REQUIRE(*it);
    }


    s_TestTree(kNumUsedSequences, calc.GetTree());
    s_TestTreeContainer(kNumUsedSequences, *calc.GetSerialTree());

    return true;
}


// Check tree computation from Seq_align_set
static bool s_TestCalc(const CSeq_align_set& seq_align_set,
                       const CPhyTreeCalc& calc)
{
    vector< CRef<CSeq_id> > input_seq_ids;
    input_seq_ids.reserve(seq_align_set.Get().size());
    input_seq_ids.push_back(
            (*seq_align_set.Get().begin())->GetSegs().GetDenseg().GetIds()[0]);

    ITERATE (CSeq_align_set::Tdata, it, seq_align_set.Get()) {
        BOOST_REQUIRE_EQUAL((*it)->GetSegs().GetDenseg().GetDim(), 2);
        int i = (int)input_seq_ids.size() - 1;
        while (i >= 0 && !(*it)->GetSegs().GetDenseg().GetIds()[1]->Match(
                                                        *input_seq_ids[i])) {
                i--;
        }
        if (i < 0) {
            input_seq_ids.push_back((*it)->GetSegs().GetDenseg().GetIds()[1]);
        }
    }

    const int kNumInputSequences = (int)input_seq_ids.size();
    const int kNumUsedSequences = calc.GetSeqIds().size();
    
    const CPhyTreeCalc::CDistMatrix& diverg = calc.GetDivergenceMatrix();
    BOOST_REQUIRE_EQUAL(kNumUsedSequences, diverg.GetNumElements());
    for (int i=0; i < diverg.GetNumElements();i++) {
        for (int j=0;j < diverg.GetNumElements();j++) {
            BOOST_REQUIRE(diverg(i, j) >= 0.0 && diverg(i, j) <= 1.0);
        }
    }

    const CPhyTreeCalc::CDistMatrix& dist = calc.GetDistanceMatrix();
    BOOST_REQUIRE_EQUAL(kNumUsedSequences, dist.GetNumElements());
    for (int i=0; i < dist.GetNumElements();i++) {
        for (int j=0;j < dist.GetNumElements();j++) {
            BOOST_REQUIRE(dist(i, j) >= 0.0);
        }
    }
   
    BOOST_REQUIRE_EQUAL(kNumUsedSequences, (int)calc.GetSeqAlign()->GetDim());


    BOOST_REQUIRE_EQUAL(kNumInputSequences,
                        kNumUsedSequences + (int)calc.GetRemovedSeqIds().size());

    // Make sure that each Seq-id in calc's Seq-align is appears in input
    // Seq-align
    const vector< CRef<CSeq_id> > used_seq_ids
        = calc.GetSeqAlign()->GetSegs().GetDenseg().GetIds();

    vector<bool> found(used_seq_ids.size(), false);
    for (size_t i=0;i < used_seq_ids.size();i++) {
        for (size_t j=0;j < input_seq_ids.size();j++) {
            if (used_seq_ids[i]->Match(*input_seq_ids[j])) {
                found[i] = true;
                break;
            }
        }
    }
    ITERATE (vector<bool>, it, found) {
        BOOST_REQUIRE(*it);
    }


    s_TestTree(kNumUsedSequences, calc.GetTree());
    s_TestTreeContainer(kNumUsedSequences, *calc.GetSerialTree());

    return true;
}


#endif /* SKIP_DOXYGEN_PROCESSING */

