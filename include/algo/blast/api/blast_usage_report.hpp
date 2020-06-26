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
 * Authors: Amelia Fong
 *
 */

/** @file blast_usage_report.hpp
 *  BLAST usage report api
 */

#ifndef ALGO_BLAST_API___BLAST_USAGE_REPORT__HPP
#define ALGO_BLAST_API___BLAST_USAGE_REPORT__HPP

#include <connect/ncbi_usage_report.hpp>
#include <algo/blast/core/blast_export.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

class NCBI_XBLAST_EXPORT CBlastUsageReport : public CUsageReport
{

public:
	enum EUsageParams {
		eApp,
		eVersion,
		eProgram,
		eTask,
		eExitStatus,
		eRunTime,
		eDBName,
		eDBLength,
		eDBNumSeqs,
		eDBDate,
		eBl2seq,
		eNumSubjects,
		eSubjectsLength,
		eNumQueries,
		eTotalQueryLength,
		eEvalueThreshold,
		eNumThreads,
		eHitListSize,
		eOutputFmt,
		eTaxIdList,
		eNegTaxIdList,
		eGIList,
		eNegGIList,
		eSeqIdList,
		eNegSeqIdList,
		eIPGList,
		eNegIPGList,
		eCompBasedStats
	};
	CBlastUsageReport();
	~CBlastUsageReport();
	void AddParam(EUsageParams p, int val);
	void AddParam(EUsageParams p, const string & val);
	void AddParam(EUsageParams p, const double & val);
	void AddParam(EUsageParams p, Int8 val);
	void AddParam(EUsageParams p, bool val);

private:
	void x_CheckBlastUsageEnv();
	string x_EUsageParmsToString(EUsageParams p);
	CUsageReportParameters m_Params;
};

END_SCOPE(blast)
END_NCBI_SCOPE

#endif /* ALGO_BLAST_API___BLAST_USAGE_REPORT__HPP */
