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
 *     Search for replacements of local sequences in a CD.
 *
 * ===========================================================================
 */

#ifndef CU_SEQSWAPPER_HPP
#define CU_SEQSWAPPER_HPP

#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <algo/structure/cd_utils/cuAlignmentCollection.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT SeqSwapper
{
public:
	SeqSwapper(CCdCore* cd, int identityThreshold);
	
	void swapSequences();

private:
	CCdCore* m_cd;
	int m_clusteringThreshold; //in percent identity
	int m_replacingThreshold; //in percent identity
	int m_structurePickingThrehsold;
	AlignmentCollection m_ac;

	void makeClusters(int identityThreshold, vector< vector<int> * >& clusters);
	void findReplacements(vector<int>& cluster, vector< pair<int,int> >& replacementPairs, set<int>& structs);
	void findStructuralPendings(set<int>& rows);
	void promotePendingRows(set<int>& rows, int* newMaster=0);
	void findBestPairings(const vector<int>& normal, const vector<int>& pending, vector< pair<int, int> >& pairs);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif
