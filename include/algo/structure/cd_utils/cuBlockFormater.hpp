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
 *       to find and format intersecting Blocks from the input vector of pairwise alignment
 *
 * ===========================================================================
 */
#ifndef CU_BLOCK_FORMATER_HPP
#define CU_BLOCK_FORMATER_HPP

#include <algo/structure/cd_utils/cuBlock.hpp>
#include <algo/structure/cd_utils/cuBlockIntersector.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT BlockFormater
{
public:
	BlockFormater(vector< CRef< CSeq_align > > & seqAlignVec, int masterSeqLen);
	~BlockFormater();

	void setReferenceSeqAlign(const CRef< CSeq_align > sa);
	void getOverlappingPercentages(vector<int>& percentages);
	//-1 find intersection for all the rows
	//return the number of rows over the overlappingPercentage
	int findIntersectingBlocks(int overlappinPercentage=-1);
	int getQualifiedRows(vector<int>& rows);
	int getDisqualifiedRows(vector<int>& rows);
	void formatBlocksForQualifiedRows(list< CRef< CSeq_align > > & seqAlignVec);

private:
	vector< CRef< CSeq_align > >& m_seqAlignVec;
	int m_masterLen;
	BlockIntersector* m_intersector;
	CRef< CSeq_align > m_refSeqAlign;
	vector<int> m_goodRows;
	vector<int> m_badRows;

	//assume seqAlign.master and guide are on the same seq-loc
	// and guide is a subset of seqAlign.master
	CRef< CSeq_align >formatOneRow(const BlockModel& guide, CRef< CSeq_align > seqAlign);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif
