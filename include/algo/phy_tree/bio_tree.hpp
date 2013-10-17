#ifndef ALGO_PHY_TREE___BIO_TREE__HPP
#define ALGO_PHY_TREE___BIO_TREE__HPP

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
 * File Description:  Things for representing and manipulating bio trees
 *
 */

/// @file bio_tree.hpp
/// Things for representing and manipulating bio trees. 
/// CBioTree is the central class here.

#include <corelib/ncbistd.hpp>

#include <map>
#include <string>
#include <memory>
#include <vector>
#include <list>

#include <corelib/ncbi_tree.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup Tree
 *
 * @{
 */


/// Feature Id. All bio tree dynamic features are encoded by feature ids.
/// Ids are shared among the tree nodes. Feature id to feature name map is 
/// supported by the tree.
typedef unsigned int TBioTreeFeatureId;

/// Tree node id. Every node has its unique id in the tree.
typedef unsigned int TBioTreeNodeId;




/// Tree node feature pair (id to string)
struct NCBI_XALGOPHYTREE_EXPORT CBioTreeFeaturePair
{
    TBioTreeFeatureId  id;
    string             value;

    CBioTreeFeaturePair(TBioTreeFeatureId fid, const string& fvalue)
    : id(fid),
      value(fvalue)
    {}

    CBioTreeFeaturePair(void)
    : id(0)
    {}
};


/// Features storage for the bio tree node
///
/// Every node in the bio tree may have a list of attached features.
/// Features are string attributes which may vary from node to node in the
/// tree.
///
/// Implementation note: This class may evolve into a specialized templates
/// parameterizing different feature storage options (like vector, map, list)
/// depending on what tree is used.
class NCBI_XALGOPHYTREE_EXPORT CBioTreeFeatureList
{
public:
    typedef  vector<CBioTreeFeaturePair>    TFeatureList;

    CBioTreeFeatureList();
    CBioTreeFeatureList(const CBioTreeFeatureList& flist);
    CBioTreeFeatureList& operator=(const CBioTreeFeatureList& flist);

    /// Set feature value, feature if exists replaced, if not added.
    void SetFeature(TBioTreeFeatureId id, const string& value);

    /// Get feature value by id
    /// @return Feature value or empty string if feature does not exists
    const string& GetFeatureValue(TBioTreeFeatureId id) const;

    /// Remove feature from the list
    void RemoveFeature(TBioTreeFeatureId id);

    /// Get feature value by id (operator semantics)
    const string& operator[](TBioTreeFeatureId id) const
    {
        return GetFeatureValue(id);
    }

    /// Return reference on the internal container
    const TFeatureList& GetFeatureList() const { return m_FeatureList; }
protected:
    TFeatureList  m_FeatureList;
};

/// Basic node data structure used by BioTreeBaseNode.
/// Strucure carries absolutely no info.
struct CBioTreeEmptyNodeData
{
};


/// Basic data contained in every bio-tree node
///
/// Template parameter NodeData allows to extend list of 
/// compile-time (non-dynamic attributes in the tree)
/// NodeFeatures - extends node with dynamic list of node attributes
///
///
template<class TNodeData     = CBioTreeEmptyNodeData, 
         class TNodeFeatures = CBioTreeFeatureList>
class BioTreeBaseNode
{
public:
    typedef  TNodeData        TNodeDataType;
    typedef  TNodeFeatures    TNodeFeaturesType;
public:

    BioTreeBaseNode(TBioTreeNodeId uid_value = 0)
     : uid(uid_value)
    {}

    TBioTreeNodeId GetId() const { return uid; }

    void SetId(TBioTreeNodeId id) { uid = id; }
public:
    TBioTreeNodeId      uid;      ///< Unique node Id
    TNodeData           data;     ///< additional node info
    TNodeFeatures       features; ///< list of node features
};


/// Feature dictionary. 
/// Used for mapping between feature ids and feature names.
///
class NCBI_XALGOPHYTREE_EXPORT CBioTreeFeatureDictionary
{
public:
    /// Feature dictionary (feature id -> feature name map)
    typedef map<TBioTreeFeatureId, string> TFeatureDict;

    /// Feature reverse index (feature name -> id)
    typedef map<string, TBioTreeFeatureId> TFeatureNameIdx;

public:
    CBioTreeFeatureDictionary();
    CBioTreeFeatureDictionary(const CBioTreeFeatureDictionary& btr);
    
    CBioTreeFeatureDictionary& 
       operator=(const CBioTreeFeatureDictionary& btr);

    /// Check if feature is listed in the dictionary
    bool HasFeature(const string& feature_name) const;

    /// Check if feature is listed in the dictionary
    bool HasFeature(TBioTreeFeatureId id) const;

    /// Register new feature, return its id.
    /// If feature is already registered just returns the id.
    /// Feature ids are auto incremented variable managed by the class.
    TBioTreeFeatureId Register(const string& feature_name);

	/// Register new feature.
	/// Throws an exception if a feature with this id or this name already exists
	void Register(TBioTreeFeatureId id, const string& feature_name);

    void Unregister(TBioTreeFeatureId id);

    /// If feature is already registered returns its id by name.
    /// If feature does not exist returns -1.
    TBioTreeFeatureId GetId(const string& feature_name) const;

    /// Return the featue name given the id, or "" if not found
    string GetName(TBioTreeFeatureId id) const;

    /// Clear the dictionary
    void Clear();

    /// Get reference on the internal map
    const TFeatureDict& GetFeatureDict() const { return m_Dict; }

protected:
    TFeatureDict     m_Dict;        ///< id -> feature name map
    TFeatureNameIdx  m_Name2Id;     ///< id -> feature name map

    unsigned int     m_IdCounter;   ///< Feature id counter
};


/// Basic tree structure for biological applications
template<class TBioNode>
class CBioTree
{
public:
    typedef CBioTree<TBioNode>   TBioTree;

    // Hide the base class from SWIG
    #ifdef NCBI_SWIG
    class CBioNode
    #else
    class CBioNode : public CTreeNode<TBioNode>
    #endif
    {
    public: 
        typedef CTreeNode<TBioNode> TParent;
        typedef CBioNode            TTreeType;

        /// Construct tree node with specified value
        CBioNode(const TBioNode& value = TBioNode()) 
        : TParent(value), m_ParentTree(0) 
        {}

        /// Construct tree node based on source tree node (recursive copy)
        CBioNode(const CBioNode& bn)
        : TParent(bn), m_ParentTree(0) 
        {
            TBioTree* pt = (TBioTree*)bn.GetParentTree();
            SetParentTree(pt);
        }

        /// Recursive assignment
        CBioNode& operator=(const CBioNode& tree)
        {
            TParent::operator=(tree);
            TBioTree* pt = (TBioTree*)tree.GetParentTree();
            SetParentTree(pt);
        }

        /// Associate node with the hosting class (non-recursive)
        void SetParentTree(TBioTree* pt) { m_ParentTree = pt; }

        /// Return pointer on the hosting tree (can be NULL)
        const TBioTree* GetParentTree() const { return m_ParentTree; }

        /// Return pointer on the hosting tree (can be NULL)
        TBioTree* GetParentTree() { return m_ParentTree; }

        TTreeType* DetachNode(TTreeType* subnode)
        {
            typename TParent::TTreeType* ptn = TParent::DetachNode(subnode);
            if (ptn) {
                TTreeType* n = (TTreeType*) ptn;
                TBioTree* btr = GetParentTree();
                if (btr) {
                    btr->SetParentTree(*n, 0);
                }
                return n;
            }
            return 0;
        }

        /// Node is detached from the tree (Parent becomes 0)
        TTreeType* DetachNode(typename TParent::TNodeList_I it)
        {
            typename TParent::TTreeType* ptn = TParent::DetachNode(it);
            if (ptn) {
                TTreeType* n = (TTreeType*) ptn;
                TBioTree* btr = GetParentTree();
                if (btr) {
                    btr->SetParentTree(*n, 0);
                }
                return n;
            }
            return 0;
        }

        /// Node is added as a subnode the this node
        void AddNode(TTreeType* subnode)
        {
            if (TBioTree* btr = GetParentTree()) {
                btr->SetParentTree(*subnode);
            }
            TParent::AddNode(subnode);
        }

        /// Add a subnode to this node based on value
        CBioNode* AddNode(const TBioNode& val = TBioNode())
        {
            CBioNode* subnode = new CBioNode(val);
            AddNode(subnode);            
            return subnode;
        }


        /// Get dynamic feature by name
        const string& GetFeature(const string& feature_name) const
        {
            const TBioTree* btr = GetParentTree();
            _ASSERT(btr);
            const CBioTreeFeatureDictionary& dict = btr->GetFeatureDict();
            
            TBioTreeFeatureId fid = dict.GetId(feature_name);

            if (fid == (TBioTreeFeatureId)-1) 
                return kEmptyStr;

            const TBioNode& value = TParent::GetValue();            
            return value.features[fid];
        }

        void SetFeature(const string&  feature_name,
                        const string&  feature_value)
        {
            TBioTree* btr = GetParentTree();
            _ASSERT(btr);
            btr->AddFeature(this, feature_name, feature_value);
        }

        const string& operator[](const string& feature_name) const
        {
            return GetFeature(feature_name);
        }

    protected:
        TBioTree*   m_ParentTree;  ///< Pointer on the hosting class
    };

    /// Biotree node (forms the tree hierarchy)
    typedef CBioNode            TBioTreeNode;
    typedef TBioNode            TBioNodeType;

public:

    CBioTree(); 
	virtual ~CBioTree() {}

    CBioTree(const CBioTree<TBioNode>& btr);

    CBioTree<TBioNode>& operator=(const CBioTree<TBioNode>& btr);


    /// Finds node by id.
    /// @return 
    ///     Node pointer or NULL if requested node id is unknown
    const TBioTreeNode* FindNode(TBioTreeNodeId node_id) const;


    /// Add feature to the tree node
    /// Function controls that the feature is registered in the 
    /// feature dictionary of this tree.
    void AddFeature(TBioTreeNode*      node, 
                    TBioTreeFeatureId  feature_id,
                    const string&      feature_value);

    /// Add feature to the tree node
    /// Function controls that the feature is registered in the 
    /// feature dictionary of this tree. If feature is not found
    /// it is added to the dictionary
    void AddFeature(TBioTreeNode*      node, 
                    const string&      feature_name,
                    const string&      feature_value);

    /// Get new unique node id
    virtual TBioTreeNodeId GetNodeId() { return m_NodeIdCounter++; }

    /// Get new unique node id 
    /// (for cases when node id depends on the node's content
    virtual TBioTreeNodeId GetNodeId(const TBioTreeNode& node) 
                                       { return m_NodeIdCounter++; }

    /// Assign new unique node id to the node
    void SetNodeId(TBioTreeNode* node);

    /// Assign new top level tree node
    void SetTreeNode(TBioTreeNode* node);

    const TBioTreeNode* GetTreeNode() const { return m_TreeNode.get(); }

    TBioTreeNode* GetTreeNodeNonConst() { return m_TreeNode.get(); }

    void DetachTreeNode() { m_TreeNode.release(); }

    /// Add node to the tree (node location is defined by the parent id
    TBioTreeNode* AddNode(const TBioNodeType& node_value, 
                          TBioTreeNodeId      parent_id);

    /// Add a node to the tree when you have already looked up
    /// the parent node (faster)
    TBioTreeNode* AddNode(const TBioNodeType& node_value, 
                          TBioTreeNode* parent_node);

    /// Recursively set this tree as parent tree for the node
    void SetParentTree(CBioNode& node) { SetParentTree(node, this); }

    /// Recursively set parent tree for the node
    void SetParentTree(CBioNode& node, CBioTree* tr) 
                       { TreeDepthFirstTraverse(node, CAssignTreeFunc(tr)); }
	
    /// Clear the bio tree
    void Clear();

    /// Return feature dictionary
    CBioTreeFeatureDictionary& GetFeatureDict() { return m_FeatureDict; }

    /// Return feature dictionary
    const CBioTreeFeatureDictionary& GetFeatureDict() const 
                                                { return m_FeatureDict; }

protected:

    /// Find node by UID functor
    ///
    /// @internal
    class CFindUidFunc 
    {
    public:
        CFindUidFunc(TBioTreeNodeId uid) : m_Uid(uid), m_Node(0) {}

        ETreeTraverseCode operator()(TBioTreeNode& tree_node, int delta)
        {
            if (delta == 0 || delta == 1) {
                if (tree_node.GetValue().GetId() == m_Uid) {
                    m_Node = &tree_node;
                    return eTreeTraverseStop;
                }
            }
            return eTreeTraverse;
        }

        const TBioTreeNode* GetNode() const { return m_Node; }
    private:
        TBioTreeNodeId  m_Uid;    ///< Node uid to search for
        TBioTreeNode*   m_Node;   ///< Search result
    };

    /// Functor to reset tree pointer in all nodes
    ///
    /// @internal
    struct CAssignTreeFunc
    {
        CAssignTreeFunc(CBioTree* tree) : m_Tree(tree) {}
        ETreeTraverseCode operator()(TBioTreeNode& tree_node, int delta)
        {
            if (delta == 0 || delta == 1) 
                tree_node.SetParentTree(m_Tree);
            return eTreeTraverse;
        }
    private:
        CBioTree*   m_Tree;
    };

protected:
    CBioTreeFeatureDictionary  m_FeatureDict;
    TBioTreeNodeId             m_NodeIdCounter;
    auto_ptr<TBioTreeNode>     m_TreeNode;      ///< Top level node
};


/// Bio tree without static elements. Everything is stored as features.
///
/// @internal
typedef 
  CBioTree<BioTreeBaseNode<CBioTreeEmptyNodeData, CBioTreeFeatureList> >
  CBioTreeDynamic;

/// Newick format output
NCBI_XALGOPHYTREE_EXPORT
CNcbiOstream& operator<<(CNcbiOstream& os, const CBioTreeDynamic& tree);

/// Nexus format output (Newick with some stuff around it).
///
/// tree_name gets put in the file.
NCBI_XALGOPHYTREE_EXPORT
void WriteNexusTree(CNcbiOstream& os, const CBioTreeDynamic& tree,
                    const string& tree_name = "the_tree");

/// Newick but without the terminal ';'
NCBI_XALGOPHYTREE_EXPORT
void PrintNode(CNcbiOstream& os, const CBioTreeDynamic& tree,
               const CBioTreeDynamic::TBioTreeNode& node);


/* @} */


/////////////////////////////////////////////////////////////////////////////
//
//  CBioTree<TBioNode>
//


template<class TBioNode>
CBioTree<TBioNode>::CBioTree()
: m_NodeIdCounter(0)
{}

template<class TBioNode>
CBioTree<TBioNode>::CBioTree(const CBioTree<TBioNode>& btr)
: m_FeatureDict(btr.m_FeatureDict),
  m_NodeIdCounter(btr.m_NodeIdCounter),
  m_TreeNode(new TBioTreeNode(*(btr.m_TreeNode)))
{
}

template<class TBioNode>
CBioTree<TBioNode>& 
CBioTree<TBioNode>::operator=(const CBioTree<TBioNode>& btr)
{
    m_FeatureDict = btr.m_FeatureDict;
    m_NodeIdCounter = btr.m_NodeIdCounter;
    m_TreeNode.reset(new TBioTreeNode(*(btr.m_TreeNode)));
    return *this;
}

template<class TBioNode>
const typename CBioTree<TBioNode>::TBioTreeNode* 
CBioTree<TBioNode>::FindNode(TBioTreeNodeId node_id) const
{
    TBioTreeNode* tree_node = 
        const_cast<TBioTreeNode*>(m_TreeNode.get());
    if (tree_node == 0) {
        return 0;
    }
    CFindUidFunc func = 
        TreeDepthFirstTraverse(*tree_node, CFindUidFunc(node_id));
    return func.GetNode();
}

template<class TBioNode>
void CBioTree<TBioNode>::AddFeature(TBioTreeNode*      node, 
                                    TBioTreeFeatureId  feature_id,
                                    const string&      feature_value)
{
    // Check if this id is in the dictionary
    bool id_found = m_FeatureDict.HasFeature(feature_id);
    if (id_found) {
        node->GetValue().features.SetFeature(feature_id, feature_value);
    } else {
        // TODO:throw an exception here
    }
}

template<class TBioNode>
void CBioTree<TBioNode>::AddFeature(TBioTreeNode*      node, 
                                    const string&      feature_name,
                                    const string&      feature_value)
{
    // Check if this id is in the dictionary
    TBioTreeFeatureId feature_id 
        = m_FeatureDict.GetId(feature_name);
    if (feature_id == (TBioTreeFeatureId)-1) {
        // Register the new feature type
        feature_id = m_FeatureDict.Register(feature_name);
    }
    AddFeature(node, feature_id, feature_value);
}

template<class TBioNode>
void CBioTree<TBioNode>::SetNodeId(TBioTreeNode* node)
{
    TBioTreeNodeId uid = GetNodeId(*node);
    node->GetValue().uid = uid;
}

template<class TBioNode>
void CBioTree<TBioNode>::SetTreeNode(TBioTreeNode* node)
{
    _ASSERT(node->GetParent() == 0);
    TreeDepthFirstTraverse(*node, CAssignTreeFunc(this));
    m_TreeNode.reset(node);
}

template<class TBioNode>
typename CBioTree<TBioNode>::TBioTreeNode* 
CBioTree<TBioNode>::AddNode(const TBioNodeType& node_value, 
                            TBioTreeNodeId      parent_id)
{
	TBioTreeNode* ret = 0;
    const TBioTreeNode* pnode = FindNode(parent_id);
    if (pnode) {
        TBioTreeNode* parent_node = const_cast<TBioTreeNode*>(pnode);
        ret = parent_node->AddNode(node_value);
        // done by AddNode:
        //TreeDepthFirstTraverse(*ret, CAssignTreeFunc(this));
    }
	return ret;
}

template<class TBioNode>
typename CBioTree<TBioNode>::TBioTreeNode* 
CBioTree<TBioNode>::AddNode(const TBioNodeType& node_value, 
                             TBioTreeNode* parent_node)
{
	TBioTreeNode* ret = 0;
    ret = parent_node->AddNode(node_value);
    //done by AddNode:
    //TreeDepthFirstTraverse(*ret, CAssignTreeFunc(this));

	return ret;
}

template<class TBioNode>
void CBioTree<TBioNode>::Clear()
{
    m_FeatureDict.Clear();
    m_NodeIdCounter = 0;
    m_TreeNode.reset(0);
}

/// Bio tree without static elements. Everything is stored as features.
///
/// @internal
typedef 
  CBioTree<BioTreeBaseNode<CBioTreeEmptyNodeData, CBioTreeFeatureList> >
  CBioTreeDynamic;


END_NCBI_SCOPE // ALGO_PHY_TREE___BIO_TREE__HPP

#endif  
