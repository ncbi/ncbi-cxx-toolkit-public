#ifndef CU_TREEFACTORY__HPP
#define CU_TREEFACTORY__HPP

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

//#include <corelib/ncbistd.hpp>
#include <algo/structure/cd_utils/cuSeqTreeStream.hpp>
#include <algo/structure/cd_utils/cuDistmat.hpp>
#include <algo/structure/cd_utils/cuSeqTreeAlg.hpp>
#include <algo/structure/cd_utils/cuScoringMatrix.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

struct TreeOptions
{
	TreeOptions() : clusteringMethod(GLOBAL_DEFAULT_ALGORITHM),
		distMethod(GLOBAL_DEFAULT_DIST_METHOD),
		matrix(GLOBAL_DEFAULT_SCORE_MATRIX),
		nTermExt(DistanceMatrix::NO_EXTENSION),
		cTermExt(DistanceMatrix::NO_EXTENSION){}

	ETreeMethod clusteringMethod;
	EDistMethod distMethod;
	EScoreMatrixType matrix; 
	int nTermExt;
	int cTermExt;

};

class TreeFactory 
{
public:

	//hide the complexity of making DistMat and TreeAlhorithm
	static SeqTree* makeTree(AlignmentCollection* alignData, const TreeOptions& treeOptions);

    TreeFactory() : m_algorithm(NULL), m_matrix(NULL), m_errMsg() {}

	//  Allow polymorphic behavior is subclass this factory

	virtual TreeAlgorithm*  GetAlgorithm(const ETreeMethod method, bool mpRoot=true);
	virtual DistanceMatrix* GetMatrix(const EDistMethod method, AlignmentCollection* alignData, const EScoreMatrixType matrix = GLOBAL_DEFAULT_SCORE_MATRIX, 
		const int nTermExt = DistanceMatrix::NO_EXTENSION, const int cTermExt = DistanceMatrix::NO_EXTENSION);
	const string& getErrMsg() const {return m_errMsg;}

    //  Simple getter; does not compute a matrix.
    const DistanceMatrix* GetMatrix() const {return m_matrix;}  

    virtual ~TreeFactory(){
		delete m_algorithm;
		delete m_matrix;
	}

protected:
    //  NOTE:  m_algorithm and m_matrix are deleted in TreeFactory destructor!
	TreeAlgorithm*   m_algorithm;
	DistanceMatrix*  m_matrix;
	string m_errMsg;
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.2  2005/05/10 20:12:28  cliu
 * fix a typo
 *
 * Revision 1.1  2005/04/19 14:28:01  lanczyck
 * initial version under algo/structure
 *
 *
 * ===========================================================================
 */

#endif   /*  CU_TREEFACTORY__HPP  */

