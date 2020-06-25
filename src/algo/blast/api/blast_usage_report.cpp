/*  $Id:
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
 * Authors:  Amelia Fong
 *
 */

/** @file blast_usage_report.cpp
 *  BLAST usage report api
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/blast_usage_report.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
#endif

static const string kNcbiAppName="standalone-blast";

CBlastUsageReport::CBlastUsageReport()
{
	x_CheckBlastUsageEnv();
	AddParam(eApp, kNcbiAppName);
}

CBlastUsageReport::~CBlastUsageReport()
{
	if (IsEnabled()) {
		Send(m_Params);
		Wait();
		Finish();
	}
}

string CBlastUsageReport::x_EUsageParmsToString(EUsageParams p)
{
    string retval;
    switch (p) {
    	case eApp:				retval.assign("ncbi_app"); break;
    	case eVersion:			retval.assign("version"); break;
    	case eProgram:          retval.assign("program"); break;
    	case eTask:        		retval.assign("task"); break;
    	case eExitStatus:    	retval.assign("exit_status"); break;
    	case eRunTime:    		retval.assign("run_time"); break;
    	case eDBName:    		retval.assign("db_name"); break;
    	case eDBLength:			retval.assign("db_length"); break;
    	case eDBNumSeqs:		retval.assign("db_num_seqs"); break;
		case eDBDate:			retval.assign("db_date"); break;
    	case eBl2seq:    		retval.assign("bl2seq"); break;
    	case eNumSubjects:		retval.assign("num_subjects"); break;
		case eSubjectsLength:	retval.assign("subjects_length"); break;
    	case eNumQueries:		retval.assign("num_queries"); break;
    	case eTotalQueryLength:	retval.assign("queries_length"); break;
    	case eEvalueThreshold:	retval.assign("evalue_threshold"); break;
    	case eNumThreads:		retval.assign("num_threads"); break;
    	case eHitListSize:		retval.assign("hitlist_size"); break;
    	case eOutputFmt:		retval.assign("output_fmt"); break;
    	case eTaxIdList:		retval.assign("taxidlist"); break;
    	case eNegTaxIdList:		retval.assign("negative_taxidlist"); break;
    	case eGIList:			retval.assign("gilist"); break;
    	case eNegGIList:		retval.assign("negative_gilist"); break;
    	case eSeqIdList:		retval.assign("seqidlist"); break;
    	case eNegSeqIdList:		retval.assign("negative_seqidlist"); break;
    	case eIPGList:			retval.assign("ipglist"); break;
    	case eNegIPGList:		retval.assign("negative_ipglist"); break;
    	default:
        	LOG_POST(Warning <<"Invalid usage params: " << (int)p);
        	abort();
        	break;
    }
    return retval;
}

void CBlastUsageReport::AddParam(EUsageParams p, int val)
{
	if (IsEnabled()){
		string t = x_EUsageParmsToString(p);
		m_Params.Add(t, NStr::IntToString(val));
	}
}

void CBlastUsageReport::AddParam(EUsageParams p, const string & val)
{
	if (IsEnabled()) {
		string t = x_EUsageParmsToString(p);
		m_Params.Add(t, val);
	}
}

void CBlastUsageReport::AddParam(EUsageParams p, const double & val)
{
	if (IsEnabled()) {
		string t = x_EUsageParmsToString(p);
		m_Params.Add(t, val);
	}
}

void CBlastUsageReport::x_CheckBlastUsageEnv()
{
	char * blast_usage_env = getenv("BLAST_USAGE_REPORT");
	if(blast_usage_env != NULL){
		bool enable = NStr::StringToBool(blast_usage_env);
		if (!enable) {
			SetEnabled(false);
			return ;
		}
	}
	SetEnabled(true);
}

void CBlastUsageReport::AddParam(EUsageParams p, Int8 val)
{
	if (IsEnabled()) {
		string t = x_EUsageParmsToString(p);
		m_Params.Add(t, val);
	}

}

void CBlastUsageReport::AddParam(EUsageParams p, bool val)
{
	if (IsEnabled()) {
		string t = x_EUsageParmsToString(p);
		m_Params.Add(t, val);
	}

}
