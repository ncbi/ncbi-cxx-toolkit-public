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
 *       Distribute PSI-BLAST hits among CDs
 *
 * ===========================================================================
 */

#ifndef CU_HITS_DISTRIBUTOR_HPP
#define CU_HITS_DISTRIBUTOR_HPP

#include "objects/seqalign/Seq_align_set.hpp"
#include <algo/structure/cd_utils/cuCdUpdater.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

struct GiFootPrint
{
	GiFootPrint(CRef<CSeq_align> seqAlign);

	int gi;
	int from;
	int to;
};

struct LessByFootPrint : public binary_function <GiFootPrint, GiFootPrint, bool> 
{
	bool operator()(const GiFootPrint& left, const GiFootPrint& right) const
	{
		if (left.gi < right.gi)
			return true;
		else if (left.gi == right.gi)
		{
			if (left.to < right.from)
				return true;
			else
				return false;
		}
		else
			return false;
	}
};


//redistribute hits based on e-value
class NCBI_CDUTILS_EXPORT HitDistributor
{
public:
	HitDistributor();
	~HitDistributor();

	void addBatch(CRef<CSeq_align_set> seqAlignSet);
	void distribute();
	void dump(string filename);

private:
	typedef map<GiFootPrint, vector< CRef< CSeq_align >* >, LessByFootPrint> FootprintToHitMap;
	FootprintToHitMap m_hitTable;
	vector< CRef< CSeq_align_set > > m_batches;
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif
