#ifndef CU_SEQTREE_ASNIZER_HPP
#define CU_SEQTREE_ASNIZER_HPP

#include <algo/structure/cd_utils/cuSeqtree.hpp>
#include <algo/structure/cd_utils/cuSeqTreeFactory.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

struct TreeNodePair
{
	TreeNodePair(SeqTreeIterator sit, CSeqTree_node* casnNode) :
		it(sit), asnNode(casnNode){}
	SeqTreeIterator it;
	CSeqTree_node* asnNode;
};

typedef map<const CSeq_interval*, TreeNodePair> SeqLocToSeqItemMap;
class SeqTreeAsnizer
{
public:
	static bool convertToTreeOption(const CRef< CAlgorithm_type>&  alg, TreeOptions& treeOption);
	static bool convertToAlgType(const TreeOptions& options, CRef< CAlgorithm_type >& alg);
	static bool convertToAsnSeqTree(const AlignmentCollection& ac, const SeqTree& seqTree, CSequence_tree& asnSeqTree);
	static bool convertToSeqTree(CSequence_tree& asnSeqTree, SeqTree& seqTree, SeqLocToSeqItemMap& liMap);
	static bool addAsnSeqTreeNode(const AlignmentCollection& ac, const SeqTree& seqTree, SeqTreeIterator cursor, CSeqTree_node& asnNode);
	static bool addChildNode(SeqTree& seqTree, SeqTreeIterator parentNode, CSeqTree_node& asnNode, SeqLocToSeqItemMap& liMap);
	static void fillAsnSeqTreeNode(const AlignmentCollection& ac, const SeqTreeIterator& cursor, CSeqTree_node& asnNode);
	static void fillSeqItem(const CSeqTree_node& node, SeqItem& seqItem);

	static bool resolveRowId(const AlignmentCollection& ac, SeqLocToSeqItemMap& liMap);
	static bool refillAsnMembership(const AlignmentCollection& ac, SeqLocToSeqItemMap& liMap);
	//serialize
	static bool readAlgType(CNcbiIstream& is, CRef< CAlgorithm_type >& alg);
	static bool writeAlgType(CNcbiOstream& os, const CRef< CAlgorithm_type >& alg);

	/*
	static bool readAsnSeqTree(const CNcbiIstream& is, CRef< CSequence_tree >& alg);
	static bool writeAlgType(CNcbiOstream& os, const CRef< CSequence_tree >& alg);
	*/
private:
		SeqTreeAsnizer(){};
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE
#endif

