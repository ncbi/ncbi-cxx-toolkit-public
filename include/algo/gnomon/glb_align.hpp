#ifndef ALGO_GNOMON___GLBALIGN__HPP
#define ALGO_GNOMON___GLBALIGN__HPP

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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <algo/gnomon/gnomon_model.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

using namespace std;

typedef pair<CResidueVec,CResidueVec> TCharAlign;


template<class T>
int EditDistance(const T &s1, const T & s2) {
	const int len1 = s1.size(), len2 = s2.size();
	vector<int> col(len2+1), prevCol(len2+1);
 
	for (int i = 0; i < (int)prevCol.size(); i++)
		prevCol[i] = i;
	for (int i = 0; i < len1; i++) {
		col[0] = i+1;
		for (int j = 0; j < len2; j++)
			col[j+1] = min( min( 1 + col[j], 1 + prevCol[1 + j]),
								prevCol[j] + (s1[i]==s2[j] ? 0 : 1) );
		col.swap(prevCol);
	}
	return prevCol[len2];
}

TCharAlign GlbAlign(const CResidueVec& a, const CResidueVec& b, int rho, int sigma, const char delta[256][256], int end_gap_releif = 0);
int GetGlbAlignScore(const TCharAlign& align, int gopen, int gap, const char delta[256][256], int end_gap_releif = 0);

struct SMatrix
{
	SMatrix(int match, int mismatch);  // matrix for DNA
    SMatrix(string name);  // matrix for proteins
	
	char matrix[256][256];
};

END_SCOPE(gnomon)
END_SCOPE(ncbi)


#endif  // ALGO_GNOMON___GLBALIGN__HPP



