#ifndef ALGO_BLAST_CORE___BLAST_PSI__H
#define ALGO_BLAST_CORE___BLAST_PSI__H

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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_psi.h
 * High level definitions and declarations for the PSI-BLAST API.
 */

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_options.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Defaults for PSI-BLAST options - these are application level options */
#define PSI_MAX_NUM_PASSES      1
#define PSI_SCALING_FACTOR      32

/** Percent identity threshold for discarding near-identical matches */
#define PSI_NEAR_IDENTICAL      0.94
#define PSI_IDENTICAL           1.0

/** Structure to describe the characteristics of a position in the model */
typedef struct PsiDesc {
    Uint1 letter;               /**< Preferred letter at this position */
    Boolean used;               /**< Is this letter being used? */
    double e_value;             /**< E-value of the highest HSP including this
                                     position */
    SSeqRange extents;          /**< Extent of this aligned position */
} PsiDesc;

typedef struct PsiInfo {
    Uint4 query_sz;  /**< Size of the query */
    Uint4 num_seqs;  /**< Number of sequences in involved w/o including
                                 the query */
} PsiInfo;

/** This structure is to be populated at the API level from the Seq-aligns */
typedef struct PsiAlignmentData {
    Uint4** res_counts;/**< matrix which keeps track of the number of
                               residues aligned with the query at each query 
                               position (columns of multiple alignment). Its
                               dimensions are query_sz by alphabet_size */
    Uint4* match_seqs;/**< Count of how many sequences match the query
                               at each query position, default value is 1 to 
                               include the query itself. This dynamically 
                               allocated array has a length of query_sz */
    PsiDesc** desc_matrix;   /**< Matrix of PsiDesc structures, each cell
                                  represents an aligned character with the 
                                  query. Its dimensions are query_sz by
                                  num_seqs + 1. */
    Boolean* use_sequences;  /**< Determines if sequences must be used or not.
                                  This dynamically allocated array has a length
                                  of num_seqs + 1. */

    PsiInfo* dimensions;     /**< Dimensions of the multiple sequence alignment
                               (query size by number of sequences aligned + 1
                               (to include the query) */
    Uint1* query;   /**< Query sequence (aka master, consensus) */
} PsiAlignmentData;

/** The functions to create internal structure to store intermediate data 
 * while creating a PSSM */
PsiAlignmentData*
PSIAlignmentDataNew(const Uint1* query, const PsiInfo* info);

PsiAlignmentData*
PSIAlignmentDataFree(PsiAlignmentData* align_data);

/** This is the return value from all the processing performed in this library.
 * At the API level this information should be copied into an Score-mat ASN.1
 * object */
typedef struct PsiMatrix {
    int** pssm;             /**< The PSSM, its dimensions are query_sz+1 by 
                                 PSI_ALPHABET_SIZE */
    int** scaled_pssm;      /**< not to be used by the public ? Dimensions are
                                 the same as above Needed in the last 2 stages
                                 of PSI-BLAST */
    double** res_freqs;     /**< The residue frequencies. Dimensions are the
                                 same as above (FIXME?) */
    Uint4 ncols;     /**< Number of columns in the matrices above
                                 (query size+1) */
} PsiMatrix;

/** Allocates a new PsiMatrix structure */
PsiMatrix*
PSIMatrixNew(Uint4 query_sz, Uint4 alphabet_size);

/** Deallocates the PsiMatrix structure passed in.
 * @param matrix structure to deallocate [in]
 * @return NULL
 */
PsiMatrix*
PSIMatrixFree(PsiMatrix* matrix);


/** This structure returns detailed information collected during the process of
 * creating a PSSM.
 */
typedef struct PsiDiagnostics {
    double** info_content;
    Uint4 ncols;     /**< Number of columns in the matrix above
                                 (query size+1) */
    /* FIXME: add sequence weights */
} PsiDiagnostics;

/** Allocates a new PSI-BLAST diagnostics structure
 */
PsiDiagnostics*
PSIDiagnosticsNew(Uint4 query_sz, Uint4 alphabet_size);

/** Deallocates the PsiDiagnostics structure passed in.
 * @param diags structure to deallocate [in]
 * @return NULL
 */
PsiDiagnostics*
PSIDiagnosticsFree(PsiDiagnostics* diags);

/****************************************************************************/

/* TOP LEVEL FUNCTION FIXME: alignment should contain data from multiple
 * sequence alignment 
 * @param diagnostics If non-NULL this structure will be populated and it is
 *        the caller's responsibility to deallocate this structure.
 * @retval The PSSM along with residue frequencies and statistical information
 * (the latter is returned in the sbp)
 */
PsiMatrix*
PSICreatePSSM(PsiAlignmentData* alignment,      /* [in] but modified */
              const PSIBlastOptions* options,   /* [in] */
              BlastScoreBlk* sbp,               /* [in] */
              PsiDiagnostics* diagnostics);     /* [out] */

#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__BLAST_PSI__H */

