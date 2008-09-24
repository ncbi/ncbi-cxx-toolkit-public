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

#include <connect/ncbi_conn_stream.hpp>
#include <connect/services/netcache_client.hpp>

#include <serial/objostrasnb.hpp>

#include <guide_tree_calc.hpp>
#include <guide_tree.hpp>

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
}

CGuideTree::CGuideTree(const CBioTreeDynamic& tree, int height, int width,
                       CImageIO::EType format) 
    : m_DataSource(new CPhyloTreeDataSource(tree))
{
    x_Init();

    m_Height = height;
    m_Width = width;
    m_ImageFormat = format;
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
        if(!filename.empty()) {
            CImageIO::WriteImage(m_Context->GetBuffer(), filename,
                                 m_ImageFormat);
        }
        else {//netcache
            CNetCacheClient_LB nc_client("blast_guide_tree", "NC_PhyloTree");
            m_ImageKey = nc_client.PutData(m_Context->GetBuffer().GetData(), 
                                           m_Width * m_Height * 3);
        }
         
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
    
    if (!filename.empty()) {

        // Writing to file
        CNcbiOfstream ostr(filename.c_str());
        if (!ostr) {
            return false;
        }
        ostr << MSerial_AsnText << btc;
    }
    else {

        // Saving in net cache
        CConn_MemoryStream mem_str;
        //        CObjectOStreamAsnBinary out_bin(mem_str);
        //        out_bin << btc;
        mem_str << MSerial_AsnBinary << btc;
        //        out_bin.Flush();

        CNetCacheClient_LB nc_client("blast_guide_tree", "NC_PhyloTree");
        size_t size = mem_str.tellp();
        string data;
        data.resize(size);
        mem_str.read(const_cast<char*>(data.data()), size);
        m_TreeKey = nc_client.PutData(&data[0], data.size());
    }

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

bool CGuideTree::SaveTreeAs(ETreeFormat format, const string& filename)
{
    switch (format) {
    case eImage :
        return WriteImage(filename);

    case eSerial :
        return WriteTree(filename);

    case eNewick :
        return PrintNewickTree(filename);

    case eNexus :
        return PrintNexusTree(filename);
    }

    return false;
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
        TreeDepthFirstTraverse(*m_DataSource->GetTree(), CPhyloTreeExpander());
        CPhyloTreeNodeGroupper groupper 
            = TreeDepthFirstTraverse(*m_DataSource->GetTree(), 
               CPhyloTreeNodeGroupper(kBlastNameTag,
                                      kNodeColorTag));

        if (!groupper.GetError().empty()) {
            NCBI_THROW(CException, eUnknown, groupper.GetError());
        }

        x_CollapseSubtrees(groupper);
        break;
    }

    //Fully expand the tree        
    case eFullyExpanded :
        //m_DataSource::ShowAll() does not correct node colors
        TreeDepthFirstTraverse(*m_DataSource->GetTree(), CPhyloTreeExpander());
        break;

    default:
      NCBI_THROW(CException, eUnknown, "Invalid tree simplify mode");
    }

    if (refresh) {
        Refresh();
    }
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
            NCBI_THROW(CException, eUnknown, tracker.GetError());
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
    CPhyloTreeSingleBlastNameExaminer examiner 
        = TreeDepthFirstTraverse(*m_DataSource->GetTree(), 
                                 CPhyloTreeSingleBlastNameExaminer());
    return examiner.IsSingleBlastName();
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
}


CPhyloTreeNode* CGuideTree::x_GetNode(int id, CPhyloTreeNode* root)
{
    CPhyloTreeNode* tree = root ? root : m_DataSource->GetTree();
    CPhyloTreeNodeFinder node_finder = TreeDepthFirstTraverse(*tree,
    //                                   *m_DataSource->GetTree(), 
                                   CPhyloTreeNodeFinder(id));

    if (!node_finder.GetNode()) {
        NCBI_THROW(CException, eUnknown, (string)"Node "
                   + NStr::IntToString(id) + (string)" not found");
    }

    return node_finder.GetNode();
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
