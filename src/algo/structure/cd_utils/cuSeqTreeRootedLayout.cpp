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
 *   lay out a rooted phylogenetic tree.
 */
#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuSeqTreeRootedLayout.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

SeqTreeRootedLayout::SeqTreeRootedLayout(int yInterval)
{
	m_yInterval = yInterval;
}

SeqTreeRootedLayout::~SeqTreeRootedLayout() 
{
}

SeqTree::iterator SeqTreeRootedLayout::findEdgeEnd(SeqTree& treeData, int x, int y, int edgeWidth)
{
	SeqTree::iterator cursor = treeData.begin();
	if (x < cursor->x)
	{
		cursor = treeData.end();
		return cursor;
	}
	++cursor;
	for (;cursor != treeData.end();++cursor)
	{
		SeqTree::iterator parentNode = treeData.parent(cursor);
		if ( (x >= parentNode->x) &&
			 (x <= cursor->x) &&
			 (y <= cursor->y) &&
			 (y >= cursor->y - edgeWidth))
			 break;
	}
	return cursor;
}


/*
void SeqTreeRootedLayout::drawEdge(wxDC& dc, int x1, int y1, int x2, int y2)
{
	dc.DrawLine(x1, y1, x1, y2);
	dc.DrawLine(x1, y2, x2, y2);
}*/


void SeqTreeRootedLayout::calculateNodePositions(SeqTree& treeData, int maxX, int maxY)
{
	if (!treeData.isPrepared())
		treeData.prepare();
	m_numLeaf = treeData.getNumLeaf();
	m_maxDist = treeData.getMaxDistanceToRoot();
	m_maxX = maxX;
	m_maxY = maxY;
	//starting at the root, recursively calculate the position for each node
	calculateNodePositions(treeData.begin());
}

void SeqTreeRootedLayout::calculateNodePositions(const SeqTree::iterator& cursor)
{
	int yInterval = getYInterval();

	// if leaf or collapsed node, calculate itself and return
	if ((cursor.number_of_children() == 0)||(cursor->collapsed) )
	{
		cursor->y = (yInterval)*cursor->id;
		cursor->x = (cursor->distanceToRoot*m_maxX)/m_maxDist;
		return;
	}
	else
	{ 
		// calculate each child
		SeqTree::sibling_iterator sib = cursor.begin();
		while (sib != cursor.end())
		{
			calculateNodePositions(sib);  //recursive
			++sib;
		}
		//then calculate itself
		cursor->x = (cursor->distanceToRoot*m_maxX)/m_maxDist;
		// the parent's Y is the average y of all childrean
		// calculate each child
		SeqTree::sibling_iterator sib2 = cursor.begin();
		int numChildren = 0;
		int sumY = 0;
		while (sib2 != cursor.end())
		{
			numChildren++;
			sumY += sib2->y;
			++sib2;
		}
		cursor->y = sumY/numChildren;
	}
}


int SeqTreeRootedLayout::getYInterval()
{
	int yInterval = 0;
	if (m_yInterval > 0)
		yInterval = m_yInterval;
	else
		yInterval = m_maxY/m_numLeaf;
	if (yInterval <= 0)
		yInterval = 2;
	return yInterval;
}

void SeqTreeRootedLayout::getSize(int& x, int& y)
{
	x = m_maxX;
	y = m_numLeaf * getYInterval();
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

