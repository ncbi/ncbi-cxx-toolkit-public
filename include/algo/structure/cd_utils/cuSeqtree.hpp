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
 *   Define SeqTree and how to read and write it.
 */

#if !defined(CU_SEQTREE_HPP)
#define CU_SEQTREE_HPP
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>
#include <algo/structure/cd_utils/tree_msvc7.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>

#include <string>
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <queue>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

typedef map<CCdCore*, bool> SelectionByCd;

class SeqItem
{
	friend ostream &operator<<(ostream& os, const SeqItem&);
public:
	SeqItem();
	SeqItem(const SeqItem& rhs);
	SeqItem(int rowid, double dist=0.0);
	explicit SeqItem(const std::string nameDist);
	SeqItem(const std::string nameDist, int rowid, double dist=0.0);
	void select(bool on=true); //select all instances of this row
	void select(CCdCore* cd, bool on=true); //select the instance in this CD
	int getAllInstances();
	int getAllSelectedInstances();
	~SeqItem(){};

	bool operator==(const SeqItem& rhs) const;

	std::string name;
	int id;
	double distance;
	double distanceToRoot;
	int x;
	int y;
	int rowID;
	SelectionByCd selections;
	bool selected;
	bool collapsed;
	bool interesting;
	long taxid;
	void* membershipColor; //color to indicate whether this node belongs to a child
private:
	void init();
};

typedef map<int, std::string> IntToStringMap;
typedef map<int, void*> RowMembershipColor;

typedef tree<SeqItem> SeqTreeBase;
typedef SeqTreeBase::iterator SeqTreeIterator;

class SeqTree: public SeqTreeBase
{
public:
	SeqTree();
	virtual ~SeqTree();

//	SeqTree(const SeqTree& rhs);
//	SeqTree& operator=(const SeqTree& rhs);
	
	//prepare deriative attributes for drawing
	void prepare();
	void forcePrepare();
	bool isPrepared();
	int getNumLeaf();
	double getMaxDistanceToRoot();
	std::string getLongestName();
	//get the order of aligntment rows in the tree.
	//master row (row=0) is not in the vector positions because it is not a real ealignment row.
	void getOrdersInTree(vector<int>& positions);
	void getDiversityRankToRow(int colRow, list<int>& rankList);
	SeqTreeIterator getLeafById(int id);
	//set child cd membership
	void setMembershipColor(const RowMembershipColor* rowColorMap);
	
	//node selection
	void getSelectedSequenceRowid(const iterator& node, vector<int>& selections);
	void getSequenceRowid(const iterator& node, vector<int>& selections);
	void clearInternalNodeSelection();
	void selectByRowID(const set<int>& receivedSelection, bool select=true, CCdCore* cd=0);
	void selectByTax(const vector<int>& rows, long taxid);
	void selectByGI(const AlignmentCollection& aligns,const vector< CRef <CSeq_id> >& gis);
	void selectNode(const iterator& node, bool select=true, CCdCore* cd=0);
	//void deselectNode(const iterator& node, bool all=false);
	void getDistantNodes(double dist, vector<SeqTreeIterator>& nodes);
	//void getPeerLeafNodes(const iterator& node, vector<SeqTreeIterator>& nodes);
	void uncollapseAll();

	//seq name
	enum SeqNameMode
	{
		eGI, 
		eSPECIES,
		eGI_SPECIES, 
		eGI_SEQLOC,
		eGI_SEQLOC_SPECIES
	};
	void fixRowName(AlignmentCollection& aligns, SeqNameMode mode);
	void fixRowNumber(AlignmentCollection& aligns);
	void addSelectionFields(AlignmentCollection& aligns);
	void updateSeqCounts(const AlignmentCollection& aligns);
	bool isSequenceCompositionSame(AlignmentCollection& aligns);
	bool compareSequenceCompositions(IntToStringMap& com1, IntToStringMap& com2);
	void getSequenceComposition(IntToStringMap& seqComposition);
	void getSequenceComposition(AlignmentCollection& aligns, 
								IntToStringMap& seqComposition);
private:
	bool m_prepared;
	int m_numLeaf;
	double m_maxDist;
	std::string m_longestName;
	typedef map<int, iterator> RowLeafMap;
	RowLeafMap m_leafNodes;
	typedef queue<iterator> IteQueue;
	IteQueue m_collapsedNodes;  //used in prepare() only
	
	void prepare(int& numLeaf, double& maxDist, const iterator& cursor);
	void prepareCollapsedNodes(int& numLeaf);
	void clearTaxSelection();
	void getDistantNodes(const iterator& start, double dist, vector<SeqTreeIterator>& nodes);
	void getDiversityRankUnderNode(SeqTreeIterator node, list<int>& rankList);
	//void selectByInternalRowID(const vector<int>& selection);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE
#endif 

/* 
 * ===========================================================================
 * 
 * $Log$
 * Revision 1.1  2005/04/19 14:28:01  lanczyck
 * initial version under algo/structure
 *
 * 
 * ===========================================================================
 */
