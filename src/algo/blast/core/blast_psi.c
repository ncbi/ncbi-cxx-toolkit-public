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

/* FIXME: document all local variables */

/****************************************************************************/
/* Use the following #define's to enable/disable functionality */

/* Taking gaps into account when constructing a PSSM was introduced in the 
 * 2001 paper "Improving the accuracy of PSI-BLAST protein database searches
 * with composition based-statistics and other refinements". This feature 
 * can be disabled by defining the PSI_IGNORE_GAPS_IN_COLUMNS symbol below */
/* #define PSI_IGNORE_GAPS_IN_COLUMNS */
/****************************************************************************/

PsiMatrix*
PSICreatePSSM(PsiAlignmentData* alignment,      /* [in] */
              const PSIBlastOptions* options,   /* [in] */
              BlastScoreBlk* sbp,               /* [in] */
              PsiDiagnostics* diagnostics)      /* [out] */
{
    PsiMatrix* retval = NULL;

    PsiAlignedBlock* aligned_block = NULL;
    PsiSequenceWeights* seq_weights = NULL; 

    aligned_block = _PSIAlignedBlockNew(alignment->dimensions->query_sz);
    seq_weights = _PSISequenceWeightsNew(alignment->dimensions, sbp);
    retval = PSIMatrixNew(alignment->dimensions->query_sz, sbp->alphabet_size);

    PSIPurgeBiasedSegments(alignment);
    PSIComputeAlignmentBlocks(alignment, aligned_block);
    PSIComputeSequenceWeights(alignment, aligned_block, seq_weights);
    PSIComputeResidueFrequencies(alignment, seq_weights, sbp, aligned_block,
                                 options, retval);
    PSIConvertResidueFreqsToPSSM(retval, alignment->query, sbp, 
                                 seq_weights->std_prob);
    PSIScaleMatrix(alignment->query, alignment->dimensions->query_sz, 
                   seq_weights->std_prob, NULL, retval, sbp);

    if (diagnostics) {
        diagnostics = _PSISaveDiagnostics(alignment, aligned_block,
                                          seq_weights);
    } else {

        /* FIXME: Deallocate structures selectively as some of these will be
         * copied into the diagnostics structure */
        _PSIAlignedBlockFree(aligned_block);
        _PSISequenceWeightsFree(seq_weights);
    }

    return retval;
}

/****************************************************************************/

/** Initializes the alignment data structure with the query sequence
 * information. 
 */
static void
PSIExtractQuerySequenceInfo(PsiAlignmentData* alignment);

PsiAlignmentData*
PSIAlignmentDataNew(const Uint1* query, const PsiInfo* info)
{
    PsiAlignmentData* retval = NULL;        /* the return value */
    Uint4 s = 0;                            /* index in sequences */
    Uint4 p = 0;                            /* index on positions */

    if ( !query || !info ) {
        return NULL;
    }

    retval = (PsiAlignmentData*) calloc(1, sizeof(PsiAlignmentData));
    if ( !retval ) {
         return NULL;
    }

    /* This doesn't need to be query_sz + 1 (posC) */
    retval->res_counts = (Uint4**) _PSIAllocateMatrix(info->query_sz,
                                                      PSI_ALPHABET_SIZE,
                                                      sizeof(Uint4));
    if ( !(retval->res_counts) ) {
        return PSIAlignmentDataFree(retval);
    }

    retval->match_seqs = (Uint4*) calloc(info->query_sz, sizeof(int));
    if ( !(retval->match_seqs)) {
        return PSIAlignmentDataFree(retval);
    }

    retval->desc_matrix = (PsiDesc**) _PSIAllocateMatrix(info->num_seqs + 1,
                                                         info->query_sz,
                                                         sizeof(PsiDesc));
    if (!retval->desc_matrix) {
        return PSIAlignmentDataFree(retval);
    }
    for (s = 0; s < info->num_seqs + 1; s++) {
        for (p = 0; p < info->query_sz; p++) {
            retval->desc_matrix[s][p].letter = -1;
            retval->desc_matrix[s][p].used = FALSE;
            retval->desc_matrix[s][p].e_value = kDefaultEvalueForPosition;
            retval->desc_matrix[s][p].extents.left = -1;
            retval->desc_matrix[s][p].extents.right = info->query_sz;
        }
    }

    retval->use_sequences = (Boolean*) calloc(info->num_seqs + 1, 
                                              sizeof(Boolean));
    if (!retval->use_sequences) {
        return PSIAlignmentDataFree(retval);
    }
    /* All sequences are valid candidates for taking part in 
       PSSM construction */
    for (s = 0; s < info->num_seqs + 1; s++) {
        retval->use_sequences[s] = TRUE;
    }

    if ( !(retval->dimensions = (PsiInfo*) calloc(1, sizeof(PsiInfo)))) {
        return PSIAlignmentDataFree(retval);
    }
    memcpy((void*) retval->dimensions, (void*) info, sizeof(*info));

    retval->query = (Uint1*) malloc(info->query_sz * sizeof(Uint1));
    if ( !retval->query ) {
        return PSIAlignmentDataFree(retval);
    }
    memcpy((void*) retval->query, (void*) query, info->query_sz);

    PSIExtractQuerySequenceInfo(retval);

    return retval;
}

PsiAlignmentData*
PSIAlignmentDataFree(PsiAlignmentData* alignment)
{
    if ( !alignment ) {
        return NULL;
    }

    if (alignment->res_counts) {
        _PSIDeallocateMatrix((void**) alignment->res_counts,
                             alignment->dimensions->query_sz);
        alignment->res_counts = NULL;
    }

    if (alignment->match_seqs) {
        sfree(alignment->match_seqs);
    }

    if (alignment->desc_matrix) {
        _PSIDeallocateMatrix((void**) alignment->desc_matrix,
                             alignment->dimensions->num_seqs + 1);
        alignment->desc_matrix = NULL;
    }

    if (alignment->use_sequences) {
        sfree(alignment->use_sequences);
    }

    if (alignment->dimensions) {
        sfree(alignment->dimensions);
    }

    if (alignment->query) {
        sfree(alignment->query);
    }

    sfree(alignment);
    return NULL;
}

PsiMatrix*
PSIMatrixNew(Uint4 query_sz, Uint4 alphabet_size)
{
    PsiMatrix* retval = NULL;

    retval = (PsiMatrix*) calloc(1, sizeof(PsiMatrix));
    if ( !retval ) {
        return NULL;
    }
    retval->ncols = query_sz + 1;

    retval->pssm = (int**) _PSIAllocateMatrix(query_sz + 1, alphabet_size,
                                              sizeof(int));
    if ( !(retval->pssm) ) {
        return PSIMatrixFree(retval);
    }

    retval->scaled_pssm = (int**) _PSIAllocateMatrix(query_sz + 1, 
                                                     alphabet_size,
                                                     sizeof(int));
    if ( !(retval->scaled_pssm) ) {
        return PSIMatrixFree(retval);
    }

    retval->res_freqs = (double**) _PSIAllocateMatrix(query_sz + 1, 
                                                      alphabet_size, 
                                                      sizeof(double));
    if ( !(retval->res_freqs) ) {
        return PSIMatrixFree(retval);
    }

    return retval;
}

PsiMatrix*
PSIMatrixFree(PsiMatrix* matrix)
{
    if ( !matrix ) {
        return NULL;
    }

    if (matrix->pssm) {
        _PSIDeallocateMatrix((void**) matrix->pssm, matrix->ncols);
    }

    if (matrix->scaled_pssm) {
        _PSIDeallocateMatrix((void**) matrix->scaled_pssm, matrix->ncols);
    }

    if (matrix->res_freqs) {
        _PSIDeallocateMatrix((void**) matrix->res_freqs, matrix->ncols);
    }

    sfree(matrix);

    return NULL;
}

PsiDiagnostics*
PSIDiagnosticsNew(Uint4 query_sz, Uint4 alphabet_size)
{
    PsiDiagnostics* retval = NULL;

    retval = (PsiDiagnostics*) calloc(1, sizeof(PsiDiagnostics));
    if ( !retval ) {
        return NULL;
    }

    retval->info_content = (double**) _PSIAllocateMatrix(query_sz,
                                                         alphabet_size,
                                                         sizeof(double));
    if ( !(retval->info_content) ) {
        return PSIDiagnosticsFree(retval);
    }
    retval->ncols = query_sz;

    return retval;
}

PsiDiagnostics*
PSIDiagnosticsFree(PsiDiagnostics* diags)
{
    if ( !diags )
        return NULL;

    if (diags->info_content) {
        _PSIDeallocateMatrix((void**) diags->info_content, diags->ncols);
    }

    sfree(diags);

    return NULL;
}

/****************************************************************************/
/* Auxiliary functions to populate PsiAlignmentData structure */
static void
PSIExtractQuerySequenceInfo(PsiAlignmentData* alignment)
{
    Uint4 i = 0;

    ASSERT(alignment);

    for (i = 0; i < alignment->dimensions->query_sz; i++) {
        alignment->desc_matrix[kQueryIndex][i].letter = alignment->query[i];
        alignment->desc_matrix[kQueryIndex][i].used = TRUE;
        alignment->desc_matrix[kQueryIndex][i].e_value = 
            PSI_INCLUSION_ETHRESH / 2;
        alignment->desc_matrix[kQueryIndex][i].extents.left = 0;
        alignment->desc_matrix[kQueryIndex][i].extents.right =
            alignment->dimensions->query_sz;

        alignment->res_counts[i][alignment->query[i]]++;
        alignment->match_seqs[i]++;
    }
}
