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
 *   Read and write a SeqTree.
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuSeqtree.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbiapp.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <stdio.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

void SeqItem::init()
{
	//name = std::string();
	id = 0;
	distance = 0.0;
	distanceToRoot = 0.0;
	x = -1;
	y = -1;
	rowID = -1;
	//num = 0;
	//numSelected = 0;
	selected = false;
	membershipColor = 0;
	taxid = -1;
	collapsed = false;
	interesting = false;
}


SeqItem::SeqItem() :selections(), name()
{
	init();
}

SeqItem::SeqItem(int rowid, double dist) : selections(), name()
{
	init();
	distance = dist;
	rowID = rowid;
}

SeqItem::SeqItem(const SeqItem& rhs): name(rhs.name), id(rhs.id), distance(rhs.distance), 
	distanceToRoot(rhs.distanceToRoot),x(rhs.x), y(rhs.y), rowID(rhs.rowID),	
	selected(rhs.selected), membershipColor(rhs.membershipColor), 
	collapsed(rhs.collapsed),interesting(rhs.interesting), selections(rhs.selections) {}

//The format of nameDist is "rowID_gi:distance"
SeqItem::SeqItem(const std::string nameDist) : selections(), name()
{
	init();

	//try to get row ID
	int _Pos = nameDist.find('_');
	std::string tail;
	if(_Pos < 0) //no row id
		tail = nameDist;
	else
	{
		rowID = atoi(nameDist.substr(0, _Pos).c_str());
		tail = nameDist.substr(_Pos + 1);
	}

	//then try to get name(gi) and distance
	int colonPos = tail.find(':');
	
	if (colonPos >= 0) //found name
	{
		name = tail.substr(0,colonPos);
		if (_Pos < 0) //no row id has been found
			rowID = atoi(name.c_str());
		distance = atof(tail.substr(colonPos+1).c_str());
	}
}

//To give any old name plus a rowid (for testing)
SeqItem::SeqItem(const std::string nameDist, int row, double dist) : selections(), name()
{
	init();
    name     = nameDist;
    rowID    = row;
    distance = dist;
}

//select all instances of this row
void SeqItem::select(bool on) 
{
	SelectionByCd::iterator sit = selections.begin();
	for(; sit != selections.end(); sit++)
	{
		sit->second = on;
	}
}

//select the instance in this CD
void SeqItem::select(CCdCore* cd, bool on)
{
	SelectionByCd::iterator sit = selections.find(cd);
	if (sit != selections.end())
		sit->second = on;
}

int SeqItem::getAllInstances()
{
	return selections.size();
}

int SeqItem::getAllSelectedInstances()
{
	SelectionByCd::iterator sit = selections.begin();
	int num = 0;
	for(; sit != selections.end(); sit++)
	{
		if (sit->second)
			num++;
	}
	return num;
}


//////////////// SeqTree ////////////////////

SeqTree::SeqTree() : m_prepared(false), m_numLeaf(0), m_maxDist(0), 
	m_longestName(), m_leafNodes(), m_collapsedNodes(),  SeqTreeBase()
{
}


ostream &operator<<(ostream& os, const SeqItem& si) {

    if (si.name == std::string()) {
        os << "<no name>";
    } else {
        os << si.name;
    }
    os << "  ID: " << si.id << " RowID: " << si.rowID 
       << " Distance =" << si.distance << std::endl;
    return os;
}

bool SeqItem::operator==(const SeqItem& rhs) const {
    bool isEqual = false;
    if (name == rhs.name && 
        id == rhs.id &&
        distance == rhs.distance &&
        distanceToRoot == rhs.distanceToRoot &&
        x == rhs.x &&
        y == rhs.y &&
        rowID == rhs.rowID &&
        selected == rhs.selected) {
        isEqual = true;
    }
    return isEqual;
}


SeqTree::~SeqTree()
{
}


// calculate distances to root, find the max distance,
// count number of leaf nodes, tag each leaf node with a sequential id.
// find the longest name
void SeqTree::prepare()
{
	if (isPrepared())
		return;
	prepare(m_numLeaf, m_maxDist, begin());
	//take care of the cases where collapsed nodes at the end
	prepareCollapsedNodes(m_numLeaf);
	m_prepared = true;
}

void SeqTree::forcePrepare()
{
	m_prepared = false;
	m_numLeaf = 0;
	m_maxDist = 0.0;
	m_leafNodes.clear();
	m_longestName.erase();
	prepare();
}

void SeqTree::prepare(int& numLeaf, double& maxDist, const SeqTreeBase::iterator& cursor)
{
	// if leaf, prepare itsself and return
	if ((cursor.number_of_children() == 0) && (cursor != begin()))
	{ 
		//if there are collapsed nodes before this leaf, count them in numLeaf
		prepareCollapsedNodes(numLeaf);
		cursor->distanceToRoot = parent(cursor)->distanceToRoot + cursor->distance;
		numLeaf++;
		cursor->id = numLeaf; //imply leaf id starts at 1
		if (cursor->distanceToRoot > maxDist)
			maxDist = cursor->distanceToRoot;
		if (cursor->name.size() > m_longestName.size())
			m_longestName = cursor->name;
		m_leafNodes.insert(RowLeafMap::value_type(cursor->rowID, cursor));
		return;
	}
	else
	{ 
		// prepare itself
		if ( cursor == begin()) //root
			cursor->distanceToRoot = 0;
		else
			cursor->distanceToRoot = parent(cursor)->distanceToRoot + cursor->distance;
		if (cursor->collapsed)
		{
			if (cursor->distanceToRoot > maxDist)
				maxDist = cursor->distanceToRoot;
			m_collapsedNodes.push(cursor); //keep the node and set its id later
			return;
		}
		// prepare each child
		SeqTree::sibling_iterator sib = cursor.begin();
		while (sib != cursor.end())
		{
			prepare(numLeaf, maxDist,sib);  //recursive
			++sib;
		}
	}
}

void SeqTree::prepareCollapsedNodes(int& numLeaf)
{
	while(!m_collapsedNodes.empty())
	{
		iterator& colNode = m_collapsedNodes.front();
		numLeaf++;
		colNode->id = numLeaf;
		m_collapsedNodes.pop();
	}
}

bool SeqTree::isPrepared() 
{ 
	return m_prepared;
}
int SeqTree::getNumLeaf()
{
	if (!isPrepared())
		prepare();
	return m_numLeaf;
}

double SeqTree::getMaxDistanceToRoot()
{
	if (!isPrepared())
		prepare();
	return m_maxDist;
}

void SeqTree::getDistantNodes(double dist, vector<SeqTreeIterator>& nodes)
{
	//if (!isPrepared())
	//	prepare();
	if(dist > m_maxDist) //no nodes is further away than m_maxDist
		return;
	getDistantNodes(begin(), dist, nodes);
}

void SeqTree::getDistantNodes(const iterator& start, double dist, vector<SeqTreeIterator>& nodes)
{
	if (start->distanceToRoot > dist)
	{
		nodes.push_back(start);
		return;
	}
	sibling_iterator sib = start.begin();
	while (sib != start.end())
	{
		getDistantNodes(sib,dist,nodes);  //recursive
		++sib;
	}
}

void SeqTree::uncollapseAll()
{
	for (iterator it = begin(); it != end(); ++it)
	{
		it->collapsed = false;
	}
}

std::string SeqTree::getLongestName()
{
	if (!isPrepared())
		prepare();
	return m_longestName;
}
//assume the seqTree is made from one CD.
void SeqTree::getOrdersInTree(vector<int>& positions)
{
	if (!isPrepared())
		prepare();
	int masterRow = 0; 
	int masterRowTreeId = m_leafNodes[masterRow]->id;
	assert(m_numLeaf == m_leafNodes.size());
	for (int row = masterRow + 1; row < masterRow + m_numLeaf; row++)
	{
		int treeId = m_leafNodes[row]->id;
		if (treeId > masterRowTreeId)
			treeId--; //to skip master row
		treeId--;	//so treeId starts at 0
		positions.push_back(treeId);
	}
}

void SeqTree::getDiversityRankToRow(int colRow, list<int>& rankList)
{
	if (!isPrepared())
		prepare();
	SeqTreeIterator node = m_leafNodes[colRow];
	while (node.is_valid() && node != begin())
	{
		//add each of its siblings to the list
		sibling_iterator sib = previous_sibling(node);
		SeqTreeIterator sibNode;
		if (sib.is_valid())
			sibNode = sib;
		while(sibNode.is_valid())
		{
			getDiversityRankUnderNode(sibNode, rankList);
			sib = previous_sibling(sibNode);
			if (sib.is_valid())
				sibNode = sib;
			else
				sibNode = SeqTreeIterator(0);
		}
		sib = next_sibling(node);
		if (sib.is_valid())
				sibNode = sib;
			else
				sibNode = SeqTreeIterator(0);
		while(sibNode.is_valid())
		{
			getDiversityRankUnderNode(sibNode, rankList);
			sib = next_sibling(sibNode);
			if (sib.is_valid())
				sibNode = sib;
			else
				sibNode = SeqTreeIterator(0);
		}
		node = parent(node); //go up
	}
	getDiversityRankUnderNode(m_leafNodes[colRow], rankList);
	rankList.reverse();
}

void SeqTree::getDiversityRankUnderNode(SeqTreeIterator node, list<int>& rankList)
{
	if (!node.is_valid())
		return;
	if (node.number_of_children() == 0 ) //take care leaf itself
	{
		rankList.push_back(node->rowID);
	}
	else //take care of its children
	{
		sibling_iterator sib = node.begin();
		while (sib != node.end()) 
		{
			getDiversityRankUnderNode(sib, rankList);
			sib++;
		}
	}
}

void SeqTree::getSelectedSequenceRowid(const iterator& node,  vector<int>& selections)
{
	sibling_iterator sib = node.begin();
	if (sib == node.end()) //no child, so it is a leaf(sequence)
	{
		if (node->selected)
		{
			selections.push_back(node->rowID); 
		}
		return;
	}
	while (sib != node.end())
	{
			getSelectedSequenceRowid(sib,selections);  //recursive
			++sib;
	}
}

void SeqTree::getSequenceRowid(const iterator& node,  vector<int>& selections)
{
	sibling_iterator sib = node.begin();
	if (sib == node.end()) //no child, so it is a leaf(sequence)
	{
		selections.push_back(node->rowID); 
		return;
	}
	while (sib != node.end())
	{
		getSequenceRowid(sib,selections);  //recursive
		++sib;
	}
}

//cd=0 , select all instances
void SeqTree::selectNode(const iterator& node, bool select, CCdCore* cd)
{
	//select itself
	node->selected = select;
	//select all of its children
	SeqTree::sibling_iterator sib = node.begin();
	if (sib == node.end()) //leaf
	{
		if (cd)
			node->select(cd, select);
		else
			node->select(select);
		if (node->getAllSelectedInstances() > 0)
			node->selected = true;
		else
			node->selected = false;
		return;
	}
	while (sib != node.end())
	{
			selectNode(sib,select, cd);  //recursive
			++sib;
	}
}

void SeqTree::selectByRowID(const set<int>& selection, bool select, CCdCore* cd)
{
	if (!isPrepared())
		prepare();
	for (RowLeafMap::iterator mapIt = m_leafNodes.begin(); mapIt != m_leafNodes.end(); mapIt++)
	{
		SeqTreeIterator leaf = mapIt->second;
		int row = leaf->rowID;
		if (selection.find(row) != selection.end())
		{
			selectNode(leaf, select, cd);
		}
			
	}
}

SeqTreeIterator SeqTree::getLeafById(int id)
{
	for (RowLeafMap::iterator mapIt = m_leafNodes.begin(); mapIt != m_leafNodes.end(); mapIt++)
	{
		SeqTreeIterator leaf = mapIt->second;
		if (leaf->id == id)
			return leaf;
	}
	return end();
}

void SeqTree::selectByGI(const AlignmentCollection& aligns, const vector<CRef<CSeq_id> >& gis)
{
	set<int> rows;
	vector<int> rowsForOne;
	for (int i = 0; i < gis.size(); i++)
	{
		CRef<CSeq_id> seqId = gis[i];
		rowsForOne.clear();
		aligns.GetRowsWithSeqID(seqId, rowsForOne);
		for (int k = 0; k < rowsForOne.size(); k++)
			rows.insert(rowsForOne[k]);
	}
	selectByRowID(rows);
}

void SeqTree::selectByTax(const vector<int>& rows, long taxid)
{
	if (!isPrepared())
		prepare();
	clearTaxSelection();
	int num = rows.size();
	for (int i = 0; i < num; i++)
	{
		RowLeafMap::iterator mapIt = m_leafNodes.find(rows[i]);
		iterator leafNode;
		if (mapIt != m_leafNodes.end())
		{
			leafNode = mapIt->second;
			leafNode->taxid = taxid;
		}
	}
}

void SeqTree::clearTaxSelection()
{
	if (!isPrepared())
		prepare();
	for (RowLeafMap::iterator mapIt = m_leafNodes.begin();
		mapIt != m_leafNodes.end(); ++mapIt)
			mapIt->second->taxid = -1;
}

//make seq name that may have gi, specie and foortprint
void SeqTree::fixRowName(AlignmentCollection& aligns, SeqNameMode mode)
{
	int num = aligns.GetNumRows();
	iterator cursor = begin();
	for (; cursor != end(); ++cursor)
	{
		if ((cursor.number_of_children() == 0) && (cursor->rowID >= 0) && (cursor->rowID < num))  // has a valid row ID
		{
			cursor->name.erase();
			std::string gi, species;
			char loc[200];
			sprintf(loc, "[%d~%d]", aligns.GetLowerBound(cursor->rowID),  aligns.GetUpperBound(cursor->rowID));
			aligns.Get_GI_or_PDB_String_FromAlignment(cursor->rowID, gi);
				//get species name
			aligns.GetSpeciesForRow(cursor->rowID, species);
			switch (mode)
			{
			case eSPECIES:
				cursor->name = species;
				break;
			case eGI_SPECIES:
				cursor->name = gi +"[" + species + "]";
				break;
			case eGI_SEQLOC:
				cursor->name = gi + loc;
				break;
			case eGI_SEQLOC_SPECIES:
				cursor->name = gi + loc + "[" + species + "]";
				break;
			default:
				cursor->name = gi;
				break;
			}
		}
	}
}

void SeqTree::clearInternalNodeSelection()
{
	iterator cursor = begin();
	for (; cursor != end(); ++cursor)
	{
		if (cursor.number_of_children() != 0)  // a leaf
		{
			cursor->selected = false; 
		}
	}
}

//fix row number by making row start at 0 instead of 1 when the tree was returned from tree factory
void SeqTree::fixRowNumber(AlignmentCollection& aligns)
{
	iterator cursor = begin();
	for (; cursor != end(); ++cursor)
	{
		if (cursor.number_of_children() == 0)  // a leaf
		{
			cursor->rowID--; //make row start at 0
			vector<RowSource> rss;
			aligns.GetRowSourceTable().findEntries(cursor->rowID, rss, true);
			for (int i = 0; i < rss.size(); i++)
			{
				cursor->selections.insert(SelectionByCd::value_type(rss[i].cd, false));
			}
		}
	}
}

//fix row number by making row start at 0 instead of 1 when the tree was returned from tree factory
void SeqTree::addSelectionFields(AlignmentCollection& aligns)
{
	iterator cursor = begin();
	for (; cursor != end(); ++cursor)
	{
		if (cursor.number_of_children() == 0)  // a leaf
		{
			cursor->selections.clear();
			vector<RowSource> rss;
			aligns.GetRowSourceTable().findEntries(cursor->rowID, rss, true);
			for (int i = 0; i < rss.size(); i++)
			{
				cursor->selections.insert(SelectionByCd::value_type(rss[i].cd, false));
			}
		}
	}
}

void SeqTree::updateSeqCounts(const AlignmentCollection& aligns)
{
	iterator cursor = begin();
	for (; cursor != end(); ++cursor)
	{
		if (cursor.number_of_children() == 0)  // a leaf
		{
			vector<RowSource> rss;
			aligns.GetRowSourceTable().findEntries(cursor->rowID, rss, true);
			for (int i = 0; i < rss.size(); i++)
			{
				if ((cursor->selections).find(rss[i].cd) == cursor->selections.end())
					cursor->selections.insert(SelectionByCd::value_type(rss[i].cd, false));
			}
		}
	}
}

bool SeqTree::isSequenceCompositionSame(AlignmentCollection& aligns)
{
	IntToStringMap treeComposition, cdComposition;
	getSequenceComposition(treeComposition);
	getSequenceComposition(aligns, cdComposition);
	return compareSequenceCompositions(treeComposition, cdComposition);
}

bool SeqTree::compareSequenceCompositions(IntToStringMap& com1, 
								IntToStringMap& com2)
{
	int size = com1.size();
	if (size != com2.size())
		return false;
	for (int i = 0; i < size; i++)
	{
		if (com1[i].compare(com2[i]))
			return false;
	}
	return true;
}

void SeqTree::getSequenceComposition(IntToStringMap& seqComposition)
{
	iterator cursor;
//	vector<std::string>::iterator vit;
	for (cursor = begin(); cursor != end(); ++cursor)
	{
		if (cursor.number_of_children() == 0)
		{
			//vit = seqComposition.begin();
			//assume rowid starts at 1 in treeData
			seqComposition[cursor->rowID] = cursor->name;
		}
	}
}


void SeqTree::getSequenceComposition(AlignmentCollection& aligns, 
										 IntToStringMap& seqComposition)
{
	int num = aligns.GetNumRows();
//	seqComposition.reserve(num);
	//vector<std::string>::iterator vit;
	for (int i = 0; i < num; ++i)
	{
		aligns.Get_GI_or_PDB_String_FromAlignment(i, seqComposition[i]);;
	}
}
void SeqTree::setMembershipColor(const RowMembershipColor* rowColorMap)
{
	if (!rowColorMap)
		return;
	RowMembershipColor::const_iterator cit = rowColorMap->begin();
	while (cit != rowColorMap->end())
	{
		RowLeafMap::iterator mapIt = m_leafNodes.find(cit->first);
		iterator leafNode;
		if (mapIt != m_leafNodes.end())
		{
			leafNode = mapIt->second;
			leafNode->membershipColor = cit->second;
		}
		++cit;
	}
}
END_SCOPE(cd_utils)
END_NCBI_SCOPE
/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.2  2005/04/19 22:05:04  ucko
 * +<stdio.h> due to use of sprintf()
 *
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
