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
 * Author:  Greg Boratyn, Irena Zaretskaya
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/hash_set.hpp>

#include <objects/taxon1/taxon1.hpp>
#include <objects/blastdb/defline_extra.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/create_defline.hpp>

#include <algo/phy_tree/bio_tree.hpp>
#include <algo/phy_tree/bio_tree_conv.hpp>
#include <algo/phy_tree/phytree_format/phytree_simplify.hpp>
#include <algo/phy_tree/phytree_format/phytree_format.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


// query node colors
static const string s_kQueryNodeColor = "255 0 0";
static const string s_kQueryNodeBgColor = "255 255 0";
static const string s_kSeqOfTypeNodeBgColor = "204 255 204";

map<int,string> linkotTypeToBGColor;

// tree leaf label for unknown taxonomy
static const string s_kUnknown = "unknown";

// initial value for collapsed subtree feature
static const string s_kSubtreeDisplayed = "0";

// feature value for query nodes
const string CPhyTreeFormatter::kNodeInfoQuery = "query";

// feature value for sequences from type
const string CPhyTreeFormatter::kNodeInfoSeqFromType = "sequence_from_type";


CPhyTreeFormatter::CPhyTreeFormatter(CPhyTreeCalc& guide_tree_calc,
                                     ELabelType label_type,
                                     bool mark_query_node)
                                     
{
    x_Init();

    CRef<CBioTreeContainer> btc = guide_tree_calc.GetSerialTree();
    vector<int> mark_leaves;
    if (mark_query_node) {
        mark_leaves.push_back(0);
    }
    x_InitTreeFeatures(*btc, guide_tree_calc.GetSeqIds(),
                       *guide_tree_calc.GetScope(), 
                       label_type, mark_leaves,
                       m_BlastNameColorMap);
                       

    BioTreeConvertContainer2Dynamic(m_Dyntree, *btc, true);
}


CPhyTreeFormatter::CPhyTreeFormatter(CPhyTreeCalc& guide_tree_calc,
                                     const vector<int>& mark_leaves,
                                     ELabelType label_type)
{
    x_Init();

    CRef<CBioTreeContainer> btc = guide_tree_calc.GetSerialTree();
    x_InitTreeFeatures(*btc, guide_tree_calc.GetSeqIds(),
                       *guide_tree_calc.GetScope(), 
                       label_type, mark_leaves,
                       m_BlastNameColorMap,
                       m_LinkoutDB,
                       m_LinkoutType);

    BioTreeConvertContainer2Dynamic(m_Dyntree, *btc, true);
}


CPhyTreeFormatter::CPhyTreeFormatter(CPhyTreeCalc& guide_tree_calc,
                                     vector<string>& seq_ids,
                                     ELabelType label_type,
                                     ILinkoutDB* linkoutDB,
                                     int linkoutType,
                                     string mv_build_name)
{
    x_Init();
    vector<int> mark_leaves;
    m_LinkoutDB = linkoutDB;
    m_LinkoutType = linkoutType;
    m_MapViewerBuildName = mv_build_name;

    CRef<CBioTreeContainer> btc = guide_tree_calc.GetSerialTree();
    x_InitTreeFeatures(*btc, guide_tree_calc.GetSeqIds(),
                       *guide_tree_calc.GetScope(), 
                       label_type, mark_leaves,
                       m_BlastNameColorMap,
                       m_LinkoutDB,
                       m_LinkoutType);

    x_MarkLeavesBySeqId(*btc, seq_ids, *guide_tree_calc.GetScope());

    BioTreeConvertContainer2Dynamic(m_Dyntree, *btc, true);
}


CPhyTreeFormatter::CPhyTreeFormatter(CBioTreeContainer& btc,
                                  CPhyTreeFormatter::ELabelType lblType)
{
    x_Init();

    x_InitTreeLabels(btc,lblType);
    BioTreeConvertContainer2Dynamic(m_Dyntree, btc, true);
}


CPhyTreeFormatter::CPhyTreeFormatter(CBioTreeContainer& btc,
                                     const vector< CRef<CSeq_id> >& seqids,
                                     CScope& scope,
                                     CPhyTreeFormatter::ELabelType lbl_type,
                                     bool mark_query_node)
{
    x_Init();
    vector<int> mark_leaves;
    if (mark_query_node) {
        mark_leaves.push_back(0);
    }
    x_InitTreeFeatures(btc, seqids, scope, lbl_type, mark_leaves,
                       m_BlastNameColorMap,
                       m_LinkoutDB,
                       m_LinkoutType);

    BioTreeConvertContainer2Dynamic(m_Dyntree, btc, true);
}


CPhyTreeFormatter::CPhyTreeFormatter(const CBioTreeDynamic& tree)
    : m_Dyntree(tree)
{
    x_Init();
}


bool CPhyTreeFormatter::WriteTreeAs(CNcbiOstream& out,
                                    CPhyTreeFormatter::ETreeFormat format)
{
    switch (format) {
    case eNewick: return PrintNewickTree(out);
    case eNexus : return PrintNexusTree(out);
    case eASN   : return WriteTree(out);
    }

    return false;
}

bool CPhyTreeFormatter::WriteTree(CNcbiOstream& out)
{
    out << MSerial_AsnText << *GetSerialTree();

    return true;
}


bool CPhyTreeFormatter::PrintNewickTree(CNcbiOstream& ostr)
{
    // an array of labels is needed for Nexus format, here it is discarded
    vector<string> labels;
    x_PrintNewickTree(ostr, *m_Dyntree.GetTreeNode(), labels);
    ostr << NcbiEndl;
    return true;
}

bool CPhyTreeFormatter::PrintNexusTree(CNcbiOstream& ostr,
                                       const string& tree_name)
{
    // generate tree in Newick format and collect leaf labels
    vector<string> labels; // tree leaf labels
    ostrstream buff;
    x_PrintNewickTree(buff, *m_Dyntree.GetTreeNode(), labels, false);

    CNcbiOstrstreamToString s(buff);
    string tree(s);

    // print tree in nexus format using the labels and tree generated above
    ostr << "#NEXUS"
         << NcbiEndl << NcbiEndl;

    ostr << "BEGIN TAXA;" << NcbiEndl
         << "  DIMENSIONS ntax=" << labels.size() << ";" << NcbiEndl
         << "  TAXLABELS";
    ITERATE (vector<string>, it, labels) {
        ostr << " " << *it;
    }
    ostr << ";" << NcbiEndl;
    ostr << "ENDBLOCK;" << NcbiEndl << NcbiEndl;

    ostr << "BEGIN TREES;" << NcbiEndl
         <<  "  TREE " << tree_name << " = " << tree
         << NcbiEndl
         << "ENDBLOCK;" << NcbiEndl;

    return true;
}


void CPhyTreeFormatter::FullyExpand(void)
{
    TreeDepthFirstTraverse(*m_Dyntree.GetTreeNodeNonConst(), CExpander());
}


void CPhyTreeFormatter::SimplifyTree(ETreeSimplifyMode method)
{

    switch (method) {

    // Do nothing
    case eNone:
        break;
        
    // Collapse all subtrees with common blast name       
    case eByBlastName :
    {
        FullyExpand();
        CPhyTreeNodeGroupper groupper 
            = TreeDepthFirstTraverse(*m_Dyntree.GetTreeNodeNonConst(), 
                           CPhyTreeNodeGroupper(GetFeatureTag(eBlastNameId),
                                                  GetFeatureTag(eNodeColorId),
                                                  m_Dyntree));

        if (!groupper.GetError().empty()) {
            NCBI_THROW(CPhyTreeFormatterException, eTraverseProblem,
                       groupper.GetError());
        }

        x_CollapseSubtrees(groupper);
        break;
    }

    //Fully expand the tree        
    case eFullyExpanded :
        FullyExpand();
        break;

    default:
      NCBI_THROW(CPhyTreeFormatterException, eInvalidOptions,
                 "Invalid tree simplify mode");
    }

    m_SimplifyMode = method;
}

bool CPhyTreeFormatter::ExpandCollapseSubtree(int node_id)
{
    CBioTreeDynamic::CBioNode* node = x_GetBioNode(node_id);
    
    if (x_IsExpanded(*node)) {

        // Collapsing
        x_Collapse(*node);

        // Track labels in order to select proper color for collapsed node
        CPhyTreeLabelTracker 
            tracker = TreeDepthFirstTraverse(*node, CPhyTreeLabelTracker(
                                                  GetFeatureTag(eBlastNameId), 
                                                  GetFeatureTag(eNodeColorId),
                                                  m_Dyntree));

        if (!tracker.GetError().empty()) {
            NCBI_THROW(CPhyTreeFormatterException, eTraverseProblem,
                       tracker.GetError());
        }

        CPhyTreeLabelTracker::TLabelColorMap_I it = tracker.Begin();
        string label = it->first;
        ++it;
        for (; it != tracker.End(); ++it) {
            label += ", " + it->first;
        }
        node->SetFeature(GetFeatureTag(eLabelId), label);
        if (tracker.GetNumLabels() == 1) {
            node->SetFeature(GetFeatureTag(eNodeColorId),
                             tracker.Begin()->second);
        }
        if (tracker.FoundQueryNode()) {
            x_MarkNode(node, s_kQueryNodeBgColor);
        }
        else if (tracker.FoundSeqFromType()) {
            x_MarkNode(node, s_kSeqOfTypeNodeBgColor);
        }
    }
    else {

        // Expanding
        x_Expand(*node);
        node->SetFeature(GetFeatureTag(eNodeColorId), "");
    }

    m_SimplifyMode = eNone;
    return x_IsExpanded(*node);
}

