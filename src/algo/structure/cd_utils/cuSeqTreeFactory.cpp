/*  $Id$
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
* Author:  Chris Lanczycki
*
* File Description:
*   Factory class to create TreeAlgorithm and DistanceMatrix instances.
*   Specific subclass determined by parameters supplied by client.
*   Enums referenced here defined in the two base classes.
*
*/

#include <ncbi_pch.hpp>
#include <algo/structure/cd_utils/cuSeqTreeFactory.hpp>

//  Distance matrix headers for concrete classes
#include <algo/structure/cd_utils/cuDmIdentities.hpp>
#include <algo/structure/cd_utils/cuDmAlignedscore.hpp>
//#include <algo/structure/cd_utils/cuDmBlastscore.hpp>
#include <algo/structure/cd_utils/cuDmAlignedOptimalScore.hpp>
//  Tree algorithm method headers
#include <algo/structure/cd_utils/cuSeqTreeSlc.hpp>
#include <algo/structure/cd_utils/cuSeqTreeNj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

void static dummyMeter(int Num, int Total)
{
}

SeqTree* TreeFactory::makeTree(AlignmentCollection* alignData, const TreeOptions& treeOptions)
{
	SeqTree* treeData = 0;
	TreeFactory treeFactory;
	DistanceMatrix*  distMat = treeFactory.GetMatrix(treeOptions.distMethod, alignData, treeOptions.matrix, 
						treeOptions.nTermExt, treeOptions.cTermExt);
	
	if (distMat) 
	{
		DMAlignedOptimalScore * dmAos  ;
		BlockExtender* blockExtender = 0;
		try { 
			dmAos = dynamic_cast<DMAlignedOptimalScore *> (distMat);
		}catch (...){
			dmAos = 0;
		}
		if (dmAos)
		{
			BlockExtender* blockExtender = new BlockExtender();
			dmAos->setBlockExtender(blockExtender);
		}
        distMat->ComputeMatrix(dummyMeter);
		if (blockExtender)
			delete blockExtender;
		TreeAlgorithm*   treeAlg = treeFactory.GetAlgorithm(treeOptions.cluteringMethod);

		if (treeAlg) 
		{
			treeAlg->SetDistMat(distMat); 
			treeData = new SeqTree();
            treeAlg->ComputeTree(treeData, dummyMeter);
			treeData->fixRowNumber(*alignData);
		}
	}
	return treeData;
}

TreeAlgorithm*  TreeFactory::GetAlgorithm(const ETreeMethod method, bool mpRoot) {

	if (!m_algorithm) {
		switch (method) {
			case eNJ:
				m_algorithm = new NJ_TreeAlgorithm();
				break;
			case eSLC:
				m_algorithm = new SLC_TreeAlgorithm();
				break;
				/*
			case eME:
				m_algorithm = new ME_TreeAlgorithm();
				break;*/
			case eNoTreeMethod:
			default:
				break;
		}
	}
	m_algorithm->SetMidpointRooting(mpRoot);
	return m_algorithm;
}

DistanceMatrix* TreeFactory::GetMatrix(const EDistMethod method, AlignmentCollection* alignData, const EScoreMatrixType scoreMatrix,
									   const int nTermExt, const int cTermExt) {
	MultipleAlignment*  pma;
	try{
		pma = dynamic_cast<MultipleAlignment*>(alignData);
	} 
	catch (...){
		pma =0;
	}
	if (!m_matrix) {
		switch (method) {
			case ePercentIdentity:
				if (pma)
				{
					m_matrix = new DM_Identities(scoreMatrix);
					((DM_Identities*)m_matrix)->SetKimura(false);
					((AlignedDM*)m_matrix)->setData(pma);
				}
				break;
			case ePercIdWithKimura:
				if(pma)
				{
					m_matrix = new DM_Identities(scoreMatrix);
					((DM_Identities*)m_matrix)->SetKimura(true);
					((AlignedDM*)m_matrix)->setData(pma);
				}
				break;
			case eScoreAligned:
				if(pma)
				{
					m_matrix = new DM_AlignedScore(scoreMatrix);
					((AlignedDM*)m_matrix)->setData(pma);
				}
				break;
			case eScoreAlignedOptimal:
				if(pma)
				{
					m_matrix = new DMAlignedOptimalScore(scoreMatrix);
					((AlignedDM*)m_matrix)->setData(pma);
					m_matrix->SetNTermExt(nTermExt);
					m_matrix->SetCTermExt(cTermExt);
				}
				break;
				/*
			case eScoreBlastFoot:
				m_matrix = new DM_BlastScore(scoreMatrix, nTermExt, cTermExt);
				((DM_BlastScore*)m_matrix)->SetUseFullSequence(false);
				m_matrix->SetData(alignData);
				break;
			case eScoreBlastFull:
				m_matrix = new DM_BlastScore(scoreMatrix);
				((DM_BlastScore*)m_matrix)->SetUseFullSequence(true);
				m_matrix->SetData(alignData);
				break;*/
			case eNoDistMethod:
			default:
				break;
		}
	}

	return m_matrix;
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

