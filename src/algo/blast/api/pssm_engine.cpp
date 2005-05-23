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

/// This function makes sure that none of the required data is returned as NULL
/// or "empty"
/// @param pssm_input_msa interface which provides the data [in]
/// @throw CBlastException in case of validation failure
static void
s_CheckAgainstNullData(IPssmInputData* pssm_input_msa)
{
    if ( !pssm_input_msa ) {
        NCBI_THROW(CBlastException, eBadParameter,
           "IPssmInputData is NULL");
    }

    if ( !pssm_input_msa->GetOptions() ) {
        NCBI_THROW(CBlastException, eBadParameter,
           "IPssmInputData returns NULL PSIBlastOptions");
    }

    if ( !pssm_input_msa->GetQuery() ) {
        NCBI_THROW(CBlastException, eBadParameter, 
           "IPssmInputData returns NULL query sequence");
    }

    if (pssm_input_msa->GetQueryLength() == 0) {
        NCBI_THROW(CBlastException, eBadParameter, 
           "Query length provided by IPssmInputData is 0");
    }
}

/// This function makes sure that none of the required data is returned as NULL
/// or "empty"
/// @param pssm_input_freqratios interface which provides the data [in]
/// @throw CBlastException in case of validation failure
static void
s_CheckAgainstNullData(IPssmInputFreqRatios* pssm_input_freqratios)
{
    if ( !pssm_input_freqratios ) {
        NCBI_THROW(CBlastException, eBadParameter,
           "IPssmInputFreqRatios is NULL");
    }

    if ( !pssm_input_freqratios->GetQuery() ) {
        NCBI_THROW(CBlastException, eBadParameter, 
           "IPssmInputFreqRatiosFreqRatios returns NULL query sequence");
    }

    const unsigned int kQueryLength = pssm_input_freqratios->GetQueryLength();
    if (kQueryLength == 0) {
        NCBI_THROW(CBlastException, eBadParameter, 
           "Query length provided by IPssmInputFreqRatiosFreqRatios is 0");
    }

    if (pssm_input_freqratios->GetData().GetCols() != kQueryLength) {
        NCBI_THROW(CBlastException, eBadParameter, 
           "Number of columns returned by IPssmInputFreqRatiosFreqRatios does "
           "not match query length");
    }
    if (pssm_input_freqratios->GetData().GetRows() != BLASTAA_SIZE) {
        NCBI_THROW(CBlastException, eBadParameter, 
           "Number of rows returned by IPssmInputFreqRatiosFreqRatios differs "
           "from " + NStr::IntToString(BLASTAA_SIZE));
    }
}

/// Performs validation on data provided before invoking the CORE PSSM
/// engine. Should be called after invoking Process() on its argument
/// @throws CBlastException if validation fails
static void
s_Validate(IPssmInputData* pssm_input_msa)
{
    ASSERT(pssm_input_msa);

    if ( !pssm_input_msa->GetData() ) {
        NCBI_THROW(CBlastException, eBadParameter,
           "IPssmInputData returns NULL multiple sequence alignment");
    }

    Blast_Message* errors = NULL;
    if (PSIBlastOptionsValidate(pssm_input_msa->GetOptions(), &errors) != 0) {
        string msg("IPssmInputData returns invalid PSIBlastOptions: ");
        msg += string(errors->message);
        errors = Blast_MessageFree(errors);
        NCBI_THROW(CBlastException, eBadParameter, msg);
    }
}

/// Performs validation on data provided before invoking the CORE PSSM
/// engine. Should be called after invoking Process() on its argument
/// @throws CBlastException if validation fails
static void
s_Validate(IPssmInputFreqRatios* pssm_input_fr)
{
    ASSERT(pssm_input_fr);

    ITERATE(CNcbiMatrix<double>, itr, pssm_input_fr->GetData()) {
        if (*itr < 0.0) {
            NCBI_THROW(CBlastException, eBadParameter, "PSSM frequency "
                       "ratios cannot have negative values");
        }
    }
}

CPssmEngine::CPssmEngine(IPssmInputData* input)
    : m_PssmInput(input), m_PssmInputFreqRatios(NULL)
{
    s_CheckAgainstNullData(input);
    x_InitializeScoreBlock(x_GetQuery(), x_GetQueryLength(), x_GetMatrixName());
}

CPssmEngine::CPssmEngine(IPssmInputFreqRatios* input)
    : m_PssmInput(NULL), m_PssmInputFreqRatios(input)
{
    s_CheckAgainstNullData(input);
    x_InitializeScoreBlock(x_GetQuery(), x_GetQueryLength(), x_GetMatrixName());
}

CPssmEngine::~CPssmEngine()
{
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
        retval = "Unknown error code returned from PSSM engine: " + 
            NStr::IntToString(error_code);
    }

    return retval;
}

CRef<CPssmWithParameters>
CPssmEngine::Run()
{
    return (m_PssmInput ? x_CreatePssmFromMsa() : x_CreatePssmFromFreqRatios());
}

/// Auxiliary inner class to convert from a CNcbiMatrix into a double** as
/// required by the C API. Used only by CPssmEngine::x_CreatePssmFromFreqRatios
struct SNcbiMatrix2DoubleMatrix 
{
    SNcbiMatrix2DoubleMatrix(const CNcbiMatrix<double>& m) 
    {
        m_Data = new double*[m.GetCols()];
        for (size_t i = 0; i < m.GetCols(); i++) {
            m_Data[i] = const_cast<double*>(&m[i*m.GetRows()]);
        }
    }

    ~SNcbiMatrix2DoubleMatrix() { delete [] m_Data; }

    operator double**() { return m_Data; }
    
private:
    double** m_Data;
};

CRef<CPssmWithParameters>
CPssmEngine::x_CreatePssmFromFreqRatios()
{
    ASSERT(m_PssmInputFreqRatios);

    m_PssmInputFreqRatios->Process();
    s_Validate(m_PssmInputFreqRatios);

    CPSIMatrix pssm;
    SNcbiMatrix2DoubleMatrix freq_ratios(m_PssmInputFreqRatios->GetData());

    int status = 
        PSICreatePssmFromFrequencyRatios
            (m_PssmInputFreqRatios->GetQuery(), 
             m_PssmInputFreqRatios->GetQueryLength(),
             m_ScoreBlk,
             freq_ratios,
             kPSSM_NoImpalaScaling,
             &pssm);
    if (status != PSI_SUCCESS) {
        string msg = x_ErrorCodeToString(status);
        NCBI_THROW(CBlastException, eInternal, msg);
    }

    // Convert core BLAST matrix structure into ASN.1 score matrix object
    CRef<CPssmWithParameters> retval;
    retval = x_PSIMatrix2Asn1(pssm, m_PssmInputFreqRatios->GetMatrixName());

    return retval;
}

CRef<CPssmWithParameters>
CPssmEngine::x_CreatePssmFromMsa()
{
    ASSERT(m_PssmInput);

    m_PssmInput->Process();
    s_Validate(m_PssmInput);

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
    CRef<CPssmWithParameters> retval;
    retval = x_PSIMatrix2Asn1(pssm, m_PssmInput->GetMatrixName(), 
                              m_PssmInput->GetOptions(), diagnostics);

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

    retval[0] = retval[query_length+1] = GetSentinelByte(eBlastEncodingProtein);
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

    retval->num_queries              = 1;
    retval->first_context            = 0;
    retval->last_context             = 0;
    retval->contexts                 = (BlastContextInfo*) 
                                       calloc(1, sizeof(BlastContextInfo));
    retval->contexts[0].query_offset = 0;
    retval->contexts[0].query_length = query_length;
    retval->max_length               = query_length;

    return retval;
}

void
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

    TAutoUint1Ptr guarded_query(x_GuardProteinQuery(query, query_length));

    // Setup the scoring options
    CBlastScoringOptions opts;
    status = BlastScoringOptionsNew(kProgramType, &opts);
    if (status != 0) {
        NCBI_THROW(CBlastException, eOutOfMemory, "BlastScoringOptions");
    }
    BlastScoringOptionsSetMatrix(opts, matrix_name);
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
    status = BlastSetup_ScoreBlkInit(query_blk,
                                     query_info,
                                     opts,
                                     kProgramType,
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

    ASSERT(retval->kbp_ideal);

    m_ScoreBlk.Reset(retval);
}

unsigned char*
CPssmEngine::x_GetQuery() const
{
    return (m_PssmInput ? 
            m_PssmInput->GetQuery() : m_PssmInputFreqRatios->GetQuery());
}

unsigned int
CPssmEngine::x_GetQueryLength() const
{
    return (m_PssmInput ?
            m_PssmInput->GetQueryLength() :
            m_PssmInputFreqRatios->GetQueryLength());
}

const char*
CPssmEngine::x_GetMatrixName() const
{
    return (m_PssmInput ?
            m_PssmInput->GetMatrixName() :
            m_PssmInputFreqRatios->GetMatrixName());
}

CRef<CPssmWithParameters>
CPssmEngine::x_PSIMatrix2Asn1(const PSIMatrix* pssm,
                              const char* matrix_name,
                              const PSIBlastOptions* opts,
                              const PSIDiagnosticsResponse* diagnostics)
{
    ASSERT(pssm);

    CRef<CPssmWithParameters> retval(new CPssmWithParameters);

    // Record the parameters
    string mtx(matrix_name);
    mtx = NStr::ToUpper(mtx); // save the matrix name in all capital letters
    retval->SetParams().SetRpsdbparams().SetMatrixName(mtx);
    if (opts) {
        retval->SetParams().SetPseudocount(opts->pseudo_count);
    }

    CPssm& asn1_pssm = retval->SetPssm();
    asn1_pssm.SetIsProtein(true);
    // number of rows is alphabet size
    asn1_pssm.SetNumRows(pssm->nrows);
    // number of columns is query length
    asn1_pssm.SetNumColumns(pssm->ncols);
    asn1_pssm.SetByRow(false);  // this is the default

    asn1_pssm.SetFinalData().SetLambda(pssm->lambda);
    asn1_pssm.SetFinalData().SetKappa(pssm->kappa);
    asn1_pssm.SetFinalData().SetH(pssm->h);
    if (asn1_pssm.GetByRow() == false) {
        for (unsigned int i = 0; i < pssm->ncols; i++) {
            for (unsigned int j = 0; j < pssm->nrows; j++) {
                asn1_pssm.SetFinalData().SetScores().
                    push_back(pssm->pssm[i][j]);
            }
        }
    } else {
        for (unsigned int i = 0; i < pssm->nrows; i++) {
            for (unsigned int j = 0; j < pssm->ncols; j++) {
                asn1_pssm.SetFinalData().SetScores().
                    push_back(pssm->pssm[j][i]);
            }
        }
    }
    if (opts && opts->impala_scaling_factor != kPSSM_NoImpalaScaling) {
        asn1_pssm.SetFinalData().
            SetScalingFactor(static_cast<int>(opts->impala_scaling_factor));
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
        if (asn1_pssm.GetByRow() == false) {
            for (unsigned int i = 0; i < pssm->ncols; i++) {
                for (unsigned int j = 0; j < pssm->nrows; j++) {
                    res_freqs.push_back(diagnostics->residue_freqs[i][j]);
                }
            }
        } else {
            for (unsigned int i = 0; i < pssm->nrows; i++) {
                for (unsigned int j = 0; j < pssm->ncols; j++) {
                    res_freqs.push_back(diagnostics->residue_freqs[j][i]);
                }
            }
        }
    }
 
    if (diagnostics->weighted_residue_freqs) {
        CPssmIntermediateData::TWeightedResFreqsPerPos& wres_freqs =
            asn1_pssm.SetIntermediateData().SetWeightedResFreqsPerPos();
        if (asn1_pssm.GetByRow() == false) {
            for (unsigned int i = 0; i < pssm->ncols; i++) {
                for (unsigned int j = 0; j < pssm->nrows; j++) {
                    wres_freqs.
                        push_back(diagnostics->weighted_residue_freqs[i][j]);
                }
            }
        } else {
            for (unsigned int i = 0; i < pssm->nrows; i++) {
                for (unsigned int j = 0; j < pssm->ncols; j++) {
                    wres_freqs.
                        push_back(diagnostics->weighted_residue_freqs[j][i]);
                }
            }
        }
    }

    if (diagnostics->frequency_ratios) {
        CPssmIntermediateData::TFreqRatios& freq_ratios = 
            asn1_pssm.SetIntermediateData().SetFreqRatios();
        if (asn1_pssm.GetByRow() == false) {
            for (unsigned int i = 0; i < pssm->ncols; i++) {
                for (unsigned int j = 0; j < pssm->nrows; j++) {
                    freq_ratios.push_back(diagnostics->frequency_ratios[i][j]);
                }
            }
        } else {
            for (unsigned int i = 0; i < pssm->nrows; i++) {
                for (unsigned int j = 0; j < pssm->ncols; j++) {
                    freq_ratios.push_back(diagnostics->frequency_ratios[j][i]);
                }
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
 * Revision 1.40  2005/05/23 15:32:47  camacho
 * doxygen fixes
 *
 * Revision 1.39  2005/05/20 20:30:55  camacho
 * Remove unneeded data member
 *
 * Revision 1.38  2005/05/20 20:23:58  ucko
 * Define SNcbiMatrix2DoubleMatrix *outside* x_CreatePssmFromFreqRatios
 * to avoid breaking the MIPSpro compiler (on IRIX).
 *
 * Revision 1.37  2005/05/20 18:29:43  camacho
 * Add use of IPssmInputFreqRatios to PSSM engine
 *
 * Revision 1.36  2005/05/10 16:08:39  camacho
 * Changed *_ENCODING #defines to EBlastEncoding enumeration
 *
 * Revision 1.35  2005/05/03 20:33:11  camacho
 * Minor
 *
 * Revision 1.34  2005/05/02 19:39:07  camacho
 * Introduced constant for IMPALA-style PSSM scaling
 *
 * Revision 1.33  2005/04/27 19:58:34  dondosha
 * BlastSetup_ScoreBlkInit no longer has a phi-blast boolean argument
 *
 * Revision 1.32  2005/03/21 18:05:36  camacho
 * Added code to write matrix by row to aid in testing
 *
 * Revision 1.31  2005/03/08 17:00:49  camacho
 * Added unknown error code value to error message in case of PSSM engine failure
 *
 * Revision 1.30  2005/02/22 22:50:23  camacho
 * + impala_scaling_factor, first cut
 *
 * Revision 1.29  2005/02/08 18:33:45  dondosha
 * Use BlastScoringOptionsSetMatrix to set matrix name
 *
 * Revision 1.28  2004/12/28 16:47:43  camacho
 * 1. Use typedefs to AutoPtr consistently
 * 2. Remove exception specification from blast::SetupQueries
 * 3. Use SBlastSequence structure instead of std::pair as return value to
 *    blast::GetSequence
 *
 * Revision 1.27  2004/12/13 23:07:36  camacho
 * Remove validation functions moved to algo/blast/core
 *
 * Revision 1.26  2004/12/13 22:27:06  camacho
 * Consolidated structure group customizations in option: nsg_compatibility_mode
 *
 * Revision 1.25  2004/12/09 15:23:30  dondosha
 * BlastSetup_GetScoreBlock renamed to BlastSetup_ScoreBlkInit
 *
 * Revision 1.24  2004/12/08 14:34:37  camacho
 * Call PSIBlastOptions validation function
 *
 * Revision 1.23  2004/12/02 16:01:24  bealer
 * - Change multiple-arrays to array-of-struct in BlastQueryInfo
 *
 * Revision 1.22  2004/11/23 21:48:44  camacho
 * Removed local initialization of ideal Karlin-Altschul parameters
 *
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