// Traverse tree from new root up to old root and change parents to children
// @param node Parent of new root
// @param fid Feature id for node's distance (child's edge length)
void s_RerootUpstream(CBioTreeDynamic::CBioNode* node, TBioTreeFeatureId fid)
{
    _ASSERT(node);

    CBioTreeDynamic::CBioNode* parent
        = (CBioTreeDynamic::CBioNode*)node->GetParent();

    if (!parent) {
        return;
    }

    s_RerootUpstream(parent, fid);

    parent->GetValue().features.SetFeature(fid,
                              node->GetValue().features.GetFeatureValue(fid));

    node = parent->DetachNode(node);
    node->AddNode(parent);    
}

void CPhyTreeFormatter::RerootTree(int new_root_id)
{
    CBioTreeDynamic::CBioNode* node = x_GetBioNode(new_root_id);

    // Collapsed node cannot be a root, so a parent node will be a new
    // root if such node is selected
    if (node && x_IsLeafEx(*node) && node->GetParent()) {
        node = (CBioTreeDynamic::CBioNode*)node->GetParent();
    }

    // if new root is already a root, do nothing
    if (!node->GetParent()) {
        return;
    }

    // tree is deleted when new dyntree root is set, hence the old
    // root node must be copied and its children moved
    CBioTreeDynamic::CBioNode* old_root = m_Dyntree.GetTreeNodeNonConst();

    // get old root children
    vector<CBioTreeDynamic::CBioNode*> children;
    CBioTreeDynamic::CBioNode::TParent::TNodeList_I it
        = old_root->SubNodeBegin();

    for(; it != old_root->SubNodeEnd();it++) {
        children.push_back((CBioTreeDynamic::CBioNode*)*it);
    }
    NON_CONST_ITERATE (vector<CBioTreeDynamic::CBioNode*>, ch, children) {
        old_root->DetachNode(*ch);
    }
    
    // copy old root node and attach old root's children
    CBioTreeDynamic::CBioNode* new_old_root
        = new CBioTreeDynamic::CBioNode(*old_root);    
    ITERATE (vector<CBioTreeDynamic::CBioNode*>, ch, children) {
        new_old_root->AddNode(*ch);
    }
  
    // detach new root from the tree
    CBioTreeDynamic::CBioNode* parent
        = (CBioTreeDynamic::CBioNode*)node->GetParent();
    node = parent->DetachNode(node);
   
    // replace child - parent relationship in all parents of the new root
    s_RerootUpstream(parent, (TBioTreeFeatureId)eDistId);

    // make new root's parent its child and set new tree root
    node->AddNode(parent);
    m_Dyntree.SetTreeNode(node);
    parent->SetFeature(GetFeatureTag(eDistId),
                       node->GetFeature(GetFeatureTag(eDistId)));

    node->SetFeature(GetFeatureTag(eDistId), "0");
}

bool CPhyTreeFormatter::ShowSubtree(int root_id)
{
    CBioTreeDynamic::CBioNode* node = x_GetBioNode(root_id);
    _ASSERT(node);

    // If a collapsed node is clicked, then it needs to be expanded
    // in order to show the subtree
    bool collapsed = false;
    if (!x_IsExpanded(*node)) {
        collapsed = true;
        x_Expand(*node);
        m_SimplifyMode = eNone;        
    }

    // replace root, unused part of the tree will be deleted
    CBioTreeDynamic::CBioNode::TParent* parent = node->GetParent();
    if (parent) {
        parent->DetachNode(node);
        m_Dyntree.SetTreeNode(node);
    }

    return collapsed;
}

bool CPhyTreeFormatter::IsSingleBlastName(void)
{
    CSingleBlastNameExaminer examiner
        = TreeDepthFirstTraverse(*m_Dyntree.GetTreeNodeNonConst(), 
                                 CSingleBlastNameExaminer(m_Dyntree));
    return examiner.IsSingleBlastName();
}


void CPhyTreeFormatter::x_Init(void)
{
    linkotTypeToBGColor[eFromType] = s_kSeqOfTypeNodeBgColor;

    m_SimplifyMode = eNone;
    m_LinkoutDB = NULL;
    m_LinkoutType = 0;
}


CBioTreeDynamic::CBioNode* CPhyTreeFormatter::x_GetBioNode(TBioTreeNodeId id,
                                                    bool throw_if_null)
{
    CBioNodeFinder finder = TreeDepthFirstTraverse(
                                            *m_Dyntree.GetTreeNodeNonConst(),
                                            CBioNodeFinder(id));

    if (!finder.GetNode() && throw_if_null) {
        NCBI_THROW(CPhyTreeFormatterException, eNodeNotFound, (string)"Node "
                   + NStr::IntToString(id) + (string)" not found");
    }

    return finder.GetNode();
}

bool CPhyTreeFormatter::x_IsExpanded(const CBioTreeDynamic::CBioNode& node)
{
    return node.GetFeature(GetFeatureTag(eTreeSimplificationTagId))
        == s_kSubtreeDisplayed;
}

bool CPhyTreeFormatter::x_IsLeafEx(const CBioTreeDynamic::CBioNode& node)
{
    return node.IsLeaf() || !x_IsExpanded(node);
}

void CPhyTreeFormatter::x_Collapse(CBioTreeDynamic::CBioNode& node)
{
    node.SetFeature(GetFeatureTag(eTreeSimplificationTagId), "1");
}

void CPhyTreeFormatter::x_Expand(CBioTreeDynamic::CBioNode& node)
{
    node.SetFeature(GetFeatureTag(eTreeSimplificationTagId),
                    s_kSubtreeDisplayed);
}


//TO DO: Input parameter should be an interface for future groupping options
void CPhyTreeFormatter::x_CollapseSubtrees(CPhyTreeNodeGroupper& groupper)
{
    for (CPhyTreeNodeGroupper::CLabeledNodes_I it=groupper.Begin();
         it != groupper.End(); ++it) {
        x_Collapse(*it->GetNode());
        it->GetNode()->SetFeature(GetFeatureTag(eLabelId), it->GetLabel());
        it->GetNode()->SetFeature(GetFeatureTag(eNodeColorId), it->GetColor());

        CQueryNodeChecker query_checker
            = TreeDepthFirstTraverse(*it->GetNode(),
                                     CQueryNodeChecker(m_Dyntree));

        if (query_checker.HasQueryNode()) {
            x_MarkNode(it->GetNode(), s_kQueryNodeBgColor);
        }
        else if (query_checker.HasSeqFromType()) {
            x_MarkNode(it->GetNode(), s_kSeqOfTypeNodeBgColor);
        }
    }
}

void CPhyTreeFormatter::x_MarkNode(CBioTreeDynamic::CBioNode* node,
                                   const string& color)
{
    node->SetFeature(GetFeatureTag(eLabelBgColorId), color);
}


//Recusrive
void CPhyTreeFormatter::x_PrintNewickTree(CNcbiOstream& ostr,
                                   const CBioTreeDynamic::CBioNode& node,
                                          vector<string>& labels,
                                          bool name_subtrees /* = true */,
                                          bool is_outer_node /* = true */)
{

    string label;

    if (!node.IsLeaf()) {
        ostr << '(';
        for (CBioTreeDynamic::CBioNode::TNodeList_CI it = node.SubNodeBegin(); it != node.SubNodeEnd(); ++it) {
            if (it != node.SubNodeBegin())
                ostr << ", ";
            x_PrintNewickTree(ostr, (CBioTreeDynamic::CBioNode&)**it, labels,
                              name_subtrees, false);
        }
        ostr << ')';
    }

    if (!is_outer_node) {
        label = node.GetFeature(GetFeatureTag(eLabelId));
        if (node.IsLeaf() || !label.empty()) {
            for (size_t i=0;i < label.length();i++)
                if (!isalpha(label.at(i)) && !isdigit(label.at(i)))
                    label.at(i) = '_';
            if (node.IsLeaf() || name_subtrees) {
                ostr << label;
                labels.push_back(label);
            }
        }
        ostr << ':' << node.GetFeature(GetFeatureTag(eDistId));
    }
    else
        ostr << ';';
}

void CPhyTreeFormatter::x_InitTreeLabels(CBioTreeContainer &btc,
                                  CPhyTreeFormatter::ELabelType lblType) 
{
   
    NON_CONST_ITERATE (CNodeSet::Tdata, node, btc.SetNodes().Set()) {
        if ((*node)->CanGetFeatures()) {
            string  blastName = "",accNbr;
            //int id = (*node)->GetId();
            CRef< CNodeFeature > label_feature_node;
            CRef< CNodeFeature > selected_feature_node;
            int featureSelectedID = eLabelId;
            if (lblType == eSeqId || lblType == eSeqIdAndBlastName) {
//              featureSelectedID = s_kSeqIdId;
                featureSelectedID = eAccessionNbrId;                
            }
            else if (lblType == eTaxName || lblType == eTaxNameAndAccession) {
                featureSelectedID = eOrganismId;
            }                
            else if (lblType == eSeqTitle) {
                featureSelectedID = eTitleId;
            }
            else if (lblType == eBlastName) {
                featureSelectedID = eBlastNameId;
            }
            
            NON_CONST_ITERATE (CNodeFeatureSet::Tdata, node_feature,
                               (*node)->SetFeatures().Set()) {

                
                //typedef list< CRef< CNodeFeature > > Tdata

                if ((*node_feature)->GetFeatureid() == eLabelId) { 
                   label_feature_node = *node_feature;                                       
                }
                //If label typ = GI and blast name - get blast name here
                if ((*node_feature)->GetFeatureid() == eBlastNameId && 
                                    lblType == eSeqIdAndBlastName) { 
                   blastName = (*node_feature)->GetValue();
                }
                if ((*node_feature)->GetFeatureid() == eAccessionNbrId && 
                                    lblType == eTaxNameAndAccession) { 
                   accNbr = (*node_feature)->GetValue();
                }                
                if ((*node_feature)->GetFeatureid() == featureSelectedID) {
                    // a terminal node
                    // figure out which sequence this corresponds to
                    // from the numerical id we stuck in as a label
                    selected_feature_node = *node_feature;                                        
                }                
            }
            if(label_feature_node.NotEmpty() && selected_feature_node.NotEmpty())
            {
                string  label = selected_feature_node->GetValue();
                if(lblType == eSeqIdAndBlastName) {
                    //concatinate with blastName
                    label = label + "(" + blastName + ")";
                }
                if(lblType == eTaxNameAndAccession) {
                    //concatinate with accession number
                    label = label + "(" + accNbr + ")";
                }                
                //label_feature_node->SetValue(label);
                label_feature_node->ResetValue();
                label_feature_node->SetValue() = label;                
            }
        }
    }            
}



int CPhyTreeFormatter::GetRootNodeID()
{
    return m_Dyntree.GetTreeNode()->GetValue().GetId();
}


CBioTreeDynamic::CBioNode* CPhyTreeFormatter::GetNode(TBioTreeNodeId id)
{
    return x_GetBioNode(id, false);
}


CBioTreeDynamic::CBioNode* CPhyTreeFormatter::GetNonNullNode(TBioTreeNodeId id)
{
    return x_GetBioNode(id, true);
}


CRef<CBioTreeContainer> CPhyTreeFormatter::GetSerialTree(void)
{
    CRef<CBioTreeContainer> btc(new CBioTreeContainer());
    BioTreeConvert2Container(*btc, m_Dyntree);

    return btc;
}

// Get SeqID string from CBioseq_Handle such as gi|36537373
// If getGIFirst tries to get gi. If gi does not exist tries to get be 'Best ID'
static string s_GetSeqIDString(CBioseq_Handle& handle, bool get_gi_first)
{
    CSeq_id_Handle seq_id_handle;
    bool get_best_id = true;

    if(get_gi_first) {        
        try {
            seq_id_handle = sequence::GetId(handle, sequence::eGetId_ForceGi);
            if(seq_id_handle.IsGi()) {
                get_best_id = false;
            }
        }
        // This exception is handled by the calling sequence::GetId() below
        catch(CException&) {}
    }

    if(get_best_id) {
        seq_id_handle = sequence::GetId(handle, sequence::eGetId_Best);                 
    }
    CConstRef<CSeq_id> seq_id = seq_id_handle.GetSeqId();

    string id_string;
    (*seq_id).GetLabel(&id_string);

    return id_string;
}

// Generate Blast Name-based colors for tree leaves
// TO DO: This needs to be redesigned
#define MAX_NODES_TO_COLOR 24
static string s_GetBlastNameColor(
                   CPhyTreeFormatter::TBlastNameColorMap& blast_name_color_map,
                   string blast_tax_name)
{
    string color = "";

    //This should be rewritten in more elegant way
    string colors[MAX_NODES_TO_COLOR] 
                   = {"0 0 255", "0 255 0", "191 159 0", "30 144 255",
                      "255 0 255", "223 11 95", "95 79 95", "143 143 47",
                      "0 100 0", "128 0 0", "175 127 255", "119 136 153",
                      "255 69 0", "205 102 0", "0 250 154", "173 255 47",
                      "139 0 0", "255 131 250", "155 48 255", "205 133 0",
                      "127 255 212", "255 222 173", "221 160 221", "200 100 0"};


    unsigned int i = 0;
    for(;i < blast_name_color_map.size();i++) {
        pair<string, string>& map_item = blast_name_color_map[i];

        if(map_item.first == blast_tax_name) {
            color = map_item.second;
            break;
        }
    }
    
    if(color == "") { //blast name not in the map
        if(blast_name_color_map.size() >= MAX_NODES_TO_COLOR) {
            i = MAX_NODES_TO_COLOR - 1;
        }
        color = colors[i];
        blast_name_color_map.push_back(make_pair(blast_tax_name, color));
    }
    return color;
}    


void CPhyTreeFormatter::x_InitTreeFeatures(CBioTreeContainer& btc,
                                    const vector< CRef<CSeq_id> >& seqids,
                                    CScope& scope,
                                    CPhyTreeFormatter::ELabelType label_type,
                                    const vector<int>& mark_leaves,
                                    TBlastNameColorMap& bcolormap,
                                    ILinkoutDB* linkoutDB,
                                    int linkoutType)
{
    CTaxon1 tax;

    bool success = tax.Init();
    if (!success) {
        NCBI_THROW(CPhyTreeFormatterException, eTaxonomyError,
                   "Problem initializing taxonomy information.");
    }

    sequence::CDeflineGenerator defgen;
    
    // Come up with some labels for the terminal nodes
    int num_rows = (int)seqids.size();
    vector<string> labels(num_rows);
    vector<string> organisms(num_rows);
    vector<string> accession_nbrs(num_rows);    
    vector<string> titles(num_rows);
    vector<string> blast_names(num_rows);    
    vector<string> tax_node_colors(num_rows);
    vector<CBioseq_Handle> bio_seq_handles(num_rows);

    for (int i=0;i < num_rows;i++) {
        bio_seq_handles[i] = scope.GetBioseqHandle(*seqids[i]);        
        int tax_id = 0;
        try{
            const COrg_ref& org_ref = sequence::GetOrg_ref(bio_seq_handles[i]);                                
            organisms[i] = org_ref.GetTaxname();
            tax_id = org_ref.GetTaxId();
            if (success) {
                tax.GetBlastName(tax_id, blast_names[i]);
            }

            if (!success || blast_names[i].empty()) {
                blast_names[i] = s_kUnknown;
            }
        }
        catch(CException&) {            
            organisms[i] = s_kUnknown;
            blast_names[i]= s_kUnknown;
        }

        try{
            titles[i] = defgen.GenerateDefline(bio_seq_handles[i]);
        }
        catch(CException&) {
            titles[i] = s_kUnknown;
        }
                   
        CSeq_id_Handle accession_handle = sequence::GetId(bio_seq_handles[i],
                                                        sequence::eGetId_Best);

        CConstRef<CSeq_id> accession = accession_handle.GetSeqId();
        (*accession).GetLabel(&accession_nbrs[i]);

        tax_node_colors[i] = s_GetBlastNameColor(bcolormap,
                                                 blast_names[i]);

        switch (label_type) {
        case eTaxName:
            labels[i] = organisms[i];
            break;

        case eSeqTitle:
            labels[i] = titles[i];
            break;

        case eBlastName:
            labels[i] = blast_names[i];
            break;

        case eSeqId:
            labels[i] = accession_nbrs[i];
            break;

        case eSeqIdAndBlastName:
            labels[i] = accession_nbrs[i] + "(" + blast_names[i] + ")";
            break;

        case eTaxNameAndAccession:
            labels[i] = organisms[i] + "(" + accession_nbrs[i] + ")";
            break;        
        }
     
        if (labels[i].empty()) {
            CSeq_id_Handle best_id_handle = sequence::GetId(bio_seq_handles[i],
                                                       sequence::eGetId_Best);

            CConstRef<CSeq_id> best_id = best_id_handle.GetSeqId();
            (*best_id).GetLabel(&labels[i]);            
        }

    }
    
    // Add attributes to terminal nodes
    x_AddFeatureDesc(eSeqIdId, GetFeatureTag(eSeqIdId), btc);
    x_AddFeatureDesc(eOrganismId, GetFeatureTag(eOrganismId), btc);
    x_AddFeatureDesc(eTitleId, GetFeatureTag(eTitleId), btc);
    x_AddFeatureDesc(eAccessionNbrId, GetFeatureTag(eAccessionNbrId), btc);
    x_AddFeatureDesc(eBlastNameId, GetFeatureTag(eBlastNameId), btc);
    x_AddFeatureDesc(eAlignIndexId, GetFeatureTag(eAlignIndexId), btc);
    x_AddFeatureDesc(eNodeColorId, GetFeatureTag(eNodeColorId), btc);
    x_AddFeatureDesc(eLabelColorId, GetFeatureTag(eLabelColorId), btc);
    x_AddFeatureDesc(eLabelBgColorId, GetFeatureTag(eLabelBgColorId), btc);
    x_AddFeatureDesc(eLabelTagColorId, GetFeatureTag(eLabelTagColorId), btc);
    x_AddFeatureDesc(eTreeSimplificationTagId,
                     GetFeatureTag(eTreeSimplificationTagId), btc);
    x_AddFeatureDesc(eNodeInfoId, GetFeatureTag(eNodeInfoId), btc);

    
    int num_leaves = 0;
    NON_CONST_ITERATE (CNodeSet::Tdata, node, btc.SetNodes().Set()) {
        if ((*node)->CanGetFeatures()) {
            NON_CONST_ITERATE (CNodeFeatureSet::Tdata, node_feature,
                               (*node)->SetFeatures().Set()) {
                if ((*node_feature)->GetFeatureid() == eLabelId) {
                    // a terminal node
                    // figure out which sequence this corresponds to
                    // from the numerical id we stuck in as a label
                    
                    string label_id = (*node_feature)->GetValue();
                    unsigned int seq_number;
                    if(!isdigit((unsigned char) label_id[0])) {
                        const char* ptr = label_id.c_str();
                        // For some reason there is "N<number>" now, 
                        // not numerical any more. Need to skip "N" 
                        seq_number = NStr::StringToInt((string)++ptr);
                    }
                    else {
                        seq_number = NStr::StringToInt(
                                                 (*node_feature)->GetValue());                    
                    }

                    if ((int)seq_number >= num_rows) {
                        NCBI_THROW(CPhyTreeFormatterException, eInvalidInput,
                                   "Number of Seq-ids is smaller than number "
                                   "of tree leaves");
                    }
                    num_leaves++;

                    // Replace numeric label with real label
                    (*node_feature)->SetValue(labels[seq_number]);

                    //Gets gi, if cnnot gets best id
                    string id_string 
                        = s_GetSeqIDString(bio_seq_handles[seq_number], true);

                    x_AddFeature(eSeqIdId, id_string, node); 

                    // add organism attribute if possible
                    if (!organisms[seq_number].empty()) {
                        x_AddFeature(eOrganismId, organisms[seq_number], node);
                    }

                    // add seq-title attribute if possible
                    if (!titles[seq_number].empty()) {
                        x_AddFeature(eTitleId, titles[seq_number], node); 
                    }
                    // add blast-name attribute if possible
                    if (!accession_nbrs[seq_number].empty()) {
                        x_AddFeature(eAccessionNbrId, accession_nbrs[seq_number],
                                     node);
                    }

                    // add blast-name attribute if possible
                    if (!blast_names[seq_number].empty()) {
                        x_AddFeature(eBlastNameId, blast_names[seq_number],
                                     node);
                    }                   

                    x_AddFeature(eAlignIndexId, NStr::IntToString(seq_number),
                                 node); 

                    x_AddFeature(eNodeColorId,
                                 tax_node_colors[seq_number], node);

                    // mark query node
                    if (!mark_leaves.empty()
                        && binary_search(mark_leaves.begin(),
                                         mark_leaves.end(), seq_number)) {

                        // color for query node
                        x_AddFeature(eLabelBgColorId,
                                     s_kQueryNodeBgColor, node); 

                        x_AddFeature(eLabelTagColorId,
                                     s_kQueryNodeColor, node);

                        x_AddFeature(eNodeInfoId, kNodeInfoQuery, node);

                    }
                    // mark sequence of type, unless the node represents query
                    else if(linkoutDB) {
                        int seqLinkout = linkoutDB->GetLinkout(*seqids[seq_number],"");
                        if(seqLinkout & linkoutType) {
                            // color for "linkout" node
                            string bgColor = linkotTypeToBGColor[linkoutType];
                            if(!bgColor.empty()) {
                                x_AddFeature(eLabelBgColorId,bgColor, node); 
                                x_AddFeature(eNodeInfoId, kNodeInfoSeqFromType,
                                             node);
                            }
                        }
                    }
                    
                    // done with this node
                    break;
                }
            }
            x_AddFeature(eTreeSimplificationTagId, s_kSubtreeDisplayed, node);
        }
    }

    if ((int)num_rows != num_leaves) {
        NCBI_THROW(CPhyTreeFormatterException, eInvalidInput, "There are more Seq-ids"
                   " then tree leaves");
    }
}


