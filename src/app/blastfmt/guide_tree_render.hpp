#ifndef GUIDE_TREE_RENDER__HPP
#define GUIDE_TREE_RENDER__HPP
/*  $Id: guide_tree.hpp$
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
 * 
 */

#include <corelib/ncbiexpt.hpp>

#include <util/image/image_io.hpp>

#include <gui/opengl/mesa/glcgi_image.hpp>
#include <gui/opengl/mesa/gloscontext.hpp>
#include <gui/opengl/glutils.hpp>
#include <gui/opengl/glpane.hpp>

#include <algo/phy_tree/bio_tree.hpp>

#include <gui/widgets/phylo_tree/phylo_tree_ds.hpp>
#include <gui/widgets/phylo_tree/phylo_tree_render.hpp>

#include "guide_tree.hpp"



USING_NCBI_SCOPE;

/// Wrapper class for rendering phylogenetic tree.
/// Wrapper around libw_phylo_tree classes: CPhyloTreeDataSource,
/// CPhyloTreeScheme, IPhyloTreeRenderer, etc.
class CGuideTreeRenderer
{
public:

    /// Type for BlastName to color map
    typedef pair<string, string> TBlastNameToColor;
    typedef vector<TBlastNameToColor> TBlastNameColorMap;

    /// Information about tree leaves
    typedef struct STreeInfo {
        TBlastNameColorMap blastname_color_map;
        vector<string> seq_ids;
        vector<string> accessions;        
    } STreeInfo;

    /// Tree rendering formats
    enum ETreeRenderer {
        eRect, eSlanted, eRadial, eForce
    }; 
    

public:

    /// Constructor
    /// @param tree Tree [in]
    ///
    CGuideTreeRenderer(const CGuideTree& tree);

    /// Constructor
    /// @param tree Tree [in]
    ///
    CGuideTreeRenderer(const CBioTreeDynamic& tree);

    /// Constructor
    /// @param btc Tree container [in]
    ///
    CGuideTreeRenderer(const CBioTreeContainer& btc);


    /// Destructor
    ~CGuideTreeRenderer() {}


    // --- Setters ---

    /// Set rendering format
    /// @param format Rendering format [in]
    ///
    void SetRenderFormat(ETreeRenderer format) {m_RenderFormat = format;}

    /// Set distance mode
    /// @param mode Distance mode, egde lengths will not be shown if false [in]
    ///
    void SetDistanceMode(bool mode) {m_DistanceMode = mode;}

    /// Set tree node size
    /// @param size Node size [in]
    ///
    void SetNodeSize(int size) {m_NodeSize = size;}

    /// Set tree edge with
    /// @param width Edge with [in]
    ///
    void SetLineWidth(int width) {m_LineWidth = width;}

    /// Set width of generated image
    /// @param width Image width [in]
    ///
    void SetImageWidth(int width) {m_Width = width;}

    /// Set height of generated image
    /// @param height Image height [in]
    ///
    void SetImageHeight(int height) {m_Height = height;}

    /// Set format of generated image
    /// @param format Image format [in]
    ///
    void SetImageFormat(CImageIO::EType format) {m_ImageFormat = format;}

    /// Initialization related to the selected branch of the tree
    ///
    /// @param hit selected node id [in]
    ///
    void SetSelection(int hit);


    // --- Getters ---

    ///Get the minimum image height which should be acceptable to output 
    ///
    /// @return min height
    int GetMinHeight(void) const {return m_MinDimRect.Height();}

    /// Get information about leaves (such as seqids, blast name to color map,
    /// usually used for auxilary information on the web) for selected subtree
    /// @param node_id Node id of subtree root [in]
    /// @return Tree information
    ///
    auto_ptr<STreeInfo> GetTreeInfo(int node_id);

    /// Get error message
    /// @return Error message
    ///
    string GetErrorMessage(void) const {return m_ErrorMessage;}


    // --- Generating output ---

    /// Write image to stream
    /// @param out Output stream [in|out]
    /// @return True on success, false on failure
    ///
    bool WriteImage(CNcbiOstream& out);

    /// Write image to file
    /// @param filename Output file name [in]
    /// @return True on success, false on failure
    ///
    bool WriteImage(const string& filename = "");

    /// Write image to netcache
    /// @param netcacheServiceName Netcache Service Name [in]
    /// @param netcacheClientName  Netcache Client Name  [in]
    /// @return string netcache key
    ///
    string WriteImageToNetcache(string netcacheServiceName,string  netcacheClientName);

    /// Write tree to netcache
    /// @param netcacheServiceName Netcache Service Name [in]
    /// @param netcacheClientName  Netcache Client Name  [in]
    /// @return string netcache key
    ///
    string WriteTreeInNetcache(string netcacheServiceName,string netcacheClientName);

    ///Get mapping of tree nodes to image coordinates
    ///
    ///@param jsClickNode javascript function name for non-leaf node click [in]
    ///@param jsClickLeaf javascript function name for leaf node click [in]
    ///@param jsMouseover javascript function name for node mouseover [in]
    ///@param jsMouseout javascript function name for node mouseout [in]
    ///@param jsClickQuery javascript function name for query node click [in]
    ///@param showQuery if true higlight query node [in]
    ///@return HTML map field
    ///
    string GetMap(string jsClickNode, string jsClickLeaf, string jsMouseover,
                  string jsMouseout, string jsClickQuery = "",
                  bool showQuery = true);

    ///Calculates the minimum width and height of tree image that ensures
    ///that all nodes are visible
    ///
    void PreComputeImageDimensions(void);


protected:    
    
    /// Forbiding copy constructor
    ///
    CGuideTreeRenderer(CGuideTreeRenderer& tree);

    /// Forbiding assignment operator
    ///
    CGuideTreeRenderer& operator=(CGuideTreeRenderer& tree);

    /// Find pointer to a node with given numerical id. Throws excepion if node
    /// not found.
    /// @param id Numerical node id [in]
    /// @param throw_if_null If true, throw exception if node not found [in]
    /// @param root Root of the searched subtree, tree root if NULL
    /// @return Pointer to the node with desired id or NULL of node not found
    ///
    CPhyloTreeNode* x_GetNode(int id, bool throw_if_null = true,
                              CPhyloTreeNode* root = NULL);

    /// Get feature value for given node
    /// @param node Node [in]
    /// @param fid Feature id [in]
    /// @return Feature value
    ///
    inline static string x_GetNodeFeature(const CPhyloTreeNode* node,
                                   TBioTreeFeatureId fid);

    /// Create layout for image rendering
    ///
    void x_CreateLayout(void);

    /// Render image
    ///
    void x_Render(void);

    /// Create image
    /// @return True on success, false on failure
    ///
    bool x_RenderImage(void);


private:
    
    /// Add very short length to edges descending directly from root in
    /// in rendered tree.
    ///
    /// The purpose of this function is to avoid zero length edges at root,
    /// because then the rendered tree is difficult to analyze.
    void x_ExtendRoot(void);

    /// Initialize class attributes to default values
    ///
    void x_Init(void);


    // Tree visitor classes used for manipulating the guide tree

    /// Tree visitor, finds tree node by id
    class CNodeFinder
    {
    public:

        /// Constructor
        /// @param node_id Id of node to be found [in]
        CNodeFinder(IPhyNode::TID node_id) : m_NodeId(node_id), m_Node(NULL) {}

        /// Get pointer to found node
        /// @return Pointer to node or NULL if node not found
        CPhyloTreeNode* GetNode(void) const {return m_Node;}

        /// Check node id. Function invoked on each node by traversal function.
        /// @param node Tree root [in]
        /// @param delta Direction of tree traversal [in]
        /// @return Traverse action
        ETreeTraverseCode operator()(CPhyloTreeNode& node, int delta)
        {
            if (delta == 0 || delta == 1) {
                if ((*node).GetId() == m_NodeId) {
                    m_Node = &node;
                    return eTreeTraverseStop;
                } 
            }
            return eTreeTraverse;
        }

    protected:
        IPhyNode::TID m_NodeId;  ///< Id of searched node
        CPhyloTreeNode* m_Node;  ///< Pointer to node with desired id
    };


    /// Tree visitor, finds pointers to all leaves in a subtree
    class CLeaveFinder
    {
    public:
        /// Get list of pointers to leave nodes
        /// @return List of pointers to leave nodes
        vector<CPhyloTreeNode*>& GetLeaves(void) {return m_Leaves;}

        /// Examine node, find leave nodes and save pointers to them
        /// @param node Tree node [in]
        /// @param delta Direction of traversal [in]
        ETreeTraverseCode operator()(CPhyloTreeNode& node, int delta)
        {
            if (delta == 0 || delta == 1) {
                if (node.IsLeaf()) {
                    m_Leaves.push_back(&node);
                }
            }
            return eTreeTraverse;
        }


    private:
        /// List of pointers to leave nodes
        vector<CPhyloTreeNode*> m_Leaves;
    };


    ///Class for creating image map
    class CGIMap
    {
    public:
    
        CGIMap(CGlPane * x_pane, 
               string jsClickNode,
               string jsClickLeaf,                     
               string jsMouseover,
               string jsMouseout,
               string jsClickQuery,
               bool showQuery) : m_Pane(x_pane), m_Map("")  {
            m_JSClickNodeFunction = jsClickNode;
            m_JSClickLeafFunction = jsClickLeaf;
            m_JSMouseoverFunction = jsMouseover;
            m_JSMouseoutFunction = jsMouseout;
            m_JSClickQueryFunction = jsClickQuery;
            m_ShowQuery = showQuery;
        }
     

        ///Create concatinated html <area...> string for tree image
        ///
        ///Get coordinates of the tree nodes from m_DataSource and create html
        /// <area...> for each  node with javascript for actions on click and
        /// mouseover
        ///
        ///@return 
        /// A string concatinating all "areas" corresponding to the image    
        const string & GetMap(void) { return m_Map;}
            
        /// Get mapping for tree node to rendered image coordinates.
        /// Function invoked by tree traversal
        /// @param node Node [in]
        /// @param delta Traversal direction [in]
        ETreeTraverseCode operator()(CPhyloTreeNode &  tree_node, int delta)
        {
            if (delta==1 || delta==0){
                x_FillNodeMapData(tree_node);
            }                
            return eTreeTraverse;
        }

    protected:

        /// Generate mapping for tree node to rendered image coordinates
        /// @param tree_node Node [in]
        void x_FillNodeMapData(CPhyloTreeNode&  tree_node);
        
        /// Gl pane
        CGlPane * m_Pane;

        /// Java Script function name
        string  m_JSMouseoverFunction;
        
        /// Java Script function name
        string  m_JSMouseoutFunction;

        /// Java Script function name
        string  m_JSClickNodeFunction;

        /// Java Script function name
        string  m_JSClickLeafFunction;

        /// Java Script function name
        string  m_JSClickQueryFunction;

        /// Highlight query node
        bool m_ShowQuery;

        /// Map string
        string m_Map;
    };


protected:

    /// Contains tree structure and attributes
    CBioTreeDynamic m_Dyntree;

    /// Contains tree structure and attributes for rendering
    CRef<CPhyloTreeDataSource> m_DataSource;

    ///Phylo Tree Scheme
    CRef<CPhyloTreeScheme> m_PhyloTreeScheme;

    /// Height of output image
    int m_Height;

    /// Width of outputs image
    int m_Width;

    /// Format of output image
    CImageIO::EType m_ImageFormat;

    /// Tree rendering format
    ETreeRenderer m_RenderFormat;

    /// Should edge lengths be shown in rendered tree
    bool m_DistanceMode;

    /// Tree node size
    int m_NodeSize;

    /// Line width for tree edges
    int m_LineWidth;

    /// GL context
    CRef<CGlOsContext> m_Context;

    /// GL pane
    auto_ptr<CGlPane> m_Pane;

    /// Phylogenetic tree renderer
    auto_ptr<IPhyloTreeRenderer> m_Renderer;

    /// Min image dimensions
    TVPRect m_MinDimRect;

    /// Error message
    string m_ErrorMessage;
};


#endif // GUIDE_TREE_RENDER__HPP
