#ifndef ALGO_PHY_TREE___BIO_TREE_CONV__HPP
#define ALGO_PHY_TREE___BIO_TREE_CONV__HPP

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
 * File Description:  bio trees convertions
 *
 */

/// @file bio_tree.hpp
/// Things for bio tree convertions

BEGIN_NCBI_SCOPE


/// Functor to convert bio tree nodes to dynamic tree
///
/// @internal

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

/// Convert ASN.1 BioTree container to dynamic tree
///
template<class TBioTreeContainer, class TDynamicTree>
void BioTreeConvertContainer2Dynamic(TDynamicTree&             dyn_tree,
		                             const TBioTreeContainer&  tree_container)
{
	dyn_tree.Clear();
	
    // Convert feature dictionary

    typedef typename TBioTreeContainer::TFdict  TContainerDict;

    CBioTreeFeatureDictionary& dict = dyn_tree.GetFeatureDict();
    const TContainerDict& fd = tree_container.GetFdict();
    typename const TContainerDict::Tdata& feat_list = fd.Get();

		ITERATE(typename TContainerDict::Tdata, it, feat_list) {
        TBioTreeFeatureId fid = (*it)->GetId();
        const string& fvalue = (*it)->GetName();
		
		dict.Register(fid, fvalue);
    }

	// convert tree data (nodes)
    typedef typename TBioTreeContainer::TNodes           TCNodeSet;
    typedef typename TCNodeSet::Tdata                    TNodeList;

    const TNodeList node_list = tree_container.GetNodes().Get();

	ITERATE(TNodeList, it, node_list) {

		const CRef<CNode>& cnode = *it;

        TBioTreeNodeId uid = cnode->GetId();

	    typedef typename TDynamicTree::TBioTreeNode         TDynamicNodeType;
		typedef typename TDynamicNodeType::TValueType       TDynamicNodeValueType;

		TDynamicNodeValueType v;
		v.SetId(uid);

		if (cnode->CanGetFeatures()) {
			const typename CNodeFeatureSet& fset = cnode->GetFeatures();

			const typename CNodeFeatureSet::Tdata& flist = fset.Get();

			ITERATE(typename CNodeFeatureSet::Tdata, fit, flist) {
				unsigned int fid = (*fit)->GetFeatureid();
				const string& fvalue = (*fit)->GetValue();

				v.features.SetFeature(fid, fvalue);

			} // ITERATE 

		}

		if (cnode->CanGetParent()) {
	        TBioTreeNodeId parent_id = cnode->GetParent();
			/*TDynamicNodeType* dnode = */dyn_tree.AddNode(v, parent_id);
		} else {
			TDynamicNodeType* dnode = new TDynamicNodeType(v);
			dyn_tree.SetTreeNode(dnode);
		}


	} // ITERATE TNodeList
}



END_NCBI_SCOPE 


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/06/01 15:20:48  kuznets
 * + coversion function ASN.1 -> dynamic tree
 *
 * Revision 1.1  2004/05/26 15:15:19  kuznets
 * Initial revision. Tree conversion algorithms moved from bio_tree.hpp
 *
 *
 * ===========================================================================
 */


#endif

