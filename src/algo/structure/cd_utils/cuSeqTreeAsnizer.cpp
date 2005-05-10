#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuSeqTreeAsnizer.hpp>
#include <algo/structure/cd_utils/cuDistmat.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)
bool SeqTreeAsnizer::convertToTreeOption(const CRef< CAlgorithm_type >&  alg, TreeOptions& treeOption)
{
	//treeOption.alignUsage = CCd::USE_NORMAL_ALIGNMENT;
	treeOption.cTermExt = alg->GetCTerminalExt();
	treeOption.nTermExt = alg->GetNTerminalExt();
	treeOption.clusteringMethod = (ETreeMethod)(alg->GetClustering_Method());

    // bug fix. dih.  eScoring_Scheme_aligned_score -> eScoring_Scheme_aligned_score_ext
    treeOption.distMethod = (EDistMethod)((alg->GetScoring_Scheme() >  CAlgorithm_type::eScoring_Scheme_aligned_score_ext) ?
        alg->GetScoring_Scheme() - 1 : alg->GetScoring_Scheme());

	//treeOption.forFamily = true;
	
	treeOption.matrix = (EScoreMatrixType)(alg->GetScore_Matrix());

    

	/* the difference between ASN spec and treeOption
			eScoring_Scheme_percent_id = 1,
	        eScoring_Scheme_kimura_corrected = 2,
	        eScoring_Scheme_aligned_score = 3,
	        eScoring_Scheme_aligned_score_ext = 4,
	        eScoring_Scheme_aligned_score_filled = 5,
	        eScoring_Scheme_blast_footprint = 6,
	        eScoring_Scheme_blast_full = 7,

			ePercentIdentity,
			ePercIdWithKimura,
			eScoreAligned,
			eScoreAlignedOptimal,
			eScoreBlastFoot,
			eScoreBlastFull

			enum EScore_Matrix {
          eScore_Matrix_unassigned = 0,
          eScore_Matrix_blosum45 = 1,
          eScore_Matrix_blosum62 = 2,
          eScore_Matrix_blosum80 = 3,
          eScore_Matrix_pam30 = 4,
          eScore_Matrix_pam70 = 5,
          eScore_Matrix_pam250 = 6,
          eScore_Matrix_other = 255
		};

		enum EScoreMatrixType {
    eInvalidMatrixType   = 0,
    eBlosum45  = 1,
    eBlosum62  = 2,
    eBlosum80  = 3,
    ePam30     = 4,
    ePam70     = 5,
    ePam250    = 6
};
	*/
	return true;
}

bool SeqTreeAsnizer::convertToAlgType(const TreeOptions& treeOption, CRef< CAlgorithm_type >& alg)
{
	
	alg->SetCTerminalExt(treeOption.cTermExt) ;
	alg->SetNTerminalExt(treeOption.nTermExt);
	alg->SetClustering_Method(treeOption.clusteringMethod);
	int dm = treeOption.distMethod;
	if (dm == eScoreAligned)
	{
		if (treeOption.cTermExt != 0 || treeOption.nTermExt != 0)
			alg->SetScoring_Scheme(dm + 1);
		else
			alg->SetScoring_Scheme(dm);
	}
	else if (dm > eScoreAligned)
		alg->SetScoring_Scheme(dm + 1);
	else
		alg->SetScoring_Scheme(dm);

	alg->SetScore_Matrix(treeOption.matrix);
	return true;
}

bool SeqTreeAsnizer::convertToAsnSeqTree(const AlignmentCollection& ac, const SeqTree& seqTree, CSequence_tree& asnSeqTree)
{
	if (seqTree.begin() == seqTree.end())
		return false;
	asnSeqTree.ResetRoot();
	CSeqTree_node& asnNode = asnSeqTree.SetRoot();
	fillAsnSeqTreeNode(ac, seqTree.begin(), asnNode);
	SeqTree::sibling_iterator sit = (seqTree.begin()).begin();
	while (sit != (seqTree.begin()).end())
	{
		addAsnSeqTreeNode(ac, seqTree, sit, asnNode);
		++sit;
	}
	return true;
}

bool SeqTreeAsnizer::addAsnSeqTreeNode(const AlignmentCollection& ac, const SeqTree& seqTree, SeqTreeIterator cursor, CSeqTree_node& asnNode)
{
	// add cursor as a child of asnNode
	list< CRef< CSeqTree_node > >& childList = asnNode.SetChildren().SetChildren();
	CRef< CSeqTree_node >  nodeRef(new CSeqTree_node);
	fillAsnSeqTreeNode(ac, cursor, nodeRef.GetObject());
	childList.push_back(nodeRef);

	//add the children of cursor as children of  nodeRef
	SeqTree::sibling_iterator sit = cursor.begin();
	while (sit != cursor.end())
	{
		addAsnSeqTreeNode(ac, seqTree, sit, nodeRef.GetObject());
		++sit;
	}
	return true;
}

void SeqTreeAsnizer::fillAsnSeqTreeNode(const AlignmentCollection& ac, const SeqTreeIterator& cursor, CSeqTree_node& asnNode)
{
	asnNode.SetName(cursor->name);
	asnNode.SetDistance(cursor->distance);
	CSeqTree_node::C_Children& child = asnNode.SetChildren();
	if (cursor.number_of_children() == 0)
	{
		CSeqTree_node::C_Children::C_Footprint& fp = child.SetFootprint();
		CSeq_interval& range = fp.SetSeqRange(); //to be filled up later
		CSeq_id& seqId = range.SetId();
		CRef<CSeq_id> seqIdRef;
		ac.GetSeqIDForRow(cursor->rowID, seqIdRef);
		seqId.Assign(*seqIdRef);
		range.SetFrom(ac.GetLowerBound(cursor->rowID));
		range.SetTo(ac.GetUpperBound(cursor->rowID));
		fp.SetRowId(cursor->rowID);
	}
}

bool SeqTreeAsnizer::convertToSeqTree(const CSequence_tree & asnSeqTree, SeqTree& seqTree, SeqLocToSeqItemMap& liMap)
{	
	if (!asnSeqTree.IsSetRoot())
		return false;
	const CSeqTree_node& root = asnSeqTree.GetRoot();
	//add the node self
	SeqItem sItem;
	fillSeqItem(root, sItem);
	SeqTreeIterator rootIterator = seqTree.insert(seqTree.begin(), sItem);
	//add its children
	if (root.CanGetChildren())
	{
		if (root.GetChildren().IsChildren())
		{
			const list< CRef< CSeqTree_node > >& children = root.GetChildren().GetChildren();
			list< CRef< CSeqTree_node > >::const_iterator lcit = children.begin();
			for(; lcit != children.end(); lcit++)
			{
				addChildNode(seqTree, rootIterator, **lcit, liMap);
			}
		}
		else
			return false;
	}
	else
		return false;
	return true;
}

void SeqTreeAsnizer::fillSeqItem(const CSeqTree_node& node, SeqItem& seqItem)
{
	seqItem.distance = node.GetDistance();
	seqItem.name = node.GetName();
	if (node.GetChildren().IsFootprint())
	{
		//use rowID for now
		if (node.GetChildren().GetFootprint().IsSetRowId())
			seqItem.rowID = node.GetChildren().GetFootprint().GetRowId();
		//keep a map of seqLoc and seqItem so it can be used to resolve rowId later.
		
		/*liMap.insert(SeqLocToSeqItemMap::value_type(&(node.GetChildren().GetFootprint().GetSeqRange()),
			&seqItem));*/
	}
}

//recursive
bool SeqTreeAsnizer::addChildNode(SeqTree& seqTree, SeqTreeIterator parentNode, 
								  const CSeqTree_node& asnNode, SeqLocToSeqItemMap& liMap)
{
	SeqItem sItem;
	fillSeqItem(asnNode, sItem);
	SeqTreeIterator np = seqTree.append_child(parentNode, sItem);
	if (asnNode.CanGetChildren())
	{
		if (asnNode.GetChildren().IsChildren())
		{
			const list< CRef< CSeqTree_node > >& children = asnNode.GetChildren().GetChildren();
			list< CRef< CSeqTree_node > >::const_iterator lcit = children.begin();
			for(; lcit != children.end(); lcit++)
			{
				addChildNode(seqTree, np, **lcit, liMap);
			}
		}
		else //it is Footprint Node
			liMap.insert(SeqLocToSeqItemMap::value_type(
				&(asnNode.GetChildren().GetFootprint().GetSeqRange()), np));
	}
	return true;
}

bool SeqTreeAsnizer::resolveRowId(const AlignmentCollection& ac, SeqLocToSeqItemMap& liMap)
{
	SeqLocToSeqItemMap::iterator sit = liMap.begin();
	for (; sit != liMap.end(); sit++)
	{
		int row = ac.FindSeqInterval(*(sit->first));
		if (row < 0)
			return false;
		sit->second->rowID = row;
	}
	return true;
}

bool SeqTreeAsnizer::readAlgType(CNcbiIstream& is, CRef< CAlgorithm_type >& alg)
{
	string err;
	return ReadASNFromStream(is, &(*alg), false, &err);
}

bool SeqTreeAsnizer::writeAlgType(CNcbiOstream& os, const CRef< CAlgorithm_type >& alg)
{
	string err;
	return WriteASNToStream(os, *alg, false, &err);
}

/*
bool readAsnSeqTree(const CNcbiIStream& is, CRef< CSequence_tree >& alg)
{

}

bool SeqTreeAsnizer::writeAlgType(CNcbiOStream& os, const CRef< CSequence_tree >& alg)
{

}*/

END_SCOPE(cd_utils)
END_NCBI_SCOPE