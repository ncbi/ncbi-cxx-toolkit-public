#ifndef GUIDE_TREE__HPP
#define GUIDE_TREE__HPP
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

///@guide_tree.hpp
///Phylogenetic tree.

#include <corelib/ncbiexpt.hpp>

#include <util/image/image_io.hpp>

#include <gui/opengl/mesa/glcgi_image.hpp>
#include <gui/opengl/mesa/gloscontext.hpp>
#include <gui/opengl/glutils.hpp>
#include <gui/opengl/glpane.hpp>

#include <algo/phy_tree/bio_tree.hpp>

#include <gui/widgets/phylo_tree/phylo_tree_ds.hpp>
#include <gui/widgets/phylo_tree/phylo_tree_render.hpp>

#include "guide_tree_calc.hpp"
#include "guide_tree_simplify.hpp"


USING_NCBI_SCOPE;

/// Wrapper around libw_phylo_tree.a classes: CPhloTreeDataSource,
/// IPhyloTreeRenderer, etc, used for manipulation of phylogenetic tree
class CGuideTree
{
public:

    typedef CGuideTreeCalc::TBlastNameColorMap TBlastNameColorMap;

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

    /// Tree simplification modes
    enum ETreeSimplifyMode {
        eNone, eFullyExpanded, eBlastName};


    /// Output formats
    enum ETreeFormat {
        eImage, eASN, eNewick, eNexus
    };

    
public:

    /// Constructor
    /// @param guide_tree_calc GuideTreeCalc object
    ///
    CGuideTree(const CGuideTreeCalc& guide_tree_calc);

    /// Constructor
    /// @param btc BioTreeContainer object
    /// @param lbl_type ELabelType
    ///
    CGuideTree(CBioTreeContainer& btc, CGuideTreeCalc::ELabelType lbl_type
               = CGuideTreeCalc::eSeqId);

    /// Constructor with initialization of tree features
    /// @param btc BioTreeContainer object [in]
    /// @param lbl_type Type of lables for tree leaves [in]
    /// @param mark_query_node Query node will be marked if true [in]
    ///
    CGuideTree(CBioTreeContainer& btc, const vector< CRef<CSeq_id> >& seqids,
               CScope& scope, CGuideTreeCalc::ELabelType lbl_type
               = CGuideTreeCalc::eSeqId, bool mark_query_node = true);


    /// Contructor
    /// @param tree Tree structure [in]
    ///
    CGuideTree(const CBioTreeDynamic& tree);


    /// Destructor
    ~CGuideTree() {}


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

    /// Set query node id
    /// @param id Query node id [in]
    ///
    /// Query node is marked by different label color
    void SetQueryNodeId(int id) {m_QueryNodeId = id;}


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

    /// Set Blast Name to color map
    /// @return Reference to Blast Name to color map
    ///
    TBlastNameColorMap& SetBlastNameColorMap(void) {return m_BlastNameColorMap;}


    // --- Getters ---

    /// Check if query node id is set
    /// @return True if query node id is set, false otherwise
    ///
    bool IsQueryNodeSet(void) {return m_QueryNodeId > -1;}

    /// Check whether tree is composed of sequences with the same Blast Name
    /// @return True if all sequences have the same Blast Name, false otherwise
    ///
    bool IsSingleBlastName(void);

    /// Get query node id
    /// @return Query node id
    ///
    int GetQueryNodeId(void) const {return m_QueryNodeId;}

    /// Get error message
    /// @return Error messsage
    ///
    string GetErrorMessage(void) const {return m_ErrorMessage;}

    /// Get current tree simplification mode
    /// @return tree simplifcation mode
    ///
    ETreeSimplifyMode GetSimplifyMode(void) const {return m_SimplifyMode;}

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

    /// Get tree root node id
    ///
    /// @return root node id
    int GetRootNodeID(void);

    /// Get serialized tree
    /// @return Biotree container
    ///
    CRef<CBioTreeContainer> GetSerialTree(void);


    // --- Generating output ---

    /// Save tree as: image, ASN object, or text format in a file.
    /// Wraper around methods below.
    /// @param format Output format [in]
    /// @param name Output file name, if empty string, output will be
    /// directed to stdout for Newic and Nexus [in]
    /// @return True on success, false on failure
    ///
    bool SaveTreeAs(CNcbiOstream& ostr, ETreeFormat format);


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

