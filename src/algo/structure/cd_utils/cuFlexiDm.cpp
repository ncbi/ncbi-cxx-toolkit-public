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
* Author:  Charlie Liu
*
* File Description: 
*
*      Concrete distance matrix class.
*      Distance is computed based on pure percent pairwise AA identity in 
*      aligned blocks, with or without a correction for multiple AA
*
*/

#include <ncbi_pch.hpp>
#include <math.h>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <algo/structure/cd_utils/cuFlexiDm.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

ResidueMatrix::ResidueMatrix(unsigned numRows)
: m_numRows(numRows), m_rows(numRows, RowContent())
{
}

void ResidueMatrix::read(ColumnResidueProfile& crp)
{
    vector<char> residues;
    residues.assign(m_numRows, '-');
    crp.getResiduesByRow(residues, false);
    for (unsigned row = 0; row < residues.size(); row++)
        m_rows[row].push_back(ResidueCell(residues[row], crp.isAligned(row)));
}

bool ResidueMatrix::getAlignedPair(unsigned row1, unsigned row2, pair< string, string >& seqPair)
{
    /*if (row1 > m_rows.size() || row2 > m_rows.size())
        return false;*/
    RowContent& rc1 = m_rows[row1];
    RowContent& rc2 = m_rows[row2];
	seqPair.first.reserve(rc1.size());
	seqPair.second.reserve(rc2.size());
    //assert(rc1.size() == rc2.size());
    for (int i = 0; i < rc1.size(); i++)
    {
        if (rc1[i].aligned && rc2[i].aligned)
        {
            seqPair.first += rc1[i].residue;
            seqPair.second += rc2[i].residue;
        }
    }
    return true;
}

//  FlexiDm class

//  This class simply uses the number of AA identities in the specified region
//  to define the distance between two sequences:
//
//  d[i][j] = 1 - (n_matched/n_tested);  d is in [0, 1]

const double FlexiDm::MAX_DISTANCE = 100.0;
const EDistMethod FlexiDm::DIST_METHOD = ePercentIdentityRelaxed;

FlexiDm::~FlexiDm() {
}

FlexiDm::FlexiDm(EScoreMatrixType type) : DistanceMatrix() {
    initDMIdentities(type);
}
    
void FlexiDm::initDMIdentities(EScoreMatrixType type, int nExt, int cExt) {
    m_scoreMatrix = new ScoreMatrix(type);
    m_useAligned  = true;
    m_nTermExt    = nExt;
    m_cTermExt    = cExt;
    m_dMethod     = DIST_METHOD;
    if (m_nTermExt != 0 || m_cTermExt != 0) {
        m_useAligned = false;
    }
}


bool FlexiDm::ComputeMatrix(pProgressFunction pFunc) {

    bool result;
    if (m_aligns) {
        GetPercentIdentities(pFunc);
        result = true;
    } else {
        result = false;
    }
    return result;
}

void FlexiDm::GetPercentIdentities(pProgressFunction pFunc) 
{
    int nrows = m_aligns->GetNumRows();
	//LOG_POST("Start building Distance Matrix");
	//LOG_POST("Start building ResidueProfiles with "<<nrows<<" rows.");
    ResidueProfiles* rp = new ResidueProfiles();
    string mseq = m_aligns->GetSequenceForRow(0);
    for (int i = 1; i < nrows; i++)
    {
        string sseq = m_aligns->GetSequenceForRow(i);
        BlockModelPair bmp(m_aligns->getSeqAlign(i));
        rp->addOneRow(bmp, mseq, sseq);
    }
	//LOG_POST("Done building ResidueProfiles.  Start building ResidueMatrix");
    ResidueMatrix * rm = new ResidueMatrix(nrows);
    rp->traverseColumnsOnMaster(*rm);
	//LOG_POST("Done building ResidueMatrix.  Starting making Distance Matrix");
	delete rp;
    int Identity, TotalAligned;
    int count = 0;
    int total = (int)((double)nrows * (((double)nrows-1)/2));

    // for each row in the alignment
    for (int j=0; j<nrows; j++) 
    {
        m_Array[j][j] = 0.0;
		ResidueMatrix::RowContent& rc1 = rm->getRow(j);
        // for each other row in the alignment
        for (int k=j+1; k<nrows; k++) 
        {			 
			Identity = 0;
			TotalAligned = 0;
			ResidueMatrix::RowContent& rc2 = rm->getRow(k);
		    for (int i = 0; i < rc1.size(); i++)
			{
				if (rc1[i].aligned && rc2[i].aligned)
				{
					TotalAligned++;
					if (rc1[i].residue == rc2[i].residue)
						Identity++;
				}
			}
            m_Array[j][k] = GetDistance(Identity, TotalAligned);
            m_Array[k][j] = m_Array[j][k]; 
        }
        count += nrows - (j+1);
        pFunc(count, total);
    }
	//LOG_POST("Done building DistanceMatrix");
    assert(count == total);
	delete rm;
//    cout << "Total number rows:  " << nrows << "  Alignment length:  " << alignLen << endl;
}


double FlexiDm::GetDistance(int nIdentities, int alignLen) 
{

    return 1.0 - (TMatType(nIdentities) / TMatType (alignLen));
}


END_SCOPE(cd_utils)
END_NCBI_SCOPE
