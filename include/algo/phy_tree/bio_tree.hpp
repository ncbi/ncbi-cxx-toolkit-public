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

#include <corelib/ncbi_tree.hpp>

BEGIN_NCBI_SCOPE

/// Feature Id. All bio tree dynamic features are encoded by feature ids.
/// Ids are shared among the tree nodes. Feature id to feature name map is 
/// supported by the tree.
typedef unsigned int TBioTreeFeatureId;

/// Tree node id. Every node has its unique id in the tree.
typedef unsigned int TBioTreeNodeId;


/// Tree node feature pair (id to string)
struct CBioTreeFeaturePair
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
class CBioTreeFeatureList
{
public:
    typedef  vector<CBioTreeFeaturePair>    TFeatureList;

    CBioTreeFeatureList();
    CBioTreeFeatureList(const CBioTreeFeatureList& flist);
    CBioTreeFeatureList& operator=(const CBioTreeFeatureList& flist);

    /// Set feature value
    void SetFeature(CBioTreeFeatureId id, const string& value);

    /// Get feature
    const string& GetFeatureValue(CBioTreeFeatureId id) const;

    /// Remode feature from the list
    void RemoveFeature(CBioTreeFeatureId id);
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
template<class NodeData     = CBioTreeEmptyNodeData, 
         class NodeFeatures = CBioTreeFeatureList>
struct BioTreeBaseNode
{
    TBioTreeNodeId uid;      ///< Unique node Id
    NodeData       data;     ///< additional node info
    NodeFeatures   features; ///< list of node features

    typedef  NodeData      TNodeData;
    typedef  NodeFeatures  TNodeFeatures;

    BioTreeBaseNode(TBioTreeNodeId uid_value = 0)
    : uid(uid_value)
    {}

    BioTreeBaseNode(const BioTreeBaseNode<NodeData, NodeFeatures>& node)
    : uid(node.uid),
      data(node.data),
      features(node.features)
    {}

    BioTreeBaseNode<NodeData, NodeFeatures>& 
    operator=(const BioTreeBaseNode<NodeData, NodeFeatures>& node)
    {
        uid = node.uid;
        data = node.data;
        features = node.features;
        return *this;
    }
};


/// Feature dictionary. 
/// Used for mapping between feature ids and feature names.
class CBioTreeFeatureDictionary
{
public:
    /// Feature dictionary (feature id -> feature name map)
    typedef map<TBioTreeFeatureId, string> TFeatureDict;

public:
    CBioTreeFeatureDictionary();
    CBioTreeFeatureDictionary(const CBioTreeFeatureDictionary& btr);
    
    CBioTreeFeatureDictionary& 
    operator=(const CBioTreeFeatureDictionary& btr);

    /// Check if feature is listed in the dictionary
    bool HasFeature() const;

    /// Register new feature, return its id.
    /// If feature is already registered just returns the id.
    /// Feature ids are auto incremented variable managed by the class.
    TBioTreeFeatureId Register(const string& feature_name);

    /// If feature is already registered returns its id by name.
    /// If feature does not exist returns 0.
    TBioTreeFeatureId GetId(const string& feature_name);


protected:
    TFeatureDict  m_Dict;      ///< id -> feature name map
    unsigned int  m_IdCounter; ///< Feature id counter
}


template<class BioNode>
class CBioTree
{
public:
    /// Biotree node (forms the tree hierarchy)
    typedef CTreeNode<BioNode> TBioTreeNode;


    CBioTreeFeatureDictionary& GetFeatureDictionary();
    const CBioTreeFeatureDictionary& GetFeatureDictionary() const;

    /// Finds node by id.
    /// @return 
    ///     Node pointer or NULL if requested node id is unknown
    const TBioTreeNode* FindNode(TBioTreeNodeId node_id) const;

    /// Add feature to the tree node
    /// Function controls that the feature is registered in the 
    /// feature dictionary of this tree.
    void AddFeature(TBioTreeNode* node, 
                    TBioTreeFeatureId feature_id,
                    const string& feature_value);

    /// Get new unique node id
    virtual TBioTreeNodeId GetNodeId();

    /// Get new unique node id 
    /// (for cases when node id depends on the node's content
    virtual TBioTreeNodeId GetNodeId(const TBioTreeNode& node);

    /// Assign new unique node id to the node
    void SetNodeId(TBioTreeNode* node);

    /// Add node to the tree (node location is defined by the parent id
    void AddNode(TBioTreeNode* node, TBioTreeNodeId parent);

protected:
    CBioTreeFeatureDictionary  m_FeatureDict;
    TBioTreeNodeId             m_NodeIdCounter;
    TBioTreeNode*              m_TreeNode;      ///< Top level node

};


END_NCBI_SCOPE // ALGO_PHY_TREE___BIO_TREE__HPP


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/03/29 16:43:38  kuznets
 * Initial revision
 *
 * ===========================================================================
 */


#endif  
