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

#include <util/image/image.hpp>
#include <util/image/image_util.hpp>
#include <util/image/image_io.hpp>

#include <gui/widgets/phylo_tree/phylo_tree_rect_cladogram.hpp>
#include <gui/widgets/phylo_tree/phylo_tree_slanted_cladogram.hpp>
#include <gui/widgets/phylo_tree/phylo_tree_radial.hpp>
#include <gui/widgets/phylo_tree/phylo_tree_force.hpp>

#include <connect/ncbi_conn_stream.hpp>

#include <serial/objostrasnb.hpp>

#include "guide_tree_calc.hpp"
#include "guide_tree.hpp"

USING_NCBI_SCOPE;

const string CGuideTree::kLabelTag = "label";
const string CGuideTree::kSeqIDTag = "seq-id";
const string CGuideTree::kSeqTitleTag = "seq-title";
const string CGuideTree::kOrganismTag = "organism";
const string CGuideTree::kAccessionNbrTag = "accession-nbr";        
const string CGuideTree::kBlastNameTag = "blast-name";    
const string CGuideTree::kAlignIndexIdTag = "align-index";     

const string CGuideTree::kNodeColorTag = "$NODE_COLOR";
const string CGuideTree::kLabelColorTag = "$LABEL_COLOR";
const string CGuideTree::kLabelBgColorTag = "$LABEL_BG_COLOR";
const string CGuideTree::kLabelTagColor = "$LABEL_TAG_COLOR";
const string CGuideTree::kCollapseTag = "$NODE_COLLAPSED";


CGuideTree::CGuideTree(const CGuideTreeCalc& guide_tree_calc) 
{
    x_Init();

    CBioTreeDynamic dyntree;
    BioTreeConvertContainer2Dynamic(dyntree, guide_tree_calc.GetSerialTree());
    m_DataSource.Reset(new CPhyloTreeDataSource(dyntree));

    m_QueryNodeId = guide_tree_calc.GetQueryNodeId();
    const CGuideTreeCalc::TBlastNameColorMap map
        = guide_tree_calc.GetBlastNameColorMap();

    // copy blast name to color map
    m_BlastNameColorMap.resize(map.size());
    copy(map.begin(), map.end(), m_BlastNameColorMap.begin());
}

CGuideTree::CGuideTree(const CBioTreeContainer& btc)
{
    x_Init();

    CBioTreeDynamic dyntree;
    BioTreeConvertContainer2Dynamic(dyntree, btc);
    m_DataSource.Reset(new CPhyloTreeDataSource(dyntree));
}


CGuideTree::CGuideTree(const CBioTreeDynamic& tree)
    : m_DataSource(new CPhyloTreeDataSource(tree))
{
    x_Init();
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


bool CGuideTree::WriteTree(CNcbiOstream& out)
{
    CBioTreeDynamic dyntree;
    CBioTreeContainer btc;

    m_DataSource->Save(dyntree);
    BioTreeConvert2Container(btc, dyntree);

    out << MSerial_AsnText << btc;

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
               CPhyloTreeNodeGroupper(kBlastNameTag,
                                      kNodeColorTag));

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

void CGuideTree::ExpandCollapseSubtree(int node_id, bool refresh)
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
                               kBlastNameTag, kNodeColorTag));

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
            (**node).SetFeature(kNodeColorTag,
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
        (**node).SetFeature(kNodeColorTag, "");      
    }

    if (refresh) {
        Refresh();
    }

    m_SimplifyMode = eNone;
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

void CGuideTree::ShowSubtree(int root_id, bool refresh)
{
    CPhyloTreeNode* node = x_GetNode(root_id);

    // If a collapsed node is clicked, then it needs to be expanded
    // in order to show the subtree
    if (!node->Expanded()) {
        m_DataSource->ExpandCollapse(node, IPhyGraphicsNode::eShowChilds);
    }
    m_DataSource->SetSubtree(node);

    if (refresh) {
        Refresh();
    }
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
                                 CSingleBlastNameExaminer());
    return examiner.IsSingleBlastName();
}


auto_ptr<CGuideTree::STreeInfo> CGuideTree::GetTreeInfo(int node_id)
{
    CPhyloTreeNode* node = x_GetNode(node_id);

    // find all subtree leaves
    CLeaveFinder leave_finder = TreeDepthFirstTraverse(*node, CLeaveFinder());
    vector<CPhyloTreeNode*>& leaves = leave_finder.GetLeaves();

    TBioTreeFeatureId fid_blast_name = x_GetFeatureId(kBlastNameTag);
    TBioTreeFeatureId fid_node_color = x_GetFeatureId(kNodeColorTag);
    TBioTreeFeatureId fid_seqid = x_GetFeatureId(kSeqIDTag);

    auto_ptr<STreeInfo> info(new STreeInfo);
    hash_set<string> found_blast_names;

    // for each leave get feature values
    ITERATE (vector<CPhyloTreeNode*>, it, leaves) {
        string blast_name = x_GetNodeFeature(*it, fid_blast_name);
        string color = x_GetNodeFeature(*it, fid_node_color);
        string seqid = x_GetNodeFeature(*it, fid_seqid);
        
	// only one entry per blast name for color map
        if (found_blast_names.find(blast_name) == found_blast_names.end()) {
            info->blastname_color_map.push_back(make_pair(blast_name, color));
            found_blast_names.insert(blast_name);
        }

        info->seq_ids.push_back(seqid);
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
    const CBioTreeFeatureDictionary& fdict = CPhyTreeNode::GetDictionary();
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



void CGuideTree::x_CreateLayout(void)
{
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
    
    CPhyloTreeScheme *phyloTreeScheme = new CPhyloTreeScheme();
    phyloTreeScheme->SetSize(CPhyloTreeScheme::eNodeSize) = m_NodeSize;
    phyloTreeScheme->SetSize(CPhyloTreeScheme::eLineWidth) = m_LineWidth;

    GLdouble mleft = 10;
    GLdouble mtop = 10;
    GLdouble mright = 140;
    GLdouble mbottom = 10;

    //For now until done automatically  
    if(m_RenderFormat == eRadial || m_RenderFormat == eForce) {
        mleft = 200;
    }
    phyloTreeScheme->SetMargins(mleft, mtop, mright, mbottom);

    // Min dimensions check
    // The minimum width and height which should be acceptable to output 
    // image to display all data without information loss.
    auto_ptr<IPhyloTreeRenderer> calcRender;       
    //use recatngle for calulations of all min sizes
    calcRender.reset(new CPhyloRectCladogram());      
    TVPRect dimRect = IPhyloTreeRenderer::GetMinDimensions(*m_DataSource,
                                                           *calcRender,
                                                           *phyloTreeScheme);

    m_Renderer->SetModelDimensions(m_Width, m_Height);

    // Setting variable sized collapsed nodes
    phyloTreeScheme->SetBoaNodes(true);

    m_Renderer->SetScheme(*phyloTreeScheme);    

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
        it->GetNode()->GetValue().SetFeature(kNodeColorTag, 
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

        const CBioTreeFeatureDictionary& fdict = CPhyTreeNode::GetDictionary();
        TBioTreeFeatureId fid = fdict.GetId(kLabelBgColorTag);

        const CBioTreeFeatureList& 
            flist = (**query_node).GetBioTreeFeatureList();

        const string& color = flist.GetFeatureValue(fid);
        (**node).SetFeature(kLabelBgColorTag, color);
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
