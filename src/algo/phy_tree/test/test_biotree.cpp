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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description
 *
 */

#include <ncbi_pch.hpp>
#include <stdio.h>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_tree.hpp>
#include <util/bitset/bitset_debug.hpp>
#include <algo/tree/tree_algo.hpp>
#include <util/bitset/ncbi_bitset.hpp>
#include <algorithm>
#include <algo/phy_tree/bio_tree.hpp>
#include <algo/phy_tree/bio_tree_forest.hpp>
#include <algo/phy_tree/bio_tree_conv.hpp>

#include <objects/biotree/BioTreeContainer.hpp>
#include <objects/biotree/FeatureDescr.hpp>
#include <objects/biotree/FeatureDictSet.hpp>
#include <objects/biotree/NodeFeatureSet.hpp>
#include <objects/biotree/NodeFeature.hpp>
#include <objects/biotree/NodeSet.hpp>
#include <objects/biotree/Node.hpp>

#include <objects/taxon1/taxon1.hpp>

#include <common/test_assert.h>


USING_NCBI_SCOPE;
USING_SCOPE(objects);

static int label_id;
static int name_id = 1;

static string s_Node2String(const CBioTreeDynamic::TBioTreeNode& node)
{
    const CBioTreeDynamic::TBioTreeNode::TValueType& v = node.GetValue();
//    cout << v.GetTree() << endl;
//    _ASSERT(v.GetTree());
    return v.features.GetFeatureValue(label_id);
}

static string s_Node2String_name(const CBioTreeDynamic::TBioTreeNode& node)
{
    const CBioTreeDynamic::TBioTreeNode::TValueType& v = node.GetValue();
    return v.features.GetFeatureValue(name_id);

    CNodeSet s;
}



static void TestBioTreeContainer()
{
    typedef CBioTreeContainer::TFdict  TContainerDict;
    typedef TContainerDict::Tdata::value_type::element_type TCFeatureDescr;
    typedef CBioTreeContainer::TNodes  TContainerNodes;

    // create the bio-tree
    CBioTreeContainer btrc;
    btrc.SetTreetype("prot_cluster");

    // Build/populate feature dictionary
    //
    {{
        TContainerDict& fd = btrc.SetFdict();
        TContainerDict::Tdata& feat_list = fd.Set();
        // create id-value pairs (id MUST be unique)
        {{
        CRef<TCFeatureDescr> d(new TCFeatureDescr);
        d->SetId(1); d->SetName("label");
        feat_list.push_back(d);
        }}
        {{
        CRef<TCFeatureDescr> d(new TCFeatureDescr);
        d->SetId(2); d->SetName("dist");
        feat_list.push_back(d);
        }}
        {{
        CRef<TCFeatureDescr> d(new TCFeatureDescr);
        d->SetId(3); d->SetName("seq-id");
        feat_list.push_back(d);
        }}
        {{
        CRef<TCFeatureDescr> d(new TCFeatureDescr);
        d->SetId(4); d->SetName("cluster-id");
        feat_list.push_back(d);
        }}
    }}

    // Add tree nodes with topology and features
    //
    {{
        unsigned node_id = 1;
        TContainerNodes& nodes = btrc.SetNodes();
        TContainerNodes::Tdata& node_lst = nodes.Set();

        // make root node
        {{
        CRef<CNode> n(new CNode);
        n->SetId(node_id);  // unique node id

        CNode::TFeatures& nfeatures = n->SetFeatures();
        CNode::TFeatures::Tdata& feature_list = nfeatures.Set();

            // Add node features -- ids MUST correspond the feature dictionary
            //
            {{
            CRef<CNodeFeature> nf(new CNodeFeature);
            nf->SetFeatureid(1);  //  "label"
            nf->SetValue("RootNode_LBL");
            feature_list.push_back(nf);
            }}

            {{
            CRef<CNodeFeature> nf(new CNodeFeature);
            nf->SetFeatureid(3);  //  "seq-id"
            nf->SetValue("gi|123");
            feature_list.push_back(nf);
            }}

        node_lst.push_back(n);
        }}
        unsigned parent_node_id = node_id;
        ++node_id; 

        // make child node 1
        {{
        CRef<CNode> n(new CNode);
        n->SetId(node_id);  // unique node id
        n->SetParent(parent_node_id);

        CNode::TFeatures& nfeatures = n->SetFeatures();
        CNode::TFeatures::Tdata& feature_list = nfeatures.Set();

            // Add node features -- ids MUST correspond the feature dictionary
            //
            {{
            CRef<CNodeFeature> nf(new CNodeFeature);
            nf->SetFeatureid(1);  //  "label"
            nf->SetValue("Child1");
            feature_list.push_back(nf);
            }}

            {{
            CRef<CNodeFeature> nf(new CNodeFeature);
            nf->SetFeatureid(3);  //  "seq-id"
            nf->SetValue("gi|124");
            feature_list.push_back(nf);
            }}

        node_lst.push_back(n);
        }}
        ++node_id; 

        // make child node 2
        {{
        CRef<CNode> n(new CNode);
        n->SetId(node_id);  // unique node id
        n->SetParent(parent_node_id);

        CNode::TFeatures& nfeatures = n->SetFeatures();
        CNode::TFeatures::Tdata& feature_list = nfeatures.Set();

            // Add node features -- ids MUST correspond the feature dictionary
            //
            {{
            CRef<CNodeFeature> nf(new CNodeFeature);
            nf->SetFeatureid(1);  //  "label"
            nf->SetValue("Child2");
            feature_list.push_back(nf);
            }}

            {{
            CRef<CNodeFeature> nf(new CNodeFeature);
            nf->SetFeatureid(3);  //  "seq-id"
            nf->SetValue("gi|125");
            feature_list.push_back(nf);
            }}

        node_lst.push_back(n);
        }}


    }}

    // Dump tree to file (cout)
    cout << MSerial_AsnText << btrc;
}


