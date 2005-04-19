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
 *       to find the intersection of the group of alignments on the same sequence
 *
 * ===========================================================================
 */
#ifndef CU_BLOCK_INTERSECTOR_HPP
#define CU_BLOCK_INTERSECTOR_HPP

#include <algo/structure/cd_utils/cuBlock.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class BlockIntersector
{
public:
	BlockIntersector(int seqLen);
	~BlockIntersector();

	void addOneAlignment(const BlockModel& bm);
	BlockModel* getIntersectedAlignment();

private:
	int m_seqLen;
	unsigned m_totalRows;
	const BlockModel* m_firstBm;
	unsigned* m_aligned;
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
