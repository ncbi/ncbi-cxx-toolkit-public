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
/// Things for representing and manipulating bio trees

#include <corelib/ncbistd.hpp>

#include <map>
#include <string>
#include <memory>
#include <vector>
#include <list>

#include <corelib/ncbi_tree.hpp>

BEGIN_NCBI_SCOPE


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

/*
/// Base node interface used for dynamic access to the node properties
struct NCBI_XALGOPHYTREE_EXPORT IBioTreeNode
{
    virtual ~IBioTreeNode();

    /// Get feature by name
    virtual void Get(const string& fetaure_name, string* value) const = 0;
    /// Get feature by value
    virtual void Get(TBioTreeFeatureId feature_id, string* value) const = 0;
    /// Check if feature id is supported
    virtual bool HasFeature(const string& fetaure_name) const = 0;
    /// Check if feature id is supported
    virtual bool HasFeature(TBioTreeFeatureId id) const = 0;
    /// Get list of supported features
    virtual void GetFeatures(list<CBioTreeFeaturePair>* features) const = 0;
};
*/

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

/*
class NCBI_XALGOPHYTREE_EXPORT CBioTreeNode
{
public:
    CBioTreeNode(IBioTreeNode* inode = 0, TBioTreeNodeId id = 0)
    : m_INode(inode), m_Id(id) 
    {}

    ~CBioTreeNode() { delete m_INode; }

    CBioTreeNode(const CBioTreeNode& node) 
    : m_INode(node.m_INode),
      m_Id(node.m_Id)
    {}

    CBioTreeNode& operator=(const CBioTreeNode& node)
    {
        delete m_INode;
        m_INode = node.m_INode;
        m_Id = node. m_Id;
        return *this;
    }

    /// Return interface to access node properties dynamically
    const IBioTreeNode* GetDynamicNode() const { return m_INode; }

    TBioTreeNodeId GetId() const { return m_Id; }
    void SetId(TBioTreeNodeId id) { m_Id = id; }

private:
    IBioTreeNode*       m_INode;
    TBioTreeNodeId      m_Id;      ///< Unique node Id
};
*/

/// Basic data contained in every bio-tree node
///
/// Template parameter NodeData allows to extend list of 
/// compile-time (non-dynamic attributes in the tree)
/// NodeFeatures - extends node with dynamic list of node attributes
///
///
template<class TNodeData     = CBioTreeEmptyNodeData, 
         class TNodeFeatures = CBioTreeFeatureList>
struct BioTreeBaseNode
{
    TBioTreeNodeId      uid;      ///< Unique node Id
    TNodeData           data;     ///< additional node info
    TNodeFeatures       features; ///< list of node features

    typedef  TNodeData        TNodeDataType;
    typedef  TNodeFeatures    TNodeFeaturesType;


    BioTreeBaseNode(TBioTreeNodeId uid_value = 0)
     : uid(uid_value)
    {}

    BioTreeBaseNode(const BioTreeBaseNode<TNodeData, TNodeFeatures>& node)
     : uid(node.uid),
      data(node.data),
      features(node.features)
    {}

    BioTreeBaseNode<TNodeData, TNodeFeatures>& 
    operator=(const BioTreeBaseNode<TNodeData, TNodeFeatures>& node)
    {
        uid = node.uid;
        data = node.data;
        features = node.features;
        return *this;
    }

    TBioTreeNodeId GetId() const { return uid; }

    void SetId(TBioTreeNodeId id) { uid = id; }
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
    bool HasFeature(const string& feature_name);

    /// Check if feature is listed in the dictionary
    bool HasFeature(TBioTreeFeatureId id);

    /// Register new feature, return its id.
    /// If feature is already registered just returns the id.
    /// Feature ids are auto incremented variable managed by the class.
    TBioTreeFeatureId Register(const string& feature_name);

    /// If feature is already registered returns its id by name.
    /// If feature does not exist returns 0.
    TBioTreeFeatureId GetId(const string& feature_name) const;

    /// Clear the dictionary
    void Clear();

    /// Get reference on the internal map
    const TFeatureDict& GetFeatureDict() const { return m_Dict; }

protected:
    TFeatureDict     m_Dict;        ///< id -> feature name map
    TFeatureNameIdx  m_Name2Id;     ///< id -> feature name map

    unsigned int     m_IdCounter;   ///< Feature id counter
};

/*
class NCBI_XALGOPHYTREE_EXPORT CBioTree
{
public:
    typedef CTreeNode<CBioTreeNode> TBioTreeNode;

public:
    CBioTree() 
     : m_NodeIdCounter(0),
       m_TreeNode(0)
    {}

    CBioTreeFeatureDictionary& GetFeatureDictionary()
    {
        return m_FeatureDict;
    }

    const CBioTreeFeatureDictionary& GetFeatureDictionary() const
    {
        return m_FeatureDict;
    }

    const TBioTreeNode* GetTopNode() const { return m_TreeNode.get(); }

    void SetTopNode(TBioTreeNode* node) { m_TreeNode.reset(node); }

private:
    CBioTree(const CBioTree& btr);
    CBioTree& operator=(const CBioTree& btr);

protected:
    CBioTreeFeatureDictionary  m_FeatureDict;
    TBioTreeNodeId             m_NodeIdCounter;
    auto_ptr<TBioTreeNode>     m_TreeNode;      ///< Top level node
};
*/


/// Basic tree structure for biological applications
template<class TBioNode>
class CBioTree
{
public:
    /// Biotree node (forms the tree hierarchy)
    typedef CTreeNode<TBioNode> TBioTreeNode;

    typedef TBioNode            TBioNodeType;


public:

    CBioTree() 
     : m_NodeIdCounter(0),
       m_TreeNode(0)
    {}

    CBioTree(const CBioTree<TBioNode>& btr)
    : m_FeatureDict(btr.m_FeatureDict),
      m_NodeIdCounter(btr.m_NodeIdCounter),
      m_TreeNode(new TBioTreeNode(*(btr.m_TreeNode)))
    {
    }

    CBioTree<TBioNode>&
        operator=(const CBioTree<TBioNode>& btr)
    {
        m_FeatureDict = btr.m_FeatureDict;
        m_NodeIdCounter = btr.m_NodeIdCounter;
        m_TreeNode = new TBioTreeNode(*(btr.m_TreeNode));
    }



    /// Finds node by id.
    /// @return 
    ///     Node pointer or NULL if requested node id is unknown
    const TBioTreeNode* FindNode(TBioTreeNodeId node_id) const
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

    /// Add feature to the tree node
    /// Function controls that the feature is registered in the 
    /// feature dictionary of this tree.
    void AddFeature(TBioTreeNode*      node, 
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

    /// Add feature to the tree node
    /// Function controls that the feature is registered in the 
    /// feature dictionary of this tree. If feature is not found
    /// it is added to the dictionary
    void AddFeature(TBioTreeNode*      node, 
                    const string&      feature_name,
                    const string&      feature_value)
    {
        // Check if this id is in the dictionary
        TBioTreeFeatureId feature_id 
            = m_FeatureDict.GetId(feature_name);
        if (!feature_id) {
            // Register the new feature type
            feature_id = m_FeatureDict.Register(feature_name);
        }
        AddFeature(node, feature_id, feature_value);
    }


    /// Get new unique node id
    virtual TBioTreeNodeId GetNodeId()
    {
        return m_NodeIdCounter++;
    }

    /// Get new unique node id 
    /// (for cases when node id depends on the node's content
    virtual TBioTreeNodeId GetNodeId(const TBioTreeNode& node)
    {
        return m_NodeIdCounter++;
    }

    /// Assign new unique node id to the node
    void SetNodeId(TBioTreeNode* node)
    {
        TBioTreeNodeId uid = GetNodeId(*node);
        node->uid = uid;
    }

    /// Assign new top level tree node
    void SetTreeNode(TBioTreeNode* node)
    {
        _ASSERT(node->GetParent() == 0);
        m_TreeNode.reset(node);
    }

    const TBioTreeNode* GetTreeNode() const { return m_TreeNode.get(); }

    /// Add node to the tree (node location is defined by the parent id
    void AddNode(const TBioNodeType& node_value, 
                 TBioTreeNodeId      parent_id)
    {
        const TBioTreeNode* pnode = FindNode(parent_id);
        if (pnode) {
            TBioTreeNode* parent_node = const_cast<TBioTreeNode*>(pnode);
            pnode->AddNode(node_value);
        }
    }

    /// Clear the bio tree
    void Clear()
    {
        m_FeatureDict.Clear();
        m_NodeIdCounter = 0;
        m_TreeNode.reset(0);
    }

    CBioTreeFeatureDictionary& GetFeatureDict() { return m_FeatureDict; }
    const CBioTreeFeatureDictionary& GetFeatureDict() const 
    { return m_FeatureDict; }

protected:

    /// Find node by UID functor
    class CFindUidFunc 
    {
    public:
        CFindUidFunc(TBioTreeNodeId uid)
         : m_Uid(uid), 
           m_Node(0)
        {}

        const TBioTreeNode* GetNode() const { return m_Node; }

        ETreeTraverseCode operator()(TBioTreeNode& tree_node, int delta)
        {
            if (delta == 0 || delta == 1) {
                if (tree_node.uid == m_Uid) {
                    m_Node = &tree_node;
                    return eTreeTraverseStop;
                }
            }
            return eTreeTraverse;
        }

    private:
        TBioTreeNodeId  m_Uid;    ///< Node uid to search for
        TBioTreeNode*   m_Node;   ///< Search result
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

/*
template<class TDynamicTree, class TSrcBioTree, class TNodeConvFunc>
class CBioTreeConvert2DynamicFunc
{
public:
    typedef typename TSrcBioTree::TBioTreeNode   TBioTreeNodeType;
    typedef typename TDynamicTree::TBioTreeNode  TDynamicNodeType;

public:
    CBioTreeConvert2DynamicFunc(TDynamicTree* dyn_tree, TNodeConvFunc func)
    : m_DynTree(dyn_tree),
      m_ConvFunc(func)
    {}

    ETreeTraverseCode 
    operator()(const TBioTreeNodeType& node, 
               int                     delta_level)
    {
        if (m_TreeStack.size() == 0) {
            auto_ptr<TDynamicNodeType> pnode(MakeDynamicNode(node));

            m_TreeStack.push_back(pnode.get());
            m_DynTree->SetTreeNode(pnode.release());
            return eTreeTraverse;
        }

        if (delta_level == 0) {
            if (m_TreeStack.size() > 0) {
                m_TreeStack.pop_back();
            }
            TDynamicNodeType* parent_node = m_TreeStack.back();
            TDynamicNodeType* pnode= MakeDynamicNode(node);

            parent_node->AddNode(pnode);
            m_TreeStack.push_back(pnode);
            return eTreeTraverse;
        }

        if (delta_level == 1) {
            TDynamicNodeType* parent_node = m_TreeStack.back();
            TDynamicNodeType* pnode= MakeDynamicNode(node);

            parent_node->AddNode(pnode);
            m_TreeStack.push_back(pnode);
            return eTreeTraverse;                        
        }
        if (delta_level == -1) {
            m_TreeStack.pop_back();
        }

        return eTreeTraverse;
    }

protected:

    TDynamicNodeType* MakeDynamicNode(const TBioTreeNodeType& src_node)
    {
        auto_ptr<TDynamicNodeType> pnode(new TDynamicNodeType());
        TBioTreeNodeId uid = src_node.GetValue().GetId();
        pnode->GetValue().SetId(uid);

        m_ConvFunc(*pnode.get(), src_node);
        return pnode.release();        
    }

private:
    TDynamicTree*                m_DynTree;
    TNodeConvFunc                m_ConvFunc;
    vector<TDynamicNodeType*>    m_TreeStack;
};


/// Convert any tree to dynamic tree using a node converter
///
template<class TDynamicTree, class TBioTree, class TNodeConvFunc>
void BioTreeConvert2Dynamic(TDynamicTree&      dyn_tree, 
                            const TBioTree&    bio_tree,
                            TNodeConvFunc      node_conv)
{
    dyn_tree.Clear();

    CBioTreeConvert2DynamicFunc<TDynamicTree, TBioTree, TNodeConvFunc> 
       func(&dyn_tree, node_conv);

    typedef typename TBioTree::TBioTreeNode TTreeNode;
    const TTreeNode *n = bio_tree.GetTreeNode();

    TreeDepthFirstTraverse(*(const_cast<TTreeNode*>(n)), func);
}



/// Functor to convert dynamic tree nodes to ASN.1 BioTree container
///
/// @internal
template<class TBioTreeContainer, class TDynamicTree>
class CBioTreeConvert2ContainerFunc
{
protected:
    typedef typename TDynamicTree::TBioTreeNode         TDynamicNodeType;
    typedef typename TDynamicNodeType::TValueType       TDynamicNodeValueType;

    typedef typename TBioTreeContainer::TNodes           TCNodeSet;
    typedef typename TCNodeSet::Tdata                    TNodeList;
    typedef typename TNodeList::value_type::element_type TCNode;
    typedef typename TCNode::TFeatures                   TCNodeFeatureSet;
    typedef typename TCNodeFeatureSet::Tdata             TNodeFeatureList;
    typedef typename 
       TNodeFeatureList::value_type::element_type        TCNodeFeature;
public:
    CBioTreeConvert2ContainerFunc(TBioTreeContainer* tree_container)
    : m_Container(tree_container)
    {
        m_NodeList = &(tree_container->SetNodes().Set());
    }

    ETreeTraverseCode 
    operator()(const TDynamicNodeType& node, 
               int                     delta_level)
    {
        if (delta_level < 0) {
            return eTreeTraverse;
        }

        const TDynamicNodeValueType& v = node.GetValue();
        TBioTreeNodeId uid = v.GetId();

        CRef<TCNode> cnode(new TCNode);
        cnode->SetId(uid);

        const TDynamicNodeType* node_parent = node.GetParent();
        if (node_parent) {
            cnode->SetParent(node_parent->GetValue().GetId());
        }
        
        typedef typename 
           TDynamicNodeValueType::TNodeFeaturesType::TFeatureList TFList;
        const TFList& flist = v.features.GetFeatureList();

        if (!flist.empty()) {
            
            TCNodeFeatureSet& fset = cnode->SetFeatures();

            ITERATE(typename TFList, it, flist) {
                TBioTreeFeatureId fid = it->id;
                const string fvalue = it->value;

                CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
                cfeat->SetFeatureid(fid);
                cfeat->SetValue(fvalue);
                
                fset.Set().push_back(cfeat);

            
            } // ITERATE
        }

        m_NodeList->push_back(cnode);

        return eTreeTraverse;
    }

private:
    TBioTreeContainer*   m_Container;
    TNodeList*           m_NodeList;
};



/// Convert Dynamic tree to ASN.1 BioTree container
///
template<class TBioTreeContainer, class TDynamicTree>
void BioTreeConvert2Container(TBioTreeContainer&      tree_container,
                              const TDynamicTree&     dyn_tree)
{
    // Convert feature dictionary

    typedef typename TBioTreeContainer::TFdict  TContainerDict;

    const CBioTreeFeatureDictionary& dict = dyn_tree.GetFeatureDict();
    const CBioTreeFeatureDictionary::TFeatureDict& dict_map = 
                                                dict.GetFeatureDict();

    TContainerDict& fd = tree_container.SetFdict();
    typename TContainerDict::Tdata& feat_list = fd.Set();
    typedef 
    typename TContainerDict::Tdata::value_type::element_type TCFeatureDescr;
    
    ITERATE(CBioTreeFeatureDictionary::TFeatureDict, it, dict_map) {
        TBioTreeFeatureId fid = it->first;
        const string& fvalue = it->second;

        {{
        CRef<TCFeatureDescr> d(new TCFeatureDescr);
        d->SetId(fid);
        d->SetName(fvalue);

        feat_list.push_back(d);
        }}
    } // ITERATE


    // convert tree data (nodes)

    typedef typename TBioTreeContainer::TNodes  TDynNodes;
    TDynNodes& nodes = tree_container.SetNodes();

    typedef typename TDynamicTree::TBioTreeNode TTreeNode;
    const TTreeNode *n = dyn_tree.GetTreeNode();

    CBioTreeConvert2ContainerFunc<TBioTreeContainer, TDynamicTree>
        func(&tree_container);
    TreeDepthFirstTraverse(*(const_cast<TTreeNode*>(n)), func);
    
}

*/

END_NCBI_SCOPE // ALGO_PHY_TREE___BIO_TREE__HPP


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/05/26 15:14:33  kuznets
 * Tree convertion algorithms migrated to bio_tree_conv.hpp, plus multiple
 * minor changes.
 *
 * Revision 1.7  2004/05/19 14:37:03  ucko
 * Remove extraneous "typename" that confused some compilers.
 *
 * Revision 1.6  2004/05/19 12:46:25  kuznets
 * Added utilities to convert tree to a fully dynamic variant.
 *
 * Revision 1.5  2004/05/10 20:51:27  ucko
 * Repoint m_TreeNode with .reset() rather than assignment, which not
 * all auto_ptr<> implementations support.
 *
 * Revision 1.4  2004/05/10 15:45:46  kuznets
 * Redesign. Added dynamic interface for UI viewer compatibility.
 *
 * Revision 1.3  2004/04/06 20:32:42  ucko
 * +<memory> for auto_ptr<>
 *
 * Revision 1.2  2004/04/06 17:59:00  kuznets
 * Work in progress
 *
 * Revision 1.1  2004/03/29 16:43:38  kuznets
 * Initial revision
 *
 * ===========================================================================
 */


#endif  
