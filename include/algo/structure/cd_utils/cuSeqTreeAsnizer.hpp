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
class NCBI_CDUTILS_EXPORT SeqTreeAsnizer
{
public:
	static bool convertToTreeOption(const CRef< CAlgorithm_type>&  alg, TreeOptions& treeOption);
	static bool convertToAlgType(const TreeOptions& options, CRef< CAlgorithm_type >& alg);

    //  The first version fills in footprint and child membership fields in the asnSeqTree
    //  from the data in the AlignmentCollection.  The second version, simply constructs
    //  a CSequence_tree with the topology of the seqTree that contains dummy footprint information
    //  and no child membership data (Seq-ids for leaf nodes are of type e_Gi if the parsed name is 
    //  numeric, or type e_Local if the parsed name is non-numeric.
	static bool convertToAsnSeqTree(const AlignmentCollection& ac, const SeqTree& seqTree, CSequence_tree& asnSeqTree);
	static bool convertToAsnSeqTree(const SeqTree& seqTree, CSequence_tree& asnSeqTree);

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

	static bool addAsnSeqTreeNode(const SeqTree& seqTree, SeqTreeIterator cursor, CSeqTree_node& asnNode);
	static void fillAsnSeqTreeNode(const SeqTreeIterator& cursor, CSeqTree_node& asnNode);

};

END_SCOPE(cd_utils)
END_NCBI_SCOPE
#endif

