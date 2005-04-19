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
* File Description:  dm_identities.hpp
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

#include <algo/structure/cd_utils/cuDmIdentities.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cd_utils)

//  DM_Identities class

//  This class simply uses the number of AA identities in the specified region
//  to define the distance between two sequences:
//
//  d[i][j] = 1 - (n_matched/n_tested);  d is in [0, 1]

const double DM_Identities::MAX_DISTANCE = 100.0;
const EDistMethod DM_Identities::DIST_METHOD = ePercentIdentity;

DM_Identities::~DM_Identities() {
}


DM_Identities::DM_Identities(EScoreMatrixType type, int ext) : AlignedDM() {
    initDMIdentities(type, ext, ext);
}

DM_Identities::DM_Identities(EScoreMatrixType type, int nTermExt, int cTermExt) : AlignedDM() {
    initDMIdentities(type, nTermExt, cTermExt);
}
/*
DM_Identities::DM_Identities(const CCd* cd, EScoreMatrixType type, int ext) :
    DistanceMatrix(cd) {
    initDMIdentities(type, ext, ext);
}
	
DM_Identities::DM_Identities(const CCd* cd, EScoreMatrixType type, int nTermExt, int cTermExt) :
    DistanceMatrix(cd) {
    initDMIdentities(type, nTermExt, cTermExt);
}
*/
	
void DM_Identities::initDMIdentities(EScoreMatrixType type, int nTermExt, int cTermExt) {
    m_scoreMatrix = new ScoreMatrix(type);
    m_useAligned  = true;
    m_kimura      = false;
    m_nTermExt    = nTermExt;
    m_cTermExt    = cTermExt;
    m_dMethod     = DIST_METHOD;
    if (m_nTermExt != 0 || m_cTermExt != 0) {
        m_useAligned = false;
    }
}


bool DM_Identities::ComputeMatrix(pProgressFunction pFunc) {

    bool result;
    if (m_maligns && GetResidueListsWithShifts()) {
        GetPercentIdentities(pFunc);
        result = true;
    } else {
        result = false;
    }
    return result;
}

//  Adapts some of Dave H's code from cdt_cd.cpp:A_CD::GetDiversity()

void DM_Identities::GetPercentIdentities(pProgressFunction pFunc) {

    int i, j, k;
    int nrows = m_maligns->GetNumRows();

    //  alignLen != nCompared if there are negative shifts; nCompared is the
    //  proper normalization for percent identities.  alignLen is used for
    //  tracking where residues are in the m_ppAlignedResidues array.
    int alignLen  = m_maligns->GetAlignmentLength() + Max(0, m_nTermExt) + Max(0, m_cTermExt);
    int nCompared = m_maligns->GetAlignmentLength() + m_nTermExt + m_cTermExt;

    int Identity;
    char Res1, Res2;

    int count = 0;
    int total = (int)((double)nrows * (((double)nrows-1)/2));
    // for each row in the alignment
    for (j=0; j<nrows; j++) {
        m_Array[j][j] = 0.0;
        // for each other row in the alignment
        for (k=j+1; k<nrows; k++) {
            Identity = 0;
            // for each column of the alignment +/- extensions
            for (i=0; i<alignLen; i++) {
                Res1 = m_ppAlignedResidues[j][i];
                Res2 = m_ppAlignedResidues[k][i];
                // calculate percent of identical residues
                if (Res1 == Res2 && Res1 != 0) {
                    Identity++;
                }
            }
            if (m_kimura) {
                m_Array[j][k] = GetKimuraDistance(Identity, nCompared);
            } else {
                m_Array[j][k] = GetDistance(Identity, nCompared);
            }
            m_Array[k][j] = m_Array[j][k];
//             if (j==0) {
//                 printf("Sequences %d and %d have %d of %d residues identical (d = %6.3f)\n",
//                        j, k, Identity, GetCdd().GetAlignmentLength(), m_Array[j][k]);
//             }
        }
        count += nrows - (j+1);
        pFunc(count, total);
    }
    assert(count == total);
//    cout << "Total number rows:  " << nrows << "  Alignment length:  " << alignLen << endl;
}


double DM_Identities::GetDistance(int nIdentities, int alignLen) {

    return 1.0 - (TMatType(nIdentities) / TMatType (alignLen));
}

    //  Correct for multiple AA substitutions a la Kimura:
    //  D = -ln(1 - p - .2p^2).  
    //
    //  WARNING:  
    //  For p = fraction of different residues ~> 0.854102, D becomes infinite.
    //  Set D = MAX_DISTANCE for all p such that 1-p-.2p^2 <= 0.

double DM_Identities::GetKimuraDistance(int nIdentities, int alignLen) {

    double kdist = MAX_DISTANCE;
    double p;         //  fraction of matching aligned residues

    if (alignLen > 0.0) {
        p = 1.0 - (TMatType(nIdentities) / TMatType (alignLen));
        p = 1.0 - p - 0.2*p*p;
        kdist = (p > 0.0) ? -log(p) : MAX_DISTANCE;
    }
    return kdist;
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
