static char const rcsid[] =
    "$Id$";
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
 * Implementation of the high level PSI-BLAST API
 */
    
#include <algo/blast/core/blast_psi.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_encoding.h>
#include "blast_psi_priv.h"

/****************************************************************************/
/* Function prototypes */

/** Convenience function to deallocate data structures allocated in
 * PSICreatePssmWithDiagnostics 
 */
static void
_PSICreatePssmCleanUp(PSIMatrix* pssm,
                      _PSIMsa* msa,
                      _PSIAlignedBlock* aligned_block,
                      _PSISequenceWeights* seq_weights,
                      _PSIInternalPssmData* internal_pssm);

/** Saves PSIMatrix return value of PSICreatePssmWithDiagnostics function */
static void
_PSISavePssm(const _PSIInternalPssmData* internal_pssm,
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
                                            sbp->alphabet_size);
    *pssm = PSIMatrixNew(msa->dimensions->query_length, sbp->alphabet_size);
    if ( !msa || ! aligned_block || !seq_weights || !internal_pssm || !*pssm ) {
        _PSICreatePssmCleanUp(*pssm, msa, aligned_block, seq_weights,
                              internal_pssm);
        return PSIERR_OUTOFMEM;
    }

    /*** Run the engine's stages ***/
    status = _PSIPurgeBiasedSegments(msa);
    if (status != PSI_SUCCESS) {
        _PSICreatePssmCleanUp(*pssm, msa, aligned_block, seq_weights, 
                              internal_pssm);
        return status;
    }

    status = _PSIComputeAlignmentBlocks(msa, aligned_block);
    if (status != PSI_SUCCESS) {
        _PSICreatePssmCleanUp(*pssm, msa, aligned_block, seq_weights, 
                              internal_pssm);
        return status;
    }

    status = _PSIComputeSequenceWeights(msa, aligned_block, seq_weights);
    if (status != PSI_SUCCESS) {
        _PSICreatePssmCleanUp(*pssm, msa, aligned_block, seq_weights, 
                              internal_pssm);
        return status;
    }

    status = _PSIComputeResidueFrequencies(msa, seq_weights, sbp, 
                                           aligned_block, 
                                           options->pseudo_count, 
                                           internal_pssm);
    if (status != PSI_SUCCESS) {
        _PSICreatePssmCleanUp(*pssm, msa, aligned_block, seq_weights, 
                              internal_pssm);
        return status;
    }

    status = _PSIConvertResidueFreqsToPSSM(internal_pssm, msa->query, sbp, 
                                           seq_weights->std_prob);
    if (status != PSI_SUCCESS) {
        _PSICreatePssmCleanUp(*pssm, msa, aligned_block, seq_weights, 
                              internal_pssm);
        return status;
    }

    /* FIXME: instead of NULL pass options->scaling_factor */
    status = _PSIScaleMatrix(msa->query, msa->dimensions->query_length, 
                             seq_weights->std_prob, NULL, internal_pssm, sbp);
    if (status != PSI_SUCCESS) {
        _PSICreatePssmCleanUp(*pssm, msa, aligned_block, seq_weights, 
                              internal_pssm);
        return status;
    }

    /*** Save the pssm outgoing parameter ***/
    _PSISavePssm(internal_pssm, sbp, *pssm);

    /*** Save diagnostics if required ***/
    if (request && diagnostics) {
        *diagnostics = PSIDiagnosticsResponseNew(msa->dimensions,
                                                 sbp->alphabet_size,
                                                 request);
        if ( !*diagnostics ) {
            _PSICreatePssmCleanUp(*pssm, msa, aligned_block, seq_weights, 
                                  internal_pssm);
            return PSIERR_OUTOFMEM;
        }
        status = _PSISaveDiagnostics(msa, aligned_block, seq_weights, 
                                     internal_pssm, *diagnostics);
        if (status != PSI_SUCCESS) {
            *diagnostics = PSIDiagnosticsResponseFree(*diagnostics);
            _PSICreatePssmCleanUp(*pssm, msa, aligned_block, seq_weights,
                                  internal_pssm);
            return status;
        }
    }
    _PSICreatePssmCleanUp(NULL, msa, aligned_block, seq_weights, internal_pssm);

    return PSI_SUCCESS;
}

/****************************************************************************/

static void
_PSICreatePssmCleanUp(PSIMatrix* pssm,
                      _PSIMsa* msa,
                      _PSIAlignedBlock* aligned_block,
                      _PSISequenceWeights* seq_weights,
                      _PSIInternalPssmData* internal_pssm)
{
    PSIMatrixFree(pssm);
    _PSIMsaFree(msa);
    _PSIAlignedBlockFree(aligned_block);
    _PSISequenceWeightsFree(seq_weights);
    _PSIInternalPssmDataFree(internal_pssm);
}

static void
_PSISavePssm(const _PSIInternalPssmData* internal_pssm,
             const BlastScoreBlk* sbp,
             PSIMatrix* pssm)
{
    ASSERT(internal_pssm);
    ASSERT(sbp);
    ASSERT(pssm);

    _PSICopyIntMatrix(pssm->pssm, internal_pssm->pssm,
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

PSIDiagnosticsResponse*
PSIDiagnosticsResponseNew(const PSIMsaDimensions* dimensions,
                          Uint4 alphabet_size,
                          const PSIDiagnosticsRequest* wants)
{
    PSIDiagnosticsResponse* retval = NULL;

    if ( !dimensions || !wants ) {
        return NULL;
    }

    /* MUST use calloc to allocate structure because code that uses this
     * structure assumes that non-NULL members will require to be populated */
    retval = (PSIDiagnosticsResponse*) calloc(1, 
                                              sizeof(PSIDiagnosticsResponse));
    if ( !retval ) {
        return NULL;
    }

    retval->alphabet_size = alphabet_size;
    retval->dimensions = (PSIMsaDimensions*) malloc(sizeof(PSIMsaDimensions));
    if ( !retval->dimensions ) {
        return PSIDiagnosticsResponseFree(retval);
    }
    memcpy((void*) retval->dimensions, (void*) dimensions, 
           sizeof(PSIMsaDimensions));

    if (wants->information_content) {
        retval->information_content = (double*) 
            calloc(dimensions->query_length, sizeof(double));
        if ( !(retval->information_content) ) {
            return PSIDiagnosticsResponseFree(retval);
        }
    }

    if (wants->residue_frequencies) {
        retval->residue_frequencies = (double**) 
            _PSIAllocateMatrix(dimensions->query_length, 
                               alphabet_size, 
                               sizeof(double));
        if ( !(retval->residue_frequencies) ) {
            return PSIDiagnosticsResponseFree(retval);
        }
    }

    if (wants->raw_residue_counts) {
        retval->raw_residue_counts = (Uint4**) 
            _PSIAllocateMatrix(dimensions->query_length, 
                               alphabet_size, 
                               sizeof(Uint4));
        if ( !(retval->raw_residue_counts) ) {
            return PSIDiagnosticsResponseFree(retval);
        }
    }

    if (wants->sequence_weights) {
        retval->sequence_weights = (double*) calloc(dimensions->num_seqs + 1, 
                                                    sizeof(double));
        if ( !retval->sequence_weights ) {
            return PSIDiagnosticsResponseFree(retval);
        }
    }

    if (wants->gapless_column_weights) {
        retval->gapless_column_weights = (double*) 
            calloc(dimensions->query_length, sizeof(double));
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

    if (diags->residue_frequencies) {
        _PSIDeallocateMatrix((void**) diags->residue_frequencies,
                             diags->dimensions->query_length);
    }

    if (diags->raw_residue_counts) {
        _PSIDeallocateMatrix((void**) diags->raw_residue_counts,
                             diags->dimensions->query_length);
    }

    if (diags->sequence_weights) {
        sfree(diags->sequence_weights);
    }

    if (diags->gapless_column_weights) {
        sfree(diags->gapless_column_weights);
    }

    if (diags->dimensions) {
        sfree(diags->dimensions);
    }

    sfree(diags);

    return NULL;
}

