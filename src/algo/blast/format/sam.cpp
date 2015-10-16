/* $Id:
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* Author: Amelia Fong
*
* ===========================================================================
*/

/// @file: tabular.cpp
/// Formatting of pairwise sequence alignments in tabular form.
/// One line is printed for each alignment with tab-delimited fielalnVec.

#include <ncbi_pch.hpp>

#include <algo/blast/format/sam.hpp>
#include <serial/iterator.hpp>
#include <objtools/align_format/align_format_util.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CBlast_SAM_Formatter::CBlast_SAM_Formatter(CNcbiOstream& out, CScope& scope,
			                               const string & custom_spec,
			                               const CSAM_Formatter::SProgramInfo & info,
			                               bool isVDB) :
			                               CSAM_Formatter(out,scope),
			                               m_isVDB(isVDB),
			                               m_refRow(isVDB? 1: 0)
{
	CSAM_Formatter::SetFlag(CSAM_Formatter::fSAM_PlainSeqIds);
	x_ProcessCustomSpec(custom_spec, info);
}

void CBlast_SAM_Formatter::x_Print(const CSeq_align_set & aln)
{
	if(m_refRow == 1) {
		CSeq_align_set sorted;
		sorted.Set() = aln.Get();
		sorted.Set().sort(align_format::CAlignFormatUtil::SortHspByMasterStartAscending);
		CSAM_Formatter::Print(sorted, m_refRow);
	}
	else {
		CSAM_Formatter::Print(aln, m_refRow);
	}
}

void CBlast_SAM_Formatter::Print(const CSeq_align_set &  aln)
{
	if (aln.Get().front()->GetSegs().Which() == CSeq_align::C_Segs::e_Dendiag){
		CSeq_align_set d_aln;
		ITERATE(CSeq_align_set::Tdata, itr, aln.Get()){
			CRef<CSeq_align> dense_seg = align_format::CAlignFormatUtil::CreateDensegFromDendiag(**itr);
			CDense_seg::TScores & scores = dense_seg->SetSegs().SetDenseg().SetScores();
			dense_seg->SetScore().swap(scores);
			d_aln.Set().push_back(dense_seg);
		}
		x_Print(d_aln);
	}
	else {
		x_Print(aln);
	}
}

void CBlast_SAM_Formatter::x_ProcessCustomSpec(const string & custom_spec,
		                                       const CSAM_Formatter::SProgramInfo & info)
{
	vector<string> format_tokens;
	NStr::Tokenize(custom_spec, " ", format_tokens);
	CSAM_Formatter::SetProgram(info);
	ITERATE (vector<string>, iter, format_tokens) {
		if("SR" == *iter){
			m_refRow = 0;
		} else if ("QR" == *iter ) {
			m_refRow = 1;
		} else if ("FA" == *iter) {
			CSAM_Formatter::UnsetFlag(CSAM_Formatter::fSAM_PlainSeqIds);
		}
	}

	if(m_refRow == 1) {
		CSAM_Formatter::SetGroupOrder(eGO_Reference);
		CSAM_Formatter::SetSortOrder(eSO_Coordinate);
	}
}


END_NCBI_SCOPE
