/* $Id$
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
* Author:  Amelia fong
*/

/** @file data4xml2format.cpp
 * Produce data required for generating BLAST XML2 output
 */

#include <ncbi_pch.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <util/tables/raw_scoremat.h>
#include <algo/blast/format/data4xml2format.hpp>       /* NCBI_FAKE_WARNING */

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
USING_SCOPE(align_format);
#endif

CCmdLineBlastXML2ReportData::CCmdLineBlastXML2ReportData (
	CConstRef<CBlastSearchQuery> query,
    const CSearchResults& results,
    CConstRef<CBlastOptions> opts,
    CRef<CScope> scope,
    const vector<align_format::CAlignFormatUtil::SDbInfo> & dbsInfo) :
    	m_Query(query), m_Options (opts), m_Scope(scope),
    	m_DbName(kEmptyStr), m_NumSequences(0), m_NumBases(0),
    	m_TaxDBFound(false), m_isBl2seq(false), m_isIterative(false), m_Matrix(NULL)
{
	x_InitCommon(results, opts);
	x_InitDB(dbsInfo);
	results.GetMaskedQueryRegions(m_QueryMasks);
	x_InitResults(results);
}

CCmdLineBlastXML2ReportData::CCmdLineBlastXML2ReportData(
	CConstRef<CBlastSearchQuery> query,
	const CSearchResults& results,
    CConstRef<CBlastOptions> opts,
    CRef<CScope> scope,
    CConstRef<IBlastSeqInfoSrc> subjectsInfo) :
    	m_Query(query), m_Options (opts), m_Scope(scope),
    	m_DbName(kEmptyStr), m_NumSequences(0), m_NumBases(0),
    	m_TaxDBFound(false), m_isBl2seq(true), m_isIterative(false), m_Matrix(NULL)
{
	x_InitCommon(results, opts);
	x_InitSubjects(subjectsInfo);
	results.GetMaskedQueryRegions(m_QueryMasks);
	x_InitResults(results);
}

CCmdLineBlastXML2ReportData::CCmdLineBlastXML2ReportData(
	CConstRef<CBlastSearchQuery> query,
    const CSearchResultSet& resultSet,
    CConstRef<CBlastOptions> opts,
    CRef<CScope> scope,
    const vector<CAlignFormatUtil::SDbInfo> & dbsInfo) :
    	m_Query(query), m_Options (opts), m_Scope(scope),
    	m_DbName(kEmptyStr), m_NumSequences(0), m_NumBases(0),
    	m_TaxDBFound(false), m_isBl2seq(false), m_isIterative(true), m_Matrix(NULL)
{
	x_InitCommon(resultSet[0], opts);
	x_InitDB(dbsInfo);
	resultSet[0].GetMaskedQueryRegions(m_QueryMasks);
	for(unsigned int i = 0; i < resultSet.size(); i++) {
		x_InitResults(resultSet[i]);
	}
}

CCmdLineBlastXML2ReportData::CCmdLineBlastXML2ReportData(
	CConstRef<CBlastSearchQuery> query,
    const CSearchResultSet& resultSet,
    CConstRef<CBlastOptions> opts,
    CRef<CScope>  scope,
    CConstRef<IBlastSeqInfoSrc> subjectsInfo) :
    	m_Query(query), m_Options (opts), m_Scope(scope),
    	m_DbName(kEmptyStr), m_NumSequences(0), m_NumBases(0),
    	m_TaxDBFound(false), m_isBl2seq(true), m_isIterative(true), m_Matrix(NULL)
{
	x_InitCommon(resultSet[0], opts);
	x_InitSubjects(subjectsInfo);
	resultSet[0].GetMaskedQueryRegions(m_QueryMasks);
	for(unsigned int i = 0; i < resultSet.size(); i++) {
		x_InitResults(resultSet[i]);
	}
}

