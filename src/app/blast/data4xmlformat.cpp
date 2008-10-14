#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

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
#include "blast_format.hpp"
#include "blast_app_util.hpp"
#include "data4xmlformat.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

CCmdLineBlastXMLReportData::CCmdLineBlastXMLReportData
    (CRef<blast::CBlastQueryVector> queries,
     const blast::CSearchResultSet& results,
     const blast::CBlastOptions& opts,
     const string& dbname, bool db_is_aa,
     int qgencode, int dbgencode,
     const vector<int>& dbfilt_algorithms /* = vector<int>() */)
: m_Queries(queries), m_Options(opts), 
  m_DbName(dbname),
  m_QueryGeneticCode(qgencode), 
  m_DbGeneticCode(dbgencode),
  m_NoHitsFound(false)
{
    _ASSERT( !m_Queries->Empty() );

    x_FillScoreMatrix(m_Options.GetMatrixName());
    if (m_DbName.empty() ||
        results.GetResultType() != eDatabaseSearch) {
        NCBI_THROW(CBlastException, eNotSupported,
                   "XML formatting is only supported for a database search");
    }
    BlastFormat_GetBlastDbInfo(m_DbInfo, m_DbName, db_is_aa,
                               dbfilt_algorithms);

    if (results.size() == 0) {
        m_NoHitsFound = true;
        m_Errors.insert(m_Errors.end(), m_Queries->Size(),
                        CBlastFormat::kNoHitsFound);
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

            if (i < results.size()) {
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
                    m_Errors.push_back(errors);
                }
            } else {
                m_Errors.push_back(CBlastFormat::kNoHitsFound);
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
