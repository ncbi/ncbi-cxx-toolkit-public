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
 *       Replace local sequences in a CD
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <algo/structure/cd_utils/cuSeqSwapper.hpp>
#include <algo/structure/cd_utils/cuSeqTreeFactory.hpp>
#include <algo/structure/cd_utils/cuBlast2Seq.hpp>
#include <algo/structure/cd_utils/cuCD.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)
SeqSwapper::SeqSwapper(CCdCore* cd, int identityThreshold):
	m_cd(cd), m_clusteringThreshold(identityThreshold), m_replacingThreshold(identityThreshold), m_ac(cd, CCdCore::USE_ALL_ALIGNMENT)
{
}
	
void SeqSwapper::swapSequences()
{
	int numNormal = m_cd->GetNumRows();
	LOG_POST("Clustering");
	vector< vector<int> * > clusters;
	makeClusters(m_clusteringThreshold, clusters);
	vector< pair<int, int> > replacementPairs;
	set<int> structures;
	LOG_POST("Clustering is done (made " << clusters.size() << " clusters)");
	LOG_POST("Find replacements by BLAST in each cluster");
	for (int i = 0; i < clusters.size(); i++)
	{
		vector<int>* cluster = clusters[i];
        if (cluster) {
            findReplacements(*cluster, replacementPairs, structures);
            delete cluster;
        }
	}
	LOG_POST("Done with BLAST in each cluster");
	set<int> usedPendings;
	
	vector<int> selectedNormalRows;
	int newMaster = -1;
	for (int p = 0; p < replacementPairs.size(); p++)
	{
		//debug
		CRef< CSeq_id > seqId;
		m_ac.GetSeqIDForRow(replacementPairs[p].first, seqId);
		string nid = seqId->AsFastaString();
		m_ac.GetSeqIDForRow(replacementPairs[p].second, seqId);
		string pid = seqId->AsFastaString();
		LOG_POST("replacing "<<nid<<" with "<<pid);
		//take care of master replacement
		if (replacementPairs[p].first == 0)
			newMaster = replacementPairs[p].second - numNormal;
		else
		{
			selectedNormalRows.push_back(replacementPairs[p].first);
			usedPendings.insert(replacementPairs[p].second - numNormal);
		}
	}
	m_cd->EraseTheseRows(selectedNormalRows);
	if (structures.size() > 0)
	{
		LOG_POST("Adding "<<structures.size()<<" structures");
		for (set<int>::iterator sit = structures.begin(); sit != structures.end(); sit++)
			usedPendings.insert(*sit - numNormal);
	}
	if (newMaster >= 0)
		promotePendingRows(usedPendings, &newMaster);
	else
		promotePendingRows(usedPendings);
	//findStructuralPendings(structures);
	
	if (newMaster > 0)
	{
		ReMasterCdWithoutUnifiedBlocks(m_cd, newMaster, true);
		vector<int> rows;
		rows.push_back(newMaster);
		m_cd->EraseTheseRows(rows);
	}
	m_cd->ResetPending();
	m_cd->EraseSequences();
}

void SeqSwapper::makeClusters(int identityThreshold, vector< vector<int> * >& clusters)
{
	TreeOptions option;
	option.clusteringMethod = eSLC;
	option.distMethod = ePercentIdentityRelaxed;
	SeqTree* seqtree = TreeFactory::makeTree(&m_ac, option);
	if (seqtree)
	{
		seqtree->prepare();
		assert(seqtree->getNumLeaf() == m_ac.GetNumRows());
		//seqtree->fixRowNumber(m_ac);
		double pid = ((double)identityThreshold)/100.0;
		double distTh = 1.0 -  pid;
		double distToRoot = seqtree->getMaxDistanceToRoot() - distTh;
		vector<SeqTreeIterator> nodes;
		seqtree->getDistantNodes(distToRoot, nodes);
		for (int i = 0; i < nodes.size(); i++)
		{
			vector<int>* rowids = new vector<int>;
			seqtree->getSequenceRowid(nodes[i], *rowids);
			clusters.push_back(rowids);
		}
	}
	delete seqtree;
}

void SeqSwapper::findReplacements(vector<int>& cluster, vector< pair<int,int> >& replacementPairs, set<int>& structs)
{
	if (cluster.size() == 0)
		return;
	// seperate normal from pending
	vector<int> normalLocal, normalNonLocal, pending;
	bool hasNormalPDB = false;
	set<int> pending3D;
	for (int i = 0; i < cluster.size(); i++)
	{
		if (m_ac.IsPending(cluster[i]))
		{
			pending.push_back(cluster[i]);
			if (m_ac.IsPdb(cluster[i]))
			{
				pending3D.insert(cluster[i]);
			}
		}
		else
		{
			CRef< CSeq_id > seqId;
			m_ac.GetSeqIDForRow(cluster[i], seqId);
			if (seqId->IsLocal())
				normalLocal.push_back(cluster[i]);
			else
			{
				normalNonLocal.push_back(cluster[i]);
				if (seqId->IsPdb())
					hasNormalPDB = true;
			}
		}
	}
	//find replacement for local sequences
	if ((normalLocal.size() != 0) && (pending.size() != 0))
	{
		vector< pair<int,int> > pairs;
		findBestPairings(normalLocal, pending, pairs);
		for (int i = 0; i < pairs.size(); i++)
		{
			replacementPairs.push_back(pairs[i]);
			set<int>::iterator sit = pending3D.find(pairs[i].second);
			if (sit != pending3D.end())
			{
				pending3D.erase(sit); //this 3D has been used.
			}
		}
	}
	//try to add extra PDBs
	if (!hasNormalPDB && //there is no normal PDB in the cluster
		!pending3D.empty() && //there is pending 3D left
		!normalNonLocal.empty()) //at least one normal in the cluster
	{
		vector< pair<int,int> > pairsWith3D;
		vector<int> pending3DVec;
		for (set<int>::iterator sit = pending3D.begin(); sit != pending3D.end(); sit++)
			pending3DVec.push_back(*sit);
		findBestPairings(normalNonLocal, pending3DVec, pairsWith3D);
		if (pairsWith3D.size() > 0) //just need one
		{
			structs.insert(pairsWith3D[0].second);
		}
	}
}

void SeqSwapper::findBestPairings(const vector<int>& normal, const vector<int>& pending, vector< pair<int, int> >& pairs)
{
	if ((normal.size() == 0) || (pending.size() == 0))
	{
		return;
	}
	//blast normal against pending
	CdBlaster blaster(m_ac);
	blaster.setQueryRows(&normal);
	blaster.setSubjectRows(&pending);
	blaster.setScoreType(CSeq_align::eScore_PercentIdentity);
	blaster.blast();
	//for each normal, find the hightest-scoring (in identity), unused and above the threshold pending
	set<int> usedPendingIndice;
	for (int n = 0; n < normal.size(); n++)
	{
		int maxId = 0;
		int maxIdIndex = -1;
		for (int p = 0; p < pending.size(); p++)
		{
			if (usedPendingIndice.find(p) == usedPendingIndice.end())//this pending has not been used
			{
				int pid = (int)blaster.getPairwiseScore(n, p);
				if ((pid > maxId) && (pid >= m_replacingThreshold))
				{
					maxId = pid;
					maxIdIndex = p;
				}
			}
		}
		if ( maxIdIndex >= 0)
		{
			usedPendingIndice.insert(maxIdIndex);
			pairs.push_back(pair<int, int>(normal[n], pending[maxIdIndex]));
		}
	}
}


void SeqSwapper::findStructuralPendings(set<int>& rows)
{
	AlignmentCollection ac(m_cd);// default, pending only
	int num = ac.GetNumRows();
	for (int i = 0; i < num; i++)
		if (ac.IsPdb(i))
			rows.insert(i);
}

void SeqSwapper::promotePendingRows(set<int>& rows, int* newMaster)
{
	AlignmentCollection ac(m_cd);// default, pending only
	int masterInPending = -1;
	if (newMaster)
	{
		masterInPending = *newMaster;
		m_cd->AddSeqAlign(ac.getSeqAlign(*newMaster));
		*newMaster = m_cd->GetNumRows() - 1;
	}
	for (set<int>::iterator sit = rows.begin(); sit != rows.end(); sit++)
	{
		m_cd->AddSeqAlign(ac.getSeqAlign(*sit));
	}
	if (masterInPending >= 0)
	rows.insert(masterInPending);
	m_cd->ErasePendingRows(rows);
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE

