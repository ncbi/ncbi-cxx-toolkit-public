/* $Id$
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
 * Author:  Charlie Liu
 *
 * File Description:
 *
 *       A simple API layer to access Seqtree and its layout
 *
 * ===========================================================================
 */

#ifndef CU_SEQTREE_API_HPP
#define CU_SEQTREE_API_HPP

#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <algo/structure/cd_utils/cuTaxTree.hpp>
#include <algo/structure/cd_utils/cuSeqtree.hpp>
#include <algo/structure/cd_utils/cuSeqTreeFactory.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils) // namespace ncbi::objects::


struct SeqTreeNode
{
	string name; //gi or PDB for a leaf node; empty for internal node
	int x;
	int y;
	bool isLeaf;
	string annotation;
};
typedef pair<SeqTreeNode, SeqTreeNode> SeqTreeEdge;

class SeqTreeAPI
{
public:
	SeqTreeAPI(vector<CCdCore*>& cds, bool loadExistingTreeOnly=true);
	~SeqTreeAPI();

	void annotateTreeByMembership();

	enum TaxonomyLevel
	{
		BySuperkingdom,
		ByKingdom/*,
		ByBlink*/
	};
	void annotateTreeByTaxonomy(TaxonomyLevel tax);

	int getNumOfLeaves();
	//return a string of tree method names
	//lay out the tree to the specified area, .i.e. with the "fit to screen" style
	string layoutSeqTree(int maxX, int maxY, vector<SeqTreeEdge>& edgs);
	//lay out the tree with a fixed spacing between sequences
	string layoutSeqTree(int maxX, vector<SeqTreeEdge>& edgs, int yInt = 3);
	CCdCore* getRootCD(){return m_ma.getFirstCD();}
	
	static bool loadAndValidateExistingTree(MultipleAlignment& ma, TreeOptions* treeOptions=0, SeqTree* seqTree=0);
private:
	MultipleAlignment m_ma;
	SeqTree* m_seqTree;
	TaxTreeData* m_taxTree;
	bool m_useMembership;
	TaxonomyLevel m_taxLevel;
	TreeOptions m_treeOptions;
	bool m_triedTreeMaking;
	bool m_loadOnly;

	bool makeOrLoadTree();
	string layoutSeqTree(int maxX, int maxY, int yInt, vector<SeqTreeEdge>& edges);
	int getAllEdges(vector<SeqTreeEdge>& edges);
	void getEgesFromSubTree(const SeqTree::iterator& cursor, vector<SeqTreeEdge>& edges);
	void annotateLeafNode(const SeqItem& nodeData, SeqTreeNode& node);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE


#endif 

/* 
 * ===========================================================================
 *
 * $Log$
 * Revision 1.6  2006/05/15 18:52:49  cliu
 * do not create new tree if m_loadOnly=true
 *
 * Revision 1.5  2005/08/04 21:33:23  cliu
 * annotate with Tax work
 *
 * Revision 1.4  2005/07/27 18:52:20  cliu
 * guard against failure to make a tree for a CD.
 *
 * Revision 1.2  2005/07/20 20:52:41  ucko
 * Properly (irregularly) capitalize cuSeqtree.hpp.
 *
 * Revision 1.1  2005/07/20 20:04:32  cliu
 * redesign SeqTreeAPI
 *
 *
 *
 * ===========================================================================
 */
