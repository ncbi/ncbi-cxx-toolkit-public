#ifndef ALGO_PHY_TREE___BIO_TREE_FOREST__HPP
#define ALGO_PHY_TREE___BIO_TREE_FOREST__HPP

/*  $Id: bio_tree_forest.hpp
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
 * Authors:  Robert Falk
 *
 * File Description:  BioTreeForest is a set of BioTrees
 *
 */

/// @file bio_tree_forest.hpp
/// Hold a group of bio trees in a single container

#include <corelib/ncbistd.hpp>

#include <string>
#include <vector>

#include <algo/phy_tree/bio_tree.hpp>

BEGIN_NCBI_SCOPE



/*******************************************************/
/* Bio Tree Forest                                     */
/*******************************************************/

/// Set of BioTree objects managed as a single forest
template<class BioTree>
class CBioTreeForest
{
public:
    typedef BioTree        TBioTree;

    /// Biotree node (forms the tree hierarchy)
    typedef typename TBioTree::TBioTreeNode   TBioTreeNode;
    typedef typename TBioTree::TBioNodeType   TBioNodeType;

    typedef typename std::vector<BioTree*> TBioTreeVec;

public:    
    CBioTreeForest(); 
    CBioTreeForest(const CBioTreeForest<BioTree>& btr);

    virtual ~CBioTreeForest();

    CBioTreeForest<BioTree>& operator=(const CBioTreeForest<BioTree>& btr);

    /// Finds node by id.
    /// @return 
    ///     Node pointer or NULL if requested node id is unknown
    const TBioTreeNode* FindNode(TBioTreeNodeId node_id) const;

    /// Add a new tree to the set of managed BioTrees
    void AddTree(BioTree* t);
    /// Remove a tree from the forest, but do not delete it
    void RemoveTree(BioTree* t);
    /// Remove and delete a tree from the forest
    void DeleteTree(BioTree* t);
    /// Return a copy of the forest vector for direct access to the trees.
    std::vector<BioTree*> GetTrees() const { return m_Forest; }
    //std::vector<const TBioTree*> GetTrees() const; //{ return m_Forest; }

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

//    TBioTreeNode* GetTreeNodeNonConst() { return m_TreeNode.get(); }

    /// Add node to the tree (node location is defined by the parent id
    TBioTreeNode* AddNode(const TBioNodeType& node_value,
                          TBioTreeNodeId      parent_id);

    /// Clear the bio tree
    void Clear();

    /// Return feature dictionary
    CBioTreeFeatureDictionary& GetFeatureDict() { return m_FeatureDict; }

    /// Return feature dictionary
    const CBioTreeFeatureDictionary& GetFeatureDict() const 
                                                { return m_FeatureDict; }

protected:
  
    TBioTreeNodeId             m_NodeIdCounter;
    CBioTreeFeatureDictionary  m_FeatureDict;

    // Do we own the trees or not?  (CRef only works with CObject-derived)
    TBioTreeVec m_Forest;
};


/////////////////////////////////////////////////////////////////////////////
//
//  CBioTreeForest<TBioNode>
//
template<class BioTree>
CBioTreeForest<BioTree>::CBioTreeForest()
: m_NodeIdCounter(0)
{}

template<class BioTree>
CBioTreeForest<BioTree>::CBioTreeForest(const CBioTreeForest<BioTree>& btr)
: m_FeatureDict(btr.m_FeatureDict)
, m_NodeIdCounter(btr.m_NodeIdCounter)
, m_Forest(btr.m_Forest)
{
}

template<class BioTree>
CBioTreeForest<BioTree>::~CBioTreeForest()
{
    Clear();
}

template<class BioTree>
CBioTreeForest<BioTree>& 
CBioTreeForest<BioTree>::operator=(const CBioTreeForest<BioTree>& btr)
{
    m_FeatureDict = btr.m_FeatureDict;
    m_NodeIdCounter = btr.m_NodeIdCounter;
    m_Forest = m_Forest;
    return *this;
}

template<class BioTree>
const typename CBioTreeForest<BioTree>::TBioTreeNode* 
CBioTreeForest<BioTree>::FindNode(TBioTreeNodeId node_id) const
{
    const typename CBioTreeForest<BioTree>::TBioTreeNode* n = NULL;
    
    typename std::vector<BioTree*>::const_iterator iter;
    for (iter=m_Forest.begin(); iter!=m_Forest.end() && (n==NULL); ++iter) {
        n = (*iter)->FindNode(node_id);
    }
    
    return n;
}

template<class BioTree>
void CBioTreeForest<BioTree>::AddTree(BioTree* t)
{
    typename std::vector<BioTree*>::iterator iter;
    iter = std::find(m_Forest.begin(), m_Forest.end(), t);

    if (iter == m_Forest.end())
        m_Forest.push_back(t);
}

template<class BioTree>
void CBioTreeForest<BioTree>::RemoveTree(BioTree* t)
{
    typename std::vector<BioTree*>::iterator iter;
    iter = std::find(m_Forest.begin(), m_Forest.end(), t);

    _ASSERT(iter != m_Forest.end());

    m_Forest.erase(iter);
}

template<class BioTree>
void CBioTreeForest<BioTree>::DeleteTree(BioTree* t)
{
    typename std::vector<BioTree*>::iterator iter;
    iter = std::find(m_Forest.begin(), m_Forest.end(), t);

    _ASSERT(iter != m_Forest.end());   
    
    m_Forest.erase(iter);
    delete t;
}
 

template<class BioTree>
void CBioTreeForest<BioTree>::AddFeature(TBioTreeNode*      node, 
                                          TBioTreeFeatureId  feature_id,
                                          const string&      feature_value)
{
    // Check if this id is in the dictionary
    bool id_found = m_FeatureDict.HasFeature(feature_id);
    if (id_found) {
        node->GetValue().features.SetFeature(feature_id, feature_value);
    } else {
        // TODO:throw an exception here?
        _ASSERT(id_found==true);
    }
}

template<class BioTree>
void CBioTreeForest<BioTree>::AddFeature(TBioTreeNode*      node, 
                                         const string&      feature_name,
                                         const string&      feature_value)
{
    // Check if this id is in the dictionary
    TBioTreeFeatureId feature_id 
        = m_FeatureDict.GetId(feature_name);
    if (feature_id == (TBioTreeFeatureId)-1) {
        // Register the new feature type
        feature_id = m_FeatureDict.Register(feature_name);

        //?? Do we update other dictionaries here too
    }
    AddFeature(node, feature_id, feature_value);
}

template<class BioTree>
void CBioTreeForest<BioTree>::SetNodeId(TBioTreeNode* node)
{
    TBioTreeNodeId uid = GetNodeId(*node);
    node->GetValue().uid = uid;
}

template<class BioTree>
typename CBioTreeForest<BioTree>::TBioTreeNode*
CBioTreeForest<BioTree>::AddNode(const TBioNodeType& node_value,
                                 TBioTreeNodeId      parent_id)
{
	TBioTreeNode* ret = 0;
    const TBioTreeNode* pnode = FindNode(parent_id);
    if (pnode) {
        TBioTreeNode* parent_node = const_cast<TBioTreeNode*>(pnode);
        ret = parent_node->AddNode(node_value);
        TreeDepthFirstTraverse(*ret, CAssignTreeFunc(this));
    }
	return ret;
}

template<class BioTree>
void CBioTreeForest<BioTree>::Clear()
{
    m_FeatureDict.Clear();
    m_NodeIdCounter = 0;

    ITERATE(typename std::vector<BioTree*>, iter, m_Forest) {
        (*iter)->Clear();
        delete *iter;
    }

    m_Forest.clear();
}


/// Bio tree without static elements. Everything is stored as features.
///
/// @internal
typedef CBioTreeForest<CBioTreeDynamic>  CBioTreeForestDynamic;


END_NCBI_SCOPE // ALGO_PHY_TREE___BIO_TREE_FOREST__HPP

#endif  
