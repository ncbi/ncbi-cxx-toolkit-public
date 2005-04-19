#ifndef CU_ALIGNED_DM__HPP
#define CU_ALIGNED_DM__HPP

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
* File Description:  algAlignedDM.hpp
*
*      Representation of a distance matrix for phylogenetic calculations.
*      This class of DMs operates on Multiple-aligned sequences
*
*/
#include <algo/structure/cd_utils/cuDistmat.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class AlignedDM : public DistanceMatrix
{

public:

	AlignedDM();
	void setData(MultipleAlignment* malign);
	virtual bool ComputeMatrix(pProgressFunction pFunc) = 0;
	virtual ~AlignedDM();
protected:
    //  fractional offset added to score->distance mapping
    static const double FRACTIONAL_EXTRA_OFFSET;  

    CharPtr* m_ppAlignedResidues;
	MultipleAlignment* m_maligns;

	  // When distance is sum over scores in aligned region
	int     GetMaxScoreForAligned(); 
    // Compute score for an exact match
    int     GetMaxScore(CharPtr residues);
    // Compute minimum possible score for current CD's aligned length
	int     SetMinScore();
    // Append extra residues for positive shifts; remove residues for negative shifts
    bool    GetResidueListsWithShifts();
private:
	void setData(AlignmentCollection* data) {} //hide this one
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE
#endif        
        


/* 
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/04/19 14:28:00  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */
