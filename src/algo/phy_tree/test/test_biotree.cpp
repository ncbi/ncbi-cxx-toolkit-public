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
#include <corelib/ncbi_tree.hpp>
#include <util/bitset/bitset_debug.hpp>
#include <algo/tree/tree_algo.hpp>
#include <util/bitset/ncbi_bitset.hpp>
#include <algorithm>
#include <algo/phy_tree/bio_tree.hpp>
#include <algo/phy_tree/bio_tree_conv.hpp>

#include <serial/serial.hpp>
#include <serial/serialasn.hpp>
#include <serial/stdtypes.hpp>
#include <serial/classinfo.hpp>
#include <serial/choice.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/continfo.hpp>


#include <objects/biotree/BioTreeContainer.hpp>
#include <objects/biotree/FeatureDescr.hpp>
#include <objects/biotree/FeatureDictSet.hpp>
#include <objects/biotree/NodeFeatureSet.hpp>
#include <objects/biotree/NodeFeature.hpp>
#include <objects/biotree/NodeSet.hpp>
#include <objects/biotree/Node.hpp>

#include <objects/taxon1/taxon1.hpp>


USING_NCBI_SCOPE;
USING_SCOPE(objects);

static int label_id;
static int name_id = 1;

static string s_Node2String(const CBioTreeDynamic::TBioTreeNode& node)
{
    const CBioTreeDynamic::TBioTreeNode::TValueType& v = node.GetValue();
    return v.features.GetFeatureValue(label_id);
}

static string s_Node2String_name(const CBioTreeDynamic::TBioTreeNode& node)
{
    const CBioTreeDynamic::TBioTreeNode::TValueType& v = node.GetValue();
    return v.features.GetFeatureValue(name_id);
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
    
    SetupDiag(eDS_ToStdout);

	CTaxon1 tax;
	bool res = tax.Init();

	if (!res) {
		return 1;
	}


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

		tax.Fini();
	}
	catch (exception& ex) 
	{
		cout << "Error! " << ex.what() << endl;
	}

    return 0;
}




struct PhyNode
{
    int     distance;
    string  label;
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



int main(int argc, char** argv)
{
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

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/06/09 13:39:05  kuznets
 * Fixed compilation errors
 *
 * Revision 1.1  2004/06/08 15:33:31  kuznets
 * Initial revision
 *
 *
 *
 * ===========================================================================
 */
