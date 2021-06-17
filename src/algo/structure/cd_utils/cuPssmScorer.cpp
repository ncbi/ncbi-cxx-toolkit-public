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
 *       Score an seq-align by PSSM
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuPssmScorer.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/Pssm.hpp>
#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cuSequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

PssmScorer::PssmScorer(CRef< CPssmWithParameters > pssm)
:m_pssm(pssm), 
m_scoresFromPssm(pssm->GetPssm().GetNumColumns(), vector<int>(pssm->GetPssm().GetNumRows()))
{
	if (m_pssm->GetPssm().GetByRow()) //deal with byColumn for now
	{
		m_scoresFromPssm.clear();
	}
	else
	{
		if (m_pssm->GetPssm().CanGetFinalData())
		{
			const list< int >& scoreList = m_pssm->GetPssm().GetFinalData().GetScores();
			list<int>::const_iterator lit = scoreList.begin();
			int nCol = m_pssm->GetPssm().GetNumColumns();
			int nRow = pssm->GetPssm().GetNumRows();
			for (int col = 0; col < nCol; col++)
			{
				for (int row = 0; row < nRow; row++)
				{
					m_scoresFromPssm[col][row] = *lit;
					lit++;
				}
			}
		}
		else
			m_scoresFromPssm.clear();
	}
}

//assume the master is the query/consensus in pssm
int PssmScorer::score(const CRef<CSeq_align>  align, const CRef<CBioseq> bioseq)
{
	BlockModelPair bmp(align);
	return score(bmp, bioseq);
}
int PssmScorer::score(BlockModelPair& bmp, const CRef<CBioseq> bioseq)
{
	int score = -1;
	const BlockModel& master = bmp.getMaster();
	const BlockModel& slave = bmp.getSlave();
	int masterLen = m_pssm->GetPssm().GetQuery().GetSeq().GetInst().GetLength();
	vector<char> slaveSeq;
	GetNcbistdSeq(*bioseq, slaveSeq);
	if ((master.getLastAlignedPosition() >= masterLen) 
		|| (slave.getLastAlignedPosition() >= (int) slaveSeq.size()))
		return score;
	if (m_scoresFromPssm.size() == 0)
		return score;
	int nBlocks = master.getBlocks().size();
	for (int b = 0; b < nBlocks; b++)
	{
		const Block& mb = master.getBlocks()[b];
		const Block& sb = slave.getBlocks()[b];
		for (int cb = 0; cb < mb.getLen(); cb++)
		{
			score += scoreOneColumn(mb.getStart() + cb, slaveSeq[sb.getStart()+cb]);
		}
	}
	return score;
}

int PssmScorer::scoreOneColumn(int col, char aa)
{
	return m_scoresFromPssm[col][aa];
}


END_SCOPE(cd_utils)
END_NCBI_SCOPE

