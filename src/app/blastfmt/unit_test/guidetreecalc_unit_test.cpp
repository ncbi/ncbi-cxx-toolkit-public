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

#include "../guide_tree_calc.hpp"

#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

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
                       const CGuideTreeCalc& calc);

// Check tree computation from Seq-align-set
static bool s_TestCalc(const CSeq_align_set& seq_align_set,
                       const CGuideTreeCalc& calc);



BOOST_AUTO_TEST_SUITE(guide_tree_calc)

// Test tree computation with protein Seq-align as input
BOOST_AUTO_TEST_CASE(TestProteinSeqAlign)
{
    CRef<CScope> scope = s_CreateScope("data/protein.fa");
    CNcbiIfstream istr("data/seqalign_protein_local.asn");
    BOOST_REQUIRE(istr);
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CGuideTreeCalc calc(seq_align, scope);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestCalc(seq_align, calc);
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

    CGuideTreeCalc calc(seq_align_set, scope);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestCalc(seq_align_set, calc);
}


// Test tree computation with nucleotide Seq-align as input
BOOST_AUTO_TEST_CASE(TestNucleotideSeqAlign)
{
    CRef<CScope> scope = s_CreateScope("data/nucleotide.fa");
    CNcbiIfstream istr("data/seqalign_nucleotide_local.asn");
    BOOST_REQUIRE(istr);
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CGuideTreeCalc calc(seq_align, scope);
    calc.SetDistMethod(CGuideTreeCalc::eJukesCantor);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestCalc(seq_align, calc);
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

    CGuideTreeCalc calc(seq_align_set, scope);
    calc.SetDistMethod(CGuideTreeCalc::eJukesCantor);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestCalc(seq_align_set, calc);
}


// Test that exceptions are thrown for invalid input
BOOST_AUTO_TEST_CASE(TestInvalidInput)
{
    CRef<CScope> scope = s_CreateScope("data/protein.fa");
    CNcbiIfstream istr("data/seqalign_protein_local.asn");
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CGuideTreeCalc calc(seq_align, scope);

    // If set, number of labels must be the same as number of input sequences
    vector<string>& labels = calc.SetLabels();
    labels.push_back("label1");
    labels.push_back("label2");
    BOOST_CHECK_THROW(calc.CalcBioTree(), CGuideTreeCalcException);
    calc.SetLabels().clear();

    // Max divergence must be a positive number
    calc.SetMaxDivergence(-0.1);
    BOOST_CHECK_THROW(calc.CalcBioTree(), CGuideTreeCalcException);

    // If Kimura distance is selected, then max divergence must be smaller
    // than 0.85
    calc.SetDistMethod(CGuideTreeCalc::eKimura);
    calc.SetMaxDivergence(0.851);
    BOOST_CHECK_THROW(calc.CalcBioTree(), CGuideTreeCalcException);

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

    CGuideTreeCalc calc(seq_align, scope);
    calc.SetDistMethod(CGuideTreeCalc::eGrishin);
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
    CGuideTreeCalc calc(seq_align_set, scope);

    calc.SetDistMethod(CGuideTreeCalc::eKimura);
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

    CGuideTreeCalc calc(seq_align, scope);
    calc.SetDistMethod(CGuideTreeCalc::eGrishinGeneral);
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

    CGuideTreeCalc calc(seq_align, scope);
    calc.SetDistMethod(CGuideTreeCalc::eJukesCantor);
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

    CGuideTreeCalc calc(seq_align, scope);
    calc.SetDistMethod(CGuideTreeCalc::ePoisson);
    BOOST_REQUIRE(calc.CalcBioTree());

    CNcbiOfstream ostr("tree.asn");
    ostr << MSerial_AsnText << *calc.GetSerialTree();

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

    CGuideTreeCalc calc(seq_align, scope);
    calc.SetTreeMethod(CGuideTreeCalc::eNJ);
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

    CGuideTreeCalc calc(seq_align, scope);
    calc.SetTreeMethod(CGuideTreeCalc::eFastME);
    BOOST_REQUIRE(calc.CalcBioTree());

    s_TestTree(calc.GetSeqIds().size(), calc.GetTree());
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
                       const CGuideTreeCalc& calc)
{
    const int kNumInputSequences = seq_align.GetDim();
    const int kNumUsedSequences = calc.GetSeqIds().size();
    
    // divergence matrix must have the same number of rows/columns as number
    // sequences included in tree computation
    const CGuideTreeCalc::CDistMatrix& diverg = calc.GetDivergenceMatrix();
    BOOST_REQUIRE_EQUAL(kNumUsedSequences, diverg.GetNumElements());
    for (int i=0; i < diverg.GetNumElements();i++) {
        for (int j=0;j < diverg.GetNumElements();j++) {
            BOOST_REQUIRE(diverg(i, j) >= 0.0 && diverg(i, j) <= 1.0);
        }
    }

    // distance matrix must have the same number of rows/columns as number
    // sequences included in tree computation
    const CGuideTreeCalc::CDistMatrix& dist = calc.GetDistanceMatrix();
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
                       const CGuideTreeCalc& calc)
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
    
    const CGuideTreeCalc::CDistMatrix& diverg = calc.GetDivergenceMatrix();
    BOOST_REQUIRE_EQUAL(kNumUsedSequences, diverg.GetNumElements());
    for (int i=0; i < diverg.GetNumElements();i++) {
        for (int j=0;j < diverg.GetNumElements();j++) {
            BOOST_REQUIRE(diverg(i, j) >= 0.0 && diverg(i, j) <= 1.0);
        }
    }

    const CGuideTreeCalc::CDistMatrix& dist = calc.GetDistanceMatrix();
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

