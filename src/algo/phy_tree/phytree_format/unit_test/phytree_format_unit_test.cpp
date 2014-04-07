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

#include <serial/serial.hpp>    
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <algo/phy_tree/phytree_calc.hpp>
#include <algo/phy_tree/phytree_format/phytree_format.hpp>

#include <corelib/test_boost.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(objects);


// Create scope
static CRef<CScope> s_CreateScope(void);

// Check whether tree container node is a leaf
static bool s_IsLeaf(const CNode& node);

// Check serialized tree
static bool s_TestTreeContainer(const CBioTreeContainer& tree, int num_leaves);

// Id of node that is not a leaf used in tests
static const int kNodeId = 2;


BOOST_AUTO_TEST_SUITE(guide_tree)

BOOST_AUTO_TEST_CASE(TestCreateTreeFromCalcProtein)
{
    CRef<CScope> scope = s_CreateScope();
    CNcbiIfstream istr("data/seqalign_protein.asn");
    BOOST_REQUIRE(istr);
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CPhyTreeCalc calc(seq_align, scope);
    calc.SetMaxDivergence(0.92);
    BOOST_REQUIRE(calc.CalcBioTree());
    
    CPhyTreeFormatter tree(calc);
    s_TestTreeContainer(*tree.GetSerialTree(), calc.GetSeqAlign()->GetDim());
}

BOOST_AUTO_TEST_CASE(TestCreateTreeFromCalcNucleotide)
{
    CRef<CScope> scope = s_CreateScope();
    CNcbiIfstream istr("data/seqalign_nucleotide.asn");
    BOOST_REQUIRE(istr);
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CPhyTreeCalc calc(seq_align, scope);
    calc.SetMaxDivergence(0.92);
    BOOST_REQUIRE(calc.CalcBioTree());

    CPhyTreeFormatter tree(calc);
    s_TestTreeContainer(*tree.GetSerialTree(), calc.GetSeqAlign()->GetDim());
}

BOOST_AUTO_TEST_CASE(TestInitTreeFeatures)
{
    CRef<CScope> scope = s_CreateScope();

    CNcbiIfstream istr("data/bare_tree.asn");
    BOOST_REQUIRE(istr);
    CBioTreeContainer btc;
    istr >> MSerial_AsnText >> btc;

    // find number of leaves
    int num = 0;
    ITERATE (CNodeSet::Tdata, it, btc.GetNodes().Get()) {
        // if root node
        if (!(*it)->IsSetParent()) {
            continue;
        }

        if (s_IsLeaf(**it)) {
            num++;
        }
    }

    // create fake seq-ids
    CRef<CSeq_id> seq_id(new CSeq_id("gi|129295"));
    vector< CRef<CSeq_id> > ids(num, seq_id);

    CPhyTreeFormatter tree(btc, ids, *scope);

    s_TestTreeContainer(*tree.GetSerialTree(), num);
}

BOOST_AUTO_TEST_CASE(TestMultipleQueries)
{
    CRef<CScope> scope = s_CreateScope();
    CNcbiIfstream istr("data/mole_seqalign.asn");
    BOOST_REQUIRE(istr);
    CSeq_align seq_align;
    istr >> MSerial_AsnText >> seq_align;

    CPhyTreeCalc calc(seq_align, scope);
    BOOST_REQUIRE(calc.CalcBioTree());

    vector<string> queries = {"gb|KC128904.1", "dbj|AB682225.1",
                              "gb|HM748810.1"};

    CPhyTreeFormatter tree(calc, queries);
    CRef<CBioTreeContainer> btc = tree.GetSerialTree();
    s_TestTreeContainer(*btc, calc.GetSeqAlign()->GetDim());

    // find query nodes and check that node info is set properly
    size_t num_queries_found = 0;
    ITERATE (CNodeSet::Tdata, node, btc->GetNodes().Get()) {

        // if root node
        if (!(*node)->IsSetParent()) {
            continue;
        }

        // skip non-leaf nodes
        if (!s_IsLeaf(**node)) {
            continue;
        }

        string accession;
        string node_info;
        
        // iterate over node features
        ITERATE (CNodeFeatureSet::Tdata, it, (*node)->GetFeatures().Get()) {

            // collect sequence accesion
            if ((*it)->GetFeatureid() == CPhyTreeFormatter::eAccessionNbrId) {
                accession = (*it)->GetValue();
            }

            // collect node info
            if ((*it)->GetFeatureid() == CPhyTreeFormatter::eNodeInfoId) {
                node_info = (*it)->GetValue();
            }
        }

        // accession must always be set
        BOOST_REQUIRE(!accession.empty());
        bool found_query = false;

        // check if the accession is for a query
        ITERATE (vector<string>, q, queries) {
            // if this is a query
            if (accession == *q) {
                // node info feature must be set to kNodeInfoQuery
                BOOST_REQUIRE_EQUAL(node_info,
                                    CPhyTreeFormatter::kNodeInfoQuery);
                num_queries_found++;
                found_query = true;
            }
        }
        // node info must be different from kNodeInfoQuery for other nodes
        BOOST_REQUIRE(found_query ||
                      node_info != CPhyTreeFormatter::kNodeInfoQuery);
    }

    // check that all query nodes were found
    BOOST_REQUIRE_EQUAL(num_queries_found, queries.size());    
}


// Check that exceptions are thrown for incorrect constructor arguments
BOOST_AUTO_TEST_CASE(TestInitTreeFeaturesWithBadInput)
{
    CRef<CScope> scope = s_CreateScope();

    CNcbiIfstream istr("data/bare_tree.asn");
    BOOST_REQUIRE(istr);
    CBioTreeContainer btc;
    istr >> MSerial_AsnText >> btc;
    istr.close();

    // find number of leaves
    int num = 0;
    ITERATE (CNodeSet::Tdata, it, btc.GetNodes().Get()) {
        // if root node
        if (!(*it)->IsSetParent()) {
            continue;
        }

        if (s_IsLeaf(**it)) {
            num++;
        }
    }

    // create fake seq-ids
    CRef<CSeq_id> seq_id(new CSeq_id("gi|129295"));
    vector< CRef<CSeq_id> > ids(num - 1, seq_id);

    CRef<CPhyTreeFormatter> tree;

    // number of seq-ids must be the same as number of leaves in btc
    BOOST_REQUIRE_THROW(tree.Reset(new CPhyTreeFormatter(btc, ids, *scope)),
                        CPhyTreeFormatterException);

    istr.open("data/bare_tree.asn");
    BOOST_REQUIRE(istr);
    istr >> MSerial_AsnText >> btc;

    // number of seq-ids must be the same as number of leaves in btc
    ids.resize(num + 1, seq_id);
    BOOST_REQUIRE_THROW(tree.Reset(new CPhyTreeFormatter(btc, ids, *scope)),
                        CPhyTreeFormatterException);
}


BOOST_AUTO_TEST_CASE(TestInitLabels)
{
    CRef<CScope> scope = s_CreateScope();

    CNcbiIfstream istr("data/tree.asn");
    BOOST_REQUIRE(istr);
    CBioTreeContainer btc;
    istr >> MSerial_AsnText >> btc;

    CPhyTreeFormatter tree(btc, CPhyTreeFormatter::eSeqTitle);

    // find number of leaves
    int num = 0;
    ITERATE (CNodeSet::Tdata, it, btc.GetNodes().Get()) {
        // if root node
        if (!(*it)->IsSetParent()) {
            continue;
        }

        if (s_IsLeaf(**it)) {
            num++;
        }
    }

    s_TestTreeContainer(*tree.GetSerialTree(), num);
}


BOOST_AUTO_TEST_CASE(TestIsSingleBlastName)
{
    CRef<CScope> scope = s_CreateScope();

    // read tree with single blast name
    CNcbiIfstream istr("data/tree_single_bn.asn");
    BOOST_REQUIRE(istr);
    CBioTreeContainer btc;
    istr >> MSerial_AsnText >> btc;
    istr.close();

    // feature id for blast name
    const int kBlastNameFeatureId = 6;
    string blast_name;

    // pointer to a blast name node feature
    CNodeFeature* bn_node_feature = NULL;

    // check pre condition -- the tree has single blast name
    bool is_single_blast_name = true;
    // for each node
    ITERATE(CNodeSet::Tdata, node, btc.GetNodes().Get()) {
        // if root node
        if (!(*node)->IsSetParent()) {
            continue;
        }

        // for each node feature
        ITERATE (CNodeFeatureSet::Tdata, it, (*node)->GetFeatures().Get()) {
            if ((*it)->CanGetFeatureid()
                && (*it)->GetFeatureid() == kBlastNameFeatureId) {

                if ((*it)->CanGetValue()) {
                    if (blast_name.empty()) {
                        blast_name = (*it)->GetValue();
                    }
                    else if (blast_name != (*it)->GetValue()) {
                        is_single_blast_name = false;
                        break;
                    }

                    // set the pointer to a node feature for later use
                    if (!bn_node_feature) {
                        bn_node_feature = const_cast<CNodeFeature*>(&**it);
                    }
                }
            }
        }
        if (!is_single_blast_name) {
            break;
        }
    }
    // check pre condition
    BOOST_REQUIRE(is_single_blast_name);

    // check post condition
    CRef<CPhyTreeFormatter> tree(new CPhyTreeFormatter(btc));
    BOOST_REQUIRE(tree->IsSingleBlastName());

    // change blast name for a single node
    bn_node_feature->SetValue(blast_name + "fake_name");
    tree.Reset(new CPhyTreeFormatter(btc));
    BOOST_REQUIRE(!tree->IsSingleBlastName());
}


BOOST_AUTO_TEST_CASE(TestSimplifyByBlastName)
{
    CRef<CScope> scope = s_CreateScope();

    CNcbiIfstream istr("data/tree_single_bn.asn");
    BOOST_REQUIRE(istr);
    CBioTreeContainer btc;
    istr >> MSerial_AsnText >> btc;

    // feature id for blast name
    const int kBlastNameFeatureId = 6;
    string blast_name;

    // check pre condition -- the tree has single blast name
    bool is_single_blast_name = true;
    // for each node
    ITERATE(CNodeSet::Tdata, node, btc.GetNodes().Get()) {
        // if root node
        if (!(*node)->IsSetParent()) {
            continue;
        }

        // for each node feature
        ITERATE (CNodeFeatureSet::Tdata, it, (*node)->GetFeatures().Get()) {
            if ((*it)->CanGetFeatureid()
                && (*it)->GetFeatureid() == kBlastNameFeatureId) {

                if ((*it)->CanGetValue()) {
                    if (blast_name.empty()) {
                        blast_name = (*it)->GetValue();
                    }
                    else if (blast_name != (*it)->GetValue()) {
                        is_single_blast_name = false;
                        break;
                    }
                }
            }
        }
        if (!is_single_blast_name) {
            break;
        }
    }
    // check pre condition
    BOOST_REQUIRE(is_single_blast_name);

    CPhyTreeFormatter tree(btc);
    tree.SimplifyTree(CPhyTreeFormatter::eByBlastName);

    const CBioTreeDynamic::CBioNode* node
        = tree.GetNonNullNode(tree.GetRootNodeID());

    // this tree must be collapsed at root
    BOOST_REQUIRE_EQUAL(node->GetFeature(
        CPhyTreeFormatter::GetFeatureTag(
                    CPhyTreeFormatter::eTreeSimplificationTagId)), "1");

    BOOST_REQUIRE_EQUAL(tree.GetSimplifyMode(),
                        CPhyTreeFormatter::eByBlastName);
}


BOOST_AUTO_TEST_CASE(TestFullyExpand)
{
    CRef<CScope> scope = s_CreateScope();

    CNcbiIfstream istr("data/bare_tree.asn");
    BOOST_REQUIRE(istr);
    CBioTreeContainer btc;
    istr >> MSerial_AsnText >> btc;

    // find number of leaves
    int num = 0;
    ITERATE (CNodeSet::Tdata, it, btc.GetNodes().Get()) {
        // if root node
        if (!(*it)->IsSetParent()) {
            continue;
        }

        if (s_IsLeaf(**it)) {
            num++;
        }
    }

    // create fake seq-ids with the same blast name
    CRef<CSeq_id> seq_id(new CSeq_id("gi|129295"));
    vector< CRef<CSeq_id> > ids(num, seq_id);

    CPhyTreeFormatter tree(btc, ids, *scope);

    // collapse node so that action fully expand will make a change
    tree.ExpandCollapseSubtree(2);

    tree.SimplifyTree(CPhyTreeFormatter::eFullyExpanded);

    CRef<CBioTreeContainer> tree_cont = tree.GetSerialTree();

    // by checking BioTreeContainer we do not have to do recursion
    ITERATE (CNodeSet::Tdata, node, tree_cont->GetNodes().Get()) {

        if (!(*node)->IsSetParent()) {
            continue;
        }

        ITERATE (CNodeFeatureSet::Tdata, it, (*node)->GetFeatures().Get()) {

            // for expanded nodes the feature does not need to be present
            if ((*it)->GetFeatureid()
                == CPhyTreeFormatter::eTreeSimplificationTagId) {

                // but if it is, then its value must be "0"
                BOOST_REQUIRE_EQUAL((*it)->GetValue(), "0");
            }
        }
    }

    BOOST_REQUIRE_EQUAL(tree.GetSimplifyMode(),
                        CPhyTreeFormatter::eFullyExpanded);
}


BOOST_AUTO_TEST_CASE(TestExpandCollapse)
{
    CRef<CScope> scope = s_CreateScope();

    CNcbiIfstream istr("data/bare_tree.asn");
    BOOST_REQUIRE(istr);
    CBioTreeContainer btc;
    istr >> MSerial_AsnText >> btc;

    // find number of leaves
    int num = 0;
    ITERATE (CNodeSet::Tdata, it, btc.GetNodes().Get()) {
        // if root node
        if (!(*it)->IsSetParent()) {
            continue;
        }

        if (s_IsLeaf(**it)) {
            num++;
        }
    }

    // create fake seq-ids with the same blast name
    CRef<CSeq_id> seq_id(new CSeq_id("gi|129295"));
    vector< CRef<CSeq_id> > ids(num, seq_id);

    CPhyTreeFormatter tree(btc, ids, *scope);

    // collapse node (assuming it is expanded)
    tree.ExpandCollapseSubtree(kNodeId);

    // subtree must be collapsed
    BOOST_REQUIRE_EQUAL(tree.GetNonNullNode(kNodeId)->GetFeature(
               CPhyTreeFormatter::GetFeatureTag(
                               CPhyTreeFormatter::eTreeSimplificationTagId)),
                        "1");
    
    // expand node (assuming it is collapsed)
    tree.ExpandCollapseSubtree(kNodeId);

    // subtree must be expanded
    BOOST_REQUIRE_EQUAL(tree.GetNonNullNode(kNodeId)->GetFeature(
               CPhyTreeFormatter::GetFeatureTag(
                               CPhyTreeFormatter::eTreeSimplificationTagId)),
                        "0");

    BOOST_REQUIRE_EQUAL(tree.GetSimplifyMode(), CPhyTreeFormatter::eNone);
}


BOOST_AUTO_TEST_CASE(TestRerootTree)
{
    CRef<CScope> scope = s_CreateScope();

    CNcbiIfstream istr("data/tree.asn");
    BOOST_REQUIRE(istr);
    CBioTreeContainer btc;
    istr >> MSerial_AsnText >> btc;

    CPhyTreeFormatter tree(btc, CPhyTreeFormatter::eSeqTitle);

    int old_root_id = tree.GetRootNodeID();
    BOOST_REQUIRE(old_root_id != kNodeId);
    
    tree.RerootTree(kNodeId);

    // new root's id must be kNodeId
    BOOST_REQUIRE_EQUAL(tree.GetRootNodeID(), kNodeId);
    BOOST_REQUIRE_EQUAL((int)tree.GetTree().GetTreeNode()->GetValue().GetId(),
                        kNodeId);

    // node above kNodeId must be present in the tree
    BOOST_REQUIRE(tree.GetNode(old_root_id));
}


BOOST_AUTO_TEST_CASE(TestShowSubtree)
{
    CRef<CScope> scope = s_CreateScope();

    CNcbiIfstream istr("data/tree.asn");
    BOOST_REQUIRE(istr);
    CBioTreeContainer btc;
    istr >> MSerial_AsnText >> btc;

    CPhyTreeFormatter tree(btc, CPhyTreeFormatter::eSeqTitle);

    int old_root_id = tree.GetRootNodeID();
    BOOST_REQUIRE(old_root_id != kNodeId);

    tree.ShowSubtree(kNodeId);

    // new root node must have the new id
    BOOST_REQUIRE_EQUAL(tree.GetRootNodeID(), kNodeId);
    BOOST_REQUIRE_EQUAL((int)tree.GetTree().GetTreeNode()->GetValue().GetId(),
                        kNodeId);

    // node above kNodeId must not be present in the tree
    BOOST_REQUIRE(!tree.GetNode(old_root_id));
}


