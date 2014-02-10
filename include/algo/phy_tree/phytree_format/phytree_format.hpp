#ifndef PHYTREE_FORMAT__HPP
#define PHYTREE_FORMAT__HPP
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
#include <algo/phy_tree/bio_tree.hpp>
#include <algo/phy_tree/phytree_calc.hpp>


BEGIN_NCBI_SCOPE

// forward declaration
class CPhyTreeNodeGroupper;


/// Class for adding tree features, maniplating and printing tree in standard
/// text formats
class NCBI_XALGOPHYTREE_EXPORT CPhyTreeFormatter : public CObject
{
public:

    /// Type for BlastName to color map
    typedef pair<string, string> TBlastNameToColor;
    typedef vector<TBlastNameToColor> TBlastNameColorMap;

    /// Tree simplification modes
    enum ETreeSimplifyMode {
        eNone,             ///< No simplification mode
        eFullyExpanded,    ///< Tree fully expanded
        eByBlastName       ///< Subtrees that contain sequences with the
                           ///< the same Blast Name are collapsed
    };

    /// Output formats
    enum ETreeFormat {
        eASN, eNewick, eNexus
    };

    /// Information shown as labels in the guide tree
    ///
    enum ELabelType {
        eTaxName, eSeqTitle, eBlastName, eSeqId, eSeqIdAndBlastName, eTaxNameAndAccession
    };

    /// Feature IDs used in guide tree
    ///
    enum EFeatureID {
        eLabelId = 0,       ///< Node label
        eDistId,            ///< Edge length from parent to this node
        eSeqIdId,           ///< Sequence id
        eOrganismId,        ///< Taxonomic organism id (for sequence)
        eTitleId,           ///< Sequence title
        eAccessionNbrId,    ///< Sequence accession
        eBlastNameId,       ///< Sequence Blast Name
        eAlignIndexId,      ///< Index of sequence in Seq_align
        eNodeColorId,       ///< Node color
        eLabelColorId,      ///< Node label color
        eLabelBgColorId,    ///< Color for backgroud of node label
        eLabelTagColorId,
        eTreeSimplificationTagId, ///< Is subtree collapsed
        eNodeInfoId,     ///< Used for denoting query nodes
        eLastId = eNodeInfoId ///< Last Id (with largest index)
    };

    
public:

    /// Constructor
    /// @param guide_tree_calc GuideTreeCalc object [in]
    /// @param lbl_type Type of labels to be used for tree leaves [in]
    /// @param mark_query_node If true, query node will be marked with
    /// different color (query node is the node correspondig to query 
    /// sequence used in guide_tree_calc) [in]
    ///
    CPhyTreeFormatter(CPhyTreeCalc& guide_tree_calc,
                      ELabelType lbl_type = eSeqId,
                      bool mark_query_node = true);

    /// Constructor
    /// @param guide_tree_calc GuideTreeCalc object [in]
    /// @param mark_leaves Indeces of sequences to marked in the tree [in]
    /// @param lbl_type Type of labels to be used for tree leaves [in]
    ///
    CPhyTreeFormatter(CPhyTreeCalc& guide_tree_calc,
                      const vector<int>& mark_leaves,
                      ELabelType lbl_type = eSeqId);

    /// Constructor
    /// @param guide_tree_calc GuideTreeCalc object with computed tree [in]
    /// @param seq_ids Sequence ids to be marked in the tree
    /// (can be gis, accessions or local ids, in the form gi|129295) [in]
    /// @param lbl_type Type of labels to be used for tree leaves [in]
    ///
    CPhyTreeFormatter(CPhyTreeCalc& guide_tree_calc,
                      vector<string>& seq_ids,
                      ELabelType lbl_type = eSeqId);

    /// Constructor
    /// @param btc BioTreeContainer object [in]
    /// @param lbl_type ELabelType [in]
    /// @param query_node_id Id of query node [in]
    ///
    /// Query node will have color features set so that it will be marked
    /// when tree is rendered. Query node id of -1 denotes that none of tree
    /// nodes is the query node.
    CPhyTreeFormatter(CBioTreeContainer& btc, ELabelType lbl_type = eSeqId);

    /// Constructor with initialization of tree features
    /// @param btc BioTreeContainer object [in]
    /// @param seqids Seq-ids for sequences represented in the tree [in]
    /// @param scope Scope [in]
    /// @param lbl_type Type of lables for tree leaves [in]
    /// @param mark_query_node Query node will be marked if true [in]
    ///
    /// btc must be tree with the same number of leaves as number of elements
    /// in seqids. Tree leaf labels must be numbers from 0 to number of leaves
    /// minus 1. Node with label '0' is considered query node.
    CPhyTreeFormatter(CBioTreeContainer& btc,
                      const vector< CRef<CSeq_id> >& seqids,
                      CScope& scope, ELabelType lbl_type = eSeqId, 
                      bool mark_query_node = true);


    /// Contructor
    /// @param tree Tree structure [in]
    ///
    CPhyTreeFormatter(const CBioTreeDynamic& tree);


    /// Destructor
    ~CPhyTreeFormatter() {}


    // --- Setters ---

    /// Set Blast Name to color map
    /// @return Reference to Blast Name to color map
    ///
    TBlastNameColorMap& SetBlastNameColorMap(void) {return m_BlastNameColorMap;}


    // --- Getters ---

    /// Check whether tree is composed of sequences with the same Blast Name
    /// @return True if all sequences have the same Blast Name, false otherwise
    ///
    bool IsSingleBlastName(void);

    /// Get current tree simplification mode
    /// @return tree simplifcation mode
    ///
    ETreeSimplifyMode GetSimplifyMode(void) const {return m_SimplifyMode;}

    /// Get tree root node id
    /// @return root node id
    ///
    int GetRootNodeID(void);

    /// Get pointer to the node with given id
    /// @param id Node's numerical id [in]
    /// @return Pointer to the node or NULL if node not found
    ///
    CBioTreeDynamic::CBioNode* GetNode(TBioTreeNodeId id);

    /// Get pointer to the node with given id and throw exception if node not
    /// found
    /// @param id Node's numerical id [in]
    /// @return Pointer to the node
    CBioTreeDynamic::CBioNode* GetNonNullNode(TBioTreeNodeId id);

    /// Get tree structure
    /// @return Tree
    ///
    const CBioTreeDynamic& GetTree(void) const {return m_Dyntree;}

    /// Get serialized tree
    /// @return Biotree container
    ///
    CRef<CBioTreeContainer> GetSerialTree(void);

    /// Get tree feature tag
    /// @param feat Feature id [in]
    /// @return Feature tag
    ///
    static string GetFeatureTag(EFeatureID feat)
    {
        switch (feat) {

        case eLabelId:  return "label";
        case eDistId :  return "dist";
        case eSeqIdId:  return "seq-id";
        case eTitleId:  return "seq-title";
        case eOrganismId    : return "organism";
        case eAccessionNbrId: return "accession-nbr";        
        case eBlastNameId   : return "blast-name";    
        case eAlignIndexId  : return "align-index";     

        case eNodeColorId    : return "$NODE_COLOR";
        case eLabelColorId   : return "$LABEL_COLOR";
        case eLabelBgColorId : return "$LABEL_BG_COLOR";
        case eLabelTagColorId: return "$LABEL_TAG_COLOR";
        case eTreeSimplificationTagId : return "$NODE_COLLAPSED";
        case eNodeInfoId : return "node-info";
        default: return "";
        }
    }


    // --- Generating output ---

    /// Write tree structure to stream in selected format
    /// @param out Output stream [in|out]
    /// @param format Format for writing tree [in]
    /// @return True on success, false on failure
    ///
    bool WriteTreeAs(CNcbiOstream& out, ETreeFormat format);


    /// Write tree structure to stream
    /// @param out Output stream [in|out]
    /// @return True on success, false on failure
    ///
    bool WriteTree(CNcbiOstream& out);


    /// Write tree in Newick format to stream
    /// @param ostr Output stream [in|out]
    /// @return True on success, false on failure
    /// 
    bool PrintNewickTree(CNcbiOstream& ostr);


    /// Write tree in Nexus format to stream
    /// @param ostr Output stream [in|out]
    /// @param tree_name Name of the tree field in Nexus output [in]
    /// @return True on success, false on failure
    ///
    bool PrintNexusTree(CNcbiOstream& ostr,
                        const string& tree_name = "Blast_guide_tree");



    // --- Tree manipulators ---
    
    /// Fully expand tree
    ///
    void FullyExpand(void);

    /// Group nodes according to user-selected scheme and collapse subtrees
    /// composed of nodes that belong to the same group
    /// @param method Name of the method for simplifying the tree [in]
    ///
    void SimplifyTree(ETreeSimplifyMode method);

    /// Expand or collapse subtree rooted in given node
    /// @param node_id Numerical id of the node to expand or collapse [in]
    ///
    bool ExpandCollapseSubtree(int node_id);


    /// Reroot tree
    /// @param new_root_id Node id of the new root [in]
    ///
    void RerootTree(int new_root_id);

    /// Show subtree
    /// @param root_id Node id of the subtree root [in]
    ///
    bool ShowSubtree(int root_id);


protected:    
    
    /// Forbiding copy constructor
    ///
    CPhyTreeFormatter(CPhyTreeFormatter& tree);

    /// Forbiding assignment operator
    ///
    CPhyTreeFormatter& operator=(CPhyTreeFormatter& tree);

    /// Init class attributes to default values
    ///
    void x_Init(void);

    /// Find pointer to a BioTreeDynamic node with given numerical id.
    /// Throws excepion if node not found.
    /// @param id Numerical node id [in]
    /// @param throw_if_null Throw exception if node not found [in]
    /// @return Pointer to the node with desired id
    ///
    CBioTreeDynamic::CBioNode* x_GetBioNode(TBioTreeNodeId id,
                                            bool throw_if_null = true);

    /// Check if node is expanded (subtree shown)
    /// @param node Node [in]
    /// @return True if node expanded, false otherwise
    ///
    static bool x_IsExpanded(const CBioTreeDynamic::CBioNode& node);

    /// Check if node is a leaf or collapsed
    /// @param node Node [in]
    /// @return True if node is a leaf or collapsed, false otherwise
    ///
    static bool x_IsLeafEx(const CBioTreeDynamic::CBioNode& node);

    /// Collapse node (do not show subtree)
    /// @param node Node [in|out]
    ///
    static void x_Collapse(CBioTreeDynamic::CBioNode& node);

    /// Expand node (show subtree)
    /// @param node Node [in|out]
    ///
    static void x_Expand(CBioTreeDynamic::CBioNode& node);

    /// Generates tree in Newick format, recursive
    /// @param ostr Output stream [in|out]
    /// @param node Tree root [in]
    /// @param is_outer_node Controls recursion should be true on first call [in]
    ///
    void x_PrintNewickTree(CNcbiOstream& ostr, 
                           const CBioTreeDynamic::CBioNode& node, 
                           vector<string>& labels,
                           bool name_subtrees = true,
                           bool is_outer_node = true);
        

    /// Mark node. The function sets node feature that colors the node label
    /// background.
    /// @param node Node to mark
    ///
    void x_MarkNode(CBioTreeDynamic::CBioNode* node);

    /// Collapse given subtrees
    /// @param groupper Object groupping nodes that contains a list of subtrees
    ///
    /// Sets node attributes: m_Label, m_ShowChilds, and node color for all
    /// nodes provided by the input paramater. Used by x_SimplifyTree.
    ///
    void x_CollapseSubtrees(CPhyTreeNodeGroupper& groupper);


