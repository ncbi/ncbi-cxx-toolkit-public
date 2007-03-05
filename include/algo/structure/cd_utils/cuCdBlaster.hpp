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
 *       Functions to call C++ Blast2Seq API
 *
 * ===========================================================================
 */

#ifndef ALGBLAST2SEQ_HPP
#define ALGBLAST2SEQ_HPP

#include <vector>

#include "algMatrix.hpp"
#include "algScoringMatrix.hpp"
#include "algAlignmentCollection.hpp"

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

typedef void (* NotifierFunction) (int Num, int Total);

class CdBlaster 
{
public:
	static const string SCORING_MATRIX_DEFAULT         = BLOSUM62NAME;
	//the source of the query sequences
	CdBlaster(AlignmentCollection& source, string matrixName = SCORING_MATRIX_DEFAULT);

	void useWholeSequence(bool whole);
	void setFootprintExtension(int nExt, int CExt);
	//to do psi-blast
	void setPsiBlastTarget(CRef<CPssmWithParameters> pssm);
	void setPsiBlastTarget(CCd* targetCD);

	bool blast(NotifierFunction notifier);

	CRef<CSeq_align> getPairwiseBlastAlignement(int row1, row2)const;
	CRef<CSeq_align> getPsiBlastAlignement(int row1, row2)const;

private:
	AlignmentCollection& m_ac;
	string m_scoringMatrix;
	bool m_useWhole; // false: use aligned footprint
	int m_nExt;
	int m_cExt;
	vector<CRef< CSeq_align >> m_alignments;
	CCd* m_psiTagetCd;
	CRef<CPssmWithParameters> m_psiTargetPssm;
};



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


#endif // ALGBLAST_HPP
