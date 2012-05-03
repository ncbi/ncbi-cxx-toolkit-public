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

/** @addtogroup Tree
 *
 * @{
 */


// --------------------------------------------------------------------------


/// Visitor functor to convert one tree to another
///
/// @internal

template<class TDstTreeNode, class TSrcTreeNode, class TNodeConvFunc>
class CTree2TreeFunc
{
public:
    typedef TDstTreeNode                         TDstTreeNodeType;
    typedef TSrcTreeNode                         TSrcTreeNodeType;

public:
    CTree2TreeFunc(TNodeConvFunc& func) : m_DstTree(0), m_ConvFunc(func) {}


    ETreeTraverseCode 
    operator()(const TSrcTreeNodeType& node, 
               int                     delta_level)
    {
        if (m_TreeStack.size() == 0) {
            auto_ptr<TDstTreeNodeType> pnode(MakeNewTreeNode(node));

            m_TreeStack.push_back(pnode.get());
            m_DstTree = pnode.release();
            return eTreeTraverse;
        }

        if (delta_level == 0) {
            if (m_TreeStack.size() > 0) {
                m_TreeStack.pop_back();
            }
            TDstTreeNodeType* parent_node = m_TreeStack.back();
            TDstTreeNodeType* pnode= MakeNewTreeNode(node);

            parent_node->AddNode(pnode);
            m_TreeStack.push_back(pnode);
            return eTreeTraverse;
        }

        if (delta_level == 1) {
            TDstTreeNodeType* parent_node = m_TreeStack.back();
            TDstTreeNodeType* pnode= MakeNewTreeNode(node);

            parent_node->AddNode(pnode);
            m_TreeStack.push_back(pnode);
            return eTreeTraverse;                        
        }
        if (delta_level == -1) {
            m_TreeStack.pop_back();
        }

        return eTreeTraverse;
    }

    TDstTreeNodeType* GetTreeNode() { return m_DstTree; }

protected:

    TDstTreeNodeType* MakeNewTreeNode(const TSrcTreeNodeType& src_node)
    {
        auto_ptr<TDstTreeNodeType> pnode(new TDstTreeNodeType());
        unsigned int uid = src_node.GetValue().GetId();
        pnode->GetValue().SetId(uid);

        m_ConvFunc(*pnode.get(), src_node);
        return pnode.release();
    }

private:
    TDstTreeNodeType*            m_DstTree;
    TNodeConvFunc&               m_ConvFunc;
    vector<TDstTreeNodeType*>    m_TreeStack;
};



/// Convert biotree to dynamic tree using a node converter
///
template<class TDynamicTree, class TSrcBioTree, class TNodeConvFunc>
void BioTreeConvert2Dynamic(TDynamicTree&         dyn_tree, 
                            const TSrcBioTree&    bio_tree,
                            TNodeConvFunc         node_conv)
{
    dyn_tree.Clear();
    typedef typename TSrcBioTree::TBioTreeNode    TSrcTreeNodeType;
    typedef typename TDynamicTree::TBioTreeNode   TDstTreeNodeType;


    CTree2TreeFunc<TDstTreeNodeType, TSrcTreeNodeType, TNodeConvFunc> 
       func(node_conv);

    const TSrcTreeNodeType *n = bio_tree.GetTreeNode();

    CTree2TreeFunc<TDstTreeNodeType, TSrcTreeNodeType, TNodeConvFunc> 
        rfunc(
           TreeDepthFirstTraverse(*(const_cast<TSrcTreeNodeType*>(n)), func)
        );

    dyn_tree.SetTreeNode(rfunc.GetTreeNode());
}




/// Convert CTreeNode<> to dynamic tree using a node converter
///
template<class TDynamicTree, class TTreeNode, class TNodeConvFunc>
void TreeConvert2Dynamic(TDynamicTree&      dyn_tree, 
                         const TTreeNode*   src_tree,
                         TNodeConvFunc      node_conv)
{
    dyn_tree.Clear();

    typedef TTreeNode    TSrcTreeNodeType;
    typedef typename TDynamicTree::TBioTreeNode   TDstTreeNodeType;

    CTree2TreeFunc<TDstTreeNodeType, TSrcTreeNodeType, TNodeConvFunc> 
       func(node_conv);
    CTree2TreeFunc<TDstTreeNodeType, TSrcTreeNodeType, TNodeConvFunc> 
        rfunc(
          TreeDepthFirstTraverse(*(const_cast<TTreeNode*>(src_tree)), func)
        );
    dyn_tree.SetTreeNode(rfunc.GetTreeNode());
}



/// Convert dynamic tree to CTreeNode<>, 
/// returned CTReeNode<> to be deleted by caller.
///
template<class TDynamicTree, class TTreeNode, class TNodeConvFunc>
TTreeNode* DynamicConvert2Tree(TDynamicTree&      dyn_tree, 
                               TNodeConvFunc      node_conv,
                               TTreeNode*&        dst_node)
{
    typedef TTreeNode    TDstTreeNodeType;
    typedef typename TDynamicTree::TBioTreeNode   TSrcTreeNodeType;

    CTree2TreeFunc<TDstTreeNodeType, TSrcTreeNodeType, TNodeConvFunc> 
       func(node_conv);

    const TSrcTreeNodeType *n = dyn_tree.GetTreeNode();

    CTree2TreeFunc<TDstTreeNodeType, TSrcTreeNodeType, TNodeConvFunc> 
        rfunc(
        TreeDepthFirstTraverse(*(const_cast<TSrcTreeNodeType*>(n)), func)
        );
    return (dst_node = rfunc.GetTreeNode());
}





// --------------------------------------------------------------------------

/// Visitor functor to convert dynamic tree nodes to ASN.1 BioTree container
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

        const TDynamicNodeType* node_parent = 
                        (TDynamicNodeType*) node.GetParent();
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
    const typename TContainerDict::Tdata& feat_list = fd.Get();

    ITERATE(typename TContainerDict::Tdata, it, feat_list) {
        TBioTreeFeatureId fid = (*it)->GetId();
        const string& fvalue = (*it)->GetName();
		
		dict.Register(fid, fvalue);
    }

	// convert tree data (nodes)
    typedef typename TBioTreeContainer::TNodes            TCNodeSet;
    typedef typename TCNodeSet::Tdata                     TNodeList;
    typedef typename TNodeList::value_type::element_type  TCNode;

    const TNodeList node_list = tree_container.GetNodes().Get();

    std::map<TBioTreeNodeId, typename TDynamicTree::TBioTreeNode*> pmap;

	ITERATE(typename TNodeList, it, node_list) {

		const CRef<TCNode>& cnode = *it;

        TBioTreeNodeId uid = cnode->GetId();

	    typedef typename TDynamicTree::TBioTreeNode      TDynamicNodeType;
		typedef typename TDynamicNodeType::TValueType    TDynamicNodeValueType;

		TDynamicNodeValueType v;
    
		typedef typename TCNode::TFeatures               TCNodeFeatureSet;

		if (cnode->CanGetFeatures()) {
			const TCNodeFeatureSet& fset = cnode->GetFeatures();

			const typename TCNodeFeatureSet::Tdata& flist = fset.Get();

			ITERATE(typename TCNodeFeatureSet::Tdata, fit, flist) {
				unsigned int fid = (*fit)->GetFeatureid();
				const string& fvalue = (*fit)->GetValue();

				v.features.SetFeature(fid, fvalue);

			} // ITERATE 

		}

		if (cnode->CanGetParent()) {
	        TBioTreeNodeId parent_id = cnode->GetParent();
            typename TDynamicTree::TBioTreeNode* node = NULL;
			
            typename TDynamicTree::TBioTreeNode* parent_node = pmap[parent_id];
            if (parent_node != NULL) {              
                node = dyn_tree.AddNode(v, parent_node);
                dyn_tree.SetNodeId(node);
            }
            else {
                NCBI_THROW(CException, eUnknown, "Parent not found");
            }
                      
            pmap[uid] = node;            
		} else {
			TDynamicNodeType* dnode = new TDynamicNodeType(v);
			dyn_tree.SetTreeNode(dnode);
			dyn_tree.SetNodeId(dnode);
			pmap[uid] = dnode;            
		}


	} // ITERATE TNodeList
}

/// Convert forest of Dynamic trees to ASN.1 BioTree container
///
template<class TBioTreeContainer, class TDynamicForest>
void BioTreeForestConvert2Container(TBioTreeContainer&      tree_container,
                                    const TDynamicForest&   dyn_forest)
{
    // Convert feature dictionary

    typedef typename TBioTreeContainer::TFdict  TContainerDict;

    const CBioTreeFeatureDictionary& dict = dyn_forest.GetFeatureDict();
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
    typedef typename TDynamicForest::TBioTree::TBioTreeNode TTreeNode;
    CBioTreeConvert2ContainerFunc<TBioTreeContainer, typename TDynamicForest::TBioTree>
        func(&tree_container);

    for (unsigned int i=0; i<dyn_forest.GetTrees().size(); ++i) {
        const TTreeNode *n = dyn_forest.GetTrees()[i]->GetTreeNode();
        TreeDepthFirstTraverse(*(const_cast<TTreeNode*>(n)), func);
    }
}

/// Convert ASN.1 BioTree container to forest of dynamic trees
///
// Assume that the data in the container is sorted such that the nodes of trees
// are not interleaved with nodes of other trees, and that all nodes within a
// tree are sorted such that the parent of every node comes before the node itself.
template<class TBioTreeContainer, class TDynamicForest>
void BioTreeConvertContainer2DynamicForest(TDynamicForest&           dyn_forest,
		                                   const TBioTreeContainer&  tree_container)
{
	dyn_forest.Clear();
	
    // Convert feature dictionary

    typedef typename TBioTreeContainer::TFdict  TContainerDict;

    CBioTreeFeatureDictionary& dict = dyn_forest.GetFeatureDict();
    const TContainerDict& fd = tree_container.GetFdict();
    const typename TContainerDict::Tdata& feat_list = fd.Get();

    ITERATE(typename TContainerDict::Tdata, it, feat_list) {
        TBioTreeFeatureId fid = (*it)->GetId();
        const string& fvalue = (*it)->GetName();
		
		dict.Register(fid, fvalue);
    }

	// convert tree data (nodes)
    typedef typename TBioTreeContainer::TNodes            TCNodeSet;
    typedef typename TCNodeSet::Tdata                     TNodeList;
    typedef typename TNodeList::value_type::element_type  TCNode;

    const TNodeList node_list = tree_container.GetNodes().Get();

    typename TDynamicForest::TBioTree* current_tree = NULL;

	ITERATE(typename TNodeList, it, node_list) {

		const CRef<TCNode>& cnode = *it;

        TBioTreeNodeId uid = cnode->GetId();

        typedef typename TDynamicForest::TBioTree        TDynamicTree;
	    typedef typename TDynamicTree::TBioTreeNode      TDynamicNodeType;
		typedef typename TDynamicNodeType::TValueType    TDynamicNodeValueType;

		TDynamicNodeValueType v;
		v.SetId(uid);
    
		typedef typename TCNode::TFeatures               TCNodeFeatureSet;

		if (cnode->CanGetFeatures()) {
			const TCNodeFeatureSet& fset = cnode->GetFeatures();

			const typename TCNodeFeatureSet::Tdata& flist = fset.Get();

			ITERATE(typename TCNodeFeatureSet::Tdata, fit, flist) {
				unsigned int fid = (*fit)->GetFeatureid();
				const string& fvalue = (*fit)->GetValue();

				v.features.SetFeature(fid, fvalue);

			} // ITERATE 

		}

		if (cnode->CanGetParent()) {
            if (current_tree != NULL) {
	            TBioTreeNodeId parent_id = cnode->GetParent();
			    current_tree->AddNode(v, parent_id);
            }
            else {
                // throw exception?
            }
		} else {
            // This should be the root no   de in a new tree:
            current_tree = new typename TDynamicForest::TBioTree();
            dyn_forest.AddTree(current_tree);

			TDynamicNodeType* dnode = new TDynamicNodeType(v);
			current_tree->SetTreeNode(dnode);
		}


	} // ITERATE TNodeList
}

// --------------------------------------------------------------------------

/// Feature ids for Bio-Tree
///
enum ETaxon1ConvFeatures
{
    eTaxTree_Name         = 1,
    eTaxTree_BlastName    = 2,
    eTaxTree_Rank         = 3,
    eTaxTree_Division     = 4,
    eTaxTree_GC           = 5,
    eTaxTree_MGC          = 6,
    eTaxTree_IsUncultured = 7,
    eTaxTree_TaxId        = 8,
    eTaxTree_SeqId        = 9,
    eTaxTree_Label        = 10
};


/// Taxon1 tree visitor functor. Converts taxonomy into dynamic tree.
///
/// @internal
///
template<class TITaxon4Each,    class TITaxon1Node, 
         class TITreeIterator,  class TBioTreeContainer>
class CTaxon1NodeConvertVisitor : public TITaxon4Each   
{
public:
	typedef typename TITreeIterator::EAction  EAction;

    typedef typename TBioTreeContainer::TNodes           TCNodeSet;
    typedef typename TCNodeSet::Tdata                    TNodeList;
    typedef typename TNodeList::value_type::element_type TCNode;
    typedef typename TCNode::TFeatures                   TCNodeFeatureSet;
    typedef typename TCNodeFeatureSet::Tdata             TNodeFeatureList;
    typedef typename 
       TNodeFeatureList::value_type::element_type        TCNodeFeature;

public:
	CTaxon1NodeConvertVisitor(TBioTreeContainer*  tree_container)
	: m_TreeContainer(tree_container),
      m_MaxUID(0)
	{
        m_NodeList = &(tree_container->SetNodes().Set());
	}
	
	virtual ~CTaxon1NodeConvertVisitor() {}

	virtual 
	EAction Execute(const TITaxon1Node* pNode)
	{

        TBioTreeNodeId uid = pNode->GetTaxId();

        CRef<TCNode> cnode(new TCNode);
        cnode->SetId(uid);

        if (uid > (unsigned int)m_MaxUID) { // new tree node id max?
            m_MaxUID = (int)uid;
        }

		vector<int>::size_type psize = m_Parents.size();
		if (psize != 0) {
			int parent_tax_id = m_Parents[psize - 1];
			cnode->SetParent(parent_tax_id);
		}


        TCNodeFeatureSet& fset = cnode->SetFeatures();

		// Convert features

		// Name
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(eTaxTree_Name);
			cfeat->SetValue(pNode->GetName());

            fset.Set().push_back(cfeat);
		}}
		// Label
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(eTaxTree_Label);
			cfeat->SetValue(pNode->GetName());

            fset.Set().push_back(cfeat);
		}}

		// Blast_name
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(eTaxTree_BlastName);
			cfeat->SetValue(pNode->GetBlastName());

            fset.Set().push_back(cfeat);
		}}

		// Rank
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(eTaxTree_Rank);
			int v = pNode->GetRank();
			cfeat->SetValue(NStr::IntToString(v));

            fset.Set().push_back(cfeat);
		}}

		// Division
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(eTaxTree_Division);
			int v = pNode->GetDivision();
			cfeat->SetValue(NStr::IntToString(v));

            fset.Set().push_back(cfeat);
		}}

		// Genetic code
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(eTaxTree_GC);
			int v = pNode->GetGC();
			cfeat->SetValue(NStr::IntToString(v));

            fset.Set().push_back(cfeat);
		}}

		// Mitocondrial genetic code
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(eTaxTree_MGC);
			int v = pNode->GetMGC();
			cfeat->SetValue(NStr::IntToString(v));

            fset.Set().push_back(cfeat);
		}}

		// Uncultured
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(eTaxTree_IsUncultured);
			int v = pNode->IsUncultured();
			cfeat->SetValue(NStr::IntToString(v));

            fset.Set().push_back(cfeat);
		}}
		// tax-id
		{{
			CRef<TCNodeFeature>  cfeat(new TCNodeFeature);
			cfeat->SetFeatureid(eTaxTree_TaxId);
			cfeat->SetValue(NStr::IntToString(uid));

            fset.Set().push_back(cfeat);
		}}

        m_NodeList->push_back(cnode);

		return TITreeIterator::eOk;
	}
	
	virtual 
	EAction LevelBegin(const TITaxon1Node* pParent)
	{
		m_Parents.push_back(pParent->GetTaxId());
		return TITreeIterator::eOk; 
	}
	
	virtual 
	EAction LevelEnd(const TITaxon1Node* /*pParent*/)
	{ 
		m_Parents.pop_back();
		return TITreeIterator::eOk; 
	}

    int GetMaxNodeId() const
    {
        return m_MaxUID;
    }
private:
	TBioTreeContainer*  m_TreeContainer;
    TNodeList*          m_NodeList;
	vector<int>         m_Parents; //<! Stack of parent tax ids
    int                 m_MaxUID;  //<! Maximum node ID
};


template<class TBioTreeContainer>
void BioTreeAddFeatureToDictionary(TBioTreeContainer&  tree_container,
								  unsigned int        feature_id,
								  const string&       feature_name)
{
    typedef typename TBioTreeContainer::TFdict  TContainerDict;
	typedef typename TContainerDict::Tdata::value_type::element_type TFeatureDescr;

    TContainerDict& fd = tree_container.SetFdict();
    typename TContainerDict::Tdata& feat_list = fd.Set();

    // Don't add duplicate ids:
    ITERATE(typename TContainerDict::Tdata, it, feat_list) {
        if ( (*it)->GetId()==feature_id )
            return;
    }

    CRef<TFeatureDescr> d(new TFeatureDescr);
    d->SetId(feature_id);
    d->SetName(feature_name);

    feat_list.push_back(d);
}



template<class TBioTreeContainer, 
	 class TTaxon1, class TITaxon1Node, class TITreeIterator>
class CTaxon1ConvertToBioTreeContainer
{
public:
   typedef typename TITreeIterator::I4Each T4Each;
   typedef  CTaxon1NodeConvertVisitor<T4Each, TITaxon1Node,
                                      TITreeIterator, TBioTreeContainer>
                                      TTaxon1Visitor;

public:

  CTaxon1ConvertToBioTreeContainer()
    : m_MaxNodeId(0)
  {}

  void operator()(TBioTreeContainer&  tree_container,
		  TTaxon1&            tax, 
		  int                 tax_id)
  {
         SetupFeatureDictionary(tree_container);

        // Convert nodes    
        const TITaxon1Node* tax_node=0;
        bool res = tax.LoadSubtree(tax_id, &tax_node);
        if (res) {
            CRef<TITreeIterator> tax_tree_iter(tax.GetTreeIterator());
            tax_tree_iter->GoNode(tax_node);
            TTaxon1Visitor tax_vis(&tree_container);
            tax_tree_iter->TraverseDownward(tax_vis);
            m_MaxNodeId = tax_vis.GetMaxNodeId();
        }    
  }
  
  void operator()(TBioTreeContainer&  tree_container,
		  CRef<TITreeIterator> tax_tree_iter)
  {
        SetupFeatureDictionary(tree_container);
            
        // Convert nodes    
        tax_tree_iter->GoRoot();
        TTaxon1Visitor tax_vis(&tree_container);
        tax_tree_iter->TraverseDownward(tax_vis);
        m_MaxNodeId = tax_vis.GetMaxNodeId();
  }

  /// Get max node id (available after conversion)
  int GetMaxNodeId() const
  {
        return m_MaxNodeId;
  }
protected:
  /// Add elements to the feature dictionary
  ///
  void SetupFeatureDictionary(TBioTreeContainer&  tree_container)
  {
        BioTreeAddFeatureToDictionary(tree_container, eTaxTree_Name, "name");
        BioTreeAddFeatureToDictionary(tree_container, eTaxTree_BlastName, "blast_name");
        BioTreeAddFeatureToDictionary(tree_container, eTaxTree_Rank, "rank");
        BioTreeAddFeatureToDictionary(tree_container, eTaxTree_Division, "division");
        BioTreeAddFeatureToDictionary(tree_container, eTaxTree_GC, "GC");
        BioTreeAddFeatureToDictionary(tree_container, eTaxTree_MGC, "MGC");
        BioTreeAddFeatureToDictionary(tree_container, eTaxTree_IsUncultured, "IsUncultured");
        BioTreeAddFeatureToDictionary(tree_container, eTaxTree_TaxId, "tax-id");
        BioTreeAddFeatureToDictionary(tree_container, eTaxTree_SeqId, "seq-id");
        BioTreeAddFeatureToDictionary(tree_container, eTaxTree_Label, "label");
  }
private:
    int m_MaxNodeId;
};

/// Function to determine tree if a given biotree container
/// is a single tree or a forest.
///
/// @internal
template<class TBioTreeContainer>
bool BioTreeContainerIsForest(const TBioTreeContainer&  tree_container)
{
    // Definition of a tree  : exactly one node has no parent node (is a root).
    // Definition of a forest: more than one node has no parent node (multiple roots).

    typedef typename TBioTreeContainer::TNodes            TCNodeSet;
    typedef typename TCNodeSet::Tdata                     TNodeList;
    typedef typename TNodeList::value_type::element_type  TCNode;

    const TNodeList node_list = tree_container.GetNodes().Get();

    int number_roots = 0;

	ITERATE(typename TNodeList, it, node_list) {

		const CRef<TCNode>& cnode = *it;

        if (!cnode->CanGetParent())
            ++number_roots;

	} // ITERATE TNodeList

    return (number_roots > 1);
}
/* @} */



END_NCBI_SCOPE

#endif