/////////////////////////////////
// Test application
//

class CTestApplication : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);
};


void CTestApplication::Init(void)
{
    // Set err.-posting and tracing to maximum
    SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);
}



int CTestApplication::Run(void)
{


    //        CExceptionReporterStream reporter(cerr);
    //        CExceptionReporter::SetDefault(&reporter);
    //        CExceptionReporter::EnableDefault(false);
    //        CExceptionReporter::EnableDefault(true);
    //        CExceptionReporter::SetDefault(0);
    

	CTaxon1 tax;
	bool res = tax.Init();

	if (!res) {
		return 1;
	}


    // Create one container and tree:
	try{
		int tax_id = tax.GetTaxIdByName("Mouse");
		cout << tax_id << endl;

		CBioTreeContainer btrc;
		CTaxon1ConvertToBioTreeContainer<CBioTreeContainer, 
				                         CTaxon1,
				                         ITaxon1Node,
				                         ITreeIterator>
		conv_func;
		conv_func(btrc, tax, tax_id);

		cout << MSerial_AsnText << btrc;

	    CBioTreeDynamic dtr2;
		BioTreeConvertContainer2Dynamic(dtr2, btrc);

		TreePrint(cout, *(dtr2.GetTreeNode()), s_Node2String_name);
	}
	catch (exception& ex) 
	{
		cout << "Error! " << ex.what() << endl;
	}

    // Create one contanier with two trees in it, and a forest:
	try{
		int tax_id1 = tax.GetTaxIdByName("Mouse");
		cout << tax_id1 << endl;

	    int tax_id2 = tax.GetTaxIdByName("Rattus rattus");
		cout << tax_id2 << endl;

        //
        // Create two containers and a forest. Convert containers to forest and back, and write
        // the contents both before and after conversions.
		CBioTreeContainer c1, c2;
        CBioTreeForestDynamic f1;

        /// Add 2 separate trees to container c1
		CTaxon1ConvertToBioTreeContainer<CBioTreeContainer, 
				                         CTaxon1,
				                         ITaxon1Node,
				                         ITreeIterator>
		conv_func;
        bool is_forest;

		conv_func(c1, tax, tax_id1);
        is_forest = BioTreeContainerIsForest(c1);
        if (is_forest)
            cout << "Container with 1 tree misclassified as forest." << endl;
        else
            cout << "Container with 1 tree classified as tree." << endl; 

        conv_func(c1, tax, tax_id2);
        is_forest = BioTreeContainerIsForest(c1);
        if (!is_forest)
            cout << "Container with 2 trees misclassified as tree." << endl;
        else
            cout << "Container with 2 trees classified as forest." << endl;

        // Create temporary files for output.  Replace any previous contents.
        std::string tdir = CDir::GetTmpDir();
        cout << "Temp Output Dir: " << tdir << endl;

        std::string container1 =  CDirEntry::ConcatPath(tdir,"treetestc1.txt");
        std::string forest1 =  CDirEntry::ConcatPath(tdir,"treetestf1.txt");
        std::string container2 =  CDirEntry::ConcatPath(tdir,"treetestc2.txt");       

        CNcbiOfstream tstoutc1(container1.c_str());
        CNcbiOfstream tstoutf1(forest1.c_str());
        CNcbiOfstream tstoutc2(container2.c_str());      

        // Write iniial value of container 1 to firest container file
		tstoutc1 << MSerial_AsnText << c1;

        // Create forest from container and write contents to first forest file
		BioTreeConvertContainer2DynamicForest(f1, c1);        
        for (unsigned int i=0; i<f1.GetTrees().size(); ++i)
            TreePrint(tstoutf1, *(f1.GetTrees()[i]->GetTreeNode()), 
                      s_Node2String_name);

        // Convert forest back into a container (2), and write out container 2
        BioTreeForestConvert2Container<CBioTreeContainer, 
                                       CBioTreeForestDynamic>(c2, f1);
        tstoutc2 << MSerial_AsnText << c2;

        // close output files
        tstoutc1.close();
        tstoutf1.close();
        tstoutc2.close();
      
        // Compare contents of first container with contents
        // of container created from conversion to and from forest
        CFile  fc1(container1);
        
        if (!fc1.CompareTextContents(container2, CFile::eIgnoreWs))
            cout << "Converted container file differs from original" << endl;
        else
            cout << "Converted container file is identical to original" << endl;
	}
	catch (exception& ex) 
	{
		cout << "Error! " << ex.what() << endl;
	}

    tax.Fini();

    return 0;
}



/// Imitation of simple phylogenetic tree
struct PhyNode
{
    PhyNode() {}
    PhyNode(int d, const string& l) : distance(d), label(l){}

    int       distance;
    string    label;
};

typedef 
  CBioTree<BioTreeBaseNode<PhyNode, CBioTreeFeatureList> >
  CPhyTree;


class NodeConvert
{
public:
    NodeConvert(CBioTreeDynamic& dynamic_tree)
    : m_DynamicTree(&dynamic_tree)
    {}

    void operator()(CBioTreeDynamic::TBioTreeNode& dnode, 
                    const CPhyTree::TBioTreeNode&  src_node)
    {
        const CPhyTree::TBioTreeNode::TValueType& nv = src_node.GetValue();
        NStr::IntToString(m_TmpStr, nv.data.distance);
        m_DynamicTree->AddFeature(&dnode, "distance", m_TmpStr);
        m_DynamicTree->AddFeature(&dnode, "label", nv.data.label);
    }

private:
    CBioTreeDynamic*   m_DynamicTree;
    string             m_TmpStr;
};







struct PhyNodeId
{
    PhyNodeId() 
    {}

    PhyNodeId(unsigned x_id, int d, const string& l) 
        : id(x_id), distance(d), label(l)
    {}

    unsigned GetId() const { return id; }
    void SetId(unsigned x_id) { id = x_id; }

    unsigned  id;
    int       distance;
    string    label;
};


typedef CTreeNode<PhyNodeId> CPhyTreeNode;


/// Sample converter from tree node to dynamic tree node
class IdNodeConvert
{
public:
    IdNodeConvert(CBioTreeDynamic& dynamic_tree)
    : m_DynamicTree(&dynamic_tree)
    {}

    void operator()(CBioTreeDynamic::TBioTreeNode& dnode, 
                    const CPhyTreeNode&  src_node)
    {
        const PhyNodeId& nv = src_node.GetValue();
        NStr::IntToString(m_TmpStr, nv.distance);
        m_DynamicTree->AddFeature(&dnode, "distance", m_TmpStr);
        m_DynamicTree->AddFeature(&dnode, "label", nv.label);
    }

private:
    CBioTreeDynamic*   m_DynamicTree;
    string             m_TmpStr;
};

struct IdNodeConvertToTree
{
    void operator()(CPhyTreeNode&  dst_node,
                    const CBioTreeDynamic::TBioTreeNode& src)
    {
        const string& dist = src.GetFeature("distance");
        dst_node.GetValue().distance = atoi(dist.c_str());
        dst_node.GetValue().label = src.GetFeature("label");
    }
};


void TestTreeConvert()
{
    // ---------------------------------
    // Filling the tree

    CPhyTreeNode* n0 = new CPhyTreeNode;
    {
    PhyNodeId& nv = n0->GetValue();
    nv.id = 0;
    nv.distance = 0;
    nv.label = "root";
    }

    n0->AddNode(PhyNodeId(10, 1, "node1"));
    CPhyTreeNode* n2 = n0->AddNode(PhyNodeId(11, 1, "node2"));

    n2->AddNode(PhyNodeId(20, 3, "node20"));


    // -----------------------------------
    // Convert to dynamic

    CBioTreeDynamic dtr;

    CBioTreeFeatureDictionary& dict = dtr.GetFeatureDict();
    dict.Register("distance");
    label_id = dict.Register("label");

    IdNodeConvert func(dtr);
    TreeConvert2Dynamic(dtr, n0, func);

    // ----------------------------------------------------------    
    // Print the tree to check if everything is correct...

    TreePrint(cout, *(dtr.GetTreeNode()), s_Node2String);
	cout << endl << endl;

    delete n0;

    IdNodeConvertToTree func2;

    DynamicConvert2Tree(dtr, func2, n0);

    delete n0;
}



int main(int argc, char** argv)
{
    TestBioTreeContainer();

    TestTreeConvert();

    CPhyTree tr;

    // ----------------------------------------------------------

    CPhyTree::TBioTreeNode* n0 =  new CPhyTree::TBioTreeNode;
    CPhyTree::TBioTreeNode::TValueType& nv = n0->GetValue();
    nv.uid = 1;
    nv.data.distance = 0;
    nv.data.label = "n0";

    tr.SetTreeNode(n0);
 
    

    // ----------------------------------------------------------

    {{

    CPhyTree::TBioTreeNode* n10 =  new CPhyTree::TBioTreeNode;
    CPhyTree::TBioTreeNode::TValueType& nv = n10->GetValue();
       
    nv.uid = 10;
    nv.data.distance = 1;
    nv.data.label = "n10";

    n0->AddNode(n10);

    }}


    {{

    CPhyTree::TBioTreeNode* n11 =  new CPhyTree::TBioTreeNode;
    CPhyTree::TBioTreeNode::TValueType& nv = n11->GetValue();
       
    nv.uid = 11;
    nv.data.distance = 2;
    nv.data.label = "n11";

    n0->AddNode(n11);

    }}

    // ----------------------------------------------------------

    CBioTreeDynamic dtr;

    CBioTreeFeatureDictionary& dict = dtr.GetFeatureDict();
    dict.Register("distance");
    label_id = dict.Register("label");

    BioTreeConvert2Dynamic(dtr, tr, NodeConvert(dtr));

    {{
    const CBioTreeDynamic::TBioTreeNode* dnode = dtr.GetTreeNode();
    const string& st = dnode->GetFeature("label");
    cout << st << endl;
    }}
    {{
    CBioTreeDynamic::TBioTreeNode* dnode = dtr.GetTreeNodeNonConst();
    dnode->SetFeature("label", "n0-new");
    const string& st = (*dnode)["label"];
    cout << st << endl;
    }}

    // ----------------------------------------------------------
    
    // Print the tree to check if everything is correct...

    TreePrint(cout, *(dtr.GetTreeNode()), s_Node2String);

	cout << endl << endl;

    CBioTreeContainer btrc;
    btrc.SetTreetype("phy_tree");

    BioTreeConvert2Container(btrc, dtr);
//    cout << MSerial_AsnText << btrc;

    // ----------------------------------------------------------
/*
    CBioTreeDynamic dtr2;
    BioTreeConvertContainer2Dynamic(dtr2, btrc);

    TreePrint(cout, *(dtr2.GetTreeNode()), s_Node2String);
*/


    // ----------------------------------------------------------

    CTestApplication theTestApplication;
    return theTestApplication.AppMain(argc, argv, 0 /*envp*/, eDS_ToMemory);



    return 0;
}
