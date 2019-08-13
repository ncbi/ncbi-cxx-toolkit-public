/*
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
* Author: Jason Papadopoulos, Christiam Camacho
* 
*/

/** @file data4xmlformat.cpp
 * Produce data required for generating BLAST XML output
 */

#include <ncbi_pch.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <util/tables/raw_scoremat.h>
#include <algo/blast/format/data4xmlformat.hpp>       /* NCBI_FAKE_WARNING */

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
USING_SCOPE(align_format);
#endif

CCmdLineBlastXMLReportData::CCmdLineBlastXMLReportData
    (CRef<blast::CBlastQueryVector> queries,
     const blast::CSearchResultSet& results,
     const blast::CBlastOptions& opts,
     const string& dbname, bool db_is_aa,
     int qgencode, int dbgencode,
     bool is_remote,
     int dbfilt_algorithm /* = -1 */)
: m_Queries(queries), m_Options(opts), 
  m_DbName(dbname),
  m_QueryGeneticCode(qgencode), 
  m_DbGeneticCode(dbgencode),
  m_NoHitsFound(false),
  m_NumSequences(0),
  m_NumBases(0)
{
    _ASSERT( !m_Queries->Empty() );

    vector<CBlastFormatUtil::SDbInfo> dbinformation;
    if ( !m_DbName.empty() ){
    	CBlastFormatUtil::GetBlastDbInfo(dbinformation, m_DbName, db_is_aa,
    									dbfilt_algorithm, is_remote);
    }

	x_Init(queries, results, opts, dbinformation, qgencode, dbgencode, is_remote, dbfilt_algorithm);

}

CCmdLineBlastXMLReportData::CCmdLineBlastXMLReportData
		  (CRef<blast::CBlastQueryVector> queries,
           const blast::CSearchResultSet& results,
           const blast::CBlastOptions& opts,
           const vector<CAlignFormatUtil::SDbInfo> & dbInfo,
           int qgencode,
           int dbgencode,
           bool is_remote,
           int dbfilt_algorithm)
           : m_Queries(queries), m_Options(opts),
             m_DbName(kEmptyStr),
             m_QueryGeneticCode(qgencode),
             m_DbGeneticCode(dbgencode),
             m_NoHitsFound(false),
             m_NumSequences(0),
             m_NumBases(0)
{
	 _ASSERT(!m_Queries->Empty());
	 _ASSERT(!dbInfo.empty());

	ITERATE(vector<CBlastFormatUtil::SDbInfo>, i, dbInfo) {
		if(i != dbInfo.begin())
			m_DbName += " " ;
		m_DbName += i->name;
	}
	 x_Init(queries, results, opts, dbInfo, qgencode, dbgencode, is_remote, dbfilt_algorithm);
}

void
CCmdLineBlastXMLReportData::x_Init
		  (CRef<blast::CBlastQueryVector> queries,
           const blast::CSearchResultSet& results,
           const blast::CBlastOptions& opts,
           const vector<CAlignFormatUtil::SDbInfo> & dbInfo,
           int qgencode,
           int dbgencode,
           bool is_remote,
           int dbfilt_algorithm)
{

	x_FillScoreMatrix(m_Options.GetMatrixName());

	if(!dbInfo.empty()) {
		ITERATE(vector<CBlastFormatUtil::SDbInfo>, i, dbInfo) {
			m_NumSequences += i->number_seqs;
			m_NumBases += i->total_length;
		}
	}

	/// @todo FIXME add means to specify masked database (SB-343)
	// Is this appropriate? What if it breaks parsers?
	//if (dbfilt_algorithm != -1) {
	//    int x = 0; // should be the index of the masked DB
	//    _ASSERT(!dbinformation[x].filt_algorithm_name.empty());
	//    m_DbName += ", masked using: '" + dbinformation[x].filt_algorithm_name + "'";
	//    if ( !dbinformation[x].filt_algorithm_options.empty() ) {
	//        m_DbName += ", options: '" + dbinformation[x].filt_algorithm_options + "'";
	//    }
	//}

    if (results.size() == 0) {
        m_NoHitsFound = true;
        m_Errors.insert(m_Errors.end(), m_Queries->Size(),
                        CBlastFormatUtil::kNoHitsFound);
    } else {

        if (opts.GetProgram() == ePSIBlast && m_Queries->Size() == 1) {
            // artificially increment the number of 'queries' to match the
            // number of results, which represents the actual number of
            // iterations in PSI-BLAST
            for (size_t i = 0; i < results.size() - 1; i++) {
                m_Queries->AddQuery(m_Queries->GetBlastSearchQuery(0));
            }
        }

        m_Masks.resize(GetNumQueries());
        for (size_t i = 0; i < GetNumQueries(); i++) {

            m_Alignments.push_back(results[i].GetSeqAlign());
            m_AncillaryData.push_back(results[i].GetAncillaryData());
            results[i].GetMaskedQueryRegions(m_Masks[i]);

            // Check in case there are any errors/warnings
            {
                string errors = results[i].GetErrorStrings();
                if (results[i].HasWarnings()) {
                    if ( !errors.empty() ) {
                        errors += " ";
                    }
                    errors += results[i].GetWarningStrings();
                }
                if ( !results[i].HasAlignments() ) {
                    errors += (errors.empty() ? kEmptyStr : " ");
                    errors += CBlastFormatUtil::kNoHitsFound;
                }
                m_Errors.push_back(errors);
            }
        }
    }

}


CCmdLineBlastXMLReportData::~CCmdLineBlastXMLReportData()
{
    for (unsigned int i = 0; i < kMatrixCols; i++)
        delete [] m_Matrix[i];
}


void
CCmdLineBlastXMLReportData::x_FillScoreMatrix(const char *matrix_name)
{
    for (unsigned int i = 0; i < kMatrixCols; i++)
        m_Matrix[i] = new int[kMatrixCols];

    if (matrix_name == NULL)
        return;

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
                                           m_Options.GetProgramType());
        if (prog_name != "blastn" && prog_name != "megablast") {
            NCBI_THROW(blast::CBlastException, eInvalidArgument,
                        "unsupported score matrix");
        }
    }

    if (packed_matrix) {
        SNCBIFullScoreMatrix m;

        NCBISM_Unpack(packed_matrix, &m);

        for (unsigned int i = 0; i < kMatrixCols; i++) {
            for (unsigned int j = 0; j < kMatrixCols; j++) {
                m_Matrix[i][j] = m.s[i][j];
            }
        }
    }
}

double
CCmdLineBlastXMLReportData::GetLambda(int query_index) const
{
    if (m_NoHitsFound || query_index >= (int)m_AncillaryData.size()) {
        return -1.0;
    }

    const Blast_KarlinBlk *kbp = 
                   m_AncillaryData[query_index]->GetGappedKarlinBlk();
    if (kbp)
        return kbp->Lambda;

    kbp = m_AncillaryData[query_index]->GetUngappedKarlinBlk();
    if (kbp)
        return kbp->Lambda;
    return -1.0;
}

double
CCmdLineBlastXMLReportData::GetKappa(int query_index) const
{
    if (m_NoHitsFound || query_index >= (int)m_AncillaryData.size()) {
        return -1.0;
    }

    const Blast_KarlinBlk *kbp = 
                     m_AncillaryData[query_index]->GetGappedKarlinBlk();
    if (kbp)
        return kbp->K;

    kbp = m_AncillaryData[query_index]->GetUngappedKarlinBlk();
    if (kbp)
        return kbp->K;
    return -1.0;
}

double
CCmdLineBlastXMLReportData::GetEntropy(int query_index) const
{
    if (m_NoHitsFound || query_index >= (int)m_AncillaryData.size()) {
        return -1.0;
    }

    const Blast_KarlinBlk *kbp = 
                        m_AncillaryData[query_index]->GetGappedKarlinBlk();
    if (kbp)
        return kbp->H;

    kbp = m_AncillaryData[query_index]->GetUngappedKarlinBlk();
    if (kbp)
        return kbp->H;
    return -1.0;
}

CBlastFormattingMatrix* CCmdLineBlastXMLReportData::GetMatrix(void) const
{
    return new CBlastFormattingMatrix((int **)m_Matrix,
                                      kMatrixCols, kMatrixCols); 
}

int CCmdLineBlastXMLReportData::GetLengthAdjustment(int query_index ) const {
    if (m_NoHitsFound || query_index >= (int)m_AncillaryData.size()) {
        return 0;
    }
    return (int)m_AncillaryData[query_index]->GetLengthAdjustment();
}