void
CCmdLineBlastXML2ReportData::x_InitResults(const CSearchResults & results)
{
	m_Alignments.push_back(results.GetSeqAlign());
    m_AncillaryData.push_back(results.GetAncillaryData());
    string errors = results.GetErrorStrings();
    if (results.HasWarnings()) {
    	if ( !errors.empty() ) {
    		errors += " ";
        }
        errors += results.GetWarningStrings();
    }
    if ( !results.HasAlignments() ) {
         errors += (errors.empty() ? kEmptyStr : " ");
         errors += CBlastFormatUtil::kNoHitsFound;
    }

    m_Errors.push_back(errors);
}

void
CCmdLineBlastXML2ReportData::x_InitCommon(
	const CSearchResults & results,
    CConstRef<CBlastOptions> opts)
{
	if(opts.Empty()) {
		NCBI_THROW(CException, eUnknown, "blastxml2: Empty blast options");
	}

	if(m_Scope.Empty()) {
		NCBI_THROW(CException, eUnknown, "blastxml2: Empty scope");
	}

	x_FillScoreMatrix(m_Options->GetMatrixName());

	string resolved = SeqDB_ResolveDbPath("taxdb.bti");
	if(!resolved.empty()) {
		m_TaxDBFound = true;
	}

  	m_isIterative  = opts->IsIterativeSearch();
}

void
CCmdLineBlastXML2ReportData::x_InitDB(
	const vector<CAlignFormatUtil::SDbInfo> & dbsInfo)
{
	if(dbsInfo.empty()){
		NCBI_THROW(CException, eUnknown, "blastxml2: Empty db info");
	}
	ITERATE(vector<CBlastFormatUtil::SDbInfo>, i, dbsInfo) {
		if(i != dbsInfo.begin()) {
			m_DbName += " " ;
		}
		m_DbName += i->name;
		m_NumSequences += i->number_seqs;
		m_NumBases += i->total_length;
	}
}

void
CCmdLineBlastXML2ReportData::x_InitSubjects(CConstRef<IBlastSeqInfoSrc> subjectsInfo)
{
	if(subjectsInfo->Size() == 0) {
		NCBI_THROW(CException, eUnknown, "blastxml2: Empty seq info src");
	}

	for(unsigned int i =0; i < subjectsInfo->Size(); i++) {
		list<CRef<objects::CSeq_id> > ids = subjectsInfo->GetId(i);
		m_SubjectIds.push_back(CAlignFormatUtil::GetSeqIdString(ids, true));
	}
}

CCmdLineBlastXML2ReportData::~CCmdLineBlastXML2ReportData()
{
	if(m_Matrix) {
		delete m_Matrix;
	}
}

void
CCmdLineBlastXML2ReportData::x_FillScoreMatrix(const char *matrix_name)
{
    if (matrix_name == NULL)
        return;

    int matrix[kMatrixCols][kMatrixCols];
    int * tmp[kMatrixCols];
    const SNCBIPackedScoreMatrix *packed_matrix = 0;

    if (strcmp(matrix_name, "BLOSUM45") == 0)
        packed_matrix = &NCBISM_Blosum45;
    else if (strcmp(matrix_name, "BLOSUM50") == 0)
        packed_matrix = &NCBISM_Blosum50;
    else if (strcmp(matrix_name, "BLOSUM62") == 0)
        packed_matrix = &NCBISM_Blosum62;
    else if (strcmp(matrix_name, "BLOSUM80") == 0)
        packed_matrix = &NCBISM_Blosum80;
    else if (strcmp(matrix_name, "BLOSUM90") == 0)
        packed_matrix = &NCBISM_Blosum90;
    else if (strcmp(matrix_name, "PAM30") == 0)
        packed_matrix = &NCBISM_Pam30;
    else if (strcmp(matrix_name, "PAM70") == 0)
        packed_matrix = &NCBISM_Pam70;
    else if (strcmp(matrix_name, "PAM250") == 0)
        packed_matrix = &NCBISM_Pam250;
    else if (strcmp(matrix_name, "IDENTITY") == 0)
    	packed_matrix = &NCBISM_Identity;
    else {
        string prog_name = Blast_ProgramNameFromType(
                                           m_Options->GetProgramType());
        if (prog_name != "blastn" && prog_name != "megablast") {
            NCBI_THROW(blast::CBlastException, eInvalidArgument,
                        "unsupported score matrix");
        }
    }

    if (packed_matrix) {
        SNCBIFullScoreMatrix m;

        NCBISM_Unpack(packed_matrix, &m);

        for (unsigned int i = 0; i < kMatrixCols; i++) {
        	tmp[i] = matrix[i];
            for (unsigned int j = 0; j < kMatrixCols; j++) {
                matrix[i][j] = m.s[i][j];
            }
        }
    }

    m_Matrix = (new CBlastFormattingMatrix(tmp, kMatrixCols, kMatrixCols));
}

string
CCmdLineBlastXML2ReportData::GetBlastProgramName(void) const
{
	// Program type for deltablast is eBlastTypePsiBlast, because the
	// sequence search is done by CPsiBlast
	if ( m_Options->GetProgram() == blast::eDeltaBlast) {
    	return "deltablast";
	}
	return blast::Blast_ProgramNameFromType(m_Options->GetProgramType());
}

double
CCmdLineBlastXML2ReportData::GetLambda(int num) const
{
    if (num >= (int)m_AncillaryData.size()) {
		NCBI_THROW(CException, eUnknown, "blastxml2: Invalid iteration number");
    }

    const Blast_KarlinBlk *kbp = 
                   m_AncillaryData[num]->GetGappedKarlinBlk();
    if (kbp)
        return kbp->Lambda;

    kbp = m_AncillaryData[num]->GetUngappedKarlinBlk();
    if (kbp)
        return kbp->Lambda;
    return -1.0;
}

double
CCmdLineBlastXML2ReportData::GetKappa(int num) const
{
    if (num >= (int)m_AncillaryData.size()) {
		NCBI_THROW(CException, eUnknown, "blastxml2: Invalid iteration number");
    }

    const Blast_KarlinBlk *kbp = 
                     m_AncillaryData[num]->GetGappedKarlinBlk();
    if (kbp)
        return kbp->K;

    kbp = m_AncillaryData[num]->GetUngappedKarlinBlk();
    if (kbp)
        return kbp->K;
    return -1.0;
}

double
CCmdLineBlastXML2ReportData::GetEntropy(int num) const
{
    if (num >= (int)m_AncillaryData.size()) {
		NCBI_THROW(CException, eUnknown, "blastxml2: Invalid iteration number");
    }

    const Blast_KarlinBlk *kbp = 
                        m_AncillaryData[num]->GetGappedKarlinBlk();
    if (kbp)
        return kbp->H;

    kbp = m_AncillaryData[num]->GetUngappedKarlinBlk();
    if (kbp)
        return kbp->H;
    return -1.0;
}

CBlastFormattingMatrix*
CCmdLineBlastXML2ReportData::GetMatrix(void) const
{
    return m_Matrix;
}

int
CCmdLineBlastXML2ReportData::GetLengthAdjustment(int num) const
{
    if (num >= (int)m_AncillaryData.size()) {
		NCBI_THROW(CException, eUnknown, "blastxml2: Invalid iteration number");
    }
    return (int)m_AncillaryData[num]->GetLengthAdjustment();
}

CConstRef<CSeq_align_set>
CCmdLineBlastXML2ReportData::GetAlignmentSet(int num) const
{
	if (num >= (int) m_Alignments.size()) {
       	NCBI_THROW(CException, eUnknown, "blastxml2: Invalid iteration number");
    }
    return m_Alignments[num];
}

Int8
CCmdLineBlastXML2ReportData::GetEffectiveSearchSpace(int num) const
{
   	if (num >= (int)m_AncillaryData.size()) {
       	NCBI_THROW(CException, eUnknown, "blastxml2: Invalid iteration number");
    }
    return m_AncillaryData[num]->GetSearchSpace();
}

int CCmdLineBlastXML2ReportData::GetQueryGeneticCode() const
{
	if(Blast_QueryIsTranslated(m_Options->GetProgramType()))
		return m_Options->GetQueryGeneticCode();

	return 0;
}

int CCmdLineBlastXML2ReportData::GetDbGeneticCode() const
{
	if(Blast_SubjectIsTranslated(m_Options->GetProgramType()))
		return m_Options->GetDbGeneticCode();

	return 0;
}
