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
 * Author:  Charlie
 *
 * File Description:
 *
 *       extend blocks
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuBlockFormater.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

BlockFormater::BlockFormater(vector< CRef< CSeq_align > > & seqAlignVec, int masterSeqLen)
: m_seqAlignVec(seqAlignVec), m_masterLen(masterSeqLen), m_intersector(0)
{
}

BlockFormater::~BlockFormater()
{
	if (m_intersector)
		delete m_intersector;
}

void BlockFormater::setReferenceSeqAlign(const CRef< CSeq_align > sa)
{
	m_refSeqAlign = sa;
}

void BlockFormater::getOverlappingPercentages(vector<int>& percentages)
{
	if (m_seqAlignVec.size() == 0)
		return;
	BlockIntersector intersector(m_masterLen);
	int start = 0;
	int refAlignmentLen = 0;
	if (!m_refSeqAlign.Empty())
	{
		BlockModel bm(m_refSeqAlign,false);
		refAlignmentLen = bm.getTotalBlockLength();
		intersector.addOneAlignment(bm);
	}
	else
	{	
		BlockModel bm(m_seqAlignVec[0],false);
		refAlignmentLen = bm.getTotalBlockLength();
		intersector.addOneAlignment(bm);
		percentages.push_back(100);
		start = 1;
	}
	for (int i = start; i < m_seqAlignVec.size(); i++)
	{
		BlockModel bm(m_seqAlignVec[i], false);
		intersector.addOneAlignment(bm);
		BlockModel* intersectedBM = intersector.getIntersectedAlignment();
		int score = (intersectedBM->getTotalBlockLength() * 100)/refAlignmentLen;
		percentages.push_back(score);
		delete intersectedBM;
		intersector.removeOneAlignment(bm);
	}
}

//-1 find intersection for all the rows
//return the number of rows over the overlappingPercentage
int BlockFormater::findIntersectingBlocks(int overlappingPercentage)
{
	if (m_seqAlignVec.size() == 0)
		return 0;
	if (m_intersector)
	{
		delete m_intersector;
	}
	m_intersector = new BlockIntersector(m_masterLen);
	int minAlignedLen = 0;
	m_goodRows.clear();
	m_badRows.clear();
	int start = 0;
	if (!m_refSeqAlign.Empty())
	{
		BlockModel bm(m_refSeqAlign,false);
		minAlignedLen = bm.getTotalBlockLength() * overlappingPercentage/100;
		m_intersector->addOneAlignment(bm);
	}
	else
	{	
		BlockModel bm(m_seqAlignVec[0],false);
		minAlignedLen = bm.getTotalBlockLength() * overlappingPercentage/100;
		m_intersector->addOneAlignment(bm);
		start = 1;
		m_goodRows.push_back(0);
	}
	for (int i = start; i < m_seqAlignVec.size(); i++)
	{
		BlockModel bm(m_seqAlignVec[i], false);
		m_intersector->addOneAlignment(bm);
		BlockModel* intersectedBM = m_intersector->getIntersectedAlignment();
		if (intersectedBM->getTotalBlockLength() < minAlignedLen)
		{
			m_intersector->removeOneAlignment(bm);	
			m_badRows.push_back(i);
		}
		else
			m_goodRows.push_back(i);
		delete intersectedBM;
	}
	return m_goodRows.size();
}

int BlockFormater::getQualifiedRows(vector<int>& rows)
{
	rows.assign(m_goodRows.begin(), m_goodRows.end());
	return rows.size();
}

int BlockFormater::getDisqualifiedRows(vector<int>& rows)
{
	rows.assign(m_badRows.begin(), m_badRows.end());
	return rows.size();
}

void BlockFormater::formatBlocksForQualifiedRows(list< CRef< CSeq_align > > & seqAlignList)
{
	if (m_intersector == 0)
		return;
	cd_utils::BlockModel* finalBM = m_intersector->getIntersectedAlignment();
	//format what's already in seqAlignList
	list< CRef< CSeq_align > >::iterator lit = seqAlignList.begin();
	for (; lit != seqAlignList.end(); lit++)
	{
		(*lit) = formatOneRow(*finalBM, *lit);
	}
	for (int i = 0; i < m_goodRows.size(); i++)
	{
		seqAlignList.push_back(formatOneRow(*finalBM, m_seqAlignVec[m_goodRows[i]]));
	}
	delete finalBM;
}

//assume seqAlign.master and guide are on the same seq-loc
// and guide is a subset of seqAlign.master
CRef< CSeq_align > BlockFormater::formatOneRow(const BlockModel& guide, CRef< CSeq_align > seqAlign)
{
	BlockModelPair bmp(seqAlign);
	pair<cd_utils::DeltaBlockModel*, bool> delta = guide - bmp.getMaster();
	cd_utils::BlockModel& slave = bmp.getSlave();
	pair<cd_utils::BlockModel*, bool> sum = slave + (*delta.first);
	CRef< CSeq_align > result = sum.first->toSeqAlign(guide);
	delete delta.first;
	delete sum.first;
	return result;
}

END_SCOPE(cd_utils)
END_NCBI_SCOPE
