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
 *       API to handle SeqTree
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuSeqTreeAPI.hpp>
#include <algo/structure/cd_utils/cuSeqTreeFactory.hpp>
#include <algo/structure/cd_utils/cuSeqTreeRootedLayout.hpp>
#include <algo/structure/cd_utils/cuSeqTreeAsnizer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

SeqTreeAPI::SeqTreeAPI(vector<CCdCore*>& cds, bool loadExistingTreeOnly)
	: m_ma(), m_cd(0), m_family(0), m_seqTree(0), m_taxTree(0), m_taxClient(0), 
	m_useMembership(true), m_taxLevel(BySuperkingdom), m_treeOptions(), m_triedTreeMaking(false),
	m_loadOnly(loadExistingTreeOnly)
{
	vector<CDFamily*> families;
	CDFamily::createFamilies(cds, families);
	if(families.size() != 1 )
	{
		for (int i = 0 ; i <families.size(); i++)
			delete families[i];
		return;
	}
	m_ma.setAlignment(*families[0]);
	m_family = families[0];
}

SeqTreeAPI::SeqTreeAPI(CCdCore* cd)
	: m_ma(), m_cd(cd), m_family(0), m_seqTree(0), m_taxTree(0), m_taxClient(0), 
	m_useMembership(true), m_taxLevel(BySuperkingdom), m_treeOptions(), m_triedTreeMaking(false),
	m_loadOnly(true)
{
}

SeqTreeAPI::SeqTreeAPI(CDFamily& cdfam, TreeOptions& option)
    : m_ma(), m_cd(0), m_family(&cdfam), m_seqTree(0), m_taxTree(0), m_taxClient(0),
	m_useMembership(true), m_taxLevel(BySuperkingdom), m_treeOptions(option), m_triedTreeMaking(false),
	m_loadOnly(false)
{
	m_family = new CDFamily(cdfam);
}

SeqTreeAPI::~SeqTreeAPI()
{
	if (m_seqTree) delete m_seqTree;
	if (m_taxTree) delete m_taxTree;
	if (m_taxClient) delete m_taxClient;
	if (m_family) delete m_family;
}

void SeqTreeAPI::annotateTreeByMembership()
{
	m_useMembership = true;
}

void SeqTreeAPI::annotateTreeByTaxonomy(TaxonomyLevel tax)
{
	m_taxLevel = tax;
	m_useMembership = false;
}

int SeqTreeAPI::getNumOfLeaves()
{
	if (m_seqTree == 0)
		makeOrLoadTree();
	if (m_seqTree == 0)
		return 0;
	else
		return m_seqTree->getNumLeaf();
}
	//return a string of tree method names
	//lay out the tree to the specified area, .i.e. fit to screen
string SeqTreeAPI::layoutSeqTree(int maxX, int maxY, vector<SeqTreeEdge>& edges)
{
	if (m_seqTree == 0)
		makeOrLoadTree();
	return layoutSeqTree(maxX, maxY, -1, edges);
}

	//lay out the tree with a fixed spacing between sequences
string SeqTreeAPI::layoutSeqTree(int maxX, vector<SeqTreeEdge>& edges, int yInt)
{
	if (m_seqTree == 0)
		makeOrLoadTree();
	int maxY = 0; //not used
	if (yInt < 2)
		yInt = 2;
	return layoutSeqTree(maxX, maxY, yInt, edges);
}
bool SeqTreeAPI::makeTree()
{
	if (m_triedTreeMaking) //if already tried, don't try again
		return m_seqTree != 0;
	if (m_seqTree != 0)
	{
		delete m_seqTree;
		m_seqTree = 0;
		m_seqTree = new SeqTree;
	}
	if (!m_loadOnly)
	{
		if (m_ma.getFirstCD() == 0)
			m_ma.setAlignment(*m_family);
		if(!m_ma.isBlockAligned())
		{
			ERR_POST("Sequence tree is not made for " <<m_ma.getFirstCD()->GetAccession()
				<<" because it does not have a consistent block alognment.");
		}
		else
		{
			if ((m_treeOptions.distMethod == eScoreBlastFoot) || (m_treeOptions.distMethod == eScoreBlastFull))
				m_treeOptions.distMethod = eScoreAligned;
			m_seqTree = TreeFactory::makeTree(&m_ma, m_treeOptions);
		}
		if (m_seqTree)
			m_seqTree->fixRowName(m_ma, SeqTree::eGI);
	}
	m_triedTreeMaking = true;
	return m_seqTree != 0;
}

bool SeqTreeAPI::makeOrLoadTree()
{
	if (m_triedTreeMaking) //if already tried, don't try again
		return m_seqTree != 0;
	m_seqTree = new SeqTree();
	bool loaded = false;
	if (m_cd)
		loaded = loadExistingTree(m_cd, &m_treeOptions, m_seqTree);
	else
		loaded = loadAndValidateExistingTree();
	if (!loaded)
	{
		return makeTree();
	}
	return loaded;
}

string SeqTreeAPI::layoutSeqTree(int maxX, int maxY, int yInt, vector<SeqTreeEdge>& edges)
{
	if (!m_seqTree)
		return "";
	SeqTreeRootedLayout treeLayout(yInt);
	treeLayout.calculateNodePositions(*m_seqTree, maxX, maxY);
	getAllEdges(edges);
	string param = GetTreeAlgorithmName(m_treeOptions.clusteringMethod);
	param.append(" / " + DistanceMatrix::GetDistMethodName(m_treeOptions.distMethod));
	if (DistanceMatrix::DistMethodUsesScoringMatrix(m_treeOptions.distMethod) ) {
		param.append(" / " + GetScoringMatrixName(m_treeOptions.matrix));
	}
	return param;
}

int SeqTreeAPI::getAllEdges(vector<SeqTreeEdge>& edges)
{
	getEdgesFromSubTree(m_seqTree->begin(), edges);
	return edges.size();
}

void SeqTreeAPI::getEdgesFromSubTree(const SeqTree::iterator& cursor, vector<SeqTreeEdge>& edges)
{
    //if cursor is a leaf node, draw its name and return
    if (cursor.number_of_children() == 0)
    {
        return;
    }
    //always draw from parent to children
	SeqTreeNode nodeParent;
	nodeParent.isLeaf = false;
	nodeParent.x = cursor->x;
	nodeParent.y = cursor->y;
    SeqTree::sibling_iterator sib = cursor.begin();
    while (sib != cursor.end())
    {
		SeqTreeNode nodeChild, nodeTurn;
		nodeChild.x = sib->x;
		nodeChild.y = sib->y;
		nodeTurn.isLeaf = false;
		nodeTurn.x = nodeParent.x;
		nodeTurn.y= nodeChild.y;
		if (sib.number_of_children() == 0)
		{
			nodeChild.isLeaf =true;
			nodeChild.name = sib->name;
            nodeChild.displayAnnotation = kEmptyStr;
			annotateLeafNode(*sib,nodeChild);
		}
		else
			nodeChild.isLeaf = false;
		SeqTreeEdge e1, e2;
		e1.first = nodeParent;
		e1.second = nodeTurn;
		e2.first = nodeTurn;
		e2.second = nodeChild;
		edges.push_back(e1);
		edges.push_back(e2);
		getEdgesFromSubTree(sib, edges);
        ++sib;
    }
}

void SeqTreeAPI::annotateLeafNode(const SeqItem& nodeData, SeqTreeNode& node)
{
	if (m_useMembership)
	{
		/*if (!nodeData.membership.empty() && m_loadOnly)
		{
			node.annotation = nodeData.membership;
		}*/
		if ((m_ma.GetNumRows() > 0))
		{
			CCdCore* cd = m_ma.GetScopedLeafCD(nodeData.rowID);
			if (cd)
			{
				node.annotation = cd->GetAccession();
			}
		}
		else if (m_cd)
		{
			if (!nodeData.membership.empty())
				node.annotation = nodeData.membership;
			else
				node.annotation = m_cd->GetAccession();
		}
	}
	else
	{
		if (m_cd)
		{
			if (m_taxClient == 0)
			{
				m_taxClient = new TaxClient();
				m_taxClient->init();
			}
			int taxid = m_taxClient->GetTaxIDForSeqId(nodeData.seqId);
			if (taxid >= 0)
			{
				if (m_taxLevel == BySuperkingdom)
					node.annotation = m_taxClient->GetSuperKingdom(taxid);
			}
		}
		else
		{
			if (!m_taxTree)
				m_taxTree = new TaxTreeData(m_ma);
			string rankName = "superkingdom";
			/*if(m_taxLevel == ByKingdom)
				rankName = "kingdom";*/
			TaxTreeIterator taxNode = m_taxTree->getParentAtRank(nodeData.rowID, rankName);
			node.annotation = taxNode->orgName;
		}
	}
}

bool SeqTreeAPI::loadAndValidateExistingTree()
{
	if ( m_seqTree == 0)
		m_seqTree = new SeqTree;
	CCdCore* cd = 0;
	if (m_family)
	{
		cd = m_family->getRootCD();
	}
	else
		cd = m_ma.getFirstCD();
	if (!cd->IsSetSeqtree())
		return false;
	SeqLocToSeqItemMap liMap;
	if (!SeqTreeAsnizer::convertToSeqTree(cd->SetSeqtree(), *m_seqTree, liMap))
		return false;
	CRef< CAlgorithm_type > algType(const_cast<CAlgorithm_type*> (&(cd->GetSeqtree().GetAlgorithm())));
	SeqTreeAsnizer::convertToTreeOption(algType, m_treeOptions);
	if (m_treeOptions.scope == CAlgorithm_type::eTree_scope_immediateChildrenOnly)
	{
		CDFamily *subfam = new CDFamily;
		m_family->subfamily(m_family->begin(),subfam, true); //childrenOnly = true
		m_ma.setAlignment(*subfam);
		delete subfam;
	}
	else
	{
		m_ma.setAlignment(*m_family);
	}
	bool validated = false;
	if(m_seqTree->isSequenceCompositionSame(m_ma))
		validated = true;
	else //if not same, resolve RowID with SeqLoc
	{
		if (SeqTreeAsnizer::resolveRowId(m_ma, liMap))
			validated = m_seqTree->isSequenceCompositionSame(m_ma);
	}
	if (validated)
		SeqTreeAsnizer::refillAsnMembership(m_ma, liMap);
	return validated;
}

bool SeqTreeAPI::loadExistingTree(CCdCore* cd, TreeOptions* treeOptions, SeqTree* seqTree)
{
	if (!cd->IsSetSeqtree() || !treeOptions)
		return false;

	//bool loaded = false;
	SeqTree* tmpTree = 0;
	SeqTree tmpTreeObj;

	if (seqTree)
		tmpTree = seqTree;
	else
		tmpTree = &tmpTreeObj;

	SeqLocToSeqItemMap liMap;
	if (!SeqTreeAsnizer::convertToSeqTree(cd->SetSeqtree(), *tmpTree, liMap))
		return false;
	CRef< CAlgorithm_type > algType(const_cast<CAlgorithm_type*> (&(cd->GetSeqtree().GetAlgorithm())));
	SeqTreeAsnizer::convertToTreeOption(algType, *treeOptions);
	return true;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