BOOST_AUTO_TEST_CASE(TestPrintTreeNewick)
{
    CRef<CScope> scope = s_CreateScope();

    CNcbiIfstream istr("data/tree.asn");
    BOOST_REQUIRE(istr);
    CBioTreeContainer btc;
    istr >> MSerial_AsnText >> btc;

    CRef<CPhyTreeFormatter> tree(new CPhyTreeFormatter(btc,
                                               CPhyTreeFormatter::eSeqTitle));

    auto_ptr<ostrstream> ostr(new ostrstream());

    tree->PrintNewickTree(*ostr);
    string output = CNcbiOstrstreamToString(*ostr);

    BOOST_CHECK_EQUAL(output, "(serpin_B9__Homo_sapiens_:4.73586, (hypothetical_protein__Homo_sapiens_:0, ((serpin_peptidase_inhibitor__clade_B__ovalbumin___member_11__Homo_sapiens_:0.0646582, unnamed_protein_product__Homo_sapiens_:0):0.368709, (((antithrombin_III_precursor__Homo_sapiens_:0, antithrombin_III__Homo_sapiens_:0.00969429):0.00807382, antithrombin_III_variant__Homo_sapiens_:0):0.0311961, unnamed_protein_product__Homo_sapiens_:0):0.560057):0.656712):0);\n");


    ostr.reset(new ostrstream());
    tree.Reset(new CPhyTreeFormatter(btc, CPhyTreeFormatter::eTaxName));
    tree->PrintNewickTree(*ostr);
    output = CNcbiOstrstreamToString(*ostr);

    BOOST_CHECK_EQUAL(output, "(Homo_sapiens:4.73586, (Homo_sapiens:0, ((Homo_sapiens:0.0646582, Homo_sapiens:0):0.368709, (((Homo_sapiens:0, Homo_sapiens:0.00969429):0.00807382, Homo_sapiens:0):0.0311961, Homo_sapiens:0):0.560057):0.656712):0);\n");


    ostr.reset(new ostrstream());
    tree.Reset(new CPhyTreeFormatter(btc, CPhyTreeFormatter::eBlastName));
    tree->PrintNewickTree(*ostr);
    output = CNcbiOstrstreamToString(*ostr);
    BOOST_CHECK_EQUAL(output, "(primates:4.73586, (primates:0, ((primates:0.0646582, primates:0):0.368709, (((primates:0, primates:0.00969429):0.00807382, primates:0):0.0311961, primates:0):0.560057):0.656712):0);\n");


    ostr.reset(new ostrstream());
    tree.Reset(new CPhyTreeFormatter(btc, CPhyTreeFormatter::eSeqId));
    tree->PrintNewickTree(*ostr);
    output = CNcbiOstrstreamToString(*ostr);
    BOOST_CHECK_EQUAL(output, "(ref_NP_004146_1:4.73586, (emb_CAE45712_1:0, ((gb_EAW63158_1:0.0646582, dbj_BAG59299_1:0):0.368709, (((ref_NP_000479_1:0, gb_AAA51796_1:0.00969429):0.00807382, dbj_BAA06212_1:0):0.0311961, dbj_BAG35537_1:0):0.560057):0.656712):0);\n");


    ostr.reset(new ostrstream());
    tree.Reset(new CPhyTreeFormatter(btc,
                                     CPhyTreeFormatter::eSeqIdAndBlastName));

    tree->PrintNewickTree(*ostr);
    output = CNcbiOstrstreamToString(*ostr);
    BOOST_CHECK_EQUAL(output, "(ref_NP_004146_1_primates_:4.73586, (emb_CAE45712_1_primates_:0, ((gb_EAW63158_1_primates_:0.0646582, dbj_BAG59299_1_primates_:0):0.368709, (((ref_NP_000479_1_primates_:0, gb_AAA51796_1_primates_:0.00969429):0.00807382, dbj_BAA06212_1_primates_:0):0.0311961, dbj_BAG35537_1_primates_:0):0.560057):0.656712):0);\n");
}


BOOST_AUTO_TEST_CASE(TestPrintTreeNexus)
{
    CRef<CScope> scope = s_CreateScope();

    CNcbiIfstream istr("data/tree.asn");
    BOOST_REQUIRE(istr);
    CBioTreeContainer btc;
    istr >> MSerial_AsnText >> btc;

    CRef<CPhyTreeFormatter> tree(new CPhyTreeFormatter(btc,
                                              CPhyTreeFormatter::eSeqTitle));

    auto_ptr<ostrstream> ostr(new ostrstream());

    tree->PrintNexusTree(*ostr);
    string output = CNcbiOstrstreamToString(*ostr);

    BOOST_REQUIRE(output.find("#NEXUS") != NPOS);
    BOOST_REQUIRE(output.find("BEGIN TAXA;") != NPOS);
    BOOST_REQUIRE(output.find("DIMENSIONS ntax=8;") != NPOS);
    BOOST_REQUIRE(output.find("TAXLABELS serpin_B9__Homo_sapiens_ hypothetical_protein__Homo_sapiens_ serpin_peptidase_inhibitor__clade_B__ovalbumin___member_11__Homo_sapiens_ unnamed_protein_product__Homo_sapiens_ antithrombin_III_precursor__Homo_sapiens_ antithrombin_III__Homo_sapiens_ antithrombin_III_variant__Homo_sapiens_ unnamed_protein_product__Homo_sapiens_;")
                  != NPOS);
    BOOST_REQUIRE(output.find("BEGIN TREES;") != NPOS);
    BOOST_REQUIRE(output.find("TREE Blast_guide_tree = (serpin_B9__Homo_sapiens_:4.73586, (hypothetical_protein__Homo_sapiens_:0, ((serpin_peptidase_inhibitor__clade_B__ovalbumin___member_11__Homo_sapiens_:0.0646582, unnamed_protein_product__Homo_sapiens_:0):0.368709, (((antithrombin_III_precursor__Homo_sapiens_:0, antithrombin_III__Homo_sapiens_:0.00969429):0.00807382, antithrombin_III_variant__Homo_sapiens_:0):0.0311961, unnamed_protein_product__Homo_sapiens_:0):0.560057):0.656712):0);") 
                  != NPOS);
    BOOST_REQUIRE(output.find("ENDBLOCK;") != NPOS);


    ostr.reset(new ostrstream());
    tree.Reset(new CPhyTreeFormatter(btc, CPhyTreeFormatter::eTaxName));
    tree->PrintNexusTree(*ostr);
    output = CNcbiOstrstreamToString(*ostr);

    BOOST_REQUIRE(output.find("#NEXUS") != NPOS);
    BOOST_REQUIRE(output.find("BEGIN TAXA;") != NPOS);
    BOOST_REQUIRE(output.find("DIMENSIONS ntax=8;") != NPOS);
    BOOST_REQUIRE(output.find("TAXLABELS Homo_sapiens Homo_sapiens Homo_sapiens Homo_sapiens Homo_sapiens Homo_sapiens Homo_sapiens Homo_sapiens;")
                  != NPOS);
    BOOST_REQUIRE(output.find("BEGIN TREES;") != NPOS);
    BOOST_REQUIRE(output.find("TREE Blast_guide_tree = (Homo_sapiens:4.73586, (Homo_sapiens:0, ((Homo_sapiens:0.0646582, Homo_sapiens:0):0.368709, (((Homo_sapiens:0, Homo_sapiens:0.00969429):0.00807382, Homo_sapiens:0):0.0311961, Homo_sapiens:0):0.560057):0.656712):0);")
                  != NPOS);
    BOOST_REQUIRE(output.find("ENDBLOCK;") != NPOS);


    ostr.reset(new ostrstream());
    tree.Reset(new CPhyTreeFormatter(btc, CPhyTreeFormatter::eBlastName));
    tree->PrintNexusTree(*ostr);
    output = CNcbiOstrstreamToString(*ostr);

    BOOST_REQUIRE(output.find("#NEXUS") != NPOS);
    BOOST_REQUIRE(output.find("BEGIN TAXA;") != NPOS);
    BOOST_REQUIRE(output.find("DIMENSIONS ntax=8;") != NPOS);
    BOOST_REQUIRE(output.find("TAXLABELS primates primates primates primates primates primates primates primates;")
                  != NPOS);
    BOOST_REQUIRE(output.find("BEGIN TREES;") != NPOS);
    BOOST_REQUIRE(output.find("TREE Blast_guide_tree = (primates:4.73586, (primates:0, ((primates:0.0646582, primates:0):0.368709, (((primates:0, primates:0.00969429):0.00807382, primates:0):0.0311961, primates:0):0.560057):0.656712):0);\n")
                  != NPOS);
    BOOST_REQUIRE(output.find("ENDBLOCK;") != NPOS);


    ostr.reset(new ostrstream());
    tree.Reset(new CPhyTreeFormatter(btc, CPhyTreeFormatter::eSeqId));
    tree->PrintNexusTree(*ostr);
    output = CNcbiOstrstreamToString(*ostr);

    BOOST_REQUIRE(output.find("#NEXUS") != NPOS);
    BOOST_REQUIRE(output.find("BEGIN TAXA;") != NPOS);
    BOOST_REQUIRE(output.find("DIMENSIONS ntax=8;") != NPOS);
    BOOST_REQUIRE(output.find("TAXLABELS ref_NP_004146_1 emb_CAE45712_1 gb_EAW63158_1 dbj_BAG59299_1 ref_NP_000479_1 gb_AAA51796_1 dbj_BAA06212_1 dbj_BAG35537_1;")
                  != NPOS);
    BOOST_REQUIRE(output.find("BEGIN TREES;") != NPOS);
    BOOST_REQUIRE(output.find("TREE Blast_guide_tree = (ref_NP_004146_1:4.73586, (emb_CAE45712_1:0, ((gb_EAW63158_1:0.0646582, dbj_BAG59299_1:0):0.368709, (((ref_NP_000479_1:0, gb_AAA51796_1:0.00969429):0.00807382, dbj_BAA06212_1:0):0.0311961, dbj_BAG35537_1:0):0.560057):0.656712):0);")
                  != NPOS);
    BOOST_REQUIRE(output.find("ENDBLOCK;") != NPOS);


    ostr.reset(new ostrstream());
    tree.Reset(new CPhyTreeFormatter(btc,
                                     CPhyTreeFormatter::eSeqIdAndBlastName));

    tree->PrintNexusTree(*ostr);
    output = CNcbiOstrstreamToString(*ostr);

    BOOST_REQUIRE(output.find("#NEXUS") != NPOS);
    BOOST_REQUIRE(output.find("BEGIN TAXA;") != NPOS);
    BOOST_REQUIRE(output.find("DIMENSIONS ntax=8;") != NPOS);
    BOOST_REQUIRE(output.find("TAXLABELS ref_NP_004146_1_primates_ emb_CAE45712_1_primates_ gb_EAW63158_1_primates_ dbj_BAG59299_1_primates_ ref_NP_000479_1_primates_ gb_AAA51796_1_primates_ dbj_BAA06212_1_primates_ dbj_BAG35537_1_primates_;")
                  != NPOS);
    BOOST_REQUIRE(output.find("BEGIN TREES;") != NPOS);
    BOOST_REQUIRE(output.find("TREE Blast_guide_tree = (ref_NP_004146_1_primates_:4.73586, (emb_CAE45712_1_primates_:0, ((gb_EAW63158_1_primates_:0.0646582, dbj_BAG59299_1_primates_:0):0.368709, (((ref_NP_000479_1_primates_:0, gb_AAA51796_1_primates_:0.00969429):0.00807382, dbj_BAA06212_1_primates_:0):0.0311961, dbj_BAG35537_1_primates_:0):0.560057):0.656712):0);")
                  != NPOS);
    BOOST_REQUIRE(output.find("ENDBLOCK") != NPOS);
}


BOOST_AUTO_TEST_SUITE_END()


// Create scope
static CRef<CScope> s_CreateScope(void)
{
    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*object_manager, "ID2");
    CRef<CScope> scope(new CScope(*object_manager));
    scope->AddDefaults();
    return scope;
}


static bool s_TestFeatureDict(const CFeatureDictSet& dict)
{
    vector<CPhyTreeFormatter::EFeatureID>
        feature_ids(CPhyTreeFormatter::eLastId + 1);

    feature_ids[CPhyTreeFormatter::eLabelId] = CPhyTreeFormatter::eLabelId;
    feature_ids[CPhyTreeFormatter::eDistId] = CPhyTreeFormatter::eDistId;
    feature_ids[CPhyTreeFormatter::eSeqIdId] = CPhyTreeFormatter::eSeqIdId;
    feature_ids[CPhyTreeFormatter::eOrganismId]
        = CPhyTreeFormatter::eOrganismId;

    feature_ids[CPhyTreeFormatter::eTitleId] = CPhyTreeFormatter::eTitleId;
    feature_ids[CPhyTreeFormatter::eAccessionNbrId]
        = CPhyTreeFormatter::eAccessionNbrId;

    feature_ids[CPhyTreeFormatter::eBlastNameId]
        = CPhyTreeFormatter::eBlastNameId;

    feature_ids[CPhyTreeFormatter::eAlignIndexId]
        = CPhyTreeFormatter::eAlignIndexId;

    feature_ids[CPhyTreeFormatter::eNodeColorId]
        = CPhyTreeFormatter::eNodeColorId;

    feature_ids[CPhyTreeFormatter::eLabelColorId]
        = CPhyTreeFormatter::eLabelColorId;

    feature_ids[CPhyTreeFormatter::eLabelBgColorId]
        = CPhyTreeFormatter::eLabelBgColorId;

    feature_ids[CPhyTreeFormatter::eLabelTagColorId]
        = CPhyTreeFormatter::eLabelTagColorId;

    feature_ids[CPhyTreeFormatter::eTreeSimplificationTagId]
        = CPhyTreeFormatter::eTreeSimplificationTagId;

    feature_ids[CPhyTreeFormatter::eNodeInfoId]
        = CPhyTreeFormatter::eNodeInfoId;

    vector<bool> found(CPhyTreeFormatter::eLastId + 1, false);

    // for each feature in the dictionary
    ITERATE (CFeatureDictSet::Tdata, it, dict.Get()) {

        int id = (*it)->GetId();

        // make sure that ids and descriptors match
        BOOST_REQUIRE(id >= 0 && id <= CPhyTreeFormatter::eLastId);
        BOOST_REQUIRE_EQUAL((*it)->GetName(),
                            CPhyTreeFormatter::GetFeatureTag(feature_ids[id]));

        found[id] = true;
    }

    // all features must be present in the dictionary
    ITERATE(vector<bool>, it, found) {
        BOOST_REQUIRE(*it);
    }

    return true;
}

static bool s_IsLeaf(const CNode& node)
{
    ITERATE (CNodeFeatureSet::Tdata, it, node.GetFeatures().Get()) {
        if ((*it)->GetFeatureid() == CPhyTreeFormatter::eLabelId) {
            return true;
        }
    }
    return false;
}

static bool s_TestNode(const CNode& node, bool is_leaf)
{
    vector<bool> features(CPhyTreeFormatter::eLastId + 1, false);
    string node_info;
    string color;
    ITERATE (CNodeFeatureSet::Tdata, it, node.GetFeatures().Get()) {
        int id = (*it)->GetFeatureid();
        BOOST_REQUIRE(id >= 0 && id <= CPhyTreeFormatter::eLastId);

        if (!is_leaf) {
            BOOST_REQUIRE(id == CPhyTreeFormatter::eDistId
                         || id == CPhyTreeFormatter::eTreeSimplificationTagId);
        }

        if (is_leaf && id == CPhyTreeFormatter::eNodeInfoId) {
            node_info = (*it)->GetValue();            
        }

        if (is_leaf && id == CPhyTreeFormatter::eLabelBgColorId) {
            color = (*it)->GetValue(); 
        }

        BOOST_REQUIRE((*it)->GetValue() != "");
        BOOST_REQUIRE(!features[id]);
        features[id] = true;
    }    
    // only nodes with node info feature set may have colored labels
    BOOST_REQUIRE_EQUAL(node_info.empty(), color.empty());
    // query nodes must be marked with label background color
    if (node_info == CPhyTreeFormatter::kNodeInfoQuery) {
        BOOST_REQUIRE_EQUAL(color, "255 255 0");
    }

    if (is_leaf) {
        // the last few features are not always set
        for (int i=0;i < CPhyTreeFormatter::eLabelColorId;i++) {
            BOOST_REQUIRE(features[i]);
        }
    }
    else {
        BOOST_REQUIRE(features[CPhyTreeFormatter::eDistId]
                     && features[CPhyTreeFormatter::eTreeSimplificationTagId]);
    }

    return true;
}

static bool s_TestTreeContainer(const CBioTreeContainer& tree, int num_leaves)
{
    BOOST_REQUIRE(tree.CanGetFdict());
    s_TestFeatureDict(tree.GetFdict());

    int num = 0;
    ITERATE (CNodeSet::Tdata, it, tree.GetNodes().Get()) {

        // if root node
        if (!(*it)->IsSetParent()) {
            continue;
        }

        bool is_leaf = s_IsLeaf(**it);
        s_TestNode(**it, is_leaf);
        if (is_leaf) {
            num++;
        }
    }

    BOOST_REQUIRE_EQUAL(num, num_leaves);

    return true;
}



#endif /* SKIP_DOXYGEN_PROCESSING */

