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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_psi.c
 * Implementation of the high level functions of PSI-BLAST's PSSM engine.
 */
    
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_encoding.h>
#include "blast_psi_priv.h"

/****************************************************************************/
/* Function prototypes */

/** Convenience function to deallocate data structures allocated in
 * PSICreatePssmWithDiagnostics.
 */
static void
s_PSICreatePssmCleanUp(PSIMatrix** pssm,
                      _PSIMsa* msa,
                      _PSIAlignedBlock* aligned_block,
                      _PSISequenceWeights* seq_weights,
                      _PSIInternalPssmData* internal_pssm);

/** Copies pssm data from internal_pssm and sbp into pssm. None of its
 * parameters can be NULL. */
static void
s_PSISavePssm(const _PSIInternalPssmData* internal_pssm,
             const BlastScoreBlk* sbp,
             PSIMatrix* pssm);

/****************************************************************************/

int
PSICreatePssm(const PSIMsa* msap,
              const PSIBlastOptions* options,
              BlastScoreBlk* sbp,
              PSIMatrix** pssm)
{
    return PSICreatePssmWithDiagnostics(msap, options, sbp, NULL,
                                        pssm, NULL);
}

int
PSICreatePssmWithDiagnostics(const PSIMsa* msap,                    /* [in] */
                             const PSIBlastOptions* options,        /* [in] */
                             BlastScoreBlk* sbp,                    /* [in] */
                             const PSIDiagnosticsRequest* request,  /* [in] */
                             PSIMatrix** pssm,                      /* [out] */
                             PSIDiagnosticsResponse** diagnostics)  /* [out] */
{
    _PSIMsa* msa = NULL;
    _PSIAlignedBlock* aligned_block = NULL;
    _PSISequenceWeights* seq_weights = NULL; 
    _PSIInternalPssmData* internal_pssm = NULL;
    int status = 0;

    if ( !msap || !options || !sbp || !pssm ) {
        return PSIERR_BADPARAM;
    }

    /*** Allocate data structures ***/
    msa = _PSIMsaNew(msap, (Uint4) sbp->alphabet_size);
    aligned_block = _PSIAlignedBlockNew(msa->dimensions->query_length);
    seq_weights = _PSISequenceWeightsNew(msa->dimensions, sbp);
    internal_pssm = _PSIInternalPssmDataNew(msa->dimensions->query_length,
                                            (Uint4) sbp->alphabet_size);
    *pssm = PSIMatrixNew(msa->dimensions->query_length, 
                         (Uint4) sbp->alphabet_size);
    if ( !msa || ! aligned_block || !seq_weights || !internal_pssm || !*pssm ) {
        s_PSICreatePssmCleanUp(pssm, msa, aligned_block, seq_weights,
                              internal_pssm);
        return PSIERR_OUTOFMEM;
    }

    /*** Run the engine's stages ***/
    status = _PSIPurgeBiasedSegments(msa);
    if (status != PSI_SUCCESS) {
        s_PSICreatePssmCleanUp(pssm, msa, aligned_block, seq_weights, 
                              internal_pssm);
        return status;
    }

    /*** Enable structure group customization if needed ***/
    if (options->nsg_compatibility_mode) {
        Uint4 i;
        for (i = 0; i < msa->dimensions->query_length; i++) {
            msa->cell[kQueryIndex][i].letter = 0;
            msa->cell[kQueryIndex][i].is_aligned = FALSE;
        }
        msa->use_sequence[kQueryIndex] = FALSE;
        _PSIUpdatePositionCounts(msa);
    } else {
        status = _PSIValidateMSA(msa);
        if (status != PSI_SUCCESS) {
            s_PSICreatePssmCleanUp(pssm, msa, aligned_block, seq_weights,
                                  internal_pssm);
            return status;
        }
    }

    status = _PSIComputeAlignmentBlocks(msa, aligned_block);
    if (status != PSI_SUCCESS) {
        s_PSICreatePssmCleanUp(pssm, msa, aligned_block, seq_weights, 
                              internal_pssm);
        return status;
    }

    status = _PSIComputeSequenceWeights(msa, aligned_block, 
                                        options->nsg_compatibility_mode,
                                        seq_weights);
    if (status != PSI_SUCCESS) {
        s_PSICreatePssmCleanUp(pssm, msa, aligned_block, seq_weights, 
                              internal_pssm);
        return status;
    }

    status = _PSIComputeFreqRatios(msa, seq_weights, sbp, aligned_block, 
                                   options->pseudo_count, internal_pssm);
    if (status != PSI_SUCCESS) {
        s_PSICreatePssmCleanUp(pssm, msa, aligned_block, seq_weights, 
                              internal_pssm);
        return status;
    }

    status = _PSIConvertFreqRatiosToPSSM(internal_pssm, msa->query, sbp, 
                                         seq_weights->std_prob);
    if (status != PSI_SUCCESS) {
        s_PSICreatePssmCleanUp(pssm, msa, aligned_block, seq_weights, 
                              internal_pssm);
        return status;
    }

    /* FIXME: Use a constant here? */
    if (options->impala_scaling_factor == 1.0) {
        status = _PSIScaleMatrix(msa->query, seq_weights->std_prob,
                                 internal_pssm, sbp);
    } else {
        status = _IMPALAScaleMatrix(msa->query, seq_weights->std_prob,
                                    internal_pssm, sbp,
                                    options->impala_scaling_factor);
    }
    if (status != PSI_SUCCESS) {
        s_PSICreatePssmCleanUp(pssm, msa, aligned_block, seq_weights, 
                              internal_pssm);
        return status;
    }

    /*** Save the pssm outgoing parameter ***/
    s_PSISavePssm(internal_pssm, sbp, *pssm);

    /*** Save diagnostics if required ***/
    if (request && diagnostics) {
        *diagnostics = PSIDiagnosticsResponseNew(msa->dimensions->query_length,
                                                 (Uint4) sbp->alphabet_size,
                                                 request);
        if ( !*diagnostics ) {
            /* FIXME: This could be changed to return a warning and not
             * deallocate PSSM data */
            s_PSICreatePssmCleanUp(pssm, msa, aligned_block, seq_weights, 
                                  internal_pssm);
            return PSIERR_OUTOFMEM;
        }
        status = _PSISaveDiagnostics(msa, aligned_block, seq_weights, 
                                     internal_pssm, *diagnostics);
        if (status != PSI_SUCCESS) {
            *diagnostics = PSIDiagnosticsResponseFree(*diagnostics);
            s_PSICreatePssmCleanUp(pssm, msa, aligned_block, seq_weights,
                                  internal_pssm);
            return status;
        }
    }
    s_PSICreatePssmCleanUp(NULL, msa, aligned_block, seq_weights, internal_pssm);

    return PSI_SUCCESS;
}

