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
 *   part of CDTree app
 */
#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuTaxTree.hpp>
#include <algo/structure/cd_utils/cuTaxClient.hpp>
#include <fstream>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

	//TaxNode
TaxNode::TaxNode()
{
	init();
}

TaxNode::TaxNode(const TaxNode& rhs): seqName(rhs.seqName),numLeaves(rhs.numLeaves),
	orgName(rhs.orgName), rowId(rhs.rowId), taxId(rhs.taxId), rankId(rhs.rankId),
	selectedLeaves(rhs.selectedLeaves), cd(rhs.cd)
{
}

void TaxNode::init()
{
	taxId= -1;
	rankId =-1;

	rowId = -1;
	cd = 0;

	numLeaves = 0;
	selectedLeaves = 0;
}

TaxNode* TaxNode::makeTaxNode(int taxID, std::string taxName, short rankId)
{
	TaxNode*  node = new TaxNode();
	node->taxId = taxID;
	node->rankId = rankId;
	node->orgName = taxName;
	return node;
}

TaxNode* TaxNode::makeSeqLeaf(int rowID, std::string sequenceName)
{
	TaxNode* node = new TaxNode();
	node->rowId = rowID;
	node->seqName = sequenceName;
	node->numLeaves = 1;
	return node;
}

TaxNode* TaxNode::makeSubSeqLeaf(int rowID, CCdCore* cd, int rowInCd)
{
	TaxNode* node = new TaxNode();
	node->rowId = rowID;
	node->cd = cd;
	char name[500];
	sprintf(name,"row_%d_of_%s", rowInCd, cd->GetAccession().c_str());
	node->seqName = name;
	node->numLeaves = 1;
	return node;
}

bool TaxNode::isSeqLeaf(const TaxNode& node)
{
	return node.rowId >= 0;
}

bool TaxNode::isSubSeqLeaf(const TaxNode& node)
{
	return node.cd > 0;
}

TaxTreeData :: TaxTreeData(const AlignmentCollection& ac)
				: TaxonomyTree(), m_rankNameToId(), 
				m_rowToTaxNode(), m_ac(ac), m_failedRows()
{
	m_taxDataSource = new TaxClient();
	if (m_taxDataSource->init())
		makeTaxonomyTree();
}


TaxTreeData :: ~TaxTreeData()
{
	delete m_taxDataSource;
}

//select all seqs under this tax node
void TaxTreeData::selectTaxNode(TaxTreeIterator& taxNode, bool select)
{
	//deselectAllTaxNodes();
	vector<TaxTreeIterator> nodes;
	getAllLeafNodes(taxNode, nodes);
	for (int i = 0; i < nodes.size(); i++)
	{
		selectTaxTreeLeaf(nodes[i], select);
	}
}

int TaxTreeData::getAllLeafNodes(const TaxTreeIterator& taxNode, vector<TaxTreeIterator>& nodes) const
{
	if (TaxNode::isSeqLeaf(*taxNode))
	{
		nodes.push_back(taxNode);
		return nodes.size();
	}
	TaxonomyTree::sibling_iterator sib = taxNode.begin();
	while (sib != taxNode.end())
	{
		getAllLeafNodes(sib, nodes);  //recursive
		++sib;
	}
	return nodes.size();
}

void TaxTreeData::setSelections(const vector<int>& rowIDs, CCdCore* cd)
{
	for (int i = 0; i < rowIDs.size(); i ++)
	{
		RowToTaxNode::iterator mapIt = m_rowToTaxNode.find(rowIDs[i]);
		if (mapIt != m_rowToTaxNode.end())
		{
			TaxonomyTree::iterator taxIt = mapIt->second;
			selectTaxTreeLeaf(taxIt, true, cd);
		}
	}
}


int TaxTreeData::getSelections(vector<int>& rows)
{
	for (RowToTaxNode::iterator i = m_rowToTaxNode.begin();
		i != m_rowToTaxNode.end(); i++)
	{
		TaxonomyTree::iterator taxIt = i->second;
		if (taxIt->selectedLeaves > 0)
			rows.push_back(taxIt->rowId);
	}
	return rows.size();
}


void TaxTreeData::selectTaxTreeLeaf(const TaxonomyTree::iterator& cursor, bool select, CCdCore* cd)
{
	if (TaxNode::isSeqLeaf(*cursor)) //make sure cursor points to a leaf
	{
		if (cursor.number_of_children() > 0) //multiple instances for the seqLoc
		{
			TaxonomyTree::sibling_iterator sib = cursor.begin();
			while (sib != cursor.end())
			{
				selectTaxTreeLeaf(sib, select, cd);
				++sib;
			}
		}
		else if (TaxNode::isSubSeqLeaf(*cursor))
		{
			if (cd == 0)
			{
				if (select)
					cursor->selectedLeaves = 1;
				else
					cursor->selectedLeaves = 0;
			}
			else if (cursor->cd == cd)
			{
				if (select)
					cursor->selectedLeaves = 1;
				else
					cursor->selectedLeaves = 0;
			}	
		}
		else //seq node with no children
		{
			if (select)
				cursor->selectedLeaves = 1;
			else
				cursor->selectedLeaves = 0;
		}
	}
}

void TaxTreeData::clearSelection()
{
	deselectAllTaxNodes();
	fillLeafCount(begin());
}

void TaxTreeData::deselectAllTaxNodes()
{
	for (RowToTaxNode::iterator i = m_rowToTaxNode.begin();
		i != m_rowToTaxNode.end(); i++)
	{
		TaxonomyTree::iterator taxIt = i->second;
		selectTaxTreeLeaf(taxIt, false);
	}
}

/*
void TaxTreeData::deselectTaxNodes(CCd* cd)
{
	for (RowToTaxNode::iterator i = m_rowToTaxNode.begin();
		i != m_rowToTaxNode.end(); i++)
	{
		TaxonomyTree::iterator taxIt = i->second;
		if (taxIt->cd == cd)
			taxIt->selectedLeaves = 0;
	}
}*/

bool TaxTreeData::makeTaxonomyTree()
{
	//connect
	if (!m_taxDataSource->IsAlive())
		return false;
	//add root
	TaxNode* root = TaxNode::makeTaxNode(1,"Root");
	insert(begin(), *root);
	delete root;
	//retrieve tax lineage for each sequence and add it to the tree
	addRows(m_ac);
	fillLeafCount(begin());
	return true;
}

void TaxTreeData::addRows(const AlignmentCollection& ac)
{
	int num = ac.GetNumRows();
	m_failedRows.clear();
	for (int i = 0; i < num; i++)
	{
		std::string seqName;
		ac.Get_GI_or_PDB_String_FromAlignment(i, seqName);
		int taxid = GetTaxIDForSequence(ac, i);
		if (taxid <= 0)
			m_failedRows.push_back(i);
		else
			addSeqTax(i, seqName, taxid);
	}
}

void TaxTreeData::fillLeafCount(const TaxTreeIterator& cursor)
{
	//reset my count if I am not a leaf
	if (cursor.number_of_children() > 0)
	{
		cursor->numLeaves = 0;
		cursor->selectedLeaves = 0;
	}
	// fill each child's count
	TaxonomyTree::sibling_iterator sib = cursor.begin();
	while (sib != cursor.end())
	{
		fillLeafCount(sib);  //recursive
		++sib;
	}
	//add my count to parent
	if (cursor != begin())
	{
		TaxonomyTree::iterator parentIt = parent(cursor);
		parentIt->numLeaves += cursor->numLeaves;
		parentIt->selectedLeaves += cursor->selectedLeaves;
	}
}

void TaxTreeData::addSeqTax(int rowID, string seqName, int taxid)
{
	stack<TaxNode*> lineage;
	if (taxid <= 0) //invalid tax node, ignore this sequence
		return;
	TaxNode *seq = TaxNode::makeSeqLeaf(rowID, seqName);
	string rankName;
	short rank= m_taxDataSource->GetRankID(taxid, rankName);
	TaxNode *tax = TaxNode::makeTaxNode(taxid, m_taxDataSource->GetTaxNameForTaxID(taxid), rank);
	cacheRank(rank, rankName);
	lineage.push(seq);
	lineage.push(tax);
	growAndInsertLineage(lineage);
}

void TaxTreeData::growAndInsertLineage(stack<TaxNode*>& lineage)
{
	TaxNode* top = lineage.top();
	TaxonomyTree::iterator pos = find(begin(), end(), *top);
	if (pos != end())  //top is alreaddy in the tree
	{
		lineage.pop();
		delete top;
		insertLineage(pos, lineage);
		return; //done
	}
	else //top is not in the tree, keep growing lineage
	{
		int parentId = m_taxDataSource->GetParentTaxID(top->taxId);
		string rankName;
		short rank= m_taxDataSource->GetRankID(parentId, rankName);
		cacheRank(rank, rankName);
		TaxNode* parentTax = TaxNode::makeTaxNode(parentId, m_taxDataSource->GetTaxNameForTaxID(parentId), rank);
		lineage.push(parentTax);
		growAndInsertLineage(lineage);
	}
}

void TaxTreeData::insertLineage(TaxonomyTree::iterator& pos, stack<TaxNode*>& lineage)
{
	TaxonomyTree::iterator cursor = pos;
	while(!lineage.empty())
	{
		TaxNode* topNode = lineage.top();
		cursor = append_child(cursor,*topNode);
		lineage.pop();
		delete topNode;
	}
	//the last node added is a leaf node.
	//cache it to speed up the operation of selecting a sequence/leaf
	m_rowToTaxNode.insert(RowToTaxNode::value_type(cursor->rowId, cursor));
	//add multiple instances if the row is duplicated in several CDs
	vector<RowSource> rss;
	m_ac.GetRowSourceTable().findEntries(cursor->rowId, rss, true); //true = scoped only
	if (rss.size() <= 1)
		return;
	for (int i = 0; i < rss.size(); i++)
	{
		if (m_ac.isCDInScope(rss[i].cd))
		{
			TaxNode* subSeqNode = 
			TaxNode::makeSubSeqLeaf(cursor->rowId, rss[i].cd, rss[i].rowInSrc);
			append_child(cursor,*subSeqNode);
			delete subSeqNode;
		}
	}
}

void TaxTreeData::cacheRank(short rank, string rankName)
{
	if (rank >= 0)
		m_rankNameToId.insert(RankNameToId::value_type(rankName, rank));
}

short TaxTreeData::getRankId(string rankName)
{
	RankNameToId::iterator mit = m_rankNameToId.find(rankName);
	if (mit != m_rankNameToId.end())
		return mit->second;
	else return -1;
}

 // get integer taxid for a sequence
int TaxTreeData::GetTaxIDForSequence(const AlignmentCollection& aligns, int rowID)
{
    int taxid = 0;
    std::string err = "no gi or source info";
	// try to get "official" tax info from gi
	int gi = 0;
	bool gotGI = aligns.GetGI(rowID, gi, false);
	if (gotGI)
	{
		taxid = m_taxDataSource->GetTaxIDForGI(gi);
	}
	CRef< CSeq_entry > seqEntry;
	if (aligns.GetSeqEntryForRow(rowID, seqEntry))
	{
		if (seqEntry->IsSeq()) 
		{
			int localTaxid = m_taxDataSource->GetTaxIDFromBioseq(seqEntry->GetSeq(), true);
			if(localTaxid != taxid)
			{
				if (taxid != 0) {
				    string taxName = m_taxDataSource->GetTaxNameForTaxID(taxid);
				    addTaxToBioseq(seqEntry->SetSeq(), taxid, taxName); 
				} else
				  taxid = localTaxid;
			}

		}
	}
	return taxid;
}

void TaxTreeData::addTaxToBioseq(CBioseq& bioseq, int taxid, string& taxName)
{
	//get BioSource if there is none in bioseq
	CSeq_descr& seqDescr = bioseq.SetDescr();
	bool hasSource = false;
	if (seqDescr.IsSet())
	{
		list< CRef< CSeqdesc > >& descrList = seqDescr.Set();
		list< CRef< CSeqdesc > >::iterator cit = descrList.begin();
		//remove old orgRef
		while (cit != descrList.end())
		{
			if ((*cit)->IsSource())
			{
				cit = descrList.erase(cit);
			}
			else
				cit++;
		}
		//add new orgRef
		//create a source seedsc and add it
		CRef< CSeqdesc > source(new CSeqdesc);
		COrg_ref& orgRef = source->SetSource().SetOrg();
		orgRef.SetTaxId(taxid);
		orgRef.SetTaxname(taxName);
		descrList.push_back(source);
	}
}

bool TaxTreeData::isEmpty()const
{
	return m_rowToTaxNode.size() == 0;
}

void TaxTreeData::writeOutRanks()
{
	std::ofstream fout(".\\SeqTree\\ranks");
	if (!fout.good())
		return;
	for (RankNameToId::iterator mit = m_rankNameToId.begin(); mit != m_rankNameToId.end(); mit++)
	{
		fout<<mit->first<<' '<<mit->second<<endl;
	}
	fout.close();
}

bool TaxTreeData::writeToFile(string fname)const
{
	ofstream os(fname.c_str());
	if (os.good())
	{
		write(os, begin());
		return true;
	}
	else
		return false;
	
}

bool TaxTreeData::writeToFileAsTable(string fname)const
{
	ofstream os(fname.c_str());
	if (!os.good())
		return false;
	writeAsTable(os, begin(), begin());
	return true;
}

bool TaxTreeData::writeAsTable(std::ostream&os, const iterator& cursor, const iterator& branchingNode)const
{
	if (!os.good())
		return false;
	if (number_of_children(cursor) > 1)//new branch
	{
		os <<cursor->taxId<<","<<branchingNode->taxId<<"\n";
		sibling_iterator sib = cursor.begin();
		while (sib != cursor.end())
		{
			writeAsTable(os,sib, cursor);  //recursive
			++sib;
		}
	}
	else if (number_of_children(cursor) == 1)// continue on the same branch
	{
		sibling_iterator onlychild = child(cursor,0);
		writeAsTable(os, onlychild, branchingNode);
	} 
	else //reach the end of a branch
	{
		os<<cursor->seqName<<","<<branchingNode->taxId<<"\n";
	}
	return true;
}

bool TaxTreeData::write(std::ostream&os, const iterator& cursor)const
{
	if (!os.good())
		return false;
	// if leaf, print leaf and return
	if (cursor->rowId >= 0)
	{
		os<<cursor->seqName;
	}
	else 
	{ 
		// print (
		if (number_of_children(cursor) > 1)
			os<<'(';
		// print each child
		sibling_iterator sib = cursor.begin();
		while (sib != cursor.end())
		{
			write(os,sib);  //recursive
			++sib;
		}
		// print ) 
		if (number_of_children(cursor) > 1)
		{
			os<<") ";
			//os<<cursor->orgName;
		}
	}
	if (cursor == begin())
		os<<";";
	// if this node has sibling, print ","
	else if (number_of_siblings(cursor) > 1)
		os<<',';
	//return true;
	
	return true;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
        


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:27:18  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */

