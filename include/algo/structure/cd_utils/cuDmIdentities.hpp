#ifndef CU_DM_IDENTITIES__HPP
#define CU_DM_IDENTITIES__HPP

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
*      substitutions as per Kimura.
*
*/

#include <algo/structure/cd_utils/cuDistmat.hpp>
#include <algo/structure/cd_utils/cuAlignedDM.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

//  This class simply uses the number of AA identities in the specified region
//  to define the distance between two sequences:
//
//  d[i][j] = 1 - (n_matched/n_tested);  d is in [0, 1]

class DM_Identities : public AlignedDM {

    //  Since for the Kimura distance measure (see below) D can diverge to +infinity,
    //  place a ceiling on distances.  (for p = 0.8541, D ~ 13)
    static const double MAX_DISTANCE;   
    static const EDistMethod DIST_METHOD;

public:

    DM_Identities(EScoreMatrixType type, int ext);
    DM_Identities(EScoreMatrixType type = GLOBAL_DEFAULT_SCORE_MATRIX, int nTermExt=0, int cTermExt=0);


    bool UseKimura() const {
        return m_kimura;
    }

    void SetKimura(bool kimura) {
        m_kimura  = kimura;
		if (m_kimura) m_dMethod = ePercIdWithKimura;
    }

    virtual ~DM_Identities();
    virtual bool ComputeMatrix(pProgressFunction pFunc);

    //  true if want to correct for multiple AA substitutions a la Kimura:
    //  D = -ln(1 - p - .2p^2).  
    //
    //  WARNING:  
    //  For p = fraction of different residues ~> 0.854102, D becomes infinite.
    //  Set D = MAX_DISTANCE for all p such that 1-p-.2p^2 <= 0.
    static double GetKimuraDistance(int identities, int alignment_length);

    //     Distance is 1 - (fraction of identical residues)
    static double GetDistance(int identities, int alignment_length);

private:
    
    bool m_kimura;    

    void GetPercentIdentities(pProgressFunction pFunc);
    void initDMIdentities(EScoreMatrixType type, int nTermExt, int cTermExt);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

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

#endif /*  CU_DM_IDENTITIES__HPP  */
