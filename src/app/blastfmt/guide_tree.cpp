/*  $Id: guide_tree.cpp$
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

#include <util/image/image.hpp>
#include <util/image/image_util.hpp>
#include <util/image/image_io.hpp>

#include <gui/widgets/phylo_tree/phylo_tree_rect_cladogram.hpp>
#include <gui/widgets/phylo_tree/phylo_tree_slanted_cladogram.hpp>
#include <gui/widgets/phylo_tree/phylo_tree_radial.hpp>
#include <gui/widgets/phylo_tree/phylo_tree_force.hpp>

#include <connect/ncbi_conn_stream.hpp>

#include <serial/objostrasnb.hpp>
#include <algo/phy_tree/bio_tree_conv.hpp>

#include <connect/services/netcache_api.hpp>
//This include is here - to fix compile error related to #include <serial/objostrasnb.hpp> - not sure why
#include <objmgr/util/sequence.hpp>

#include "guide_tree_calc.hpp"
#include "guide_tree.hpp"




USING_NCBI_SCOPE;
USING_SCOPE(objects);


CGuideTree::CGuideTree(const CGuideTreeCalc& guide_tree_calc) 
{
    x_Init();

    BioTreeConvertContainer2Dynamic(m_Dyntree, guide_tree_calc.GetSerialTree());
    m_DataSource.Reset(new CPhyloTreeDataSource(m_Dyntree));

    m_QueryNodeId = guide_tree_calc.GetQueryNodeId();
    const CGuideTreeCalc::TBlastNameColorMap map
        = guide_tree_calc.GetBlastNameColorMap();

    // copy blast name to color map
    m_BlastNameColorMap.resize(map.size());
    copy(map.begin(), map.end(), m_BlastNameColorMap.begin());
}

CGuideTree::CGuideTree(CBioTreeContainer& btc,
                       CGuideTreeCalc::ELabelType lblType)
{
    x_Init();

    x_InitTreeLabels(btc,lblType);     
    BioTreeConvertContainer2Dynamic(m_Dyntree, btc);
    m_DataSource.Reset(new CPhyloTreeDataSource(m_Dyntree));
}


CGuideTree::CGuideTree(CBioTreeContainer& btc,
                       const vector< CRef<CSeq_id> >& seqids,
                       CScope& scope,
                       CGuideTreeCalc::ELabelType lbl_type,
                       bool mark_query_node)
{
    x_Init();

    CGuideTreeCalc::InitTreeFeatures(btc, seqids, scope, lbl_type,
                                     mark_query_node, m_BlastNameColorMap,
                                     m_QueryNodeId);

    BioTreeConvertContainer2Dynamic(m_Dyntree, btc);
    m_DataSource.Reset(new CPhyloTreeDataSource(m_Dyntree));
}


CGuideTree::CGuideTree(const CBioTreeDynamic& tree)
{
    x_Init();

    m_Dyntree = tree;
    m_DataSource.Reset(new CPhyloTreeDataSource(m_Dyntree));
}


bool CGuideTree::WriteImage(CNcbiOstream& out)
{
    bool success = x_RenderImage();            
    if(success) {
        CImageIO::WriteImage(m_Context->GetBuffer(), out, m_ImageFormat);
    }
    return success;
}


bool CGuideTree::WriteImage(const string& filename)
{
    bool success = x_RenderImage();
    if(success) {
        CImageIO::WriteImage(m_Context->GetBuffer(), filename, m_ImageFormat);
    }
    return success;
}

string CGuideTree::WriteImageToNetcache(string netcacheServiceName,string netcacheClientName)
{
    string imageKey;
    bool success = x_RenderImage();            
    if(success) {
        CNetCacheAPI nc_client(netcacheServiceName,netcacheClientName);
        imageKey = nc_client.PutData(m_Context->GetBuffer().GetData(),  m_Width*m_Height*3);
    }
    return imageKey;
}


bool CGuideTree::WriteTree(CNcbiOstream& out)
{
    out << MSerial_AsnText << *GetSerialTree();

    return true;
}


bool CGuideTree::WriteTree(const string& filename)
{
    CBioTreeDynamic dyntree;
    CBioTreeContainer btc;

    m_DataSource->Save(dyntree);
    BioTreeConvert2Container(btc, dyntree);
    
    // Writing to file
    CNcbiOfstream ostr(filename.c_str());
    if (!ostr) {
        return false;
    }
    ostr << MSerial_AsnText << btc;

    return true;
}

string CGuideTree::WriteTreeInNetcache(string netcacheServiceName,string netcacheClientName)
{
    CBioTreeDynamic dyntree;
    CBioTreeContainer btc;

    m_DataSource->Save(dyntree);
    BioTreeConvert2Container(btc, dyntree);
        
    //Put contetnts of the tree in memory
    CConn_MemoryStream iostr;
    CObjectOStreamAsnBinary outBin(iostr);        
    outBin << btc;
    outBin.Flush();
    
    
    CNetCacheAPI nc_client(netcacheServiceName,netcacheClientName);    
    size_t data_length = iostr.tellp();
    string data;                
    data.resize(data_length);        
    iostr.read(const_cast<char*>(data.data()), data_length);
    string treeKey = nc_client.PutData(&data[0], data.size());
    return treeKey;
}    






bool CGuideTree::PrintNewickTree(const string& filename)
{
    CNcbiOfstream out(filename.c_str());
    if (!out) {
        return false;
    }
    return PrintNewickTree(out);
}

bool CGuideTree::PrintNewickTree(CNcbiOstream& ostr)
{
    _ASSERT(m_DataSource->GetTree());

    x_PrintNewickTree(ostr, *m_DataSource->GetTree());
    ostr << NcbiEndl;
    return true;
}

bool CGuideTree::PrintNexusTree(const string& filename, 
                                const string& tree_name,
                                bool force_win_eol)
{
    CNcbiOfstream out(filename.c_str());
    if (!out) {
        return false;
    }
    return PrintNexusTree(out, tree_name, force_win_eol);
}

bool CGuideTree::PrintNexusTree(CNcbiOstream& ostr, const string& tree_name,
                                bool force_win_eol)
{
    ostr << "#NEXUS";
    // Nedded when used for downloading from web
    if (force_win_eol) {
        ostr << "\r\n";
    }
    else {
        ostr << NcbiEndl;
    }
    ostr << "BEGIN TREES;";
    if (force_win_eol) {
        ostr << "\r\n";
    }
    else {
        ostr << NcbiEndl;
    }
    ostr <<  "TREE " << tree_name << " = ";
    x_PrintNewickTree(ostr, *m_DataSource->GetTree());
    if (force_win_eol) {
        ostr << "\r\n";
    }
    else {
        ostr << NcbiEndl;
    }
    ostr << "END";
    if (force_win_eol) {
        ostr << "\r\n";
    }
    else {
        ostr << NcbiEndl;
    }

    return true;
}

bool CGuideTree::SaveTreeAs(CNcbiOstream& ostr, ETreeFormat format)
{
    switch (format) {
    case eImage :
        return WriteImage(ostr);

    case eASN :
        return WriteTree(ostr);

    case eNewick :
        return PrintNewickTree(ostr);

    case eNexus :
        return PrintNexusTree(ostr);
    }

    return false;
}


void CGuideTree::FullyExpand(void)
{
    TreeDepthFirstTraverse(*m_DataSource->GetTree(), CExpander());
}


void CGuideTree::SimplifyTree(ETreeSimplifyMode method, bool refresh)
{

    switch (method) {

    // Do nothing
    case eNone:
        break;
        
    // Collapse all subtrees with common blast name       
    case eBlastName :
    {
        FullyExpand();
        CPhyloTreeNodeGroupper groupper 
            = TreeDepthFirstTraverse(*m_DataSource->GetTree(), 
                   CPhyloTreeNodeGroupper(CGuideTreeCalc::kBlastNameTag,
                                        CGuideTreeCalc::kNodeColorTag,
                                        *m_DataSource));

        if (!groupper.GetError().empty()) {
            NCBI_THROW(CGuideTreeException, eTraverseProblem,
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
      NCBI_THROW(CGuideTreeException, eInvalidOptions,
                 "Invalid tree simplify mode");
    }

    if (refresh) {
        Refresh();
    }

    m_SimplifyMode = method;
}

bool CGuideTree::ExpandCollapseSubtree(int node_id, bool refresh)
{
    //CPhyloTreeDataSource::GetNode(int) does not return NULL when node 
    // is not found!
    CPhyloTreeNode* node = x_GetNode(node_id);
    
    if (node->Expanded()) {

        // Collapsing
        node->ExpandCollapse(IPhyGraphicsNode::eHideChilds);

        // Track labels in order to select proper color for collapsed node
        CPhyloTreeLabelTracker 
            tracker = TreeDepthFirstTraverse(*node, CPhyloTreeLabelTracker(
                                           CGuideTreeCalc::kBlastNameTag,
                                           CGuideTreeCalc::kNodeColorTag,
                                           *m_DataSource));

        if (!tracker.GetError().empty()) {
            NCBI_THROW(CGuideTreeException, eTraverseProblem,
                       tracker.GetError());
        }

        CPhyloTreeLabelTracker::TLabelColorMap_I it = tracker.Begin();
        string label = it->first;
        ++it;
        for (; it != tracker.End(); ++it) {
            label += ", " + it->first;
        }
        node->SetLabel(label);
        if (tracker.GetNumLabels() == 1) {
            (**node).SetFeature(CGuideTreeCalc::kNodeColorTag,
                                tracker.Begin()->second);
        }
        // Mark collapsed subtree that contains query node
        if (IsQueryNodeSet()) {
            x_MarkCollapsedQueryNode(node);
        }
    }
    else {

        // Expanding
        node->ExpandCollapse(IPhyGraphicsNode::eShowChilds);
        (**node).SetFeature(CGuideTreeCalc::kNodeColorTag, "");      
    }

    if (refresh) {
        Refresh();
    }

    m_SimplifyMode = eNone;
    return node->Expanded();
}

void CGuideTree::RerootTree(int new_root_id, bool refresh)
{
    //    CPhyloTreeNode* node = m_DataSource->GetNode(new_root_id);
    CPhyloTreeNode* node = x_GetNode(new_root_id);

    // Collapsed node cannot be a root, so a parent node will be a new
    // root if such node is selected
    if (node && node->IsLeafEx() && node->GetParent()) {
        node = (CPhyloTreeNode*)node->GetParent();
    }

    // node == NULL means that collapsed child of current root was selected
    if (node) {
        m_DataSource->RerootTree(node);

        if (refresh) {
            Refresh();
        }
    }
}

bool CGuideTree::ShowSubtree(int root_id, bool refresh)
{
    bool collapsed = false;
    CPhyloTreeNode* node = x_GetNode(root_id);

    // If a collapsed node is clicked, then it needs to be expanded
    // in order to show the subtree
    if (!node->Expanded()) {
        collapsed = true;
        m_DataSource->ExpandCollapse(node, IPhyGraphicsNode::eShowChilds);
        m_SimplifyMode = eNone;        
    }
    m_DataSource->SetSubtree(node);

    if (refresh) {
        Refresh();
    }
    return collapsed;
}


void CGuideTree::Refresh(void)
{
    // Often the edge starting in root has zero length which
    // does not render well. By setting very small distance this is
    // avoided.
    CPhyloTreeNode::TNodeList_I it = m_DataSource->GetTree()->SubNodeBegin();
    while (it != m_DataSource->GetTree()->SubNodeEnd()) {
        if ((*it)->GetValue().GetDistance() == 0.0) {
            (*it)->GetValue().SetDistance(
                 m_DataSource->GetNormDistance() * 0.005);
            (*it)->GetValue().Sync();
        }
        ++it;
    }

    m_DataSource->Refresh();
}

bool CGuideTree::IsSingleBlastName(void)
{
    CSingleBlastNameExaminer examiner
        = TreeDepthFirstTraverse(*m_DataSource->GetTree(), 
                                 CSingleBlastNameExaminer(*m_DataSource));
    return examiner.IsSingleBlastName();
}


auto_ptr<CGuideTree::STreeInfo> CGuideTree::GetTreeInfo(int node_id)
{
    CPhyloTreeNode* node = x_GetNode(node_id);

    // find all subtree leaves
    CLeaveFinder leave_finder = TreeDepthFirstTraverse(*node, CLeaveFinder());
    vector<CPhyloTreeNode*>& leaves = leave_finder.GetLeaves();

    TBioTreeFeatureId fid_blast_name = x_GetFeatureId(
                                              CGuideTreeCalc::kBlastNameTag);

    TBioTreeFeatureId fid_node_color = x_GetFeatureId(
                                              CGuideTreeCalc::kNodeColorTag);

    TBioTreeFeatureId fid_seqid = x_GetFeatureId(CGuideTreeCalc::kSeqIDTag);
    TBioTreeFeatureId fid_accession = x_GetFeatureId(
                                            CGuideTreeCalc::kAccessionNbrTag);
    

    auto_ptr<STreeInfo> info(new STreeInfo);
    hash_set<string> found_blast_names;

    // for each leave get feature values
    ITERATE (vector<CPhyloTreeNode*>, it, leaves) {
        string blast_name = x_GetNodeFeature(*it, fid_blast_name);
        string color = x_GetNodeFeature(*it, fid_node_color);
        string seqid = x_GetNodeFeature(*it, fid_seqid);
        string accession = x_GetNodeFeature(*it, fid_accession);
        
        // only one entry per blast name for color map
        if (found_blast_names.find(blast_name) == found_blast_names.end()) {
            info->blastname_color_map.push_back(make_pair(blast_name, color));
            found_blast_names.insert(blast_name);
        }

        info->seq_ids.push_back(seqid);
        info->accessions.push_back(accession);
    }

    return info;
}


void CGuideTree::x_Init(void)
{
    m_Width = 800;
    m_Height = 600;
    m_ImageFormat = CImageIO::ePng;
    m_RenderFormat = eRect;
    m_DistanceMode = true;
    m_NodeSize = 3;
    m_LineWidth = 1;
    m_QueryNodeId = -1;
    m_SimplifyMode = eNone;
}


CPhyloTreeNode* CGuideTree::x_GetNode(int id, CPhyloTreeNode* root)
{
    CPhyloTreeNode* tree = root ? root : m_DataSource->GetTree();
    CNodeFinder node_finder = TreeDepthFirstTraverse(*tree, CNodeFinder(id));

    if (!node_finder.GetNode()) {
        NCBI_THROW(CGuideTreeException, eNodeNotFound, (string)"Node "
                   + NStr::IntToString(id) + (string)" not found");
    }

    return node_finder.GetNode();
}

inline
TBioTreeFeatureId CGuideTree::x_GetFeatureId(const string& tag)
{
    const CBioTreeFeatureDictionary& fdict = m_DataSource->GetDictionary();
    if (!fdict.HasFeature(tag)) {
        NCBI_THROW(CGuideTreeException, eInvalid, "Feature " + tag
                   + " not present");
    }

    return fdict.GetId(tag);
}

inline
string CGuideTree::x_GetNodeFeature(const CPhyloTreeNode* node,
                                    TBioTreeFeatureId fid)
{
    _ASSERT(node);

    const CBioTreeFeatureList& flist = node->GetValue().GetBioTreeFeatureList();
    return flist.GetFeatureValue(fid);
}


void CGuideTree::PreComputeImageDimensions()
{
    m_PhyloTreeScheme.Reset(new CPhyloTreeScheme());

    m_PhyloTreeScheme->SetSize(CPhyloTreeScheme::eNodeSize) = m_NodeSize;
    m_PhyloTreeScheme->SetSize(CPhyloTreeScheme::eLineWidth) = m_LineWidth;

    GLdouble mleft = 10;
    GLdouble mtop = 10;
    GLdouble mright = 140;
    GLdouble mbottom = 10;

    //For now until done automatically  
    if(m_RenderFormat == eRadial || m_RenderFormat == eForce) {
        mleft = 200;
    }
    m_PhyloTreeScheme->SetMargins(mleft, mtop, mright, mbottom);

    // Min dimensions check
    // The minimum width and height which should be acceptable to output 
    // image to display all data without information loss.
    auto_ptr<IPhyloTreeRenderer> calcRender;       
    //use recatngle for calulations of all min sizes
    calcRender.reset(new CPhyloRectCladogram());      
    m_MinDimRect = IPhyloTreeRenderer::GetMinDimensions(*m_DataSource,
                                                        *calcRender,
                                                        *m_PhyloTreeScheme);
}

void CGuideTree::x_CreateLayout(void)
{
    if (m_PhyloTreeScheme.Empty()) {
        PreComputeImageDimensions();
    }

    // Creating pane
    m_Pane.reset(new CGlPane());

    switch (m_RenderFormat) {
    case eSlanted : 
        m_Renderer.reset(new CPhyloSlantedCladogram());
        break;

    case eRadial : 
        m_Renderer.reset(new CPhyloRadial());
        break;

    case eForce : 
        m_Renderer.reset(new CPhyloForce());
        break;

    default:
    case eRect : 
        m_Renderer.reset(new CPhyloRectCladogram());
    }

    m_Renderer->SetDistRendering(m_DistanceMode);
 
    // Creating layout
    m_Renderer->SetFont(new CGlBitmapFont(CGlBitmapFont::eHelvetica10));
    
    
    m_Renderer->SetModelDimensions(m_Width, m_Height);

    // Setting variable sized collapsed nodes
    m_PhyloTreeScheme->SetBoaNodes(true);

    m_Renderer->SetScheme(*m_PhyloTreeScheme);    

    m_Renderer->SetZoomablePrimitives(false);

    m_Renderer->Layout(m_DataSource.GetObject());


    // modifying pane to reflect layout changes
    m_Pane->SetAdjustmentPolicy(CGlPane::fAdjustAll, CGlPane::fAdjustAll);
    m_Pane->SetMinScaleX(1 / 30.0);  m_Pane->SetMinScaleY(1 / 30.0);
    m_Pane->SetOriginType(CGlPane::eOriginLeft, CGlPane::eOriginBottom);
    m_Pane->EnableZoom(true, true);
    m_Pane->SetViewport(TVPRect(0, 0, m_Width, m_Height));
    m_Pane->SetModelLimitsRect(m_Renderer->GetRasterRect());
    m_Pane->SetVisibleRect(m_Renderer->GetRasterRect());

    m_Pane->ZoomAll();
}

void CGuideTree::x_Render(void)
{
   glClearColor(0.95f, 1.0f, 0.95f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   m_Renderer->Render(*m_Pane.get(), m_DataSource.GetObject()); 
}


bool CGuideTree::x_RenderImage(void)
{
    bool success = false;
   
    try {


        m_Context.Reset(new CGlOsContext(m_Width, m_Height));
        m_Context->MakeCurrent();         

        x_CreateLayout();
        x_Render();

        glFinish();
        m_Context->SetBuffer().SetDepth(3);
        CImageUtil::FlipY(m_Context->SetBuffer());

        success = true;
    }
    catch (CException& e) {
        m_ErrorMessage = "Error rendering image: " + e.GetMsg();

    }
    catch (exception& e) {
        m_ErrorMessage = "Error rendering image: " + (string)e.what();
    }
    catch (...) {
        m_ErrorMessage = "Error rendering image: unknown error";
    }
    
    return success;
}    


//TO DO: Input parameter should be an interface for future groupping options
void CGuideTree::x_CollapseSubtrees(CPhyloTreeNodeGroupper& groupper)
{
    for (CPhyloTreeNodeGroupper::CLabeledNodes_I it=groupper.Begin();
         it != groupper.End(); ++it) {
        it->GetNode()->ExpandCollapse(IPhyGraphicsNode::eHideChilds);
        it->GetNode()->SetLabel(it->GetLabel());
        it->GetNode()->GetValue().SetFeature(CGuideTreeCalc::kNodeColorTag,
                                             it->GetColor());
        if (m_QueryNodeId > -1) {
            x_MarkCollapsedQueryNode(it->GetNode());
        }
    }
}


void CGuideTree::x_MarkCollapsedQueryNode(CPhyloTreeNode* node)
{
    //Check if query node is in the collapsed subtree
    // and marking query node
    // optimized so that number of visited nodes is decreased
    // assumes node numbering scheme
    int root_id = (int)(**node).GetId();
    if (m_QueryNodeId > root_id
        && m_QueryNodeId <= root_id + (**node).GetNodesNmb()) {

        //CPhyloTreeDataSource::GetNode(int) traverses the tree from root
        CPhyloTreeNode* query_node = x_GetNode(m_QueryNodeId, node);

        const CBioTreeFeatureDictionary& fdict = m_DataSource->GetDictionary();
        TBioTreeFeatureId fid = fdict.GetId(CGuideTreeCalc::kLabelBgColorTag);

        const CBioTreeFeatureList& 
            flist = (**query_node).GetBioTreeFeatureList();

        const string& color = flist.GetFeatureValue(fid);
        (**node).SetFeature(CGuideTreeCalc::kLabelBgColorTag, color);
    }
} 

//Recusrive
void CGuideTree::x_PrintNewickTree(CNcbiOstream& ostr,
                                   const CTreeNode<CPhyTreeNode>& node,
                                   bool is_outer_node)
{

    string label;

    if (!node.IsLeaf()) {
        ostr << '(';
        for (CTreeNode<CPhyTreeNode>::TNodeList_CI it = node.SubNodeBegin(); it != node.SubNodeEnd(); ++it) {
            if (it != node.SubNodeBegin())
                ostr << ", ";
            x_PrintNewickTree(ostr, **it, false);
        }
        ostr << ')';
    }

    if (!is_outer_node) {
        if (node.IsLeaf() || !node.GetValue().GetLabel().empty()) {
            label = node.GetValue().GetLabel();
            for (size_t i=0;i < label.length();i++)
                if (!isalpha(label.at(i)) && !isdigit(label.at(i)))
                    label.at(i) = '_';
            ostr << label;
        }
        ostr << ':' << node.GetValue().GetDistance();
    }
    else
        ostr << ';';
}

void CGuideTree::x_InitTreeLabels(CBioTreeContainer &btc,CGuideTreeCalc::ELabelType lblType) 
{
   
    NON_CONST_ITERATE (CNodeSet::Tdata, node, btc.SetNodes().Set()) {
        if ((*node)->CanGetFeatures()) {
            string  blastName = "";
            //int id = (*node)->GetId();
            CRef< CNodeFeature > label_feature_node;
            CRef< CNodeFeature > selected_feature_node;
            int featureSelectedID = CGuideTreeCalc::eLabelId;
            if (lblType == CGuideTreeCalc::eSeqId || lblType == CGuideTreeCalc::eSeqIdAndBlastName) {
//              featureSelectedID = s_kSeqIdId;
                featureSelectedID = CGuideTreeCalc::eAccessionNbrId;                
            }
            else if (lblType == CGuideTreeCalc::eTaxName) {
                featureSelectedID = CGuideTreeCalc::eOrganismId;
            }                
            else if (lblType == CGuideTreeCalc::eSeqTitle) {
                featureSelectedID = CGuideTreeCalc::eTitleId;
            }
            else if (lblType == CGuideTreeCalc::eBlastName) {
                featureSelectedID = CGuideTreeCalc::eBlastNameId;
            }
            
            NON_CONST_ITERATE (CNodeFeatureSet::Tdata, node_feature,
                               (*node)->SetFeatures().Set()) {

                
                //typedef list< CRef< CNodeFeature > > Tdata

                if ((*node_feature)->GetFeatureid() == CGuideTreeCalc::eLabelId) { 
                   label_feature_node = *node_feature;                                       
                }
                //If label typ = GI and blast name - get blast name here
                if ((*node_feature)->GetFeatureid() == CGuideTreeCalc::eBlastNameId && 
                                    lblType == CGuideTreeCalc::eSeqIdAndBlastName) { 
                   blastName = (*node_feature)->GetValue();
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
                if(lblType == CGuideTreeCalc::eSeqIdAndBlastName) {
                    //concatinate with blastName
                    label = label + "(" + blastName + ")";
                }
                //label_feature_node->SetValue(label);
                label_feature_node->ResetValue();
                label_feature_node->SetValue() = label;                
            }
        }
    }            
}



int CGuideTree::GetRootNodeID()
{
    return(m_DataSource->GetTree()->GetValue().GetId());
}


CRef<CBioTreeContainer> CGuideTree::GetSerialTree(void)
{
    CBioTreeDynamic dyntree;
    CRef<CBioTreeContainer> btc(new CBioTreeContainer());

    m_DataSource->Save(dyntree);
    BioTreeConvert2Container(*btc, dyntree);

    return btc;
}


//Sets data for display purposes marking the selected node on the tree
void CGuideTree::SetSelection(int hit)
{
    if ((hit+1) >=0){
                CPhyloTreeNode * node = m_DataSource->GetNode(hit);
                if (node) {
                        m_DataSource->SetSelection(node, true, true, true);
                }
        }
}

string CGuideTree::GetMap(string jsClickNode,string jsClickLeaf,string jsMouseover,string jsMouseout,string jsClickQuery,bool showQuery)
                                        
{
    CGuideTreeCGIMap map_visitor(m_Pane.get(),jsClickNode,jsClickLeaf,jsMouseover,jsMouseout,jsClickQuery,showQuery);    
        map_visitor = TreeDepthFirstTraverse(*m_DataSource->GetTree(), map_visitor);
        return map_visitor.GetMap();
}


void CGuideTreeCGIMap::x_FillNodeMapData(CPhyloTreeNode &  tree_node) 
{           
       if (!(*tree_node).GetDictionaryPtr()) {
           NCBI_THROW(CException, eInvalid,
                      "Tree node has no feature dicionary");
       }

       int x  = m_Pane->ProjectX((*tree_node).XY().first);
       int y  = m_Pane->GetViewport().Height() - m_Pane->ProjectY((*tree_node).XY().second);           
       int ps = 5;

       
       string strTooltip = (*tree_node).GetLabel().size()?(*tree_node).GetLabel():"No Label";
           

       int nodeID = (*tree_node).GetId();
       bool isQuery = false;
       
       //IsLeaf() instead of IsLeafEx() allows popup menu for collapsed nodes
       if(tree_node.IsLeaf()) {
            const CBioTreeFeatureList& featureList = (*tree_node).GetBioTreeFeatureList();
       
            if (m_ShowQuery && (*tree_node).GetDictionaryPtr()->HasFeature(CGuideTreeCalc::kAlignIndexIdTag)){ //"align-index"
                int align_index = NStr::StringToInt(
                                   featureList.GetFeatureValue(        
                                       (*tree_node).GetDictionaryPtr()->GetId(
                                           CGuideTreeCalc::kAlignIndexIdTag)));

                isQuery = align_index == 0; //s_kPhyloTreeQuerySeqIndex  
            }       

            if(isQuery) {
                strTooltip += "(query)";
            }
            m_Map += "<area alt=\""+strTooltip+"\" title=\""+strTooltip+"\""; 

            string accessionNbr;
            if ((*tree_node).GetDictionaryPtr()->HasFeature(
                               CGuideTreeCalc::kAccessionNbrTag)){ //accession-nbr

                accessionNbr = featureList.GetFeatureValue(
                                  (*tree_node).GetDictionaryPtr()->GetId(
                                        CGuideTreeCalc::kAccessionNbrTag));            
            }
            string arg = NStr::IntToString(nodeID);
            if(!accessionNbr.empty()) {
                arg += ",'" + accessionNbr + "'";
            }            
            if (isQuery && !m_JSClickQueryFunction.empty()) {
                m_Map += " href=\"" + m_JSClickQueryFunction + arg+");\""; 
            }
            else if(!isQuery && !m_JSClickLeafFunction.empty()) {
                    //m_Map += " href=\"" + string(JS_SELECT_NODE_FUNC) + NStr::IntToString(nodeID)+");\"";
                m_Map += " href=\"" + m_JSClickLeafFunction + arg+");\"";                
            }
       }
       else {           
            m_Map += "<area";
            if(!m_JSMouseoverFunction.empty())
                //"javascript:setupPopupMenu(" 
                m_Map += " onmouseover=\"" + m_JSMouseoverFunction + NStr::IntToString(nodeID)+ ");\"";
                    
            if(!m_JSMouseoutFunction.empty())
                //"PopUpMenu2_Hide(0);"
                m_Map += " onmouseout=\"" + m_JSMouseoutFunction + "\"";
            if(!m_JSClickNodeFunction.empty()) 
                //"//javascript:MouseHit(" //string(JS_SELECT_NODE_FUNC)
                m_Map += " href=\"" + m_JSClickNodeFunction + NStr::IntToString(nodeID)+");\"";       
       } 
       m_Map += " coords=\"";
       m_Map += NStr::IntToString(x-ps); m_Map+=",";
       m_Map += NStr::IntToString(y-ps); m_Map+=",";
       m_Map += NStr::IntToString(x+ps); m_Map+=",";
       m_Map += NStr::IntToString(y+ps); 
       m_Map+="\">";      
}

