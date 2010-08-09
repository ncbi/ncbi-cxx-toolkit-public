/*  $Id: guide_tree_renderer.cpp$
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
#include <connect/services/netcache_api.hpp>

#include "guide_tree_render.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


CGuideTreeRenderer::CGuideTreeRenderer(const CGuideTree& tree)
    : m_Dyntree(tree.GetTree()),
      m_DataSource(new CPhyloTreeDataSource(m_Dyntree))
{
    x_Init();
}


CGuideTreeRenderer::CGuideTreeRenderer(const CBioTreeDynamic& tree)
    : m_Dyntree(tree),
      m_DataSource(new CPhyloTreeDataSource(m_Dyntree))
{
    x_Init();
}


CGuideTreeRenderer::CGuideTreeRenderer(const CBioTreeContainer& btc)
{
    x_Init();

    BioTreeConvertContainer2Dynamic(m_Dyntree, btc);
    m_DataSource.Reset(new CPhyloTreeDataSource(m_Dyntree));
}


bool CGuideTreeRenderer::WriteImage(CNcbiOstream& out)
{
    bool success = x_RenderImage();            
    if(success) {
        CImageIO::WriteImage(m_Context->GetBuffer(), out, m_ImageFormat);
    }
    return success;
}


bool CGuideTreeRenderer::WriteImage(const string& filename)
{
    bool success = x_RenderImage();
    if(success) {
        CImageIO::WriteImage(m_Context->GetBuffer(), filename, m_ImageFormat);
    }
    return success;
}

string CGuideTreeRenderer::WriteImageToNetcache(string netcacheServiceName,string netcacheClientName)
{
    string imageKey;
    bool success = x_RenderImage();            
    if(success) {
        CNetCacheAPI nc_client(netcacheServiceName,netcacheClientName);
        imageKey = nc_client.PutData(m_Context->GetBuffer().GetData(),
                                     m_Width*m_Height*3);
    }
    return imageKey;
}

void CGuideTreeRenderer::x_ExtendRoot(void)
{
    _ASSERT(!m_DataSource.Empty());

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


auto_ptr<CGuideTreeRenderer::STreeInfo> CGuideTreeRenderer::GetTreeInfo(
                                                                 int node_id)
{
    CPhyloTreeNode* node = x_GetNode(node_id);

    // find all subtree leaves
    CLeaveFinder leave_finder = TreeDepthFirstTraverse(*node, CLeaveFinder());
    vector<CPhyloTreeNode*>& leaves = leave_finder.GetLeaves();
    
    auto_ptr<STreeInfo> info(new STreeInfo);
    hash_set<string> found_blast_names;

    // for each leave get feature values
    ITERATE (vector<CPhyloTreeNode*>, it, leaves) {
        string blast_name = x_GetNodeFeature(*it, CGuideTree::eBlastNameId);
        string color = x_GetNodeFeature(*it, CGuideTree::eNodeColorId);
        string seqid = x_GetNodeFeature(*it, CGuideTree::eSeqIdId);
        string accession = x_GetNodeFeature(*it, CGuideTree::eAccessionNbrId);
        
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


void CGuideTreeRenderer::x_Init(void)
{
    m_Width = 800;
    m_Height = 600;
    m_ImageFormat = CImageIO::ePng;
    m_RenderFormat = eRect;
    m_DistanceMode = true;
    m_NodeSize = 3;
    m_LineWidth = 1;
    
    m_DataSource->Relabel("$(label)");
    x_ExtendRoot();
}


CPhyloTreeNode* CGuideTreeRenderer::x_GetNode(int id, bool throw_if_null,
                                      CPhyloTreeNode* root)
{
    CPhyloTreeNode* tree = root ? root : m_DataSource->GetTree();
    CNodeFinder node_finder = TreeDepthFirstTraverse(*tree, CNodeFinder(id));

    if (!node_finder.GetNode() && throw_if_null) {
        NCBI_THROW(CGuideTreeException, eNodeNotFound, (string)"Node "
                   + NStr::IntToString(id) + (string)" not found");
    }

    return node_finder.GetNode();
}


void CGuideTreeRenderer::PreComputeImageDimensions()
{
    m_PhyloTreeScheme.Reset(new CPhyloTreeScheme());

    m_PhyloTreeScheme->SetSize(CPhyloTreeScheme::eNodeSize) = m_NodeSize;
    m_PhyloTreeScheme->SetSize(CPhyloTreeScheme::eLineWidth) = m_LineWidth;
    m_PhyloTreeScheme->SetLabelStyle(CPhyloTreeScheme::eSimpleLabels);

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


inline
string CGuideTreeRenderer::x_GetNodeFeature(const CPhyloTreeNode* node,
                                            TBioTreeFeatureId fid)
{
    _ASSERT(node);

    const CBioTreeFeatureList& flist = node->GetValue().GetBioTreeFeatureList();
    return flist.GetFeatureValue(fid);
}


void CGuideTreeRenderer::x_CreateLayout(void)
{
    if (m_PhyloTreeScheme.Empty() || m_DataSource.Empty()) {
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

void CGuideTreeRenderer::x_Render(void)
{
   glClearColor(0.95f, 1.0f, 0.95f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   m_Renderer->Render(*m_Pane.get(), m_DataSource.GetObject()); 
}


bool CGuideTreeRenderer::x_RenderImage(void)
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


void CGuideTreeRenderer::SetSelection(int hit)
{    
    _ASSERT(hit >= 0);

    CPhyloTreeNode * node = x_GetNode(hit, false);
    if (node) {
        m_DataSource->SetSelection(node, true, true, true);
    }
}


string CGuideTreeRenderer::GetMap(string jsClickNode, string jsClickLeaf,
                                  string jsMouseover, string jsMouseout,
                                  string jsClickQuery,bool showQuery)
{
    CGIMap map_visitor(m_Pane.get(), jsClickNode, jsClickLeaf,
                       jsMouseover, jsMouseout, jsClickQuery, showQuery);

    map_visitor = TreeDepthFirstTraverse(*m_DataSource->GetTree(), map_visitor);
    return map_visitor.GetMap();
}


void CGuideTreeRenderer::CGIMap::x_FillNodeMapData(CPhyloTreeNode&  tree_node)
{           
    if (!(*tree_node).GetDictionaryPtr()) {
        NCBI_THROW(CException, eInvalid,
                   "Tree node has no feature dicionary");
    }

    int x  = m_Pane->ProjectX((*tree_node).XY().first);
    int y  = m_Pane->GetViewport().Height() - m_Pane->ProjectY((*tree_node).XY().second);           
    int ps = 5;

    string strTooltip = (*tree_node).GetLabel().size()
        ? (*tree_node).GetLabel() : "No Label";
           
    int nodeID = (*tree_node).GetId();
    bool isQuery = false;
       
    //IsLeaf() instead of IsLeafEx() allows popup menu for collapsed nodes
    if(tree_node.IsLeaf()) {
       
        if (m_ShowQuery && (*tree_node).GetDictionaryPtr()->HasFeature(
                                                  CGuideTree::eAlignIndexId)) {

            int align_index = CGuideTree::eAlignIndexId;

            isQuery = align_index == 0; //s_kPhyloTreeQuerySeqIndex 
        }       

        if(isQuery) {
            strTooltip += "(query)";
        }
        m_Map += "<area alt=\""+strTooltip+"\" title=\""+strTooltip+"\""; 

        string accessionNbr;
        if ((*tree_node).GetDictionaryPtr()->HasFeature(
                                               CGuideTree::eAccessionNbrId)) {

            accessionNbr = CGuideTree::eAccessionNbrId;
        }
        string arg = NStr::IntToString(nodeID);
        if(!accessionNbr.empty()) {
            arg += ",'" + accessionNbr + "'";
        }            
        if (isQuery && !m_JSClickQueryFunction.empty()) {
            m_Map += " href=\"" + m_JSClickQueryFunction + arg+");\""; 
        }
        else if(!isQuery && !m_JSClickLeafFunction.empty()) {
            m_Map += " href=\"" + m_JSClickLeafFunction + arg+");\"";
        }
    }
    else {           
        m_Map += "<area";
        if(!m_JSMouseoverFunction.empty())
            //"javascript:setupPopupMenu(" 
            m_Map += " onmouseover=\"" + m_JSMouseoverFunction
                + NStr::IntToString(nodeID)+ ");\"";
                    
        if(!m_JSMouseoutFunction.empty())
            //"PopUpMenu2_Hide(0);"
            m_Map += " onmouseout=\"" + m_JSMouseoutFunction + "\"";
        if(!m_JSClickNodeFunction.empty()) 
            //"//javascript:MouseHit(" //string(JS_SELECT_NODE_FUNC)
            m_Map += " href=\"" + m_JSClickNodeFunction
                + NStr::IntToString(nodeID)+");\"";
    } 
    m_Map += " coords=\"";
    m_Map += NStr::IntToString(x-ps); m_Map+=",";
    m_Map += NStr::IntToString(y-ps); m_Map+=",";
    m_Map += NStr::IntToString(x+ps); m_Map+=",";
    m_Map += NStr::IntToString(y+ps); 
    m_Map+="\">";      
}

