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
 * File Description: Retrieve and create a Taxonomy tree for several CDs.
 *   part of CDTree app
 */

#ifndef CU_TAXTREE_HPP
#define CU_TAXTREE_HPP
#include <algo/structure/cd_utils/cuCppNCBI.hpp>
#include <algo/structure/cd_utils/tree_msvc7.hpp>
#include <algo/structure/cd_utils/cuTaxClient.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>
#include <list>
#include <stack>
#include <algorithm>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class TaxNode
{
public:
	//fields for internal tax nodes
	int taxId;
	std::string orgName;
	short rankId;
	//fields for external seq nodes
	int rowId;
	CCdCore* cd;
	std::string seqName;
	//total and selected leaf counts
	int numLeaves;
	int selectedLeaves; 

	//methods
	TaxNode();
	TaxNode(const TaxNode& rhs);
	bool operator==(const TaxNode& rhs) {return taxId == rhs.taxId;};
	static bool isSeqLeaf(const TaxNode& node);
	static bool isSubSeqLeaf(const TaxNode& node);
	static TaxNode* makeTaxNode(int taxID, std::string taxName, short rankId=-1);
	static TaxNode* makeSeqLeaf(int rowID, std::string sequenceName);
	static TaxNode* makeSubSeqLeaf(int rowID, CCdCore* cd, int rowInCd);

private:
	void init();
};

typedef tree<TaxNode> TaxonomyTree;
typedef TaxonomyTree::iterator TaxTreeIterator;
//typedef list<CCd*> CDList;

/*  taxonomy ranking -- total 8 level
 Superkingdom: Eukaryota  
         Kingdom: Metazoa  
           Phylum: Chordata  
             Class: Mammalia  
               Order: Primata  
                 Family: Hominidae  
                   Genus: Homo  
                     Species: sapiens  
*/

// define a family hiearchy of CDs 
class TaxTreeData : public TaxonomyTree
{
public:
	TaxTreeData(const AlignmentCollection& ac);
	
	const vector<int>& getFailedRows() { return m_failedRows;}
	void selectTaxNode(TaxTreeIterator& taxNode, bool select);
	void setSelections(const vector<int>& rowIDs, CCdCore* cd=0);
	int getSelections(vector<int>& rows);
	void clearSelection();
	void deselectAllTaxNodes();
	void fillLeafCount(const TaxTreeIterator& cursor);
	//bool isPreferredTaxNode(const TaxTreeIterator& taxNode);
	int getAllLeafNodes(const TaxTreeIterator& taxNode, vector<TaxTreeIterator>& nodes) const;
	short getRankId(string rankName);
	bool isEmpty()const;
	//bool missLocalTaxFiles()const {return m_missLocalTaxFiles;}
	~TaxTreeData();

	void addTaxToBioseq(CBioseq& bioseq, int taxid, string& taxName);
	bool writeToFile(string fname)const;
	bool writeToFileAsTable(string fname)const;
	bool write(std::ostream&os, const iterator& cursor)const;
	bool writeAsTable(std::ostream&os, const iterator& cursor, const iterator& branchingNode)const;
private:
	const AlignmentCollection& m_ac;
	typedef map<int, TaxonomyTree::iterator> RowToTaxNode;
	RowToTaxNode m_rowToTaxNode;
	typedef map<string, short> RankNameToId;
	RankNameToId m_rankNameToId;
	 // wrapper of taxonomy server class
    TaxClient* m_taxDataSource;
	vector<int> m_failedRows;

	bool makeTaxonomyTree();
	void addRows(const AlignmentCollection& ac);
    // get integer taxid for a sequence
    int GetTaxIDForSequence(const AlignmentCollection& aligns, int rowID);
    // get info for taxid
	void selectTaxTreeLeaf(const TaxTreeIterator& cursor, bool select, CCdCore* cd=0);
	void addSeqTax(int rowID, string seqName, int taxid);
	void growAndInsertLineage(stack<TaxNode*>& lineage);
	void insertLineage(TaxTreeIterator& pos, stack<TaxNode*>& lineage);
	void cacheRank(short rank, string rankName);
	void writeOutRanks();
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
