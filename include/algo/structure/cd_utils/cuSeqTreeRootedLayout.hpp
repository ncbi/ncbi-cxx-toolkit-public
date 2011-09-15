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
 *   Lay out a rooted phylogenetic tree view.
 */

#ifndef CU_SEQTREE_ROOTED_LAYOUT_HPP
#define CU_SEQTREE_ROOTED_LAYOUT_HPP

#include "cuSeqtree.hpp"
#include <algo/structure/cd_utils/cuSeqTreeLayout.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT SeqTreeRootedLayout : public SeqTreeLayout
{
public:
	explicit SeqTreeRootedLayout(int yInterval = 0);
	void calculateNodePositions(SeqTree& treeData, int maxX, int maxY);
	SeqTree::iterator findEdgeEnd(SeqTree& treeData, int x, int y, int edgeWidth);
//	int getAllEdges(SeqTree& treeData,vector<SeqTreeEdge>& edges);
	void getSize(int& x, int& y);
	virtual ~SeqTreeRootedLayout();
private:
	int m_yInterval;
	int m_numLeaf;
	int m_maxX;
	int m_maxY;
	double m_maxDist;

	void calculateNodePositions(const SeqTree::iterator& cursor);
	int getYInterval();
//	void getEgesFromSubTree(const SeqTree::iterator& cursor, vector<SeqTreeEdge>& edges);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE
#endif
