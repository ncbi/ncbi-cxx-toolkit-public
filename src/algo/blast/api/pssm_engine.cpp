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

/** @file pssm_engine.cpp
 * Implementation of the C++ API for the PSI-BLAST PSSM generation engine.
 */

#include <ncbi_pch.hpp>
#include <sstream>

#include <algo/blast/api/blast_aux.hpp>
#include <algo/blast/api/pssm_engine.hpp>
#include "blast_setup.hpp"

// Object includes
#include <objects/scoremat/Pssm.hpp>
#include <objects/scoremat/PssmParameters.hpp>
#include <objects/scoremat/PssmFinalData.hpp>
#include <objects/scoremat/PssmIntermediateData.hpp>
#include <objects/scoremat/PssmWithParameters.hpp>
#include <objects/scoremat/FormatRpsDbParameters.hpp>
#include <objects/seqset/Seq_entry.hpp>

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
/// @throw CPssmEngineException in case of validation failure
static void
s_CheckAgainstNullData(IPssmInputData* pssm_input_msa)
{
    if ( !pssm_input_msa ) {
        NCBI_THROW(CPssmEngineException, eNullInputData,
           "IPssmInputData is NULL");
    }

    if ( !pssm_input_msa->GetOptions() ) {
        NCBI_THROW(CPssmEngineException, eNullInputData,
           "IPssmInputData returns NULL PSIBlastOptions");
    }

    if ( !pssm_input_msa->GetQuery() ) {
        NCBI_THROW(CPssmEngineException, eNullInputData,
           "IPssmInputData returns NULL query sequence");
    }

    if (pssm_input_msa->GetQueryLength() == 0) {
        NCBI_THROW(CPssmEngineException, eNullInputData,
           "Query length provided by IPssmInputData is 0");
    }
}

/// This function makes sure that none of the required data is returned as NULL
/// or "empty"
/// @param pssm_input_freqratios interface which provides the data [in]
/// @throw CPssmEngineException in case of validation failure
static void
s_CheckAgainstNullData(IPssmInputFreqRatios* pssm_input_freqratios)
{
    if ( !pssm_input_freqratios ) {
        NCBI_THROW(CPssmEngineException, eNullInputData,
           "IPssmInputFreqRatios is NULL");
    }

    if ( !pssm_input_freqratios->GetQuery() ) {
        NCBI_THROW(CPssmEngineException, eNullInputData,
           "IPssmInputFreqRatiosFreqRatios returns NULL query sequence");
    }

    const unsigned int kQueryLength = pssm_input_freqratios->GetQueryLength();
    if (kQueryLength == 0) {
        NCBI_THROW(CPssmEngineException, eInvalidInputData,
           "Query length provided by IPssmInputFreqRatiosFreqRatios is 0");
    }

    if (pssm_input_freqratios->GetData().GetCols() != kQueryLength) {
        NCBI_THROW(CPssmEngineException, eInvalidInputData,
           "Number of columns returned by IPssmInputFreqRatiosFreqRatios does "
           "not match query length");
    }
    if (pssm_input_freqratios->GetData().GetRows() != BLASTAA_SIZE) {
        NCBI_THROW(CPssmEngineException, eInvalidInputData,
           "Number of rows returned by IPssmInputFreqRatiosFreqRatios differs "
           "from " + NStr::IntToString(BLASTAA_SIZE));
    }
}

/// Performs validation on data provided before invoking the CORE PSSM
/// engine. Should be called after invoking Process() on its argument
/// @throws CPssmEngineException if validation fails
static void
s_Validate(IPssmInputData* pssm_input_msa)
{
    _ASSERT(pssm_input_msa);

    if ( !pssm_input_msa->GetData() ) {
        NCBI_THROW(CPssmEngineException, eNullInputData,
           "IPssmInputData returns NULL multiple sequence alignment");
    }

    Blast_Message* errors = NULL;
    if (PSIBlastOptionsValidate(pssm_input_msa->GetOptions(), &errors) != 0) {
        string msg("IPssmInputData returns invalid PSIBlastOptions: ");
        msg += string(errors->message);
        errors = Blast_MessageFree(errors);
        NCBI_THROW(CBlastException, eInvalidOptions, msg);
    }
}

