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
 * Author:  Alejandro Schaffer, ported by Christiam Camacho
 *
 */

/** @file blast_psi_priv.c
 * Defintions for functions in private interface for Position Iterated BLAST 
 * API.
 */

#include "blast_psi_priv.h"
#include "blast_posit.h"
#include <algo/blast/core/blast_util.h>

/****************************************************************************/
/* Use the following #define's to enable/disable functionality */

/* Taking gaps into account when constructing a PSSM was introduced in the 
 * 2001 paper "Improving the accuracy of PSI-BLAST protein database searches
 * with composition based-statistics and other refinements". This feature 
 * can be disabled by defining the PSI_IGNORE_GAPS_IN_COLUMNS symbol below */
/* #define PSI_IGNORE_GAPS_IN_COLUMNS */
/****************************************************************************/

/****************************************************************************/
/* Constants */
const double kPSINearIdentical = 0.94;
const double kPSIIdentical = 1.0;
const unsigned int kQueryIndex = 0;
const double kEpsilon = 0.0001;
const int kPSIScaleFactor = 200;
const double kPositScalingPercent = 0.05;
const Uint4 kPositScalingNumIterations = 10;

/****************************************************************************/

void**
_PSIAllocateMatrix(unsigned int ncols, unsigned int nrows, 
                   unsigned int data_type_sz)
{
    void** retval = NULL;
    unsigned int i = 0;

    retval = (void**) malloc(sizeof(void*) * ncols);
    if ( !retval ) {
        return NULL;
    }

    for (i = 0; i < ncols; i++) {
        retval[i] = (void*) calloc(nrows, data_type_sz);
        if ( !retval[i] ) {
            retval = _PSIDeallocateMatrix(retval, i);
            break;
        }
    }
    return retval;
}

void**
_PSIDeallocateMatrix(void** matrix, unsigned int ncols)
{
    unsigned int i = 0;

    if (!matrix)
        return NULL;

    for (i = 0; i < ncols; i++) {
        sfree(matrix[i]);
    }
    sfree(matrix);
    return NULL;
}

/** Implements the generic copy matrix functions. Prototypes must be defined
 * in the header file manually following the naming convention for 
 * _PSICopyMatrix_int
 */
#define DEFINE_COPY_MATRIX_FUNCTION(type)                           \
void _PSICopyMatrix_##type(type** dest, type** src,                 \
                          unsigned int ncols, unsigned int nrows)   \
{                                                                   \
    unsigned int i = 0;                                             \
    unsigned int j = 0;                                             \
                                                                    \
    ASSERT(dest);                                                   \
    ASSERT(src);                                                    \
                                                                    \
    for (i = 0; i < ncols; i++) {                                   \
        for (j = 0; j < nrows; j++) {                               \
            dest[i][j] = src[i][j];                                 \
        }                                                           \
    }                                                               \
}                                                                   \

DEFINE_COPY_MATRIX_FUNCTION(int)
DEFINE_COPY_MATRIX_FUNCTION(double)

/****************************************************************************/

_PSIMsa*
_PSIMsaNew(const PSIMsa* msa, Uint4 alphabet_size)
{
    _PSIMsa* retval = NULL;     /* the return value */
    Uint4 s = 0;                /* index on sequences */
    Uint4 p = 0;                /* index on positions */

    if ( !msa || !msa->dimensions || !msa->data ) {
        return NULL;
    }

    retval = (_PSIMsa*) calloc(1, sizeof(_PSIMsa));
    if ( !retval ) {
        return _PSIMsaFree(retval);
    }

    retval->alphabet_size = alphabet_size;
    retval->dimensions = (PSIMsaDimensions*) malloc(sizeof(PSIMsaDimensions));
    if ( !retval->dimensions ) {
        return _PSIMsaFree(retval);
    }
    memcpy((void*) retval->dimensions,
           (void*) msa->dimensions,
           sizeof(PSIMsaDimensions));

    retval->cell = (_PSIMsaCell**)
        _PSIAllocateMatrix(msa->dimensions->num_seqs + 1,
                           msa->dimensions->query_length,
                           sizeof(_PSIMsaCell));
    if ( !retval->cell ) {
        return _PSIMsaFree(retval);
    }
    /* Copy the multiple sequence alignment data */
    for (s = 0; s < msa->dimensions->num_seqs + 1; s++) {
        for (p = 0; p < msa->dimensions->query_length; p++) {
            retval->cell[s][p].letter = msa->data[s][p].letter;
            retval->cell[s][p].is_aligned = msa->data[s][p].is_aligned;
            retval->cell[s][p].extents.left = -1;
            retval->cell[s][p].extents.right = msa->dimensions->query_length;
        }
    }

    retval->use_sequence = (Boolean*) malloc((msa->dimensions->num_seqs + 1) *
                                             sizeof(Boolean));
    if ( !retval->use_sequence ) {
        return _PSIMsaFree(retval);
    }
    /* All sequences are valid candidates for taking part in PSSM construction
     * by default */
    for (s = 0; s < msa->dimensions->num_seqs + 1; s++) {
        retval->use_sequence[s] = TRUE;
    }

    retval->query = (Uint1*) malloc(msa->dimensions->query_length *
                                    sizeof(Uint1));
    if ( !retval->query ) {
        return _PSIMsaFree(retval);
    }
    /* Initialize the query according to convention that first sequence in msa
     * data structure corresponds to the query */
    for (p = 0; p < msa->dimensions->query_length; p++) {
        ASSERT(IS_residue(msa->data[kQueryIndex][p].letter));
        retval->query[p] = msa->data[kQueryIndex][p].letter;
    }

    retval->residue_counts = (Uint4**)
        _PSIAllocateMatrix(msa->dimensions->query_length,
                           alphabet_size,
                           sizeof(Uint4));
    if ( !retval->residue_counts ) {
        return _PSIMsaFree(retval);
    }

    retval->num_matching_seqs = (Uint4*) calloc(msa->dimensions->query_length,
                                                sizeof(Uint4));
    if ( !retval->num_matching_seqs ) {
        return _PSIMsaFree(retval);
    }

    return retval;
}

_PSIMsa*
_PSIMsaFree(_PSIMsa* msa)
{
    if ( !msa ) {
        return NULL;
    }

    if ( msa->cell && msa->dimensions ) {
        _PSIDeallocateMatrix((void**) msa->cell,
                             msa->dimensions->num_seqs + 1);
        msa->cell = NULL;
    }

    if ( msa->use_sequence ) {
        sfree(msa->use_sequence);
    }

    if ( msa->query ) {
        sfree(msa->query);
    }

    if ( msa->residue_counts && msa->dimensions ) {
        _PSIDeallocateMatrix((void**) msa->residue_counts,
                             msa->dimensions->query_length);
        msa->residue_counts = NULL;
    }

    if ( msa->num_matching_seqs ) {
        sfree(msa->num_matching_seqs);
    }

    if ( msa->dimensions ) {
        sfree(msa->dimensions);
    }

    sfree(msa);

    return NULL;
}

_PSIInternalPssmData*
_PSIInternalPssmDataNew(Uint4 query_length, Uint4 alphabet_size)
{
    _PSIInternalPssmData* retval = NULL;

    retval = (_PSIInternalPssmData*) calloc(1, sizeof(_PSIInternalPssmData));
    if ( !retval ) {
        return NULL;
    }

    retval->ncols = query_length;
    retval->nrows = alphabet_size;

    retval->pssm = (int**) _PSIAllocateMatrix(retval->ncols,
                                              retval->nrows,
                                              sizeof(int));
    if ( !retval->pssm ) {
        return _PSIInternalPssmDataFree(retval);
    }

    retval->scaled_pssm = (int**) _PSIAllocateMatrix(retval->ncols,
                                                     retval->nrows,
                                                     sizeof(int));
    if ( !retval->scaled_pssm ) {
        return _PSIInternalPssmDataFree(retval);
    }

    retval->freq_ratios = (double**) _PSIAllocateMatrix(retval->ncols,
                                                        retval->nrows,
                                                        sizeof(double));
    if ( !retval->freq_ratios ) {
        return _PSIInternalPssmDataFree(retval);
    }

    return retval;
}

_PSIInternalPssmData*
_PSIInternalPssmDataFree(_PSIInternalPssmData* pssm_data)
{
    if ( !pssm_data ) {
        return NULL;
    }

    if (pssm_data->pssm) {
        pssm_data->pssm = (int**) 
            _PSIDeallocateMatrix((void**) pssm_data->pssm, pssm_data->ncols);
    }

    if (pssm_data->scaled_pssm) {
        pssm_data->scaled_pssm = (int**) 
            _PSIDeallocateMatrix((void**) pssm_data->scaled_pssm, 
                                 pssm_data->ncols);
    }

    if (pssm_data->freq_ratios) {
        pssm_data->freq_ratios = (double**) 
            _PSIDeallocateMatrix((void**) pssm_data->freq_ratios, 
                                 pssm_data->ncols);
    }

    sfree(pssm_data);

    return NULL;
}

_PSIAlignedBlock*
_PSIAlignedBlockNew(Uint4 query_length)
{
    _PSIAlignedBlock* retval = NULL;
    Uint4 i = 0;

    retval = (_PSIAlignedBlock*) calloc(1, sizeof(_PSIAlignedBlock));
    if ( !retval ) {
        return NULL;
    }

    retval->size = (Uint4*) calloc(query_length, sizeof(Uint4));
    if ( !retval->size ) {
        return _PSIAlignedBlockFree(retval);
    }

    retval->pos_extnt = (SSeqRange*) malloc(query_length * sizeof(SSeqRange));
    if ( !retval->pos_extnt ) {
        return _PSIAlignedBlockFree(retval);
    }

    /* N.B.: these initial values are deliberate so that the retval->size[i]
     * field is initialized with a value that exceeds query_length in
     * _PSIComputeAlignedRegionLengths and can be used as a sanity check */
    for (i = 0; i < query_length; i++) {
        retval->pos_extnt[i].left = -1;
        retval->pos_extnt[i].right = query_length;
    }
    return retval;
}

_PSIAlignedBlock*
_PSIAlignedBlockFree(_PSIAlignedBlock* aligned_blocks)
{
    if ( !aligned_blocks ) {
        return NULL;
    }

    if (aligned_blocks->size) {
        sfree(aligned_blocks->size);
    }

    if (aligned_blocks->pos_extnt) {
        sfree(aligned_blocks->pos_extnt);
    }

    sfree(aligned_blocks);
    return NULL;
}

_PSISequenceWeights*
_PSISequenceWeightsNew(const PSIMsaDimensions* dimensions, 
                       const BlastScoreBlk* sbp)
{
    _PSISequenceWeights* retval = NULL;

    ASSERT(dimensions);
    ASSERT(sbp);

    retval = (_PSISequenceWeights*) calloc(1, sizeof(_PSISequenceWeights));
    if ( !retval ) {
        return NULL;
    }

    retval->row_sigma = (double*) calloc(dimensions->num_seqs + 1, 
                                         sizeof(double));
    if ( !retval->row_sigma ) {
        return _PSISequenceWeightsFree(retval);
    }

    retval->norm_seq_weights = (double*) calloc(dimensions->num_seqs + 1, 
                                                sizeof(double));
    if ( !retval->norm_seq_weights ) {
        return _PSISequenceWeightsFree(retval);
    }

    retval->sigma = (double*) calloc(dimensions->query_length, sizeof(double));
    if ( !retval->sigma ) {
        return _PSISequenceWeightsFree(retval);
    }

    retval->match_weights = (double**) 
        _PSIAllocateMatrix(dimensions->query_length, 
                           sbp->alphabet_size, 
                           sizeof(double));
    retval->match_weights_size = dimensions->query_length;
    if ( !retval->match_weights ) {
        return _PSISequenceWeightsFree(retval);
    }

    retval->std_prob = BLAST_GetStandardAaProbabilities();
    if ( !retval->std_prob ) {
        return _PSISequenceWeightsFree(retval);
    }

    retval->gapless_column_weights = (double*)
        calloc(dimensions->query_length, sizeof(double));
    if ( !retval->gapless_column_weights ) {
        return _PSISequenceWeightsFree(retval);
    }

    return retval;
}

_PSISequenceWeights*
_PSISequenceWeightsFree(_PSISequenceWeights* seq_weights)
{
    if ( !seq_weights ) {
        return NULL;
    }

    if (seq_weights->row_sigma) {
        sfree(seq_weights->row_sigma);
    }

    if (seq_weights->norm_seq_weights) {
        sfree(seq_weights->norm_seq_weights);
    }

    if (seq_weights->sigma) {
        sfree(seq_weights->sigma);
    }

    if (seq_weights->match_weights) {
        _PSIDeallocateMatrix((void**) seq_weights->match_weights,
                             seq_weights->match_weights_size);
    }

    if (seq_weights->std_prob) {
        sfree(seq_weights->std_prob);
    }

    if (seq_weights->gapless_column_weights) {
        sfree(seq_weights->gapless_column_weights);
    }

    sfree(seq_weights);

    return NULL;
}

#ifdef _DEBUG
static char getRes(char input)
{
    switch (input) {
    case 0: return ('-');
    case 1: return ('A');
    case 2: return ('B');
    case 3: return ('C');
    case 4: return ('D');
    case 5: return ('E');
    case 6: return ('F');
    case 7: return ('G');
    case 8: return ('H');
    case 9: return ('I');
    case 10: return ('K');
    case 11: return ('L');
    case 12: return ('M');
    case 13: return ('N');
    case 14: return ('P');
    case 15: return ('Q');
    case 16: return ('R');
    case 17: return ('S');
    case 18: return ('T');
    case 19: return ('V');
    case 20: return ('W');
    case 21: return ('X');
    case 22: return ('Y');
    case 23: return ('Z');
    case 24: return ('U');
    case 25: return ('*');
    default: return ('?');
    }
}

void
__printMsa(const char* filename, const _PSIMsa* msa)
{
    Uint4 i, j;
    FILE* fp = NULL;

    ASSERT(msa);
    ASSERT(filename);

    fp = fopen(filename, "w");

    for (i = 0; i < msa->dimensions->num_seqs + 1; i++) {
        /*fprintf(fp, "%3d\t", i);*/
        for (j = 0; j < msa->dimensions->query_length; j++) {
            if (msa->cell[i][j].is_aligned) {
                fprintf(fp, "%c", getRes(msa->cell[i][j].letter));
            } else {
                fprintf(fp, ".");
            }
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
}
#endif /* _DEBUG */

/************** Validation routines *****************************************/

/** Validate that there are no gaps in the query sequence
 * @param msa multiple sequence alignment data structure [in]
 * @return PSIERR_GAPINQUERY if validation fails, else PSI_SUCCESS
 */
static int
s_PSIValidateNoGapsInQuery(const _PSIMsa* msa)
{
    const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA['-'];
    Uint4 p = 0;            /* index on positions sequences */
    ASSERT(msa);

    for (p = 0; p < msa->dimensions->query_length; p++) {
        if (msa->cell[kQueryIndex][p].letter == kGapResidue || 
            msa->query[p] == kGapResidue) {
            return PSIERR_GAPINQUERY;
        }
    }
    return PSI_SUCCESS;
}

/** Validate that there are no flanking gaps in the multiple sequence alignment
 * @param msa multiple sequence alignment data structure [in]
 * @return PSIERR_STARTINGGAP or PSIERR_ENDINGGAP if validation fails, 
 * else PSI_SUCCESS
 */
static int
s_PSIValidateNoFlankingGaps(const _PSIMsa* msa)
{
    const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA['-'];
    Uint4 s = 0;            /* index on sequences */
    Int4 p = 0;            /* index on positions sequences */
    ASSERT(msa);

    /* Look for starting gaps in alignments */
    for (s = 0; s < msa->dimensions->num_seqs + 1; s++) {

        if ( !msa->use_sequence[s] ) {
            continue;
        }

        /* find the first aligned residue */
        for (p = 0; p < (Int4) msa->dimensions->query_length; p++) {
            if (msa->cell[s][p].is_aligned) {
                if (msa->cell[s][p].letter == kGapResidue) {
                    return PSIERR_STARTINGGAP;
                } else {
                    break;
                }
            }
        }
    }

    /* Look for ending gaps in alignments */
    for (s = 0; s < msa->dimensions->num_seqs + 1; s++) {

        if ( !msa->use_sequence[s] ) {
            continue;
        }

        /* find the first aligned residue */
        for (p = msa->dimensions->query_length - 1; p >= 0; p--) {
            if (msa->cell[s][p].is_aligned) {
                if (msa->cell[s][p].letter == kGapResidue) {
                    return PSIERR_ENDINGGAP;
                } else {
                    break;
                }
            }
        }
    }

    return PSI_SUCCESS;
}

/** Validate that there are no unaligned columns or columns which only contain
 * gaps in the multiple sequence alignment.
 * @param msa multiple sequence alignment data structure [in]
 * @return PSIERR_UNALIGNEDCOLUMN or PSIERR_COLUMNOFGAPS if validation fails, 
 * else PSI_SUCCESS
 */
static int
s_PSIValidateAlignedColumns(const _PSIMsa* msa) 
{
    const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA['-'];
    Uint4 s = 0;            /* index on sequences */
    Uint4 p = 0;            /* index on positions sequences */
    ASSERT(msa);

    for (p = 0; p < msa->dimensions->query_length; p++) {

        Boolean found_aligned_sequence = FALSE;
        Boolean found_non_gap_residue = FALSE;

        for (s = kQueryIndex + 1; s < msa->dimensions->num_seqs + 1; s++) {

            if (msa->cell[s][p].is_aligned) {
                found_aligned_sequence = TRUE;
                if (msa->cell[s][p].letter != kGapResidue) {
                    found_non_gap_residue = TRUE;
                    break;
                }
            }
        }
        if ( !found_aligned_sequence ) {
            return PSIERR_UNALIGNEDCOLUMN;
        }
        if ( !found_non_gap_residue ) {
            return PSIERR_COLUMNOFGAPS;
        }
    }

    return PSI_SUCCESS;
}

/** Verify that after purging biased sequences in multiple sequence alignment
 * there are still sequences participating in the multiple sequences alignment
 * @todo merge with validation performed at the PSSM engine level. Will need to
 * pass Blast_Message to provide informative error messages.
 * @param msa multiple sequence alignment structure [in]
 * @return PSIERR_NOALIGNEDSEQS if validation fails, else PSI_SUCCESS
 */
static int
s_PSIValidateParticipatingSequences(const _PSIMsa* msa)
{
    Uint4 s = 0;            /* index on aligned sequences */
    ASSERT(msa);

    for (s = 0; s < msa->dimensions->num_seqs + 1; s++) {
        if (msa->use_sequence[s]) {
            break;
        }
    }

    if (s == msa->dimensions->num_seqs + 1) { /* break was never executed */
        return PSIERR_NOALIGNEDSEQS;
    } else {
        return PSI_SUCCESS;
    }
}

int
_PSIValidateMSA(const _PSIMsa* msa)
{
    int retval = PSI_SUCCESS;

    if ( !msa ) {
        return PSIERR_BADPARAM;
    }

    retval = s_PSIValidateNoFlankingGaps(msa);
    if (retval != PSI_SUCCESS) {
        return retval;
    }

    retval = s_PSIValidateAlignedColumns(msa);
    if (retval != PSI_SUCCESS) {
        return retval;
    }

    retval = s_PSIValidateNoGapsInQuery(msa);
    if (retval != PSI_SUCCESS) {
        return retval;
    }

    retval = s_PSIValidateParticipatingSequences(msa);
    if (retval != PSI_SUCCESS) {
        return retval;
    }

    return retval;
}

/****************************************************************************/
/* Function prototypes */

/** Remove those sequences which are identical to the query sequence 
 * @param msa multiple sequence alignment data structure [in]
 */
static void
s_PSIPurgeSelfHits(_PSIMsa* msa);

/** Keeps only one copy of any aligned sequences which are >kPSINearIdentical%
 * identical to one another
 * @param msa multiple sequence alignment data structure [in]
 */
static void
s_PSIPurgeNearIdenticalAlignments(_PSIMsa* msa);

/** This function compares the sequences in the msa->cell
 * structure indexed by sequence_index1 and seq_index2. If it finds aligned 
 * regions that have a greater percent identity than max_percent_identity, 
 * it removes the sequence identified by seq_index2.
 * FIXME: needs more descriptive name 
 * @param msa multiple sequence alignment data structure [in]
 * @param seq_index1 index of the sequence of interest [in]
 * @param seq_index2 index of the sequence of interest [in]
 * @param max_percent_identity percent identity needed to drop sequence
 * identified by seq_index2 from the multiple sequence alignment data structure
 * [in]
 */
static void
s_PSIPurgeSimilarAlignments(_PSIMsa* msa,
                           Uint4 seq_index1,
                           Uint4 seq_index2,
                           double max_percent_identity);
/****************************************************************************/

/**************** PurgeMatches stage of PSSM creation ***********************/
int
_PSIPurgeBiasedSegments(_PSIMsa* msa)
{
    if ( !msa ) {
        return PSIERR_BADPARAM;
    }

    s_PSIPurgeSelfHits(msa);
    s_PSIPurgeNearIdenticalAlignments(msa);
    _PSIUpdatePositionCounts(msa);

    return PSI_SUCCESS;
}

static void
s_PSIPurgeSelfHits(_PSIMsa* msa)
{
    Uint4 s = 0;        /* index on sequences */

    ASSERT(msa);

    for (s = kQueryIndex + 1; s < msa->dimensions->num_seqs + 1; s++) {
        s_PSIPurgeSimilarAlignments(msa, kQueryIndex, s, kPSIIdentical);
    }
}

static void
s_PSIPurgeNearIdenticalAlignments(_PSIMsa* msa)
{
    Uint4 i = 0;
    Uint4 j = 0;

    ASSERT(msa);

    for (i = 1; i < msa->dimensions->num_seqs + 1; i++) { 
        for (j = 1; (i + j) < msa->dimensions->num_seqs + 1; j++) {
            /* N.B.: The order of comparison of sequence pairs is deliberate,
             * tests on real data indicated that this approach allowed more
             * sequences to be purged */
            s_PSIPurgeSimilarAlignments(msa, j, (i + j), kPSINearIdentical);
        }
    }
}

void
_PSIUpdatePositionCounts(_PSIMsa* msa)
{
    Uint4 s = 0;        /* index on aligned sequences */
    Uint4 p = 0;        /* index on positions */

    ASSERT(msa);

    /* Reset the data in case this function is being called multiple times
     * after the initial counts were done (i.e.: structure group's
     * compatibility mode) */
    memset((void*) msa->num_matching_seqs, 0, 
           sizeof(Uint4)*msa->dimensions->query_length);
    for (p = 0; p < msa->dimensions->query_length; p++) {
        memset((void*) msa->residue_counts[p], 0, 
               sizeof(Uint4)*msa->alphabet_size);
    }

    for (s = 0; s < msa->dimensions->num_seqs + 1; s++) {

        if ( !msa->use_sequence[s] ) {
            continue;
        }

        for (p = 0; p < msa->dimensions->query_length; p++) {
            if (msa->cell[s][p].is_aligned) {
                const Uint1 res = msa->cell[s][p].letter;
                if (res >= msa->alphabet_size) {
                    continue;
                }
                msa->residue_counts[p][res]++;
                msa->num_matching_seqs[p]++;
            }
        }
    }
}

/** Defines the states of the finite state machine used in
 * s_PSIPurgeSimilarAlignments. Successor to posit.c's POS_COUNTING and
 * POS_RESTING */
typedef enum _EPSIPurgeFsmState {
    eCounting,
    eResting
} _EPSIPurgeFsmState;

/** Auxiliary structure to maintain information about two aligned regions
 * between the query and a subject sequence. It is used to store the data
 * manipulated by the finite state machine used in s_PSIPurgeSimilarAlignments.
 */
typedef struct _PSIAlignmentTraits {
    Uint4 start;            /**< starting offset of alignment w.r.t. query */
    Uint4 effective_length; /**< length of alignment not including Xs */
    Uint4 n_x_residues;     /**< number of X residues in alignment */
    Uint4 n_identical;      /**< number of identical residues in alignment */
} _PSIAlignmentTraits;

#ifdef DEBUG
static
void DEBUG_printTraits(_PSIAlignmentTraits* traits, 
                       _EPSIPurgeFsmState state, Uint4 position)
{
    fprintf(stderr, "Position: %d - State: %s\n", position,
            state == eCounting ? "eCounting" : "eResting");
    fprintf(stderr, "\tstart: %d\n", traits->start);
    fprintf(stderr, "\teffective_length: %d\n", traits->effective_length);
    fprintf(stderr, "\tn_x_residues: %d\n", traits->n_x_residues);
    fprintf(stderr, "\tn_identical: %d\n", traits->n_identical);
}
#endif

/** Resets the traits structure to restart finite state machine 
 * @param traits structure to reset [in|out]
 * @param position position in the multiple sequence alignment to which the
 * traits structure is initialized [in]
 */
static void
_PSIResetAlignmentTraits(_PSIAlignmentTraits* traits, Uint4 position)
{
    ASSERT(traits);
    memset((void*) traits, 0, sizeof(_PSIAlignmentTraits));
    traits->start = position;
}

/** Handles neither is aligned event FIXME: document better */
static void
_handleNeitherAligned(_PSIAlignmentTraits* traits, _EPSIPurgeFsmState* state,
                      _PSIMsa* msa, Uint4 seq_index, 
                      double max_percent_identity)
{
    ASSERT(traits);
    ASSERT(state);

    switch (*state) {
    case eCounting:
        /* Purge aligned region if max_percent_identity is exceeded */
        {
            if (traits->effective_length > 0) {
                double percent_identity = 
                    ((double)traits->n_identical) / traits->effective_length;
                if (percent_identity >= max_percent_identity) {
                    const unsigned int align_stop = 
                        traits->start + traits->effective_length +
                        traits->n_x_residues;
                    int rv = _PSIPurgeAlignedRegion(msa, seq_index, 
                                                    traits->start, align_stop);
                    ASSERT(rv == PSI_SUCCESS);
                    rv += 0;  /* dummy code to avoid warning in release mode */
                }
            }
        }

        *state = eResting;
        break;

    case eResting:
        /* No-op */
        break;

    default:
        abort();
    }
}

/** Handle event when both positions are aligned, using the same residue, but
 * this residue is not X */
static void
_handleBothAlignedSameResidueNoX(_PSIAlignmentTraits* traits, 
                                 _EPSIPurgeFsmState* state)
{
    ASSERT(traits);
    ASSERT(state);

    switch (*state) {
    case eCounting:
        traits->n_identical++;
        break;

    case eResting:
        /* No-op */
        break;

    default:
        abort();
    }
}

/** Handle the event when either position is aligned and either is X */
static void
_handleEitherAlignedEitherX(_PSIAlignmentTraits* traits, 
                            _EPSIPurgeFsmState* state)
{
    ASSERT(traits);
    ASSERT(state);

    switch (*state) {
    case eCounting:
        traits->n_x_residues++;
        break;

    case eResting:
        /* No-op */
        break;

    default:
        abort();
    }
}

/** Handle the event when either position is aligned and neither is X */
static void
_handleEitherAlignedNeitherX(_PSIAlignmentTraits* traits, 
                             _EPSIPurgeFsmState* state,
                             Uint4 position)
{
    ASSERT(traits);
    ASSERT(state);

    switch (*state) {
    case eCounting:
        traits->effective_length++;
        break;

    case eResting:
        _PSIResetAlignmentTraits(traits, position);
        traits->effective_length = 1;   /* count this residue */
        *state = eCounting;
        break;

    default:
        abort();
    }
}

static void
s_PSIPurgeSimilarAlignments(_PSIMsa* msa,
                           Uint4 seq_index1,
                           Uint4 seq_index2,
                           double max_percent_identity)
{

    const Uint1 kXResidue = AMINOACID_TO_NCBISTDAA['X'];
    _EPSIPurgeFsmState state = eCounting;   /* initial state of the fsm */
    _PSIAlignmentTraits traits;
    Uint4 p = 0;                    /* position on alignment */

    /* Nothing to do if sequences are the same or not selected for further
       processing */
    if ( seq_index1 == seq_index2 ||
         !msa->use_sequence[seq_index1] ||
         !msa->use_sequence[seq_index2] ) {
        return;
    }

    _PSIResetAlignmentTraits(&traits, p);

    /* Examine each position of the aligned sequences and use the fsm to
     * determine if a region of the alignment should be purged */
    for (p = 0; p < msa->dimensions->query_length; p++) {

        const _PSIMsaCell* kSeq1 = msa->cell[seq_index1];
        const _PSIMsaCell* kSeq2 = msa->cell[seq_index2];

        /* Indicates if the position in seq_index1 currently being examined is 
         * aligned. In the special case for seq_index1 == kQueryIndex, this 
         * variable is set to FALSE to force the other sequence's position to 
         * be used to proceed with processing. */
        const Boolean kPos1Aligned = (seq_index1 == kQueryIndex ? 
                                      FALSE : kSeq1[p].is_aligned);
        /* Indicates if the position in seq_index2 currently being examined is 
         * aligned. */
        const Boolean kPos2Aligned = kSeq2[p].is_aligned;

        Boolean neither_is_aligned = !kPos1Aligned && !kPos2Aligned;
        /* FIXME: kludgy solution, need to document fsm */
        Boolean both_are_aligned = kSeq1[p].is_aligned && kPos2Aligned;
        Boolean either_is_aligned = kPos1Aligned || kPos2Aligned;

        Boolean neither_is_X = 
           kSeq1[p].letter != kXResidue && kSeq2[p].letter != kXResidue;
        Boolean either_is_X = 
           kSeq1[p].letter == kXResidue || kSeq2[p].letter == kXResidue;
        Boolean same_residue = kSeq1[p].letter == kSeq2[p].letter;

        /* Look for events interesting to the finite state machine */
        if (neither_is_aligned) {
            _handleNeitherAligned(&traits, &state, msa, seq_index2,
                                  max_percent_identity);
        } else {

            if (either_is_aligned && either_is_X) {
                _handleEitherAlignedEitherX(&traits, &state);
            } 
            if (either_is_aligned && neither_is_X) {
                _handleEitherAlignedNeitherX(&traits, &state, p);
            }
            if (both_are_aligned && same_residue && neither_is_X) {
                _handleBothAlignedSameResidueNoX(&traits, &state);
            } 
        }
    }
    _handleNeitherAligned(&traits, &state, msa, seq_index2,
                          max_percent_identity);
}

/****************************************************************************/
/* Function prototypes */

/** Computes the left extents for the sequence identified by seq_index
 * @param msa multiple sequence alignment data structure [in]
 * @param seq_index index of the sequence of interest [in]
 */
static void
_PSIGetLeftExtents(const _PSIMsa* msa, Uint4 seq_index);

/** Computes the right extents for the sequence identified by seq_index
 * @param msa multiple sequence alignment data structure [in]
 * @param seq_index index of the sequence of interest [in]
 */
static void
_PSIGetRightExtents(const _PSIMsa* msa, Uint4 seq_index);

/** Computes the aligned blocks extents' for each position for the sequence 
 * identified by seq_index
 * @param msa multiple sequence alignment data structure [in]
 * @param seq_index index of the sequence of interest [in]
 * @param aligned_blocks aligned regions' extents [out]
 */
static void
_PSIComputePositionExtents(const _PSIMsa* msa, 
                           Uint4 seq_index,
                           _PSIAlignedBlock* aligned_blocks);

/** Calculates the aligned blocks lengths in the multiple sequence alignment
 * data structure.
 * @param msa multiple sequence alignment data structure [in]
 * @param nsg_compatibility_mode set to true to emulate the structure group's
 * use of PSSM engine in the cddumper application. By default should be FALSE
 * [in]
 * @param aligned_blocks aligned regions' extents [in|out]
 */
static void
_PSIComputeAlignedRegionLengths(const _PSIMsa* msa,
                                Boolean nsg_compatibility_mode,
                                _PSIAlignedBlock* aligned_blocks);

/****************************************************************************/
/******* Compute alignment extents stage of PSSM creation *******************/
/* posComputeExtents in posit.c */
int
_PSIComputeAlignmentBlocks(const _PSIMsa* msa,                  /* [in] */
                           Boolean nsg_compatibility_mode,      /* [in] */
                           _PSIAlignedBlock* aligned_blocks)    /* [out] */
{
    Uint4 s = 0;     /* index on aligned sequences */

    if ( !msa || !aligned_blocks ) {
        return PSIERR_BADPARAM;
    }

    /* no need to compute extents for query sequence */
    for (s = kQueryIndex + 1; s < msa->dimensions->num_seqs + 1; s++) {
        if ( !msa->use_sequence[s] )
            continue;

        _PSIGetLeftExtents(msa, s);
        _PSIGetRightExtents(msa, s);
        _PSIComputePositionExtents(msa, s, aligned_blocks);
    }

    _PSIComputeAlignedRegionLengths(msa, nsg_compatibility_mode,
                                    aligned_blocks);

    return PSI_SUCCESS;
}

static void
_PSIGetLeftExtents(const _PSIMsa* msa, Uint4 seq_index)
{
    const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA['-'];
    _PSIMsaCell* sequence_position = NULL;
    Uint4 prev = 0;  /* index for the first and previous position */
    Uint4 curr = 0;  /* index for the current position */

    ASSERT(msa);
    ASSERT(seq_index < msa->dimensions->num_seqs + 1);
    ASSERT(msa->use_sequence[seq_index]);

    sequence_position = msa->cell[seq_index];

    if (sequence_position[prev].is_aligned && 
        sequence_position[prev].letter != kGapResidue) {
        sequence_position[prev].extents.left = prev;
    }

    for (curr = prev + 1; curr < msa->dimensions->query_length; 
         curr++, prev++) {

        if ( !sequence_position[curr].is_aligned ) {
            continue;
        }

        if (sequence_position[prev].is_aligned) {
            sequence_position[curr].extents.left =
                sequence_position[prev].extents.left;
        } else {
            sequence_position[curr].extents.left = curr;
        }
    }
}

static void
_PSIGetRightExtents(const _PSIMsa* msa, Uint4 seq_index)
{
    const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA['-'];
    _PSIMsaCell* sequence_position = NULL;
    Uint4 last = 0;      /* index for the last position */
    Int4 curr = 0;       /* index for the current position */

    ASSERT(msa);
    ASSERT(seq_index < msa->dimensions->num_seqs + 1);
    ASSERT(msa->use_sequence[seq_index]);

    sequence_position = msa->cell[seq_index];
    last = msa->dimensions->query_length - 1;

    if (sequence_position[last].is_aligned && 
        sequence_position[last].letter != kGapResidue) {
        sequence_position[last].extents.right = last;
    }

    for (curr = last - 1; curr >= 0; curr--, last--) {

        if ( !sequence_position[curr].is_aligned ) {
            continue;
        }

        if (sequence_position[last].is_aligned) {
            sequence_position[curr].extents.right =
                sequence_position[last].extents.right;
        } else {
            sequence_position[curr].extents.right = curr;
        }
    }
}

static void
_PSIComputePositionExtents(const _PSIMsa* msa, 
                           Uint4 seq_index,
                           _PSIAlignedBlock* aligned_blocks)
{
#ifdef PSI_IGNORE_GAPS_IN_COLUMNS
    const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA['-'];
#endif
    _PSIMsaCell* sequence_position = NULL;
    Uint4 i = 0;

    ASSERT(aligned_blocks);
    ASSERT(msa);
    ASSERT(seq_index < msa->dimensions->num_seqs + 1);
    ASSERT(msa->use_sequence[seq_index]);

    sequence_position = msa->cell[seq_index];

    for (i = 0; i < msa->dimensions->query_length; i++) {
#ifdef PSI_IGNORE_GAPS_IN_COLUMNS
        if (sequence_position[i].is_aligned && 
            sequence_position[i].letter != kGapResidue) {
#else
        if (sequence_position[i].is_aligned) {
#endif
            aligned_blocks->pos_extnt[i].left = 
                MAX(aligned_blocks->pos_extnt[i].left, 
                    sequence_position[i].extents.left);
            aligned_blocks->pos_extnt[i].right = 
                MIN(aligned_blocks->pos_extnt[i].right, 
                    sequence_position[i].extents.right);
        }
    }
}

static void
_PSIComputeAlignedRegionLengths(const _PSIMsa* msa,
                                Boolean nsg_compatibility_mode,
                                _PSIAlignedBlock* aligned_blocks)
{
    const Uint1 kXResidue = AMINOACID_TO_NCBISTDAA['X'];
    Uint4 i = 0;
    
    ASSERT(msa);
    ASSERT(aligned_blocks);

    for (i = 0; i < msa->dimensions->query_length; i++) {
        aligned_blocks->size[i] = aligned_blocks->pos_extnt[i].right - 
                                   aligned_blocks->pos_extnt[i].left + 1;

        if ( !nsg_compatibility_mode ) {
            /* Sanity check: if aligned_blocks->pos_extnt[i].{right,left} was
               not modified after initialization, this assertion will fail */
            ASSERT(aligned_blocks->size[i] <= msa->dimensions->query_length);
        }
    }

    /* Do not include X's in aligned region lengths */
    for (i = 0; i < msa->dimensions->query_length; i++) {

        if (msa->query[i] == kXResidue) {

            Uint4 idx = 0;
            for (idx = 0; idx < i; idx++) {
                if ((Uint4)aligned_blocks->pos_extnt[idx].right >= i &&
                    msa->query[idx] != kXResidue) {
                    aligned_blocks->size[idx]--;
                }
            }
            for (idx = msa->dimensions->query_length - 1; idx > i; idx--) {
                if ((Uint4)aligned_blocks->pos_extnt[idx].left <= i &&
                    msa->query[idx] != kXResidue) {
                    aligned_blocks->size[idx]--;
                }
            }
        }

    }
}

/****************************************************************************/

/** Populates the array aligned_sequences with the indices of the sequences
 * which are part of the multiple sequence alignment at the request position
 * @param msa multiple sequence alignment data structure [in]
 * @param position position of interest [in]
 * @param aligned_sequences array which will contain the indices of the
 * sequences aligned at the requested position. This array must have size
 * greater than or equal to the number of sequences + 1 in multiple alignment 
 * data structure (alignment->dimensions->num_seqs + 1) [out]
 * @return number of sequences aligned at the requested position
 */
static Uint4
_PSIGetAlignedSequencesForPosition(
    const _PSIMsa* msa, 
    Uint4 position,
    Uint4* aligned_sequences);

/** Calculates the position based weights using a modified version of the
 * Henikoff's algorithm presented in "Position-based sequence weights". 
 * Skipped optimization about identical previous sets.
 * @param msa multiple sequence alignment data structure [in]
 * @param aligned_blocks aligned regions' extents [in]
 * @param position position of the query to calculate the sequence weights for
 * [in]
 * @param aligned_seqs array containing the indices of the sequences 
 * participating in the multiple sequence alignment at the requested 
 * position [in]
 * @param num_aligned_seqs length of the array above [in]
 * @param seq_weights sequence weights data structure [out]
 */
static void
_PSICalculateNormalizedSequenceWeights(
    const _PSIMsa* msa,
    const _PSIAlignedBlock* aligned_blocks,
    Uint4 position,
    const Uint4* aligned_seqs,
    Uint4 num_aligned_seqs,
    _PSISequenceWeights* seq_weights);

/** Calculate the weighted observed sequence weights
 * @param msa multiple sequence alignment data structure [in]
 * @param position position of the query to calculate the sequence weights for
 * [in]
 * @param aligned_seqs array containing the indices of the sequences 
 * participating in the multiple sequence alignment at the requested 
 * position [in]
 * @param num_aligned_seqs length of the array above [in]
 * @param seq_weights sequence weights data structure [in|out]
 */
static void
_PSICalculateMatchWeights(
    const _PSIMsa* msa,
    Uint4 position,
    const Uint4* aligned_seqs,
    Uint4 num_aligned_seqs,
    _PSISequenceWeights* seq_weights);

/** Uses disperse method of spreading the gap weights
 * @param msa multiple sequence alignment data structure [in]
 * @param seq_weights sequence weights data structure [in|out]
 * @param nsg_compatibility_mode set to true to emulate the structure group's
 * use of PSSM engine in the cddumper application. By default should be FALSE
 * [in]
 */
static void
_PSISpreadGapWeights(const _PSIMsa* msa,
                     _PSISequenceWeights* seq_weights,
                     Boolean nsg_compatibility_mode);

/** Verifies that the sequence weights for each column of the PSSM add up to
 * 1.0.
 * @param msa multiple sequence alignment data structure [in]
 * @param seq_weights sequence weights data structure [in]
 * @param nsg_compatibility_mode set to true to emulate the structure group's
 * use of PSSM engine in the cddumper application. By default should be FALSE
 * [in]
 * @return PSIERR_BADSEQWEIGHTS in case of failure, PSI_SUCCESS otherwise
 */
static int
_PSICheckSequenceWeights(
    const _PSIMsa* msa,
    const _PSISequenceWeights* seq_weights,
    Boolean nsg_compatibility_mode);

/****************************************************************************/
/******* Calculate sequence weights stage of PSSM creation ******************/
/* Needs the _PSIAlignedBlock structure calculated in previous stage as well
 * as PSIAlignmentData structure */

int
_PSIComputeSequenceWeights(const _PSIMsa* msa,                      /* [in] */
                           const _PSIAlignedBlock* aligned_blocks,  /* [in] */
                           Boolean nsg_compatibility_mode,          /* [in] */
                           _PSISequenceWeights* seq_weights)        /* [out] */
{
    Uint4* aligned_seqs = NULL;     /* list of indices of sequences
                                       which participate in an
                                       aligned position */
    Uint4 pos = 0;                  /* position index */
    int retval = PSI_SUCCESS;       /* return value */
    const Uint4 kExpectedNumMatchingSeqs = nsg_compatibility_mode ? 0 : 1;

    if ( !msa || !aligned_blocks || !seq_weights ) {
        return PSIERR_BADPARAM;
    }

    aligned_seqs = (Uint4*) malloc((msa->dimensions->num_seqs + 1) *
                                   sizeof(Uint4));
    if ( !aligned_seqs ) {
        return PSIERR_OUTOFMEM;
    }

    for (pos = 0; pos < msa->dimensions->query_length; pos++) {

        Uint4 num_aligned_seqs = 0;

        /* ignore positions of no interest */
        if (aligned_blocks->size[pos] == 0 || 
            msa->num_matching_seqs[pos] <= kExpectedNumMatchingSeqs) {
            continue;
        }

        memset((void*)aligned_seqs, 0, 
               sizeof(Uint4) * (msa->dimensions->num_seqs + 1));

        num_aligned_seqs = _PSIGetAlignedSequencesForPosition(msa, pos,
                                                              aligned_seqs);
        ASSERT(msa->num_matching_seqs[pos] == num_aligned_seqs);
        if (num_aligned_seqs <= kExpectedNumMatchingSeqs) {
            continue;
        }

        memset((void*)seq_weights->norm_seq_weights, 0, 
               sizeof(double)*(msa->dimensions->num_seqs+1));
        memset((void*)seq_weights->row_sigma, 0,
               sizeof(double)*(msa->dimensions->num_seqs+1));

        _PSICalculateNormalizedSequenceWeights(msa, aligned_blocks, pos, 
                                               aligned_seqs, num_aligned_seqs, 
                                               seq_weights);


        /* Uses seq_weights->norm_seq_weights to populate match_weights */
        _PSICalculateMatchWeights(msa, pos, aligned_seqs,
                                  num_aligned_seqs, seq_weights);
    }

    sfree(aligned_seqs);

    /* Check that the sequence weights add up to 1 in each column */
    retval = _PSICheckSequenceWeights(msa, seq_weights, 
                                      nsg_compatibility_mode);
    if (retval != PSI_SUCCESS) {
        return retval;
    }

#ifndef PSI_IGNORE_GAPS_IN_COLUMNS
    _PSISpreadGapWeights(msa, seq_weights, nsg_compatibility_mode);
    retval = _PSICheckSequenceWeights(msa, seq_weights, 
                                      nsg_compatibility_mode);
#endif

    /* Return seq_weights->match_weigths, should free others? FIXME: need to
     * keep sequence weights for diagnostics for structure group */
    return retval;
}

static void
_PSICalculateNormalizedSequenceWeights(
    const _PSIMsa* msa,
    const _PSIAlignedBlock* aligned_blocks, /* [in] */
    Uint4 position,                        /* [in] */
    const Uint4* aligned_seqs,             /* [in] */
    Uint4 num_aligned_seqs,                /* [in] */
    _PSISequenceWeights* seq_weights)       /* [out] norm_seq_weights, sigma */
{
    /* Index into aligned block for requested position */
    Uint4 i = 0;         

    /* This flag will be true if more than one different type of residue is
     * found in a column in the extent of the alignment that corresponds to 
     * the position being examined. (replaces Sigma in old code) */
    Boolean distinct_residues_found = FALSE;

    /* Number of different characters occurring in matches within a 
     * multi-alignment block including identical columns (replaces
     * intervalSigma in old code) 
     * FIXME: alternate description
     * number of distinct residues in all columns in the extent of the 
     * alignment corresponding to a position
     */
    Uint4 sigma = 0;

    /* Index into array of aligned sequences */
    Uint4 asi = 0;      

    ASSERT(msa);
    ASSERT(aligned_blocks);
    ASSERT(seq_weights);
    ASSERT(aligned_seqs);
    ASSERT(position < msa->dimensions->query_length);

    for (i = (Uint4)aligned_blocks->pos_extnt[position].left; 
         i <= (Uint4)aligned_blocks->pos_extnt[position].right; i++) {

        /* Keeps track of how many occurrences of each residue are found in a
         * column of the alignment extent corresponding to a query position */
        Uint4 residue_counts_for_column[BLASTAA_SIZE];

        /* number of distinct residues found in a column of the alignment
         * extent correspoding to a query position */
        Uint4 num_distinct_residues_for_column = 0; 

        memset((void*) residue_counts_for_column, 0, 
               sizeof(Uint4)*BLASTAA_SIZE);

        /* Assert that the alignment extents have sane values */
        ASSERT(i < msa->dimensions->query_length);

        /* Count number of residues in column i of the alignment extent
         * corresponding to position */
        for (asi = 0; asi < num_aligned_seqs; asi++) {
            const Uint4 kSeqIdx = aligned_seqs[asi];
            const Uint1 kResidue = 
                msa->cell[kSeqIdx][i].letter;

            if (residue_counts_for_column[kResidue] == 0) {
                num_distinct_residues_for_column++;
            }
            residue_counts_for_column[kResidue]++;
        }

        sigma += num_distinct_residues_for_column;
        if (num_distinct_residues_for_column > 1) {
            /* num_distinct_residues_for_column == 1 means that all residues in
             * that column of the alignment extent are the same residue */
            distinct_residues_found = TRUE;
        }

        /* Calculate row_sigma, an intermediate value to calculate the
         * normalized sequence weights */
        for (asi = 0; asi < num_aligned_seqs; asi++) {
            const Uint4 seq_idx = aligned_seqs[asi];
            const Uint1 residue = msa->cell[seq_idx][i].letter;

            /* This is a modified version of the Henikoff's idea in
             * "Position-based sequence weights" paper. The modification
             * consists in using the alignment extents. */
            seq_weights->row_sigma[seq_idx] += 
                (1.0 / (double) 
                 (residue_counts_for_column[residue] * 
                  num_distinct_residues_for_column) );
        }
    }

    /* Save sigma for this position */
    seq_weights->sigma[position] = sigma;

    if (distinct_residues_found) {
        double weight_sum = 0.0;

        for (asi = 0; asi < num_aligned_seqs; asi++) {
            const Uint4 seq_idx = aligned_seqs[asi];
            seq_weights->norm_seq_weights[seq_idx] = 
                seq_weights->row_sigma[seq_idx] / 
                (aligned_blocks->pos_extnt[position].right -
                 aligned_blocks->pos_extnt[position].left + 1);
            weight_sum += seq_weights->norm_seq_weights[seq_idx];
        }

        /* Normalize */
        for (asi = 0; asi < num_aligned_seqs; asi++) {
            const Uint4 seq_idx = aligned_seqs[asi];
            seq_weights->norm_seq_weights[seq_idx] /= weight_sum;
        }

    } else {
        /* All residues in the extent of this position's alignment are the same
         * residue, therefore we distribute the sequence weight equally among
         * all participating sequences */
        for (asi = 0; asi < num_aligned_seqs; asi++) {
            const Uint4 seq_idx = aligned_seqs[asi];
            seq_weights->norm_seq_weights[seq_idx] = 
                (1.0/(double) num_aligned_seqs);
        }
    }

    return;
}

static void
_PSICalculateMatchWeights(
    const _PSIMsa* msa,  /* [in] */
    Uint4 position,                     /* [in] */
    const Uint4* aligned_seqs,          /* [in] */
    Uint4 num_aligned_seqs,             /* [in] */
    _PSISequenceWeights* seq_weights)    /* [out] */
{
    const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA['-'];
    Uint4 asi = 0;   /* index into array of aligned sequences */

    ASSERT(msa);
    ASSERT(aligned_seqs);
    ASSERT(seq_weights);

    for (asi = 0; asi < num_aligned_seqs; asi++) {
        const Uint4 seq_idx = aligned_seqs[asi];
        const Uint1 residue = msa->cell[seq_idx][position].letter;

        seq_weights->match_weights[position][residue] += 
            seq_weights->norm_seq_weights[seq_idx];

        /* Collected for diagnostics information, not used elsewhere */
        if (residue != kGapResidue) {
            seq_weights->gapless_column_weights[position] +=
             seq_weights->norm_seq_weights[seq_idx];
        }
    }
}

static Uint4
_PSIGetAlignedSequencesForPosition(const _PSIMsa* msa, 
                                   Uint4 position,
                                   Uint4* aligned_sequences)
{
#ifdef PSI_IGNORE_GAPS_IN_COLUMNS
    const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA['-'];
#endif
    Uint4 retval = 0;
    Uint4 i = 0;

    ASSERT(msa);
    ASSERT(position < msa->dimensions->query_length);
    ASSERT(aligned_sequences);

    for (i = 0; i < msa->dimensions->num_seqs + 1; i++) {

        if ( !msa->use_sequence[i] ) {
            continue;
        }

#ifdef PSI_IGNORE_GAPS_IN_COLUMNS
        if (msa->cell[i][position].is_aligned &&
            msa->cell[i][position].letter != kGapResidue) {
#else
        if (msa->cell[i][position].is_aligned) {
#endif
            aligned_sequences[retval++] = i;
        }
    }

    return retval;
}

static void
_PSISpreadGapWeights(const _PSIMsa* msa,
                     _PSISequenceWeights* seq_weights,
                     Boolean nsg_compatibility_mode)
{
    const Uint1 kGapResidue = AMINOACID_TO_NCBISTDAA['-'];
    const Uint1 kXResidue = AMINOACID_TO_NCBISTDAA['X'];
    Uint4 pos = 0;   /* residue position (ie: column number) */
    Uint4 res = 0;   /* residue */
    const Uint4 kExpectedNumMatchingSeqs = nsg_compatibility_mode ? 0 : 1;

    ASSERT(msa);
    ASSERT(seq_weights);

    for (pos = 0; pos < msa->dimensions->query_length; pos++) {

        if (msa->num_matching_seqs[pos] <= kExpectedNumMatchingSeqs ||
            msa->cell[kQueryIndex][pos].letter == kXResidue) {
            continue;
        }

        /* Disperse method of spreading the gap weights */
        for (res = 0; res < msa->alphabet_size; res++) {
            if (seq_weights->std_prob[res] > kEpsilon) {
                seq_weights->match_weights[pos][res] += 
                    (seq_weights->match_weights[pos][kGapResidue] * 
                     seq_weights->std_prob[res]);
            }
        }
        seq_weights->match_weights[pos][kGapResidue] = 0.0;

    }
}

/* Verifies that each column of the match_weights field in the seq_weights
 * structure adds up to 1. */
static int
_PSICheckSequenceWeights(const _PSIMsa* msa,
                         const _PSISequenceWeights* seq_weights,
                         Boolean nsg_compatibility_mode)
{
    const Uint1 kXResidue = AMINOACID_TO_NCBISTDAA['X'];
    Uint4 pos = 0;   /* residue position (ie: column number) */
    Boolean check_performed = FALSE;  /* were there any sequences checked? */
    const Uint4 kExpectedNumMatchingSeqs = nsg_compatibility_mode ? 0 : 1;

    ASSERT(msa);
    ASSERT(seq_weights);

    for (pos = 0; pos < msa->dimensions->query_length; pos++) {

        double running_total = 0.0;
        Uint4 residue = 0;

        if (msa->num_matching_seqs[pos] <= kExpectedNumMatchingSeqs ||
            msa->cell[kQueryIndex][pos].letter == kXResidue) {
            continue;
        }

        for (residue = 0; residue < msa->alphabet_size; residue++) {
            running_total += seq_weights->match_weights[pos][residue];
        }

        if (running_total < 0.99 || running_total > 1.01) {
            return PSIERR_BADSEQWEIGHTS;
        }
        check_performed = TRUE;
    }

    /* This condition should never happen because it means that no sequences
     * were selected to calculate the sequence weights! */
    if ( !check_performed &&
         !nsg_compatibility_mode ) {    /* old code didn't check for this... */
        assert(!"Did not perform sequence weights check");
    }

    return PSI_SUCCESS;
}

/****************************************************************************/
/******* Compute residue frequencies stage of PSSM creation *****************/

int
_PSIComputeFreqRatios(const _PSIMsa* msa,
                      const _PSISequenceWeights* seq_weights,
                      const BlastScoreBlk* sbp,
                      const _PSIAlignedBlock* aligned_blocks,
                      Int4 pseudo_count,
                      _PSIInternalPssmData* internal_pssm)
{
    /* Subscripts are indicated as follows: N_i, where i is a subscript of N */
    const Uint1 kXResidue = AMINOACID_TO_NCBISTDAA['X'];
    SFreqRatios* freq_ratios = NULL;/* matrix-specific frequency ratios */
    Uint4 p = 0;                    /* index on positions */
    Uint4 r = 0;                    /* index on residues */

    if ( !msa || !seq_weights || !sbp || !aligned_blocks || !internal_pssm ) {
        return PSIERR_BADPARAM;
    }
    ASSERT(((Uint4)sbp->alphabet_size) == msa->alphabet_size);

    freq_ratios = _PSIMatrixFrequencyRatiosNew(sbp->name);

    for (p = 0; p < msa->dimensions->query_length; p++) {

        for (r = 0; r < msa->alphabet_size; r++) {

            /* If there is an 'X' in the query sequence at position p
               or the standard probability of residue r is close to 0 */
            if (msa->cell[kQueryIndex][p].letter == kXResidue ||
                seq_weights->std_prob[r] <= kEpsilon) {
                internal_pssm->freq_ratios[p][r] = 0.0;
            } else {

                Uint4 i = 0;             /* loop index */

                /* beta( Sum_j(f_j r_ij) ) in formula 2 */
                double pseudo = 0.0;            
                /* Effective number of independent observations for column p */
                double alpha = 0.0;
                /* Renamed to match the formula in the paper */
                const double kBeta = pseudo_count;
                double numerator = 0.0;         /* intermediate term */
                double denominator = 0.0;       /* intermediate term */
                double qOverPEstimate = 0.0;    /* intermediate term */

                /* As specified in 2001 paper, underlying matrix frequency 
                   ratios are used here */
                for (i = 0; i < msa->alphabet_size; i++) {
                    if (sbp->matrix->data[r][i] != BLAST_SCORE_MIN) {
                        pseudo += (seq_weights->match_weights[p][i] *
                                   freq_ratios->data[r][i]);
                    }
                }
                pseudo *= kBeta;

                alpha = seq_weights->sigma[p]/aligned_blocks->size[p]-1;

                numerator =
                    (alpha * seq_weights->match_weights[p][r] / 
                     seq_weights->std_prob[r]) 
                    + pseudo;

                denominator = alpha + kBeta;

                ASSERT(denominator != 0.0);
                qOverPEstimate = numerator/denominator;

                /* Note artificial multiplication by standard probability
                 * to normalize */
                internal_pssm->freq_ratios[p][r] = qOverPEstimate *
                    seq_weights->std_prob[r];

            }
        }
    }

    freq_ratios = _PSIMatrixFrequencyRatiosFree(freq_ratios);

    return PSI_SUCCESS;
}

/* port of posInitializeInformation */
double*
_PSICalculateInformationContentFromScoreMatrix(
    Int4** score_mat,
    const double* std_prob,
    const Uint1* query,
    Uint4 query_length,
    Uint4 alphabet_sz,
    double lambda)
{
    double* retval = NULL;      /* the return value */
    Uint4 p = 0;                /* index on positions */
    Uint4 r = 0;                /* index on residues */

    if ( !std_prob || !score_mat ) {
        return NULL;
    }

    retval = (double*) calloc(query_length, sizeof(double));
    if ( !retval ) {
        return NULL;
    }

    for (p = 0; p < query_length; p++) {

        double info_sum = 0.0;

        for (r = 0; r < alphabet_sz; r++) {

            if (std_prob[r] > kEpsilon) {
                Int4 score = score_mat[query[p]][r];
                double exponent = exp(score * lambda);
                double tmp = std_prob[r] * exponent;
                info_sum += tmp * log(tmp/std_prob[r])/ NCBIMATH_LN2;
            }

        }
        retval[p] = info_sum;
    }

    return retval;
}

double*
_PSICalculateInformationContentFromFreqRatios(
    double** freq_ratios,
    const double* std_prob,
    Uint4 query_length,
    Uint4 alphabet_sz)
{
    double* retval = NULL;      /* the return value */
    Uint4 p = 0;                /* index on positions */
    Uint4 r = 0;                /* index on residues */

    if ( !std_prob || !freq_ratios ) {
        return NULL;
    }

    retval = (double*) calloc(query_length, sizeof(double));
    if ( !retval ) {
        return NULL;
    }

    for (p = 0; p < query_length; p++) {

        double info_sum = 0.0;

        for (r = 0; r < alphabet_sz; r++) {

            if (std_prob[r] > kEpsilon) {
                /* Division compensates for multiplication in
                 * _PSIComputeFreqRatios */
                double qOverPEstimate = freq_ratios[p][r] / std_prob[r];
                if (qOverPEstimate > kEpsilon) {
                    info_sum += 
                        freq_ratios[p][r] * log(qOverPEstimate) / NCBIMATH_LN2;
                }
            }

        }
        retval[p] = info_sum;
    }

    return retval;
}


/****************************************************************************/
/**************** Convert residue frequencies to PSSM stage *****************/

/* FIXME: Answer questions
   FIXME: need ideal_labmda, regular scoring matrix, length of query
   port of posFreqsToMatrix
*/
int
_PSIConvertFreqRatiosToPSSM(_PSIInternalPssmData* internal_pssm,
                            const Uint1* query,
                            const BlastScoreBlk* sbp,
                            const double* std_probs)
{
    const Uint4 kXResidue = AMINOACID_TO_NCBISTDAA['X'];
    const Uint4 kStarResidue = AMINOACID_TO_NCBISTDAA['*'];
    Uint4 i = 0;
    Uint4 j = 0;
    SFreqRatios* freq_ratios = NULL;    /* only needed when there are not
                                           residue frequencies for a given 
                                           column */
    double ideal_lambda = 0.0;

    if ( !internal_pssm || !sbp || !std_probs )
        return PSIERR_BADPARAM;

    ideal_lambda = sbp->kbp_ideal->Lambda;
    freq_ratios = _PSIMatrixFrequencyRatiosNew(sbp->name);

    /* Each column is a position in the query */
    for (i = 0; i < internal_pssm->ncols; i++) {

        /* True if all frequencies in column i are zero */
        Boolean is_unaligned_column = TRUE;
        const Uint4 kResidue = query[i];

        for (j = 0; j < (Uint4) sbp->alphabet_size; j++) {

            double qOverPEstimate = 0.0;

            /* Division compensates for multiplication in
             * _PSIComputeFreqRatios */
            if (std_probs[j] > kEpsilon) {
                qOverPEstimate = 
                    internal_pssm->freq_ratios[i][j] / std_probs[j];
            }

            if (is_unaligned_column && qOverPEstimate != 0.0) {
                is_unaligned_column = FALSE;
            }

            /* Populate scaled matrix */
            if (qOverPEstimate == 0.0 || std_probs[j] < kEpsilon) {
                internal_pssm->scaled_pssm[i][j] = BLAST_SCORE_MIN;
            } else {
                double tmp = log(qOverPEstimate)/ideal_lambda;
                internal_pssm->scaled_pssm[i][j] = (int)
                    BLAST_Nint(kPSIScaleFactor * tmp);
            }

            if ( (j == kXResidue || j == kStarResidue) &&
                 (sbp->matrix->data[kResidue][kXResidue] != BLAST_SCORE_MIN) ) {
                internal_pssm->scaled_pssm[i][j] = 
                    sbp->matrix->data[kResidue][j] * kPSIScaleFactor;
            }
        }

        if (is_unaligned_column) {
            for (j = 0; j < (Uint4) sbp->alphabet_size; j++) {

                internal_pssm->pssm[i][j] = sbp->matrix->data[kResidue][j];

                if (sbp->matrix->data[kResidue][j] != BLAST_SCORE_MIN) {
                    double tmp = 
                        kPSIScaleFactor * freq_ratios->bit_scale_factor *
                        log(freq_ratios->data[kResidue][j])/NCBIMATH_LN2;

                    internal_pssm->scaled_pssm[i][j] = BLAST_Nint(tmp);
                } else {
                    internal_pssm->scaled_pssm[i][j] = BLAST_SCORE_MIN;
                }
            }
        }
    }

    freq_ratios = _PSIMatrixFrequencyRatiosFree(freq_ratios);

    return PSI_SUCCESS;
}

/****************************************************************************/
/************************* Scaling of PSSM stage ****************************/

/** FIXME
 * @param pssm PSSM [in]
 @ @param query query sequence in ncbistdaa encoding [in]
 * @param query_length length of the query sequence above [in]
 * @param std_probs array containing the standard background residue 
 * probabilities [in]
 * @param sbp Score block structure where the calculated lambda and K will be
 * returned [in|out]
 */
void
_PSIUpdateLambdaK(const int** pssm,
                  const Uint1* query,
                  Uint4 query_length,
                  const double* std_probs,
                  BlastScoreBlk* sbp);

/* FIXME: change so that only lambda is calculated inside the loop that scales
   the matrix and kappa is calculated before returning from this function.
   Scaling factor should be optional argument to accomodate kappa.c's needs?
*/
int
_PSIScaleMatrix(const Uint1* query,
                const double* std_probs,
                _PSIInternalPssmData* internal_pssm,
                BlastScoreBlk* sbp)
{
    Boolean first_time = TRUE;
    Uint4 index = 0;     /* loop index */
    int** scaled_pssm = NULL;
    int** pssm = NULL;
    double factor;
    double factor_low = 0.0;
    double factor_high = 0.0;
    double ideal_lambda = 0.0;      /* ideal value of ungapped lambda for
                                       underlying scoring matrix */
    double new_lambda = 0.0;        /* Karlin-Altschul parameter calculated 
                                       from scaled matrix*/

    Uint4 query_length = 0;
    Boolean too_high = TRUE;

    if ( !internal_pssm || !sbp || !query || !std_probs )
        return PSIERR_BADPARAM;

    ASSERT(sbp->kbp_psi[0]);
    ASSERT(sbp->kbp_ideal);

    scaled_pssm = internal_pssm->scaled_pssm;
    pssm = internal_pssm->pssm;
    ideal_lambda = sbp->kbp_ideal->Lambda;
    query_length = internal_pssm->ncols;

    factor = 1.0;
    for ( ; ; ) {
        Uint4 i = 0;
        Uint4 j = 0;

        for (i = 0; i < internal_pssm->ncols; i++) {
            for (j = 0; j < internal_pssm->nrows; j++) {
                if (scaled_pssm[i][j] != BLAST_SCORE_MIN) {
                    pssm[i][j] = 
                        BLAST_Nint(factor*scaled_pssm[i][j]/kPSIScaleFactor);
                } else {
                    pssm[i][j] = BLAST_SCORE_MIN;
                }
            }
        }
        _PSIUpdateLambdaK((const int**)pssm, query, query_length, 
                          std_probs, sbp);

        new_lambda = sbp->kbp_psi[0]->Lambda;

        if (new_lambda > ideal_lambda) {
            if (first_time) {
                factor_high = 1.0 + kPositScalingPercent;
                factor = factor_high;
                too_high = TRUE;
                first_time = FALSE;
            } else {
                if (too_high == FALSE) {
                    break;
                }
                factor_high += (factor_high - 1.0);
                factor = factor_high;
            }
        } else if (new_lambda > 0) {
            if (first_time) {
                factor_high = 1.0;
                factor_low = 1.0 - kPositScalingPercent;
                factor = factor_low;
                too_high = FALSE;
                first_time = FALSE;
            } else {
                if (too_high == TRUE) {
                    break;
                }
                factor_low += (factor_low - 1.0);
                factor = factor_low;
            }
        } else {
            return PSIERR_POSITIVEAVGSCORE;
        }
    }

    /* Binary search for kPositScalingNumIterations times */
    for (index = 0; index < kPositScalingNumIterations; index++) {
        Uint4 i = 0;
        Uint4 j = 0;

        factor = (factor_high + factor_low)/2;

        for (i = 0; i < internal_pssm->ncols; i++) {
            for (j = 0; j < internal_pssm->nrows; j++) {
                if (scaled_pssm[i][j] != BLAST_SCORE_MIN) {
                    pssm[i][j] = 
                        BLAST_Nint(factor*scaled_pssm[i][j]/kPSIScaleFactor);
                } else {
                    pssm[i][j] = BLAST_SCORE_MIN;
                }
            }
        }

        _PSIUpdateLambdaK((const int**)pssm, query, query_length, 
                          std_probs, sbp);

        new_lambda = sbp->kbp_psi[0]->Lambda;

        if (new_lambda > ideal_lambda) {
            factor_low = factor;
        } else {
            factor_high = factor;
        }
    }

    return PSI_SUCCESS;
}

int
_IMPALAScaleMatrix(const Uint1* query, const double* std_probs,
                   _PSIInternalPssmData* internal_pssm, 
                   BlastScoreBlk* sbp,
                   double scaling_factor)
{
    Kappa_posSearchItems *posSearch = NULL;
    Kappa_compactSearchItems* compactSearch = NULL;
    int retval = PSI_SUCCESS;

    posSearch = Kappa_posSearchItemsNew(internal_pssm->ncols,
                                        sbp->name, 
                                        internal_pssm->scaled_pssm,
                                        internal_pssm->freq_ratios);
    compactSearch = Kappa_compactSearchItemsNew(query, internal_pssm->ncols,
                                                sbp);

    retval = Kappa_impalaScaling(posSearch, compactSearch,
                                 scaling_factor, TRUE, sbp);

    /* FIXME: there are differences between this and what formatrspdb's scaling
     * produces */
    {
        int i, j;
        FILE* fp = fopen("scaled_pssm.txt", "w");
        for (i = 0; i < internal_pssm->ncols; i++) {
            for (j = 0; j < internal_pssm->nrows; j++) {
                fprintf(fp, "%d ", internal_pssm->scaled_pssm[i][j]);
            }
            fprintf(fp, "\n");
        }
        fclose(fp);
    }

    /* Overwrite unscaled PSSM with scaled PSSM */
    _PSICopyMatrix_int(internal_pssm->pssm, internal_pssm->scaled_pssm,
                       internal_pssm->ncols, internal_pssm->nrows);

    posSearch = Kappa_posSearchItemsFree(posSearch);
    compactSearch = Kappa_compactSearchItemsFree(compactSearch);

    return retval;
}

Uint4
_PSISequenceLengthWithoutX(const Uint1* seq, Uint4 length)
{
    const Uint1 kXResidue = AMINOACID_TO_NCBISTDAA['X'];
    Uint4 retval = 0;       /* the return value */
    Uint4 i = 0;            /* loop index */

    ASSERT(seq);

    for(i = 0; i < length; i++) {
        if (seq[i] != kXResidue) {
            retval++;
        }
    }

    return retval;
}

Blast_ScoreFreq*
_PSIComputeScoreProbabilities(const int** pssm,                     /* [in] */
                              const Uint1* query,                   /* [in] */
                              Uint4 query_length,                   /* [in] */
                              const double* std_probs,              /* [in] */
                              const BlastScoreBlk* sbp)             /* [in] */
{
    const Uint1 kXResidue = AMINOACID_TO_NCBISTDAA['X'];
    Uint1 aa_alphabet[BLASTAA_SIZE];            /* ncbistdaa alphabet */
    Uint4 alphabet_size = 0;                    /* number of elements populated
                                                   in array above */
    Uint4 effective_length = 0;                 /* length of query w/o Xs */
    Uint4 p = 0;                                /* index on positions */
    Uint4 r = 0;                                /* index on residues */
    int s = 0;                                  /* index on scores */
    int min_score = BLAST_SCORE_MAX;            /* minimum score in pssm */
    int max_score = BLAST_SCORE_MIN;            /* maximum score in pssm */
    Blast_ScoreFreq* score_freqs = NULL;        /* score frequencies */

    ASSERT(pssm);
    ASSERT(query);
    ASSERT(std_probs);
    ASSERT(sbp);
    ASSERT(sbp->alphabet_code == BLASTAA_SEQ_CODE);

    alphabet_size = (Uint4) Blast_GetStdAlphabet(sbp->alphabet_code, 
                                                 aa_alphabet, BLASTAA_SIZE);
    if (alphabet_size <= 0) {
        return NULL;
    }
    ASSERT(alphabet_size < BLASTAA_SIZE);

    effective_length = _PSISequenceLengthWithoutX(query, query_length);

    /* Get the minimum and maximum scores */
    for (p = 0; p < query_length; p++) {
        for (r = 0; r < alphabet_size; r++) {
            const int kScore = pssm[p][aa_alphabet[r]];

            if (kScore <= BLAST_SCORE_MIN || kScore >= BLAST_SCORE_MAX) {
                continue;
            }
            max_score = MAX(kScore, max_score);
            min_score = MIN(kScore, min_score);
        }
    }
    ASSERT(min_score != BLAST_SCORE_MAX);
    ASSERT(max_score != BLAST_SCORE_MIN);

    score_freqs = Blast_ScoreFreqNew(min_score, max_score);
    if ( !score_freqs ) {
        return NULL;
    }

    score_freqs->obs_min = min_score;
    score_freqs->obs_max = max_score;
    for (p = 0; p < query_length; p++) {
        if (query[p] == kXResidue) {
            continue;
        }

        for (r = 0; r < alphabet_size; r++) {
            const int kScore = pssm[p][aa_alphabet[r]];

            if (kScore <= BLAST_SCORE_MIN || kScore >= BLAST_SCORE_MAX) {
                continue;
            }

            /* Increment the weight for the score in position p, residue r */
            score_freqs->sprob[kScore] += 
                (std_probs[aa_alphabet[r]]/effective_length);
        }
    }

    ASSERT(score_freqs->score_avg == 0.0);
    for (s = min_score; s < max_score; s++) {
        score_freqs->score_avg += (s * score_freqs->sprob[s]);
    }

    return score_freqs;
}

/* Port of blastool.c's updateLambdaK */
void
_PSIUpdateLambdaK(const int** pssm,              /* [in] */
                  const Uint1* query,            /* [in] */
                  Uint4 query_length,            /* [in] */
                  const double* std_probs,       /* [in] */
                  BlastScoreBlk* sbp)            /* [in|out] */
{
    Blast_ScoreFreq* score_freqs = 
        _PSIComputeScoreProbabilities(pssm, query, query_length, 
                                      std_probs, sbp);

    /* Calculate lambda and K */
    Blast_KarlinBlkUngappedCalc(sbp->kbp_psi[0], score_freqs);

    /* Shouldn't this be in a function? */
    ASSERT(sbp->kbp_ideal);
    ASSERT(sbp->kbp_psi[0]);
    ASSERT(sbp->kbp_gap_std[0]);
    ASSERT(sbp->kbp_gap_psi[0]);

    sbp->kbp_gap_psi[0]->K = 
        sbp->kbp_psi[0]->K * sbp->kbp_gap_std[0]->K / sbp->kbp_ideal->K;
    sbp->kbp_gap_psi[0]->logK = log(sbp->kbp_gap_psi[0]->K);

    score_freqs = Blast_ScoreFreqFree(score_freqs);
}


/****************************************************************************/
/* Function definitions for auxiliary functions for the stages above */
int
_PSIPurgeAlignedRegion(_PSIMsa* msa,
                       unsigned int seq_index,
                       unsigned int start,
                       unsigned int stop)
{
    _PSIMsaCell* sequence_position = NULL;
    unsigned int i = 0;

    if ( !msa ||
        seq_index == 0 ||
        seq_index > msa->dimensions->num_seqs + 1 ||
        stop > msa->dimensions->query_length) {
        return PSIERR_BADPARAM;
    }

    sequence_position = msa->cell[seq_index];
    for (i = start; i < stop; i++) {
        /**
           @todo This function is the successor to posit.c's posCancel and it
           has been implemented to be consistent with it. However, its choice
           of sentinel values to flag positions as unused is inconsistent with
           the state of newly allocated positions (which would be preferred).
           This behavior should be fixed once the algo/blast implementation the
           PSSM engine replaces posit.c 
        sequence_position[i].letter = (unsigned char) -1;
        sequence_position[i].e_value = kDefaultEvalueForPosition;
        sequence_position[i].extents.left = (unsigned int) -1;
        */
        /* posCancel initializes positions differently than when they are
         * allocated, why?*/
        sequence_position[i].letter = 0;
        sequence_position[i].is_aligned = FALSE;
        /*sequence_position[i].e_value = PSI_INCLUSION_ETHRESH;*/
        sequence_position[i].extents.left = 0;
        sequence_position[i].extents.right = 
            msa->dimensions->query_length;
    }

    _PSIDiscardIfUnused(msa, seq_index);

    return PSI_SUCCESS;
}

/* Check if we still need this sequence */
void
_PSIDiscardIfUnused(_PSIMsa* msa, unsigned int seq_index)
{
    Boolean contains_aligned_regions = FALSE;
    unsigned int i = 0;

    for (i = 0; i < msa->dimensions->query_length; i++) {
        if (msa->cell[seq_index][i].is_aligned) {
            contains_aligned_regions = TRUE;
            break;
        }
    }

    if ( !contains_aligned_regions ) {
        msa->use_sequence[seq_index] = FALSE;
    }
}

/****************************************************************************/
int
_PSISaveDiagnostics(const _PSIMsa* msa,
                    const _PSIAlignedBlock* aligned_block,
                    const _PSISequenceWeights* seq_weights,
                    const _PSIInternalPssmData* internal_pssm,
                    PSIDiagnosticsResponse* diagnostics)
{
    Uint4 p = 0;                  /* index on positions */
    Uint4 r = 0;                  /* index on residues */

    if ( !diagnostics || !msa || !aligned_block || !seq_weights ||
         !internal_pssm  || !internal_pssm->freq_ratios ) {
        return PSIERR_BADPARAM;
    }

    ASSERT(msa->dimensions->query_length == diagnostics->query_length);

    if (diagnostics->information_content) {
        double* info = _PSICalculateInformationContentFromFreqRatios(
                internal_pssm->freq_ratios, seq_weights->std_prob,
                diagnostics->query_length, 
                diagnostics->alphabet_size);
        if ( !info ) {
            return PSIERR_OUTOFMEM;
        }
        for (p = 0; p < diagnostics->query_length; p++) {
            diagnostics->information_content[p] = info[p];
        }
        sfree(info);
    }

    if (diagnostics->residue_freqs) {
        for (p = 0; p < diagnostics->query_length; p++) {
            for (r = 0; r < diagnostics->alphabet_size; r++) {
                diagnostics->residue_freqs[p][r] = msa->residue_counts[p][r];
            }
        }
    }

    if (diagnostics->weighted_residue_freqs) {
        for (p = 0; p < diagnostics->query_length; p++) {
            for (r = 0; r < diagnostics->alphabet_size; r++) {
                diagnostics->weighted_residue_freqs[p][r] =
                    seq_weights->match_weights[p][r];
            }
        }
    }
    
    if (diagnostics->frequency_ratios) {
        for (p = 0; p < diagnostics->query_length; p++) {
            for (r = 0; r < diagnostics->alphabet_size; r++) {
                diagnostics->frequency_ratios[p][r] =
                    internal_pssm->freq_ratios[p][r];
            }
        }
    }

    if (diagnostics->gapless_column_weights) {
        for (p = 0; p < diagnostics->query_length; p++) {
            diagnostics->gapless_column_weights[p] =
                seq_weights->gapless_column_weights[p];
        }
    }

    return PSI_SUCCESS;
}

/*
 * ===========================================================================
 * $Log$
 * Revision 1.48  2005/02/22 22:49:55  camacho
 * + impala_scaling_factor, first cut
 *
 * Revision 1.47  2005/02/14 14:07:49  camacho
 * Changes to use SBlastScoreMatrix
 *
 * Revision 1.46  2005/01/31 16:50:21  camacho
 * 1. Moved constants to private header.
 * 2. Changed signature of functions to copy matrices.
 *
 * Revision 1.45  2004/12/15 16:19:27  camacho
 * Remove compiler warning
 *
 * Revision 1.44  2004/12/15 03:17:11  ucko
 * Revert previous change, as VERIFY is not necessarily #defined.
 *
 * Revision 1.43  2004/12/15 02:50:50  camacho
 * Replace ASSERT with VERIFY
 *
 * Revision 1.42  2004/12/13 22:26:59  camacho
 * Consolidated structure group customizations in option: nsg_compatibility_mode
 *
 * Revision 1.41  2004/12/09 15:22:56  dondosha
 * Renamed some functions dealing with BlastScoreBlk and Blast_KarlinBlk structures
 *
 * Revision 1.40  2004/12/08 15:06:01  camacho
 * Call _PSIUpdatePositionCounts is needed after purging query sequence for
 * structure group customization, thus this function has been changed to reset the
 * appropriate _PSIMsa fields so that residue counts/aligned sequences are
 * reported accurately.
 *
 * Revision 1.39  2004/12/07 22:07:34  camacho
 * Add sanity checks for purge identity threshold
 *
 * Revision 1.38  2004/11/29 13:53:23  camacho
 * Renamed Blast_ScoreFreq structure free function
 *
 * Revision 1.37  2004/11/23 21:48:21  camacho
 * Removed local initialization of ideal Karlin-Altschul parameters
 *
 * Revision 1.36  2004/11/22 14:38:48  camacho
 * + option to set % identity threshold to PSSM engine
 *
 * Revision 1.35  2004/11/18 16:25:32  camacho
 * Rename _PSIGetStandardProbabilities to BLAST_GetStandardAaProbabilities
 *
 * Revision 1.34  2004/11/15 16:54:36  camacho
 * break long lines
 *
 * Revision 1.33  2004/11/15 16:32:37  dondosha
 * Changed constant names and static functions names in accordance with C++ toolkit guidelines
 *
 * Revision 1.32  2004/11/15 14:21:43  camacho
 * Correction to _PSIValidateAlignedColumns when the query sequence should be accounted
 *
 * Revision 1.31  2004/11/02 20:37:30  camacho
 * Doxygen fixes
 *
 * Revision 1.30  2004/10/19 14:28:38  camacho
 * Fix memory access error
 *
 * Revision 1.29  2004/10/18 14:33:14  camacho
 * 1. Added validation routines
 * 2. Fixed bug in calculating sequence weights
 * 3. Expanded error codes
 *
 * Revision 1.28  2004/10/13 16:03:00  camacho
 * Add assertions as sanity checks
 *
 * Revision 1.27  2004/10/01 13:59:22  camacho
 * Use calloc where needed
 *
 * Revision 1.26  2004/09/23 19:51:44  camacho
 * Added sanity checks
 *
 * Revision 1.25  2004/09/17 02:06:34  camacho
 * Renaming of diagnostics structure fields
 *
 * Revision 1.24  2004/09/14 21:17:02  camacho
 * Add structure group customization to ignore query
 *
 * Revision 1.23  2004/08/31 16:13:28  camacho
 * Documentation changes
 *
 * Revision 1.22  2004/08/13 22:32:16  camacho
 * Refactoring of _PSIPurgeSimilarAlignments to use finite state machine
 *
 * Revision 1.21  2004/08/04 20:18:26  camacho
 * 1. Renaming of structures and functions that pertain to the internals of PSSM
 *    engine.
 * 2. Updated documentation (in progress)
 *
 * Revision 1.20  2004/08/02 13:25:49  camacho
 * 1. Various renaming of structures, in progress
 * 2. Addition of PSIDiagnostics structures, in progress
 *
 * Revision 1.19  2004/07/29 19:16:02  camacho
 * Moved PSIExtractQuerySequenceInfo
 *
 * Revision 1.18  2004/07/22 19:06:18  camacho
 * 1. Fix in _PSICheckSequenceWeights.
 * 2. Added functions to calculate information content.
 * 3. Cleaned up PSIComputeFreqRatios.
 * 4. Removed unneeded code to set populate extra column in PSSMs.
 * 5. Added collection of information content and gapless column weights to
 * diagnostics structure
 *
 * Revision 1.17  2004/07/06 15:23:54  camacho
 * Fix memory acccess error
 *
 * Revision 1.16  2004/07/02 19:40:48  camacho
 * Fixes for handling out-of-memory conditions
 *
 * Revision 1.15  2004/07/02 18:00:25  camacho
 * 1. Document rationale for order in which sequences are compared in
 *    _PSIPurgeNearIdenticalAlignments.
 * 2. Fix in _PSIPurgeSimilarAlignments to take into account the X residues
 * 3. Refactorings in sequence weight calculation functions:
 *    - Simplification of _PSICheckSequenceWeights
 *    - Addition of _PSISpreadGapWeights
 *    - Reorganization of _PSICalculateNormalizedSequenceWeights
 *
 * Revision 1.14  2004/06/25 20:31:11  camacho
 * Remove C++ comments
 *
 * Revision 1.13  2004/06/25 20:16:50  camacho
 * 1. Minor fixes to sequence weights calculation
 * 2. Add comments
 *
 * Revision 1.12  2004/06/21 12:52:44  camacho
 * Replace PSI_ALPHABET_SIZE for BLASTAA_SIZE
 *
 * Revision 1.11  2004/06/17 20:47:21  camacho
 * Minor fix to extent sizes
 *
 * Revision 1.10  2004/06/16 15:22:47  camacho
 * Fixes to add new unit tests
 *
 * Revision 1.9  2004/06/09 14:21:03  camacho
 * Removed msvc compiler warnings
 *
 * Revision 1.8  2004/06/08 17:30:06  dondosha
 * Compiler warnings fixes
 *
 * Revision 1.7  2004/06/07 14:18:24  dondosha
 * Added some variables initialization, to remove compiler warnings
 *
 * Revision 1.6  2004/05/28 17:35:03  camacho
 * Fix msvc6 warnings
 *
 * Revision 1.5  2004/05/28 16:00:09  camacho
 * + first port of PSSM generation engine
 *
 * Revision 1.4  2004/05/19 14:52:02  camacho
 * 1. Added doxygen tags to enable doxygen processing of algo/blast/core
 * 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
 *    location
 * 3. Added use of @todo doxygen keyword
 *
 * Revision 1.3  2004/05/06 14:01:40  camacho
 * + _PSICopyDoubleMatrix
 *
 * Revision 1.2  2004/04/07 22:08:37  kans
 * needed to include blast_def.h for sfree prototype
 *
 * Revision 1.1  2004/04/07 19:11:17  camacho
 * Initial revision
 *
 * ===========================================================================
 */