/****************************************************************************/

static void
s_PSICreatePssmCleanUp(PSIMatrix** pssm,
                      _PSIMsa* msa,
                      _PSIAlignedBlock* aligned_block,
                      _PSISequenceWeights* seq_weights,
                      _PSIInternalPssmData* internal_pssm)
{
    if (pssm) {
        *pssm = PSIMatrixFree(*pssm);
    }
    _PSIMsaFree(msa);
    _PSIAlignedBlockFree(aligned_block);
    _PSISequenceWeightsFree(seq_weights);
    _PSIInternalPssmDataFree(internal_pssm);
}

static void
s_PSISavePssm(const _PSIInternalPssmData* internal_pssm,
             const BlastScoreBlk* sbp,
             PSIMatrix* pssm)
{
    ASSERT(internal_pssm);
    ASSERT(sbp);
    ASSERT(pssm);

    _PSICopyMatrix_int(pssm->pssm, internal_pssm->pssm,
                       pssm->ncols, pssm->nrows);

    pssm->lambda = sbp->kbp_gap_psi[0]->Lambda;
    pssm->kappa = sbp->kbp_gap_psi[0]->K;
    pssm->h = sbp->kbp_gap_psi[0]->H;
}

/****************************************************************************/

PSIMsa*
PSIMsaNew(const PSIMsaDimensions* dimensions)
{
    PSIMsa* retval = NULL;

    if ( !dimensions ) {
        return NULL;
    }

    retval = (PSIMsa*) malloc(sizeof(PSIMsa));
    if ( !retval ) {
        return PSIMsaFree(retval);
    }

    retval->dimensions = (PSIMsaDimensions*) malloc(sizeof(PSIMsaDimensions));
    if ( !retval->dimensions ) {
        return PSIMsaFree(retval);
    }
    memcpy((void*) retval->dimensions,
           (void*) dimensions, 
           sizeof(PSIMsaDimensions));

    retval->data = (PSIMsaCell**) _PSIAllocateMatrix(dimensions->num_seqs + 1,
                                                     dimensions->query_length,
                                                     sizeof(PSIMsaCell));
    if ( !retval->data ) {
        return PSIMsaFree(retval);
    }
    {
        Uint4 s = 0;    /* index on sequences */
        Uint4 p = 0;    /* index on positions */

        for (s = 0; s < dimensions->num_seqs + 1; s++) {
            for (p = 0; p < dimensions->query_length; p++) {
                /* FIXME: change to 0 when old code is dropped */
                retval->data[s][p].letter = (Uint1) -1;
                retval->data[s][p].is_aligned = FALSE;
            }
        }
    }

    return retval;
}

PSIMsa*
PSIMsaFree(PSIMsa* msa)
{
    if ( !msa ) {
        return NULL;
    }

    if ( msa->data && msa->dimensions ) {
        _PSIDeallocateMatrix((void**) msa->data,
                             msa->dimensions->num_seqs + 1);
        msa->data = NULL;
    }

    if ( msa->dimensions ) {
        sfree(msa->dimensions);
    }

    sfree(msa);

    return NULL;
}

PSIMatrix*
PSIMatrixNew(Uint4 query_length, Uint4 alphabet_size)
{
    PSIMatrix* retval = NULL;

    retval = (PSIMatrix*) malloc(sizeof(PSIMatrix));
    if ( !retval ) {
        return NULL;
    }
    retval->ncols = query_length;
    retval->nrows = alphabet_size;

    retval->pssm = (int**) _PSIAllocateMatrix(query_length, alphabet_size,
                                              sizeof(int));
    if ( !(retval->pssm) ) {
        return PSIMatrixFree(retval);
    }

    retval->lambda = 0.0;
    retval->kappa = 0.0;
    retval->h = 0.0;

    return retval;
}

PSIMatrix*
PSIMatrixFree(PSIMatrix* matrix)
{
    if ( !matrix ) {
        return NULL;
    }

    if (matrix->pssm) {
        _PSIDeallocateMatrix((void**) matrix->pssm, matrix->ncols);
    }

    sfree(matrix);

    return NULL;
}

PSIDiagnosticsRequest* 
PSIDiagnosticsRequestNew()
{
    return calloc(1, sizeof(PSIDiagnosticsRequest));
}

PSIDiagnosticsRequest* 
PSIDiagnosticsRequestFree(PSIDiagnosticsRequest* diags_request)
{
    sfree(diags_request);
    return NULL;
}

PSIDiagnosticsResponse*
PSIDiagnosticsResponseNew(Uint4 query_length, Uint4 alphabet_size,
                          const PSIDiagnosticsRequest* wants)
{
    PSIDiagnosticsResponse* retval = NULL;

    if ( !wants ) {
        return NULL;
    }

    /* MUST use calloc to allocate structure because code that uses this
     * structure assumes that non-NULL members will require to be populated */
    retval = (PSIDiagnosticsResponse*) calloc(1, 
                                              sizeof(PSIDiagnosticsResponse));
    if ( !retval ) {
        return NULL;
    }

    retval->query_length = query_length;
    retval->alphabet_size = alphabet_size;

    if (wants->information_content) {
        retval->information_content = (double*) 
            calloc(query_length, sizeof(double));
        if ( !(retval->information_content) ) {
            return PSIDiagnosticsResponseFree(retval);
        }
    }

    if (wants->residue_frequencies) {
        retval->residue_freqs = (Uint4**) _PSIAllocateMatrix(query_length, 
                                                             alphabet_size, 
                                                             sizeof(Uint4));
        if ( !(retval->residue_freqs) ) {
            return PSIDiagnosticsResponseFree(retval);
        }
    }

    if (wants->weighted_residue_frequencies) {
        retval->weighted_residue_freqs = (double**) 
            _PSIAllocateMatrix(query_length, 
                               alphabet_size, 
                               sizeof(double));
        if ( !(retval->weighted_residue_freqs) ) {
            return PSIDiagnosticsResponseFree(retval);
        }
    }

    if (wants->frequency_ratios) {
        retval->frequency_ratios = (double**)
            _PSIAllocateMatrix(query_length, 
                               alphabet_size, 
                               sizeof(double));
        if ( !retval->frequency_ratios ) {
            return PSIDiagnosticsResponseFree(retval);
        }
    }

    if (wants->gapless_column_weights) {
        retval->gapless_column_weights = (double*) 
            calloc(query_length, sizeof(double));
        if ( !(retval->gapless_column_weights) ) {
            return PSIDiagnosticsResponseFree(retval);
        }
    }

    return retval;
}

PSIDiagnosticsResponse*
PSIDiagnosticsResponseFree(PSIDiagnosticsResponse* diags)
{
    if ( !diags )
        return NULL;

    if (diags->information_content) {
        sfree(diags->information_content);
    }

    if (diags->residue_freqs) {
        _PSIDeallocateMatrix((void**) diags->residue_freqs,
                             diags->query_length);
    }

    if (diags->weighted_residue_freqs) {
        _PSIDeallocateMatrix((void**) diags->weighted_residue_freqs,
                             diags->query_length);
    }

    if (diags->frequency_ratios) {
        _PSIDeallocateMatrix((void**) diags->frequency_ratios,
                             diags->query_length);
    }

    if (diags->gapless_column_weights) {
        sfree(diags->gapless_column_weights);
    }

    sfree(diags);

    return NULL;
}

