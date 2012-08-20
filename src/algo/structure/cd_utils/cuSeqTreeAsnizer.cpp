#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuSeqTreeAsnizer.hpp>
#include <algo/structure/cd_utils/cuDistmat.hpp>
#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <objects/seqloc/Seq_interval.hpp>

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

	treeOption.scope = (CAlgorithm_type::ETree_scope)((alg->IsSetTree_scope()) ? 
		alg->GetTree_scope() : CAlgorithm_type::eTree_scope_allDescendants);
	treeOption.coloringScope = (CAlgorithm_type::EColoring_scope)((alg->IsSetColoring_scope()) ?
		alg->GetColoring_scope() : CAlgorithm_type::eColoring_scope_allDescendants);
    treeOption.matrix = (EScoreMatrixType)(
		(alg->IsSetScore_Matrix()) ? alg->GetScore_Matrix(): CAlgorithm_type::eScore_Matrix_blosum62);

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
	alg->SetTree_scope(treeOption.scope);
	alg->SetColoring_scope(treeOption.coloringScope);
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
		//child memship
		CCdCore* cd = ac.GetScopedLeafCD(cursor->rowID);
        if (cd ) 
		{
			asnNode.SetAnnotation().SetPresentInChildCD(cd->GetAccession());
		}
	}
}

bool SeqTreeAsnizer::convertToSeqTree(CSequence_tree & asnSeqTree, SeqTree& seqTree, SeqLocToSeqItemMap& liMap)
{	
	if (!asnSeqTree.IsSetRoot())
		return false;
	CSeqTree_node& root = asnSeqTree.SetRoot();
	//add the node self
	SeqItem sItem;
	fillSeqItem(root, sItem);
	SeqTreeIterator rootIterator = seqTree.insert(seqTree.begin(), sItem);
	//add its children
	if (root.CanGetChildren())
	{
		if (root.GetChildren().IsChildren())
		{
			list< CRef< CSeqTree_node > >& children = root.SetChildren().SetChildren();
			list< CRef< CSeqTree_node > >::iterator lcit = children.begin();
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
		/* can't use what's stored because the child accession may be something like "loc-1"*/
		if (node.CanGetAnnotation())
		{
			seqItem.membership = node.GetAnnotation().GetPresentInChildCD();
		}
		seqItem.seqId = &(node.GetChildren().GetFootprint().GetSeqRange().GetId());
	}
}

//recursive
bool SeqTreeAsnizer::addChildNode(SeqTree& seqTree, SeqTreeIterator parentNode, 
								  CSeqTree_node& asnNode, SeqLocToSeqItemMap& liMap)
{
	SeqItem sItem;
	fillSeqItem(asnNode, sItem);
	SeqTreeIterator np = seqTree.append_child(parentNode, sItem);
	if (asnNode.CanGetChildren())
	{
		if (asnNode.GetChildren().IsChildren())
		{
			list< CRef< CSeqTree_node > >& children = asnNode.SetChildren().SetChildren();
			list< CRef< CSeqTree_node > >::iterator lcit = children.begin();
			for(; lcit != children.end(); lcit++)
			{
				addChildNode(seqTree, np, **lcit, liMap);
			}
		}
		else //it is Footprint Node
		{
			TreeNodePair tnp(np, &asnNode);
			liMap.insert(SeqLocToSeqItemMap::value_type(
				&(asnNode.GetChildren().GetFootprint().GetSeqRange()), tnp));
		}
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
		sit->second.it->rowID = row;
		CCdCore* cd = ac.GetScopedLeafCD(row);
		if (cd)
			sit->second.it->membership = cd->GetAccession();
	}
	return true;
}

bool SeqTreeAsnizer::refillAsnMembership(const AlignmentCollection& ac, SeqLocToSeqItemMap& liMap)
{
	SeqLocToSeqItemMap::iterator sit = liMap.begin();
	for (; sit != liMap.end(); sit++)
	{
		int row = sit->second.it->rowID;
		CCdCore* cd = ac.GetScopedLeafCD(row);
		if (cd)
			sit->second.asnNode->SetAnnotation().SetPresentInChildCD(cd->GetAccession());
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

bool SeqTreeAsnizer::convertToAsnSeqTree(const SeqTree& seqTree, CSequence_tree& asnSeqTree)
{
	if (seqTree.begin() == seqTree.end())
		return false;
	asnSeqTree.ResetRoot();
	CSeqTree_node& asnNode = asnSeqTree.SetRoot();
	fillAsnSeqTreeNode(seqTree.begin(), asnNode);
	SeqTree::sibling_iterator sit = (seqTree.begin()).begin();
	while (sit != (seqTree.begin()).end())
	{
		addAsnSeqTreeNode(seqTree, sit, asnNode);
		++sit;
	}
	return true;
}

bool SeqTreeAsnizer::addAsnSeqTreeNode(const SeqTree& seqTree, SeqTreeIterator cursor, CSeqTree_node& asnNode)
{
	// add cursor as a child of asnNode
	list< CRef< CSeqTree_node > >& childList = asnNode.SetChildren().SetChildren();
	CRef< CSeqTree_node >  nodeRef(new CSeqTree_node);
	fillAsnSeqTreeNode(cursor, nodeRef.GetObject());
	childList.push_back(nodeRef);

	//add the children of cursor as children of  nodeRef
	SeqTree::sibling_iterator sit = cursor.begin();
	while (sit != cursor.end())
	{
		addAsnSeqTreeNode(seqTree, sit, nodeRef.GetObject());
		++sit;
	}
	return true;
}

void SeqTreeAsnizer::fillAsnSeqTreeNode(const SeqTreeIterator& cursor, CSeqTree_node& asnNode)
{
	asnNode.SetName(cursor->name);
	asnNode.SetDistance(cursor->distance);
	CSeqTree_node::C_Children& child = asnNode.SetChildren();
	if (cursor.number_of_children() == 0)
	{
		CSeqTree_node::C_Children::C_Footprint& fp = child.SetFootprint();
		CSeq_interval& range = fp.SetSeqRange(); //to be filled up later
		CSeq_id& seqId = range.SetId();
        int gi = NStr::StringToInt(cursor->name, NStr::fConvErr_NoThrow);
        if (gi > 0) {
            seqId.SetGi(gi);
        } else {
            seqId.SetLocal().SetStr(cursor->name);
        }
//		CRef<CSeq_id> seqIdRef;
//		ac.GetSeqIDForRow(cursor->rowID, seqIdRef);
//		seqId.Assign(*seqIdRef);
		range.SetFrom(0);
		range.SetTo(1);
		fp.SetRowId(cursor->rowID);
	}
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