    /// Write tree structure to stream
    /// @param out Output stream [in|out]
    /// @return True on success, false on failure
    ///
    bool WriteTree(CNcbiOstream& out);


    /// Write tree structure to file
    /// @param filename Output file name [in]
    /// @return True on success, false on failure
    ///
    bool WriteTree(const string& filename = "");


    /// Write tree to netcache
    /// @param netcacheServiceName Netcache Service Name [in]
    /// @param netcacheClientName  Netcache Client Name  [in]
    /// @return string netcache key
    ///
    string WriteTreeInNetcache(string netcacheServiceName,string netcacheClientName);

    /// Write tree in Newick format to stream
    /// @param ostr Output stream [in|out]
    /// @return True on success, false on failure
    /// 
    bool PrintNewickTree(CNcbiOstream& ostr);

    /// Write tree in Newick format to file
    /// @param filename Ouput file name [in]
    /// @return True on success, false on failure
    ///
    bool PrintNewickTree(const string& filename);


    /// Write tree in Nexus format to stream
    /// @param ostr Output stream [in|out]
    /// @param tree_name Name of the tree field in Nexus output [in]
    /// @param force_win_eol If true end of lines will be always "\r\n" [in]
    /// @return True on success, false on failure
    ///
    bool PrintNexusTree(CNcbiOstream& ostr,
                        const string& tree_name = "Blast_guide_tree",
                        bool force_win_eol = false);

    /// Write tree in Nexus format to file
    /// @param filename Output file name [in]
    /// @param tree_name Name of the tree field in Nexus output [in]
    /// @param force_win_eol If true end of lines will be always "\r\n" [in]
    /// @return True on success, false on failure
    ///
    bool PrintNexusTree(const string& filename,
                        const string& tree_name = "Blast_guide_tree",
                        bool force_win_eol = false);



    // --- Tree manipulators ---
    
    /// Fully expand tree
    ///
    void FullyExpand(void);

    /// Group nodes according to user-selected scheme and collapse subtrees
    /// composed of nodes that belong to the same group
    /// @param method Name of the method for simplifying the tree [in]
    /// @param refresh Should dimensions of the tree be recalculated. [in]
    ///
    /// Typically tree needs to be refreshed after any manipulation.
    /// Setting refresh to false may be more efficient if many tree 
    /// manipulations are done. Refresh() method need to be called at the end
    /// in such case.
    ///
    void SimplifyTree(ETreeSimplifyMode method, bool refresh = true);

    /// Expand or collapse subtree rooted in given node
    /// @param node_id Numerical id of the node to expand or collapse [in]
    /// @param refresh Should dimensions of the tree be recalculated [in]
    ///
    /// Typically tree needs to be refreshed after any manipulation.
    /// Setting refresh to false may be more efficient if many tree 
    /// manipulations are done. Refresh() method need to be called at the end
    /// in such case.
    ///
    bool ExpandCollapseSubtree(int node_id, bool refresh = true);


    /// Reroot tree
    /// @param new_root_id Node id of the new root [in]
    /// @param refresh Should dimensions of the tree be recalculated [in]
    ///
    /// Typically tree needs to be refreshed after any manipulation.
    /// Setting refresh to false may be more efficient if many tree 
    /// manipulations are done. Refresh() method need to be called at the end
    /// in such case.
    ///
    void RerootTree(int new_root_id, bool refresh);

    /// Show subtree
    /// @param root_id Node id of the subtree root [in]
    /// @param refresh Should dimensions of the tree be recalculated [in]
    ///
    /// Typically tree needs to be refreshed after any manipulation.
    /// Setting refresh to false may be more efficient if many tree 
    /// manipulations are done. Refresh() method need to be called at the end
    /// in such case.
    ///
    bool ShowSubtree(int root_id, bool refresh);


    /// Recalculate dimensions of the tree for rendering.
    ///
    void Refresh(void);

    ///Get map corresponding to tree image
    ///
    ///@param  jsClickNode javascript function name for non-leaf node click
    ///@param jsClickLeaf javascript function name for leaf node click
    ///@param  jsMouseover javascript function name for node mouseover
    ///@param  jsMouseout javascript function name for node mouseout
    ///@param jsClickQuery javascript function name for query node click
    ///@param showQuery if true higlight query node
    ///@return string map
    ///
    string GetMap(string jsClickNode,string jsClickLeaf,string jsMouseover,string jsMouseout,string jsClickQuery = "",bool showQuery = true);

    ///Calculates the minimum width and height of tree image that ensures
    ///that all nodes are visible
    ///
    void PreComputeImageDimensions(void);

protected:    
    
    /// Forbiding copy constructor
    ///
    CGuideTree(CGuideTree& tree);

    /// Forbiding assignment operator
    ///
    CGuideTree& operator=(CGuideTree& tree);

    /// Init class attributes to default values
    ///
    void x_Init(void);

    /// Find pointer to a node with given numerical id. Throws excepion if node
    /// not found.
    /// @param id Numerical node id [in]
    /// @param root Root of the searched subtree, tree root if NULL
    /// @return Pointer to the node with desired id or NULL of node not found
    ///
    CPhyloTreeNode* x_GetNode(int id, CPhyloTreeNode* root = NULL);

    inline TBioTreeFeatureId x_GetFeatureId(const string& tag);

    /// Get feature value by tag of selected node
    /// @param node Node [in]
    /// @param feature_tag Feature tag from tree feature dictionary [in]
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


    /// Generates tree in Newick format, recursive
    /// @param ostr Output stream [in|out]
    /// @param node Tree root [in]
    /// @param is_outer_node Controls recursion should be true on first call [in]
    ///
    void x_PrintNewickTree(CNcbiOstream& ostr, 
                           const CTreeNode<CPhyTreeNode>& node, 
                           bool is_outer_node = true);
        

    /// Mark query node after collapsing subtree. Checks if query node is in
    /// the collapsed subtree and marks the collapse node.
    /// @param node Root of the collapsed subtree [in]
    ///
    void x_MarkCollapsedQueryNode(CPhyloTreeNode* node);

    /// Collapse given subtrees
    /// @param groupper Object groupping nodes that contains a list of subtrees
    ///
    /// Sets node attributes: m_Label, m_ShowChilds, and node color for all
    /// nodes provided by the input paramater. Used by x_SimplifyTree.
    ///
    void x_CollapseSubtrees(CPhyloTreeNodeGroupper& groupper);


    void x_InitTreeLabels(CBioTreeContainer &btc,CGuideTreeCalc::ELabelType lblType); 

private:
    
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


    /// Tree visitor class, expands all nodes and corrects node colors
    class CExpander
    {
    public:
        /// Expand subtree. Function invoked on each node by traversal function.
        /// @param node Tree root [in]
        /// @param delta Direction of tree traversal [in]
        /// @return Traverse action
        ETreeTraverseCode operator()(CPhyloTreeNode& node, int delta) 
        {
            if (delta == 0 || delta == 1) {
                if (!node.Expanded() && !node.IsLeaf()) {
                    node.ExpandCollapse(IPhyGraphicsNode::eShowChilds);
                    (*node).SetFeature(CGuideTreeCalc::kNodeColorTag, "");
                }
            }
            return eTreeTraverse;
        }
    };


    // Tree visitor for examining whether a phylogenetic tree contains sequences
    // with only one Blast Name
    class CSingleBlastNameExaminer
    {
    public:

        /// Constructor
        CSingleBlastNameExaminer(CPhyloTreeDataSource& tree)
            : m_IsSingleBlastName(true) 
        {
            const CBioTreeFeatureDictionary& fdict
                = tree.GetDictionary();

            if (!fdict.HasFeature(CGuideTreeCalc::kBlastNameTag)) {
                NCBI_THROW(CException, eInvalid, 
                           "No Blast Name feature CBioTreeFeatureDictionary");
            }
            else {
                m_BlastNameFeatureId = fdict.GetId(
                                              CGuideTreeCalc::kBlastNameTag);
            }
        }

        /// Check if all sequences in examined tree have the same Blast Name
        ///
        /// Meaningless if invoked before tree traversing
        /// @return True if all sequences have common blast name, false otherwise
        bool IsSingleBlastName(void) const {return m_IsSingleBlastName;}

        /// Expamine node. Function invoked on each node by traversal function.
        /// @param node Tree root [in]
        /// @param delta Direction of tree traversal [in]
        /// @return Traverse action
        ETreeTraverseCode operator()(CPhyloTreeNode& node, int delta) 
        {
            if (delta == 0 || delta == 1) {
                if (node.IsLeaf()) {

                    const CBioTreeFeatureList& flist 
                        = node.GetValue().GetBioTreeFeatureList();

                    if (m_CurrentBlastName.empty()) {
                        m_CurrentBlastName 
                            = flist.GetFeatureValue(m_BlastNameFeatureId);
                    }
                    else {
                        if (m_CurrentBlastName 
                            != flist.GetFeatureValue(m_BlastNameFeatureId)) {
                            m_IsSingleBlastName = false;
                            return eTreeTraverseStop;
                        }
                    }
                }
            }
            return eTreeTraverse;
        }
    
    protected:
        /// Is one blast name in the tree
        bool m_IsSingleBlastName;              

        /// Id of feature that holds blast name
        TBioTreeFeatureId m_BlastNameFeatureId;

        /// Last identified blast name
        string m_CurrentBlastName;
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


protected:

    /// Contains tree structure
    CRef<CPhyloTreeDataSource> m_DataSource; 

    /// Stores tree data
    CBioTreeDynamic m_Dyntree;

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

    /// Id of query node
    int m_QueryNodeId;

    /// GL context
    CRef<CGlOsContext> m_Context;

    /// GL pane
    auto_ptr<CGlPane> m_Pane;

    /// Phylogenetic tree renderer
    auto_ptr<IPhyloTreeRenderer> m_Renderer;

    /// Error message
    string m_ErrorMessage;

    /// Current tree simplification mode
    ///
    ETreeSimplifyMode m_SimplifyMode;


    /// Min image dimensions
    ///
    TVPRect m_MinDimRect;    


    /// Blast Name to color map
    TBlastNameColorMap m_BlastNameColorMap;

    ///Phylo Tree Scheme
    ///
    CRef<CPhyloTreeScheme> m_PhyloTreeScheme;
};


/// Guide tree exceptions
class CGuideTreeException : public CException
{
public:

    /// Error code
    enum EErrCode {
        eInvalid,
        eInvalidOptions,    ///< Invalid parameter values
        eNodeNotFound,      ///< Node with desired id not found
        eTraverseProblem    ///< Problem in one of the tree visitor classes
    };

    NCBI_EXCEPTION_DEFAULT(CGuideTreeException, CException);
};



///Class for creating image map
///
class CGuideTreeCGIMap
{
public:
    
    CGuideTreeCGIMap(CGlPane * x_pane, 
                     string jsClickNode,
                     string jsClickLeaf,                     
                     string jsMouseover,
                     string jsMouseout,
                     string jsClickQuery,
                     bool showQuery) : m_Pane(x_pane), m_Map(""){
        m_JSClickNodeFunction = jsClickNode;
        m_JSClickLeafFunction = jsClickLeaf;
        m_JSMouseoverFunction = jsMouseover;
        m_JSMouseoutFunction = jsMouseout;
        m_JSClickQueryFunction = jsClickQuery;
        m_ShowQuery = showQuery;
    }
     

    ///Creates concatinated html <area...> string for tree image
    ///
    ///Gets coordinates of the tree nodes from m_DataSource and creats html <area...>
    ///for each  node with javascript for actions on click and mouseover
    ///
    ///@return 
    /// A string concatinating all "areas" corresponding to the image    
        const string & GetMap(void) { return m_Map;}

            
    ETreeTraverseCode operator()(CPhyloTreeNode &  tree_node, int delta)
    {
            if (delta==1 || delta==0){
                                x_FillNodeMapData(tree_node);                                
            }                
            return eTreeTraverse;
    }
        virtual ~CGuideTreeCGIMap(){}
protected:
        virtual void x_FillNodeMapData(CPhyloTreeNode &  tree_node);

        CGlPane * m_Pane;
    string  m_JSMouseoverFunction; //"javascript:setupPopupMenu(" 
    string  m_JSMouseoutFunction; //"PopUpMenu2_Hide(0);"
    string  m_JSClickNodeFunction;// - string(JS_SELECT_NODE_FUNC) - both
    string  m_JSClickLeafFunction;// - string(JS_SELECT_NODE_FUNC) - blast ,"" - multi
    string  m_JSClickQueryFunction;// - "" blast, "" multi
    bool    m_ShowQuery;

        string    m_Map;      
};


#endif // GUIDE_TREE__HPP