/// Performs validation on data provided before invoking the CORE PSSM
/// engine. Should be called after invoking Process() on its argument
/// @throws CPssmEngineException if validation fails
static void
s_Validate(IPssmInputFreqRatios* pssm_input_fr)
{
    _ASSERT(pssm_input_fr);

    ITERATE(CNcbiMatrix<double>, itr, pssm_input_fr->GetData()) {
        if (*itr < 0.0) {
            NCBI_THROW(CPssmEngineException, eInvalidInputData, 
                       "PSSM frequency ratios cannot have negative values");
        }
    }
}

CPssmEngine::CPssmEngine(IPssmInputData* input)
    : m_PssmInput(input), m_PssmInputFreqRatios(NULL)
{
    s_CheckAgainstNullData(input);
    x_InitializeScoreBlock(x_GetQuery(), x_GetQueryLength(), x_GetMatrixName(),
           x_GetGapExistence(), x_GetGapExtension());
}

CPssmEngine::CPssmEngine(IPssmInputFreqRatios* input)
    : m_PssmInput(NULL), m_PssmInputFreqRatios(input)
{
    s_CheckAgainstNullData(input);
    x_InitializeScoreBlock(x_GetQuery(), x_GetQueryLength(), x_GetMatrixName(),
           x_GetGapExistence(), x_GetGapExtension());
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

/// Auxiliary class to convert from a CNcbiMatrix into a double** as
/// required by the C API. Used only by CPssmEngine::x_CreatePssmFromFreqRatios
struct SNcbiMatrix2DoubleMatrix 
{
    /// Constructor
    /// @param m standard c++ toolkit matrix
    SNcbiMatrix2DoubleMatrix(const CNcbiMatrix<double>& m) 
        : m_NumCols(m.GetCols())
    {
        m_Data = new double*[m.GetCols()];
        for (size_t c = 0; c < m.GetCols(); c++) {
            m_Data[c] = new double[m.GetRows()];
            for (size_t r = 0; r < m.GetRows(); r++) {
                m_Data[c][r] = m(r, c);
            }
        }
    }

    /// Destructor
    ~SNcbiMatrix2DoubleMatrix() { 
        for (size_t c = 0; c < m_NumCols; c++) {
            delete [] m_Data[c];
        }
        delete [] m_Data; 
    }

    /// Retrieves data in the format expected by the C CORE APIs
    operator double**() { return m_Data; }
    
private:
    /// double** representation of a CNcbiMatrix
    double** m_Data;        
    /// number of columns in the matrix (for deallocation)
    size_t m_NumCols;       
};

CRef<CPssmWithParameters>
CPssmEngine::x_CreatePssmFromFreqRatios()
{
    _ASSERT(m_PssmInputFreqRatios);

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
        NCBI_THROW(CBlastException, eCoreBlastError, msg);
    }

    // Convert core BLAST matrix structure into ASN.1 score matrix object
    CRef<CPssmWithParameters> retval;
    retval = x_PSIMatrix2Asn1(pssm, m_PssmInputFreqRatios->GetMatrixName());
    CRef<CBioseq> query = m_PssmInputFreqRatios->GetQueryForPssm();
    if (query.NotEmpty()) {
        retval->SetQuery().SetSeq(*query);
    }

    return retval;
}

CRef<CPssmWithParameters>
CPssmEngine::x_CreatePssmFromMsa()
{
    _ASSERT(m_PssmInput);

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
        NCBI_THROW(CBlastException, eCoreBlastError, msg);
    }

    // Convert core BLAST matrix structure into ASN.1 score matrix object
    CRef<CPssmWithParameters> retval;
    retval = x_PSIMatrix2Asn1(pssm, m_PssmInput->GetMatrixName(), 
                              m_PssmInput->GetOptions(), diagnostics);
    CRef<CBioseq> query = m_PssmInput->GetQueryForPssm();
    if (query.NotEmpty()) {
        retval->SetQuery().SetSeq(*query);
    }

    return retval;
}

unsigned char*
CPssmEngine::x_GuardProteinQuery(const unsigned char* query,
                                 unsigned int query_length)
{
    _ASSERT(query);

    unsigned char* retval = NULL;
    retval = (unsigned char*) malloc(sizeof(unsigned char)*(query_length + 2));
    if ( !retval ) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, "Query with sentinels");
    }

    retval[0] = retval[query_length+1] = GetSentinelByte(eBlastEncodingProtein);
    memcpy((void*) &retval[1], (void*) query, query_length);
    return retval;
}

BlastQueryInfo*
CPssmEngine::x_InitializeQueryInfo(unsigned int query_length)
{
    const int kNumQueries = 1;
    BlastQueryInfo* retval = BlastQueryInfoNew(eBlastTypeBlastp, kNumQueries);

    if ( !retval ) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, "BlastQueryInfo");
    }

    retval->contexts[0].query_offset = 0;
    retval->contexts[0].query_length = query_length;
    retval->max_length               = query_length;

    return retval;
}

void
CPssmEngine::SetUngappedStatisticalParams(CConstRef<CBlastAncillaryData> 
                                          ancillary_data)
{
    _ASSERT(m_ScoreBlk.Get() != NULL);
    _ASSERT(ancillary_data.NotEmpty());
    if (ancillary_data->GetPsiUngappedKarlinBlk()) {
        _ASSERT(m_ScoreBlk->kbp_psi && m_ScoreBlk->kbp_psi[0]);
        m_ScoreBlk->kbp_psi[0]->Lambda =
            ancillary_data->GetPsiUngappedKarlinBlk()->Lambda;
        m_ScoreBlk->kbp_psi[0]->K =
            ancillary_data->GetPsiUngappedKarlinBlk()->K;
        m_ScoreBlk->kbp_psi[0]->logK = log(m_ScoreBlk->kbp_psi[0]->K);
        m_ScoreBlk->kbp_psi[0]->H =
            ancillary_data->GetPsiUngappedKarlinBlk()->H;
    }

    if (ancillary_data->GetPsiGappedKarlinBlk()) {
        _ASSERT(m_ScoreBlk->kbp_gap_psi && m_ScoreBlk->kbp_gap_psi[0]);
        m_ScoreBlk->kbp_gap_psi[0]->Lambda =
            ancillary_data->GetPsiGappedKarlinBlk()->Lambda;
        m_ScoreBlk->kbp_gap_psi[0]->K =
            ancillary_data->GetPsiGappedKarlinBlk()->K;
        m_ScoreBlk->kbp_gap_psi[0]->logK = log(m_ScoreBlk->kbp_gap_psi[0]->K);
        m_ScoreBlk->kbp_gap_psi[0]->H =
            ancillary_data->GetPsiGappedKarlinBlk()->H;
    }
}

void
CPssmEngine::x_InitializeScoreBlock(const unsigned char* query,
                                    unsigned int query_length,
                                    const char* matrix_name,
                                    int gap_existence,
                                    int gap_extension)
{
    _ASSERT(query);
    _ASSERT(matrix_name);

    const EBlastProgramType kProgramType = eBlastTypePsiBlast;
    short status = 0;

    TAutoUint1Ptr guarded_query(x_GuardProteinQuery(query, query_length));

    // Setup the scoring options
    CBlastScoringOptions opts;
    status = BlastScoringOptionsNew(kProgramType, &opts);
    if (status != 0) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, "BlastScoringOptions");
    }
    BlastScoringOptionsSetMatrix(opts, matrix_name);
    opts->gap_open = gap_existence;
    opts->gap_extend = gap_extension;

    // Setup the sequence block structure
    CBLAST_SequenceBlk query_blk;
    status = BlastSeqBlkNew(&query_blk);
    if (status != 0) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, "BLAST_SequenceBlk");
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
                                     &errors,
                                     &BlastFindMatrixPath);
    if (status != 0) {
        retval = BlastScoreBlkFree(retval);
        if (errors) {
            string msg(errors->message);
            errors = Blast_MessageFree(errors);
            NCBI_THROW(CBlastException, eCoreBlastError, msg);
        } else {
            NCBI_THROW(CBlastException, eCoreBlastError, 
                       "Unknown error when setting up BlastScoreBlk");
        }
    }

    _ASSERT(retval->kbp_ideal);
    _ASSERT(retval->kbp == retval->kbp_psi);
    _ASSERT(retval->kbp_gap == retval->kbp_gap_psi);

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

int
CPssmEngine::x_GetGapExistence() const
{
    return (m_PssmInput ?
            m_PssmInput->GetGapExistence() :
            m_PssmInputFreqRatios->GetGapExistence());
}

int
CPssmEngine::x_GetGapExtension() const
{
    return (m_PssmInput ?
            m_PssmInput->GetGapExtension() :
            m_PssmInputFreqRatios->GetGapExtension());
}

CRef<CPssmWithParameters>
CPssmEngine::x_PSIMatrix2Asn1(const PSIMatrix* pssm,
                              const char* matrix_name,
                              const PSIBlastOptions* opts,
                              const PSIDiagnosticsResponse* diagnostics)
{
    _ASSERT(pssm);

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

    asn1_pssm.SetLambda(pssm->lambda);
    asn1_pssm.SetKappa(pssm->kappa);
    asn1_pssm.SetH(pssm->h);
    asn1_pssm.SetLambdaUngapped(pssm->ung_lambda);
    asn1_pssm.SetKappaUngapped(pssm->ung_kappa);
    asn1_pssm.SetHUngapped(pssm->ung_h);
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

    _ASSERT(pssm->nrows == diagnostics->alphabet_size);
    _ASSERT(pssm->ncols == diagnostics->query_length);

    if (diagnostics->information_content) {
        CPssmIntermediateData::TInformationContent& info_content =
            asn1_pssm.SetIntermediateData().SetInformationContent();
        for (Uint4 i = 0; i < diagnostics->query_length; i++) {
            info_content.push_back(diagnostics->information_content[i]);
        }
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
        CPssmIntermediateData::TGaplessColumnWeights& gcw =
            asn1_pssm.SetIntermediateData().SetGaplessColumnWeights();
        for (Uint4 i = 0; i < diagnostics->query_length; i++) {
            gcw.push_back(diagnostics->gapless_column_weights[i]);
        }
    }

    if (diagnostics->sigma) {
        CPssmIntermediateData::TSigma& sigma =
            asn1_pssm.SetIntermediateData().SetSigma();
        for (Uint4 i = 0; i < diagnostics->query_length; i++) {
            sigma.push_back(diagnostics->sigma[i]);
        }
    }

    if (diagnostics->interval_sizes) {
        CPssmIntermediateData::TIntervalSizes& interval_sizes =
            asn1_pssm.SetIntermediateData().SetIntervalSizes();
        for (Uint4 i = 0; i < diagnostics->query_length; i++) {
            interval_sizes.push_back(diagnostics->interval_sizes[i]);
        }
    }

    if (diagnostics->num_matching_seqs) {
        CPssmIntermediateData::TNumMatchingSeqs& num_matching_seqs =
            asn1_pssm.SetIntermediateData().SetNumMatchingSeqs();
        for (Uint4 i = 0; i < diagnostics->query_length; i++) {
            num_matching_seqs.push_back(diagnostics->num_matching_seqs[i]);
        }
    }

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
