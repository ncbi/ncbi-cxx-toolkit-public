#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_psi_cxx.cpp
 * Implementation of the C++ API for the PSI-BLAST PSSM generation engine.
 */

#include <ncbi_pch.hpp>
#include <sstream>

#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/blast_psi.hpp>
#include "blast_setup.hpp"

// Object includes
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmParameters.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmIntermediateData.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/scoremat/FormatRpsDbParameters.hpp>

// Core BLAST includes
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_setup.h>
#include "../core/blast_psi_priv.h"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CPssmEngine::CPssmEngine(IPssmInputData* input)
    : m_PssmInput(input)
{
    x_CheckAgainstNullData();
    m_ScoreBlk.Reset(x_InitializeScoreBlock(m_PssmInput->GetQuery(),
                                            m_PssmInput->GetQueryLength(), 
                                            m_PssmInput->GetMatrixName()));
}

CPssmEngine::~CPssmEngine()
{
}

void
CPssmEngine::x_CheckAgainstNullData()
{
    if ( !m_PssmInput ) {
        NCBI_THROW(CBlastException, eBadParameter,
           "IPssmInputData is NULL");
    }

    if ( !m_PssmInput->GetOptions() ) {
        NCBI_THROW(CBlastException, eBadParameter,
           "IPssmInputData returns NULL PSIBlastOptions");
    }

    if ( !m_PssmInput->GetQuery() ) {
        NCBI_THROW(CBlastException, eBadParameter, 
           "IPssmInputData returns NULL query sequence");
    }

    if (m_PssmInput->GetQueryLength() == 0) {
        NCBI_THROW(CBlastException, eBadParameter, 
           "Query length provided by IPssmInputData is 0");
    }
}

string
CPssmEngine::x_ErrorCodeToString(int error_code)
{
    string retval;

    switch (error_code) {
    case PSI_SUCCESS:
        retval = "No error detected";
        break;

    case PSIERR_BADPARAM:
        retval = "Bad argument to function detected";
        break;

    case PSIERR_OUTOFMEM:
        retval = "Out of memory";
        break;

    case PSIERR_BADSEQWEIGHTS:
        retval = "Error computing sequence weights";
        break;

    case PSIERR_NOFREQRATIOS:
        retval = "No matrix frequency ratios were found for requested matrix";
        break;

    case PSIERR_POSITIVEAVGSCORE:
        retval = "PSSM has positive average score";
        break;

    case PSIERR_NOALIGNEDSEQS:
        retval = "No sequences left after purging biased sequences in ";
        retval += "multiple sequence alignment";
        break;

    case PSIERR_GAPINQUERY:
        retval = "Gap found in query sequence";
        break;

    case PSIERR_UNALIGNEDCOLUMN:
        retval = "Found column with no sequences aligned in it";
        break;

    case PSIERR_COLUMNOFGAPS:
        retval = "Found column with only GAP residues";
        break;

    case PSIERR_STARTINGGAP:
        retval = "Found flanking gap at start of alignment";
        break;

    case PSIERR_ENDINGGAP:
        retval = "Found flanking gap at end of alignment";
        break;

    default:
        retval = "Unknown error code";
    }

    return retval;
}

// This method is the core of this class. It delegates the extraction of
// information from the pairwise alignments into a multiple sequence alignment
// structure to its IPsiAlignmentProcessor strategy.
// Creating the PSSM is then delegated to the core PSSM engine.
// Afterwards the results are packaged in a scoremat (structure defined from
// the ASN.1 specification).
CRef<CPssmWithParameters>
CPssmEngine::Run()
{
    m_PssmInput->Process();
    x_Validate();

    CPSIMatrix pssm;
    CPSIDiagnosticsResponse diagnostics;
    int status = 
        PSICreatePssmWithDiagnostics(m_PssmInput->GetData(),
                                     m_PssmInput->GetOptions(),
                                     m_ScoreBlk, 
                                     m_PssmInput->GetDiagnosticsRequest(),
                                     &pssm, 
                                     &diagnostics);
    if (status != PSI_SUCCESS) {
        // FIXME: need to use core level perror-like facility
        string msg = x_ErrorCodeToString(status);
        NCBI_THROW(CBlastException, eInternal, msg);
    }

    // Convert core BLAST matrix structure into ASN.1 score matrix object
    CRef<CPssmWithParameters> retval(NULL);
    retval = x_PSIMatrix2Asn1(pssm, m_PssmInput->GetOptions(), diagnostics);

    return retval;
}

unsigned char*
CPssmEngine::x_GuardProteinQuery(const unsigned char* query,
                                 unsigned int query_length)
{
    ASSERT(query);

    unsigned char* retval = NULL;
    retval = (unsigned char*) malloc(sizeof(unsigned char)*(query_length + 2));
    if ( !retval ) {
        NCBI_THROW(CBlastException, eOutOfMemory, "Query with sentinels");
    }

    retval[0] = retval[query_length+1] = GetSentinelByte(BLASTP_ENCODING);
    memcpy((void*) &retval[1], (void*) query, query_length);
    return retval;
}

BlastQueryInfo*
CPssmEngine::x_InitializeQueryInfo(unsigned int query_length)
{
    BlastQueryInfo* retval = NULL;

    retval = (BlastQueryInfo*) calloc(1, sizeof(BlastQueryInfo));
    if ( !retval ) {
        NCBI_THROW(CBlastException, eOutOfMemory, "BlastQueryInfo");
    }

    retval->num_queries             = 1;
    retval->first_context           = 0;
    retval->last_context            = 0;
    retval->context_offsets         = (Int4*) calloc(2, sizeof(Int4));
    retval->context_offsets[1]      = query_length + 1;
    retval->eff_searchsp_array      = (Int8*) calloc(1, sizeof(Int8));
    retval->length_adjustments      = (Int4*) calloc(1, sizeof(Int4));
    retval->max_length              = query_length;

    return retval;
}

BlastScoreBlk*
CPssmEngine::x_InitializeScoreBlock(const unsigned char* query,
                                    unsigned int query_length,
                                    const char* matrix_name)
{
    ASSERT(query);
    ASSERT(matrix_name);

    // This program type will need to be changed when psi-tblastn is
    // implemented
    const EBlastProgramType kProgramType = eBlastTypeBlastp;
    short status = 0;

    AutoPtr<unsigned char, CDeleter<unsigned char> > guarded_query;
    guarded_query.reset(x_GuardProteinQuery(query, query_length));

    // Setup the scoring options
    CBlastScoringOptions opts;
    status = BlastScoringOptionsNew(kProgramType, &opts);
    if (status != 0) {
        NCBI_THROW(CBlastException, eOutOfMemory, "BlastScoringOptions");
    }
    opts->matrix = strdup(matrix_name);
    opts->matrix_path = strdup(FindMatrixPath(opts->matrix, true).c_str());

    // Setup the sequence block structure
    CBLAST_SequenceBlk query_blk;
    status = BlastSeqBlkNew(&query_blk);
    if (status != 0) {
        NCBI_THROW(CBlastException, eOutOfMemory, "BLAST_SequenceBlk");
    }
    
    // Populate the sequence block structure, transferring ownership of the
    // guarded protein sequence
    status = BlastSeqBlkSetSequence(query_blk, guarded_query.release(),
                                    query_length);
    if (status != 0) {
        // should never happen, previous function only performs assignments
        abort();    
    }

    // Setup the query info structure
    CBlastQueryInfo query_info(x_InitializeQueryInfo(query_length));

    BlastScoreBlk* retval = NULL;
    Blast_Message* errors = NULL;
    const double kScaleFactor = 1.0;
    const bool kIsPhiBlast = false;
    status = BlastSetup_GetScoreBlock(query_blk,
                                      query_info,
                                      opts,
                                      kProgramType,
                                      kIsPhiBlast,
                                      &retval,
                                      kScaleFactor,
                                      &errors);
    if (status != 0) {
        retval = BlastScoreBlkFree(retval);
        if (errors) {
            string msg(errors->message);
            errors = Blast_MessageFree(errors);
            NCBI_THROW(CBlastException, eInternal, msg);
        } else {
            NCBI_THROW(CBlastException, eInternal, 
                       "Unknown error when setting up BlastScoreBlk");
        }
    }

    /*********************************************************************/
    // MUST INITIALIZE THIS: should be moved to setup code
    retval->kbp_ideal = Blast_KarlinBlkIdealCalc(retval);

    return retval;
}

void
CPssmEngine::x_Validate()
{
    if ( !m_PssmInput->GetData() ) {
        NCBI_THROW(CBlastException, eBadParameter,
           "IPssmInputData returns NULL multiple sequence alignment");
    }

    x_ValidateNoFlankingGaps();
    x_ValidateNoGapsInQuery();
    x_ValidateAlignedColumns();
}

void
CPssmEngine::x_ValidateNoFlankingGaps()
{
    const PSIMsa* msa = m_PssmInput->GetData();
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA[(Uint1)'-'];
    ostringstream os;

    ASSERT(msa);

    // Look for starting gaps in alignments
    unsigned int i = 
        m_PssmInput->GetOptions()->nsg_ignore_consensus == TRUE ? 1 : 0;
    for ( ; i < msa->dimensions->num_seqs + 1; i++) {
        // find the first aligned residue
        for (unsigned int j = 0; j < m_PssmInput->GetQueryLength(); j++) {
            if (msa->data[i][j].is_aligned) {
                if (msa->data[i][j].letter == GAP) {
                    os << "Gap at start of alignment is not allowed: "
                       << "sequence " << i << " position " << j << endl;
                }
                break;
            }
        }
    }

    // Look for ending gaps in alignments
    i = m_PssmInput->GetOptions()->nsg_ignore_consensus == TRUE ? 1 : 0;
    for ( ; i < msa->dimensions->num_seqs + 1; i++) {
        // find the last aligned residue
        for (unsigned int j = m_PssmInput->GetQueryLength() - 1; j >= 0; j--) {
            if (msa->data[i][j].is_aligned) {
                if (msa->data[i][j].letter == GAP) {
                    os << "Gap at end of alignment is not allowed: "
                       << "sequence " << i << " position " << j << endl;
                }
                break;
            }
        }
    }

    if (os.str().length() != 0) {
        NCBI_THROW(CBlastException, eBadParameter, os.str());
    }
}

void
CPssmEngine::x_ValidateNoGapsInQuery()
{
    if (m_PssmInput->GetOptions()->nsg_ignore_consensus) {
        return;
    }

    const PSIMsa* msa = m_PssmInput->GetData();
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA[(Uint1)'-'];
    ostringstream os;

    ASSERT(msa);

    // Look for gaps in query sequence
    for (unsigned int i = 0; i < m_PssmInput->GetQueryLength(); i++) {
        if (m_PssmInput->GetQuery()[i] == GAP ||
            msa->data[kQueryIndex][i].letter == GAP) {
            os << "Found GAP at query position " << i << endl;
            break;
        }
    }

    if (os.str().length() != 0) {
        NCBI_THROW(CBlastException, eBadParameter, os.str());
    }
}

void
CPssmEngine::x_ValidateAlignedColumns()
{
    const PSIMsa* msa = m_PssmInput->GetData();
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA[(Uint1)'-'];
    ostringstream os;

    for (unsigned int i = 0; i < m_PssmInput->GetQueryLength(); i++) {
        bool found_aligned_sequence = false;
        bool found_non_gap_residue = false;

        unsigned int j = 
            m_PssmInput->GetOptions()->nsg_ignore_consensus == TRUE ? 1 : 0;
        for ( ; j < msa->dimensions->num_seqs + 1; j++) {
            if (msa->data[j][i].is_aligned) {
                found_aligned_sequence = true;
                if (msa->data[j][i].letter != GAP) {
                    found_non_gap_residue = true;
                    break;
                }
            }
        }        
        if (!found_aligned_sequence) {
            os << "Found completely unaligned column at position " << i << endl;
            continue;
        }
        if (!found_non_gap_residue) {
            os << "Found column of all GAP residues at position " << i << endl;
        }
    }

    if (os.str().length() != 0) {
        NCBI_THROW(CBlastException, eBadParameter, os.str());
    }
}

CRef<CPssmWithParameters>
CPssmEngine::x_PSIMatrix2Asn1(const PSIMatrix* pssm,
                              const PSIBlastOptions* opts,
                              const PSIDiagnosticsResponse* diagnostics)
{
    ASSERT(pssm);

    CRef<CPssmWithParameters> retval(new CPssmWithParameters);

    // Record the parameters
    retval->SetParams().SetPseudocount(opts->pseudo_count);
    string mtx(m_PssmInput->GetMatrixName());
    mtx = NStr::ToUpper(mtx); // save the matrix name in all capital letters
    retval->SetParams().SetRpsdbparams().SetMatrixName(mtx);

    CPssm& asn1_pssm = retval->SetPssm();
    asn1_pssm.SetIsProtein(true);
    asn1_pssm.SetNumRows(pssm->nrows);
    asn1_pssm.SetNumColumns(pssm->ncols);
    asn1_pssm.SetByRow(false);

    asn1_pssm.SetFinalData().SetLambda(pssm->lambda);
    asn1_pssm.SetFinalData().SetKappa(pssm->kappa);
    asn1_pssm.SetFinalData().SetH(pssm->h);
    for (unsigned int i = 0; i < pssm->ncols; i++) {
        for (unsigned int j = 0; j < pssm->nrows; j++) {
            asn1_pssm.SetFinalData().SetScores().push_back(pssm->pssm[i][j]);
        }
    }

    /********** Collect information from diagnostics structure ************/
    if ( !diagnostics ) {
        return retval;
    }

    ASSERT(pssm->nrows == diagnostics->alphabet_size);
    ASSERT(pssm->ncols == diagnostics->query_length);

    if (diagnostics->information_content) {
        NCBI_THROW(CBlastException, eNotSupported, "Information content "
                   "cannot be stored in PssmWithParameters ASN.1");
    }

    if (diagnostics->residue_freqs) {
        CPssmIntermediateData::TResFreqsPerPos& res_freqs =
            asn1_pssm.SetIntermediateData().SetResFreqsPerPos();
        for (unsigned int i = 0; i < pssm->ncols; i++) {
            for (unsigned int j = 0; j < pssm->nrows; j++) {
                res_freqs.push_back(diagnostics->residue_freqs[i][j]);
            }
        }
    }
 
    if (diagnostics->weighted_residue_freqs) {
        CPssmIntermediateData::TWeightedResFreqsPerPos& wres_freqs =
            asn1_pssm.SetIntermediateData().SetWeightedResFreqsPerPos();
        for (unsigned int i = 0; i < pssm->ncols; i++) {
            for (unsigned int j = 0; j < pssm->nrows; j++) {
                wres_freqs.push_back(diagnostics->weighted_residue_freqs[i][j]);
            }
        }
    }

    if (diagnostics->frequency_ratios) {
        CPssmIntermediateData::TFreqRatios& freq_ratios = 
            asn1_pssm.SetIntermediateData().SetFreqRatios();
        for (unsigned int i = 0; i < pssm->ncols; i++) {
            for (unsigned int j = 0; j < pssm->nrows; j++) {
                freq_ratios.push_back(diagnostics->frequency_ratios[i][j]);
            }
        }
    }

    if (diagnostics->gapless_column_weights) {
        NCBI_THROW(CBlastException, eNotSupported, "Gapless column weights "
                   "cannot be stored in PssmWithParameters ASN.1");
    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.21  2004/11/22 14:38:57  camacho
 * + option to set % identity threshold to PSSM engine
 *
 * Revision 1.20  2004/11/02 20:37:34  camacho
 * Doxygen fixes
 *
 * Revision 1.19  2004/10/18 14:34:37  camacho
 * Added function to convert PSSM engine error codes to strings
 *
 * Revision 1.18  2004/10/14 19:10:48  camacho
 * Fix compiler warning
 *
 * Revision 1.17  2004/10/13 20:49:00  camacho
 * + support for requesting diagnostics information and specifying underlying matrix
 *
 * Revision 1.16  2004/10/13 15:44:47  camacho
 * + validation for columns in multiple sequence alignment
 *
 * Revision 1.15  2004/10/12 21:22:59  camacho
 * + validation methods
 *
 * Revision 1.14  2004/10/12 14:19:18  camacho
 * Update for scoremat.asn reorganization
 *
 * Revision 1.13  2004/10/08 20:20:11  camacho
 * Throw an exception is the query length is 0
 *
 * Revision 1.12  2004/10/06 18:21:08  camacho
 * Fixed contents of posFreqs field in scoremat structure
 *
 * Revision 1.11  2004/09/17 02:06:53  camacho
 * Renaming of diagnostics structure fields
 *
 * Revision 1.10  2004/08/05 18:55:09  camacho
 * Changes to use the most recent scoremat.asn specification
 *
 * Revision 1.9  2004/08/04 20:27:04  camacho
 * 1. Use of CPSIMatrix and CPSIDiagnosticsResponse classes instead of bare
 *    pointers to C structures.
 * 2. Completed population of CScore_matrix return value.
 * 3. Better error handling.
 *
 * Revision 1.8  2004/08/02 13:42:41  camacho
 * Fix warning
 *
 * Revision 1.7  2004/08/02 13:29:00  camacho
 * +Initialization of BlastScoreBlk and population of Score_matrix structure
 *
 * Revision 1.6  2004/07/29 17:54:29  camacho
 * Redesigned to use strategy pattern
 *
 * Revision 1.5  2004/06/21 12:53:05  camacho
 * Replace PSI_ALPHABET_SIZE for BLASTAA_SIZE
 *
 * Revision 1.4  2004/06/09 14:32:23  camacho
 * Added use_best_alignment option
 *
 * Revision 1.3  2004/05/28 17:42:02  camacho
 * Fix compiler warning
 *
 * Revision 1.2  2004/05/28 17:15:43  camacho
 * Fix NCBI {BEGIN,END}_SCOPE macro usage, remove warning
 *
 * Revision 1.1  2004/05/28 16:41:39  camacho
 * Initial revision
 *
 *
 * ===========================================================================
 */