void CPhyTreeFormatter::x_MarkLeavesBySeqId(CBioTreeContainer& btc,
                                            vector<string>& ids,
                                            CScope& scope)
{

    vector< pair< CNode*, CSeq_id_Handle> > nodes;
    NON_CONST_ITERATE (CNodeSet::Tdata, node, btc.SetNodes().Set()) {
        if ((*node)->CanGetFeatures()) {
            NON_CONST_ITERATE (CNodeFeatureSet::Tdata, node_feature,
                               (*node)->SetFeatures().Set()) {

                if ((*node_feature)->GetFeatureid() == eSeqIdId
                    || (*node_feature)->GetFeatureid() == eAccessionNbrId) {

                    pair<CNode*, CSeq_id_Handle> p;
                    p.first = node->GetNonNullPointer();
                    CSeq_id sid((*node_feature)->GetValue());
                    p.second = CSeq_id_Handle::GetHandle(sid);
                    nodes.push_back(p);
                }
            }
        }
    }
    sort(nodes.begin(), nodes.end(), compare_nodes_by_seqid());

    ITERATE (vector<string>, sid, ids) {
        CSeq_id seqid(*sid);
        CSeq_id_Handle idhandle = CSeq_id_Handle::GetHandle(seqid);
        pair<CNode*, CSeq_id_Handle> p(nullptr, idhandle);
        vector< pair<CNode*, CSeq_id_Handle> >::iterator node
            = lower_bound(nodes.begin(), nodes.end(), p,
                          compare_nodes_by_seqid());

        if (node != nodes.end() && scope.IsSameBioseq(idhandle,
                                                      node->second,
                                                      CScope::eGetBioseq_All)) {

            // set features
            // color for query node
            x_AddFeature(eLabelBgColorId, s_kQueryNodeBgColor, node->first); 
            x_AddFeature(eNodeInfoId, kNodeInfoQuery, node->first);
        }
    }
}


// Add feature descriptor in bio tree
void CPhyTreeFormatter::x_AddFeatureDesc(int id, const string& desc, 
                                  CBioTreeContainer& btc) 
{
    CRef<CFeatureDescr> feat_descr(new CFeatureDescr);
    feat_descr->SetId(id);
    feat_descr->SetName(desc);
    btc.SetFdict().Set().push_back(feat_descr);
}   


// Add feature to a node in bio tree
void CPhyTreeFormatter::x_AddFeature(int id, const string& value,
                              CNodeSet::Tdata::iterator iter) 
{
    CRef<CNodeFeature> node_feature(new CNodeFeature);
    node_feature->SetFeatureid(id);
    node_feature->SetValue(value);
    (*iter)->SetFeatures().Set().push_back(node_feature);
}

// Add feature to a node in bio tree
void CPhyTreeFormatter::x_AddFeature(int id, const string& value, CNode* node) 
{
    CRef<CNodeFeature> node_feature(new CNodeFeature);
    node_feature->SetFeatureid(id);
    node_feature->SetValue(value);
    node->SetFeatures().Set().push_back(node_feature);
}


ETreeTraverseCode
CPhyTreeFormatter::CExpander::operator()(CBioTreeDynamic::CBioNode& node,
                                         int delta)
{
    if (delta == 0 || delta == 1) {
        if (node.GetFeature(GetFeatureTag(eTreeSimplificationTagId))
            != s_kSubtreeDisplayed && !node.IsLeaf()) {
            
            node.SetFeature(GetFeatureTag(eTreeSimplificationTagId),
                            s_kSubtreeDisplayed);

            node.SetFeature(GetFeatureTag(eNodeColorId), "");
        }
    }
    return eTreeTraverse;
}
END_NCBI_SCOPE