    /// Init tree leaf labels with selected labels type
    /// @param btc BioTreeContainer [in|out]
    /// @param lbl_type Labels type [in]
    ///
    void x_InitTreeLabels(CBioTreeContainer& btc, ELabelType lbl_type); 

    /// Mark leave nodes corresponding to sequences with given sequence ids
    /// @param btc BioTreeContainer [in|out]
    /// @param ids List of sequence ids (gis or accessions, ex. gi|129295) [in]
    /// @param scope Scope
    ///
    void x_MarkLeavesBySeqId(CBioTreeContainer& btc, vector<string>& ids,
                             CScope& scope);

private:
    

    /// Create and initialize tree features. Initializes node labels,
    /// descriptions, colors, etc.
    /// @param btc Tree for which features are to be initialized [in|out]
    /// @param seqids Sequence ids each corresponding to a tree leaf [in]
    /// @param scope Scope [in]
    /// @param label_type Type of labels to for tree leaves [in]
    /// @param mark_leaves Indeces of sequences to be marked in the tree [in]
    /// @param bcolormap Blast name to node color map [out]
    ///
    /// Tree leaves must have labels as numbers from zero to number of leaves
    /// minus 1. This function does not initialize distance feature.
    static void x_InitTreeFeatures(CBioTreeContainer& btc,
                                 const vector< CRef<CSeq_id> >& seqids,
                                 CScope& scope,
                                 ELabelType label_type,
                                 const vector<int>& mark_leaves,
                                 TBlastNameColorMap& bcolormap);

    /// Add feature descriptor to tree
    /// @param id Feature id [in]
    /// @param desc Feature description [in]
    /// @param btc Tree [in|out]
    static void x_AddFeatureDesc(int id, const string& desc,
                                 CBioTreeContainer& btc); 

    /// Add feature to tree node
    /// @param id Feature id [in]
    /// @param value Feature value [in]
    /// @param iter Tree node iterator [in|out]
    static void x_AddFeature(int id, const string& value,
                             CNodeSet::Tdata::iterator iter);    

    /// Add feature to tree node
    /// @param id Feature id [in]
    /// @param value Feature value [in]
    /// @param node Pointer to the node [in|out]
    static void x_AddFeature(int id, const string& value, CNode* node);


    // Tree visitor classes used for manipulating the guide tree

    /// Tree visitor, finds BioTreeDynamic node by id
    class CBioNodeFinder
    {
    public:

        /// Constructor
        /// @param id Node id [in]
        CBioNodeFinder(TBioTreeNodeId id) : m_NodeId(id), m_Node(NULL) {}

        /// Get pointer to found node
        /// @return Pointer to node or NULL
        CBioTreeDynamic::CBioNode* GetNode(void) {return m_Node;}

        /// Check if node has desired id. Function invoked by tree traversal
        /// function.
        /// @param node Node [in]
        /// @param delta Traversal direction [in]
        ETreeTraverseCode operator()(CBioTreeDynamic::CBioNode& node, int delta)
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
        TBioTreeNodeId m_NodeId;
        CBioTreeDynamic::CBioNode* m_Node;
    };


    /// Tree visitor class, expands all nodes and corrects node colors
    class CExpander
    {
    public:
        /// Expand subtree. Function invoked on each node by traversal function.
        /// @param node Tree root [in]
        /// @param delta Direction of tree traversal [in]
        /// @return Traverse action
        ETreeTraverseCode operator()(CBioTreeDynamic::CBioNode& node,
                                     int delta);
    };


    // Tree visitor for examining whether a phylogenetic tree contains sequences
    // with only one Blast Name
    class CSingleBlastNameExaminer
    {
    public:

        /// Constructor
        CSingleBlastNameExaminer(CBioTreeDynamic& tree)
            : m_IsSingleBlastName(true) 
        {
            const CBioTreeFeatureDictionary& fdict
                = tree.GetFeatureDict();

            if (!fdict.HasFeature(eBlastNameId)) {
                NCBI_THROW(CException, eInvalid, 
                           "No Blast Name feature CBioTreeFeatureDictionary");
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
        ETreeTraverseCode operator()(CBioTreeDynamic::CBioNode& node,
                                     int delta) 
        {
            if (delta == 0 || delta == 1) {
                if (node.IsLeaf()) {


                    if (m_CurrentBlastName.empty()) {
                        m_CurrentBlastName = node.GetFeature(
                               CPhyTreeFormatter::GetFeatureTag(eBlastNameId));
                    }
                    else {
                        if (m_CurrentBlastName != node.GetFeature(
                             CPhyTreeFormatter::GetFeatureTag(eBlastNameId))) {
                          
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

        /// Last identified blast name
        string m_CurrentBlastName;
    };


    /// Tree visitor for checking whether a subtree contains a query node.
    /// A query node has feature node-info set to "query".
    class CQueryNodeChecker {
    public:

        /// Constructor
        CQueryNodeChecker(CBioTreeDynamic& tree)
            : m_HasQueryNode(false) 
        {
            const CBioTreeFeatureDictionary& fdict
                = tree.GetFeatureDict();

            if (!fdict.HasFeature(eNodeInfoId)) {
                NCBI_THROW(CException, eInvalid, 
                           "No NodeInfo feature in CBioTreeFeatureDictionary");
            }
        }

        /// Check if an examined subtree has a query node
        ///
        /// Meaningless if invoked before tree traversing
        /// @return True if examined subtree contains a query node,
        /// false otherwise
        bool HasQueryNode(void) const {return m_HasQueryNode;}

        /// Expamine node: check if query node. Function invoked on each
        /// node by traversal function.
        /// @param node Tree root [in]
        /// @param delta Direction of tree traversal [in]
        /// @return Traverse action
        ETreeTraverseCode operator()(CBioTreeDynamic::CBioNode& node,
                                     int delta) 
        {
            if (delta == 0 || delta == 1) {
                if (node.IsLeaf()) {

                    if (node.GetFeature(GetFeatureTag(eNodeInfoId))
                        == kNodeInfoQuery) {

                        m_HasQueryNode = true;
                        return eTreeTraverseStop;
                    }
                }
            }
            return eTreeTraverse;
        }

    private:
        bool m_HasQueryNode;
    };


    /// Compare pairs (node, sequence id handle) by sequence id
    class compare_nodes_by_seqid {
    public:
        bool operator()(const pair<CNode*, CSeq_id_Handle>& a,
                        const pair<CNode*, CSeq_id_Handle>& b)
        {
            return a.second.AsString() <  b.second.AsString();
        }
    };


protected:

    /// Stores tree data
    CBioTreeDynamic m_Dyntree;

    /// Current tree simplification mode
    ETreeSimplifyMode m_SimplifyMode;

    /// Blast Name to color map
    TBlastNameColorMap m_BlastNameColorMap;

public:
    /// Node feature "node-info" value for query nodes
    static const string kNodeInfoQuery;
};


/// Guide tree exceptions
class CPhyTreeFormatterException : public CException
{
public:

    /// Error code
    enum EErrCode {
        eInvalid,
        eInvalidInput,      ///< Invalid constructor arguments
        eInvalidOptions,    ///< Invalid parameter values
        eNodeNotFound,      ///< Node with desired id not found
        eTraverseProblem,   ///< Problem in one of the tree visitor classes
        eTaxonomyError      ///< Problem initializing CTax object
    };

    NCBI_EXCEPTION_DEFAULT(CPhyTreeFormatterException, CException);
};

END_NCBI_SCOPE

#endif // PHYTREE_FORMAT__HPP
