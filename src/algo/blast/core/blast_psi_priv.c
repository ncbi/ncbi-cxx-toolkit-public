static char const rcsid[] =
    "$Id$";
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
#include "matrix_freq_ratios.h"
#include <algo/blast/core/blast_util.h>     /* needed for IS_residue */

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

void
_PSICopyIntMatrix(int** dest, int** src, 
                  unsigned int ncols, unsigned int nrows)
{
    unsigned int i = 0;
    unsigned int j = 0;

    ASSERT(dest);
    ASSERT(src);

    for (i = 0; i < ncols; i++) {
        for (j = 0; j < nrows; j++) {
            dest[i][j] = src[i][j];
        }
    }
}

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

    retval = (_PSIMsa*) malloc(sizeof(_PSIMsa));
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

    retval = (_PSIInternalPssmData*) malloc(sizeof(_PSIInternalPssmData));
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

    retval->res_freqs = (double**) _PSIAllocateMatrix(retval->ncols,
                                                      retval->nrows,
                                                      sizeof(double));
    if ( !retval->res_freqs ) {
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

    if (pssm_data->res_freqs) {
        pssm_data->res_freqs = (double**) 
            _PSIDeallocateMatrix((void**) pssm_data->res_freqs, 
                                 pssm_data->ncols);
    }

    sfree(pssm_data);

    return NULL;
}

_PSIAlignedBlock*
_PSIAlignedBlockNew(Uint4 num_positions)
{
    _PSIAlignedBlock* retval = NULL;
    Uint4 i = 0;

    retval = (_PSIAlignedBlock*) calloc(1, sizeof(_PSIAlignedBlock));
    if ( !retval ) {
        return NULL;
    }

    retval->pos_extnt = (SSeqRange*) calloc(num_positions, sizeof(SSeqRange));
    if ( !retval->pos_extnt ) {
        return _PSIAlignedBlockFree(retval);
    }

    retval->size = (Uint4*) calloc(num_positions, sizeof(Uint4));
    if ( !retval->size ) {
        return _PSIAlignedBlockFree(retval);
    }

    for (i = 0; i < num_positions; i++) {
        retval->pos_extnt[i].left = -1;
        retval->pos_extnt[i].right = num_positions;
    }
    return retval;
}

_PSIAlignedBlock*
_PSIAlignedBlockFree(_PSIAlignedBlock* aligned_blocks)
{
    if ( !aligned_blocks ) {
        return NULL;
    }

    if (aligned_blocks->pos_extnt) {
        sfree(aligned_blocks->pos_extnt);
    }

    if (aligned_blocks->size) {
        sfree(aligned_blocks->size);
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

    retval->std_prob = _PSIGetStandardProbabilities(sbp);
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


/****************************************************************************/
/* Function prototypes */

/* Purges any aligned segments which are identical to the query sequence */
static void
_PSIPurgeSelfHits(_PSIMsa* msa);

/* Keeps only one copy of any aligned sequences which are >kPSINearIdentical%
 * identical to one another */
static void
_PSIPurgeNearIdenticalAlignments(_PSIMsa* msa);

/* FIXME: needs more descriptive name */
static void
_PSIPurgeSimilarAlignments(_PSIMsa* msa,
                           Uint4 seq_index1,
                           Uint4 seq_index2,
                           double max_percent_identity);
/****************************************************************************/

/**************** PurgeMatches stage of PSSM creation ***********************/
int
_PSIPurgeBiasedSegments(_PSIMsa* msa)
    /* FIXME: will need ignore_query option */
{
    if ( !msa ) {
        return PSIERR_BADPARAM;
    }

    _PSIPurgeSelfHits(msa);
    _PSIPurgeNearIdenticalAlignments(msa);
    _PSIUpdatePositionCounts(msa);

    return PSI_SUCCESS;
}

/** Remove those sequences which are identical to the query sequence 
 */
static void
_PSIPurgeSelfHits(_PSIMsa* msa)
{
    Uint4 s = 0;        /* index on sequences */

    ASSERT(msa);

    /* FIXME: change this to implement "exclude the query" option for structure
     * group */
    for (s = kQueryIndex + 1; s < msa->dimensions->num_seqs + 1; s++) {
        _PSIPurgeSimilarAlignments(msa, kQueryIndex, s, kPSIIdentical);
    }
}

static void
_PSIPurgeNearIdenticalAlignments(_PSIMsa* msa)
{
    Uint4 i = 0;
    Uint4 j = 0;

    ASSERT(msa);

    for (i = 1; i < msa->dimensions->num_seqs + 1; i++) { 
        for (j = 1; (i + j) < msa->dimensions->num_seqs + 1; j++) {
            /* N.B.: The order of comparison of sequence pairs is deliberate,
             * tests on real data indicated that this approach allowed more
             * sequences to be purged */
            _PSIPurgeSimilarAlignments(msa, j, (i + j),
                                       kPSINearIdentical);
        }
    }
}

/** Counts the number of sequences matching the query per query position
 * (columns of the multiple alignment) as well as the number of residues
 * present in each position of the query.
 * Should be called after multiple alignment data has been purged from biased
 * sequences.
 */
void
_PSIUpdatePositionCounts(_PSIMsa* msa)
{
    Uint4 s = 0;        /* index on aligned sequences */
    Uint4 p = 0;        /* index on positions */

    ASSERT(msa);

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
 * _PSIPurgeSimilarAlignments. Successor to posit.c's POS_COUNTING and
 * POS_RESTING */
typedef enum _EPSIPurgeFsmState {
    eCounting,
    eResting
} _EPSIPurgeFsmState;

/** Auxiliary structure to maintain information about two aligned regions
 * between the query and a subject sequence. It is used to store the data
 * manipulated by the finite state machine used in _PSIPurgeSimilarAlignments.
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

void
_PSIResetAlignmentTraits(_PSIAlignmentTraits* traits, Uint4 position)
{
    ASSERT(traits);
    memset((void*) traits, 0, sizeof(_PSIAlignmentTraits));
    traits->start = position;
}

/** Handles neither is aligned event */
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

/** This function compares the sequences in the msa->cell
 * structure indexed by sequence_index1 and seq_index2. If it finds aligned 
 * regions that have a greater percent identity than max_percent_identity, 
 * it removes the sequence identified by seq_index2.
 */
static void
_PSIPurgeSimilarAlignments(_PSIMsa* msa,
                           Uint4 seq_index1,
                           Uint4 seq_index2,
                           double max_percent_identity)
{

    const Uint1 X = AMINOACID_TO_NCBISTDAA['X'];
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

        const _PSIMsaCell* seq1 = msa->cell[seq_index1];
        const _PSIMsaCell* seq2 = msa->cell[seq_index2];

        /* Indicates if the position in seq_index1 currently being examined is 
         * aligned. In the special case for seq_index1 == kQueryIndex, this 
         * variable is set to FALSE to force the other sequence's position to 
         * be used to proceed with processing. */
        const Boolean pos1_aligned = (seq_index1 == kQueryIndex ? 
                                FALSE : seq1[p].is_aligned);
        /* Indicates if the position in seq_index2 currently being examined is 
         * aligned. */
        const Boolean pos2_aligned = seq2[p].is_aligned;

        Boolean neither_is_aligned = !pos1_aligned && !pos2_aligned;
        /* FIXME: kludgy solution, need to document fsm */
        Boolean both_are_aligned = seq1[p].is_aligned && pos2_aligned;
        Boolean either_is_aligned = pos1_aligned || pos2_aligned;

        Boolean neither_is_X = seq1[p].letter != X && seq2[p].letter != X;
        Boolean either_is_X = seq1[p].letter == X || seq2[p].letter == X;
        Boolean same_residue = seq1[p].letter == seq2[p].letter;

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
static void
_PSIGetLeftExtents(const _PSIMsa* msa, Uint4 seq_index);
static void
_PSIGetRightExtents(const _PSIMsa* msa, Uint4 seq_index);

static void
_PSIComputePositionExtents(const _PSIMsa* msa, 
                           Uint4 seq_index,
                           _PSIAlignedBlock* aligned_blocks);
static void
_PSIComputeAlignedRegionLengths(const _PSIMsa* msa,
                                _PSIAlignedBlock* aligned_blocks);

/****************************************************************************/
/******* Compute alignment extents stage of PSSM creation *******************/
/* posComputeExtents in posit.c */
int
_PSIComputeAlignmentBlocks(const _PSIMsa* msa,                  /* [in] */
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

    _PSIComputeAlignedRegionLengths(msa, aligned_blocks);

    return PSI_SUCCESS;
}

static void
_PSIGetLeftExtents(const _PSIMsa* msa, Uint4 seq_index)
{
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA['-'];
    _PSIMsaCell* sequence_position = NULL;
    Uint4 prev = 0;  /* index for the first and previous position */
    Uint4 curr = 0;  /* index for the current position */

    ASSERT(msa);
    ASSERT(seq_index < msa->dimensions->num_seqs + 1);
    ASSERT(msa->use_sequence[seq_index]);

    sequence_position = msa->cell[seq_index];

    if (sequence_position[prev].is_aligned && 
        sequence_position[prev].letter != GAP) {
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
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA['-'];
    _PSIMsaCell* sequence_position = NULL;
    Uint4 last = 0;      /* index for the last position */
    Int4 curr = 0;       /* index for the current position */

    ASSERT(msa);
    ASSERT(seq_index < msa->dimensions->num_seqs + 1);
    ASSERT(msa->use_sequence[seq_index]);

    sequence_position = msa->cell[seq_index];
    last = msa->dimensions->query_length - 1;

    if (sequence_position[last].is_aligned && 
        sequence_position[last].letter != GAP) {
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
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA['-'];
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
            sequence_position[i].letter != GAP) {
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
                                _PSIAlignedBlock* aligned_blocks)
{
    const Uint1 X = AMINOACID_TO_NCBISTDAA['X'];
    Uint4 i = 0;
    
    ASSERT(msa);
    ASSERT(aligned_blocks);

    for (i = 0; i < msa->dimensions->query_length; i++) {
        aligned_blocks->size[i] = aligned_blocks->pos_extnt[i].right - 
                                   aligned_blocks->pos_extnt[i].left + 1;
    }

    /* Do not include X's in aligned region lengths */
    for (i = 0; i < msa->dimensions->query_length; i++) {

        if (msa->query[i] == X) {

            Uint4 idx = 0;
            for (idx = 0; idx < i; idx++) {
                if ((Uint4)aligned_blocks->pos_extnt[idx].right >= i &&
                    msa->query[idx] != X) {
                    aligned_blocks->size[idx]--;
                }
            }
            for (idx = msa->dimensions->query_length - 1; idx > i; idx--) {
                if ((Uint4)aligned_blocks->pos_extnt[idx].left <= i &&
                    msa->query[idx] != X) {
                    aligned_blocks->size[idx]--;
                }
            }
        }

    }
}

/****************************************************************************/
static Uint4
_PSIGetAlignedSequencesForPosition(
    const _PSIMsa* msa, 
    Uint4 position,
    Uint4* aligned_sequences);

static void
_PSICalculateNormalizedSequenceWeights(
    const _PSIMsa* msa,     /* [in] */
    const _PSIAlignedBlock* aligned_blocks, /* [in] */
    Uint4 position,                        /* [in] */
    const Uint4* aligned_seqs,             /* [in] */
    Uint4 num_aligned_seqs,                /* [in] */
    _PSISequenceWeights* seq_weights);      /* [out] */

static void
_PSICalculateMatchWeights(
    const _PSIMsa* msa,  /* [in] */
    Uint4 position,                     /* [in] */
    const Uint4* aligned_seqs,          /* [in] */
    Uint4 num_aligned_seqs,             /* [in] */
    _PSISequenceWeights* seq_weights);   /* [out] */

static void
_PSISpreadGapWeights(const _PSIMsa* msa,
                     _PSISequenceWeights* seq_weights);

static int
_PSICheckSequenceWeights(
    const _PSIMsa* msa,         /* [in] */
    const _PSISequenceWeights* seq_weights);    /* [in] */

/****************************************************************************/
/******* Calculate sequence weights stage of PSSM creation ******************/
/* Needs the _PSIAlignedBlock structure calculated in previous stage as well
 * as PSIAlignmentData structure */

int
_PSIComputeSequenceWeights(const _PSIMsa* msa,                      /* [in] */
                           const _PSIAlignedBlock* aligned_blocks,  /* [in] */
                           _PSISequenceWeights* seq_weights)        /* [out] */
{
    Uint4* aligned_seqs = NULL;     /* list of indices of sequences
                                       which participate in an
                                       aligned position */
    Uint4 pos = 0;                  /* position index */
    int retval = PSI_SUCCESS;       /* return value */

    ASSERT(msa);
    ASSERT(aligned_blocks);
    ASSERT(seq_weights);

    aligned_seqs = (Uint4*) malloc((msa->dimensions->num_seqs + 1) *
                                          sizeof(Uint4));
    if ( !aligned_seqs ) {
        return PSIERR_OUTOFMEM;
    }

    for (pos = 0; pos < msa->dimensions->query_length; pos++) {

        Uint4 num_aligned_seqs = 0;

        /* ignore positions of no interest */
        if (aligned_blocks->size[pos] == 0 || msa->num_matching_seqs[pos] <= 1) {
        /* FIXME: num_matching_seqs could be 0 if ignore_query */
            continue;
        }

        memset((void*)aligned_seqs, 0, 
               sizeof(Uint4) * (msa->dimensions->num_seqs + 1));

        num_aligned_seqs = _PSIGetAlignedSequencesForPosition(msa, pos,
                                                              aligned_seqs);
        ASSERT(msa->num_matching_seqs[pos] == num_aligned_seqs);
        /* FIXME: could be 0 if ignore_query */
        if (num_aligned_seqs <= 1) {
            continue;
        }

        memset((void*)seq_weights->norm_seq_weights, 0, 
               sizeof(double)*(msa->dimensions->num_seqs+1));
        memset((void*)seq_weights->row_sigma, 0,
               sizeof(double)*(msa->dimensions->num_seqs+1));

        _PSICalculateNormalizedSequenceWeights(msa,
                                     aligned_blocks, pos, aligned_seqs, 
                                     num_aligned_seqs, seq_weights);


        /* Uses seq_weights->norm_seq_weights to populate match_weights */
        _PSICalculateMatchWeights(msa, pos, aligned_seqs,
                                  num_aligned_seqs, seq_weights);
    }

    sfree(aligned_seqs);

    /* Check that the sequence weights add up to 1 in each column */
    retval = _PSICheckSequenceWeights(msa, seq_weights);
    if (retval != PSI_SUCCESS) {
        return retval;
    }

#ifndef PSI_IGNORE_GAPS_IN_COLUMNS
    _PSISpreadGapWeights(msa, seq_weights);
    retval = _PSICheckSequenceWeights(msa, seq_weights);
#endif

    /* Return seq_weights->match_weigths, should free others? FIXME: need to
     * keep sequence weights for diagnostics for structure group */
    return retval;
}

/** Calculates the position based weights using a modified version of the
 * Henikoff's algorithm presented in "Position-based sequence weights". 
 * Skipped optimization about identical previous sets.
 */
static void
_PSICalculateNormalizedSequenceWeights(
    const _PSIMsa* msa,     /* [in] */
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

    for (i = (Uint4) aligned_blocks->pos_extnt[position].left; 
         i <= (Uint4) aligned_blocks->pos_extnt[position].right; i++) {

        /* Keeps track of how many occurrences of each residue are found in a
         * column of the alignment extent corresponding to a query position */
        Uint4 residue_counts_for_column[BLASTAA_SIZE];

        /* number of distinct residues found in a column of the alignment
         * extent correspoding to a query position */
        Uint4 num_distinct_residues_for_column = 0; 

        memset((void*) residue_counts_for_column, 0, 
               sizeof(Uint4)*BLASTAA_SIZE);

        /* Count number of residues in column i of the alignment extent
         * corresponding to position */
        for (asi = 0; asi < num_aligned_seqs; asi++) {
            const Uint4 seq_idx = aligned_seqs[asi];
            const Uint1 residue = 
                msa->cell[seq_idx][i].letter;

            if (residue_counts_for_column[residue] == 0) {
                num_distinct_residues_for_column++;
            }
            residue_counts_for_column[residue]++;
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
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA['-'];
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
        if (residue != GAP) {
            seq_weights->gapless_column_weights[position] +=
             seq_weights->norm_seq_weights[seq_idx];
        }
    }
}

/** Finds the sequences aligned in a given position.
 * @param alignment Multiple-alignment data [in]
 * @param position position of interest [in]
 * @param aligned_sequences array which will contain the indices of the
 * sequences aligned at the requested position. This array must have size
 * greater than or equal to the number of sequences + 1 in multiple alignment 
 * data structure (alignment->dimensions->num_seqs + 1) [out]
 * @return number of sequences aligned at the requested position
 */
static Uint4
_PSIGetAlignedSequencesForPosition(const _PSIMsa* msa, 
                                   Uint4 position,
                                   Uint4* aligned_sequences)
{
#ifdef PSI_IGNORE_GAPS_IN_COLUMNS
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA['-'];
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
            msa->cell[i][position].letter != GAP) {
#else
        if (msa->cell[i][position].is_aligned) {
#endif
            aligned_sequences[retval++] = i;
        }
    }

    return retval;
}

/* The second parameter is not really const, it's updated! */
static void
_PSISpreadGapWeights(const _PSIMsa* msa,
                     _PSISequenceWeights* seq_weights)
{
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA['-'];
    const Uint1 X = AMINOACID_TO_NCBISTDAA['X'];
    Uint4 pos = 0;   /* residue position (ie: column number) */
    Uint4 res = 0;   /* residue */

    ASSERT(msa);
    ASSERT(seq_weights);

    for (pos = 0; pos < msa->dimensions->query_length; pos++) {

        if (msa->num_matching_seqs[pos] <= 1 &&
            msa->cell[kQueryIndex][pos].letter == X) {
            continue;
        }

        /* Disperse method of spreading the gap weights */
        for (res = 0; res < msa->alphabet_size; res++) {
            if (seq_weights->std_prob[res] > kEpsilon) {
                seq_weights->match_weights[pos][res] += 
                    (seq_weights->match_weights[pos][GAP] * 
                     seq_weights->std_prob[res]);
            }
        }
        seq_weights->match_weights[pos][GAP] = 0.0;

    }
}

/* Verifies that each column of the match_weights field in the seq_weights
 * structure adds up to 1. */
static int
_PSICheckSequenceWeights(const _PSIMsa* msa,
                         const _PSISequenceWeights* seq_weights)
{
    const Uint1 X = AMINOACID_TO_NCBISTDAA['X'];
    Uint4 pos = 0;   /* residue position (ie: column number) */
    Boolean check_performed = FALSE;  /* were there any sequences checked? */

    ASSERT(msa);
    ASSERT(seq_weights);

    for (pos = 0; pos < msa->dimensions->query_length; pos++) {

        double running_total = 0.0;
        Uint4 residue = 0;

        /* FIXME: num_matching_seqs might be 0 if ignore_query... */
        if (msa->num_matching_seqs[pos] <= 1 ||
            msa->cell[kQueryIndex][pos].letter == X) {
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
    if ( !check_performed ) {
        /* FIXME: need to return some error code */
        abort();
    }

    return PSI_SUCCESS;
}

/****************************************************************************/
/******* Compute residue frequencies stage of PSSM creation *****************/

/* Implements formula 2 in Nucleic Acids Research, 2001, Vol 29, No 14.
   Subscripts are indicated as follows: N_i, where i is a subscript of N.
 */
int
_PSIComputeResidueFrequencies(const _PSIMsa* msa,     /* [in] */
                              const _PSISequenceWeights* seq_weights, /* [in] */
                              const BlastScoreBlk* sbp,              /* [in] */
                              const _PSIAlignedBlock* aligned_blocks, /* [in] */
                              Int4 pseudo_count,          /* [in] */
                              _PSIInternalPssmData* internal_pssm) /* [out] */
{
    const Uint1 X = AMINOACID_TO_NCBISTDAA['X'];
    SFreqRatios* freq_ratios = NULL;/* matrix-specific frequency ratios */
    Uint4 p = 0;                    /* index on positions */
    Uint4 r = 0;                    /* index on residues */

    if ( !msa || !seq_weights || !sbp || !aligned_blocks || !internal_pssm ) {
        return PSIERR_BADPARAM;
    }

    freq_ratios = _PSIMatrixFrequencyRatiosNew(sbp->name);

    for (p = 0; p < msa->dimensions->query_length; p++) {

        for (r = 0; r < (Uint4) sbp->alphabet_size; r++) {

            /* If there is an 'X' in the query sequence at position p
               or the standard probability of residue r is close to 0 */
            if (msa->cell[kQueryIndex][p].letter == X ||
                seq_weights->std_prob[r] <= kEpsilon) {
                internal_pssm->res_freqs[p][r] = 0.0;
            } else {

                Uint4 i = 0;             /* loop index */

                /* beta( Sum_j(f_j r_ij) ) in formula 2 */
                double pseudo = 0.0;            
                /* Effective number of independent observations for column p */
                double alpha = 0.0;
                /* Renamed to match the formula in the paper */
                double beta = pseudo_count;
                double numerator = 0.0;         /* intermediate term */
                double denominator = 0.0;       /* intermediate term */
                double qOverPEstimate = 0.0;    /* intermediate term */

                /* As specified in 2001 paper, underlying matrix frequency 
                   ratios are used here */
                for (i = 0; i < (Uint4) sbp->alphabet_size; i++) {
                    if (sbp->matrix[r][i] != BLAST_SCORE_MIN) {
                        pseudo += (seq_weights->match_weights[p][i] *
                                   freq_ratios->data[r][i]);
                    }
                }
                pseudo *= beta;

                alpha = seq_weights->sigma[p]/aligned_blocks->size[p]-1;

                numerator =
                    (alpha * seq_weights->match_weights[p][r] / 
                     seq_weights->std_prob[r]) 
                    + pseudo;

                denominator = alpha + beta;

                qOverPEstimate = numerator/denominator;

                /* Note artificial multiplication by standard probability
                 * to normalize */
                internal_pssm->res_freqs[p][r] = qOverPEstimate *
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
_PSICalculateInformationContentFromResidueFreqs(
    double** res_freqs,
    const double* std_prob,
    Uint4 query_length,
    Uint4 alphabet_sz)
{
    double* retval = NULL;      /* the return value */
    Uint4 p = 0;                /* index on positions */
    Uint4 r = 0;                /* index on residues */

    if ( !std_prob || !res_freqs ) {
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
                double qOverPEstimate = res_freqs[p][r] / std_prob[r];
                if (qOverPEstimate > kEpsilon) {
                    info_sum += 
                        res_freqs[p][r] * log(qOverPEstimate) / NCBIMATH_LN2;
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
_PSIConvertResidueFreqsToPSSM(_PSIInternalPssmData* internal_pssm,           /* [in|out] */
                              const Uint1* query,                /* [in] */
                              const BlastScoreBlk* sbp,          /* [in] */
                              const double* std_probs)           /* [in] */
{
    const Uint4 X = AMINOACID_TO_NCBISTDAA['X'];
    const Uint4 Star = AMINOACID_TO_NCBISTDAA['*'];
    Uint4 i = 0;
    Uint4 j = 0;
    SFreqRatios* freq_ratios = NULL;    /* only needed when there are not
                                           residue frequencies for a given 
                                           column */
    Blast_KarlinBlk* kbp_ideal = Blast_KarlinBlkIdealCalc(sbp);
    double ideal_lambda = kbp_ideal->Lambda;
    kbp_ideal = Blast_KarlinBlkDestruct(kbp_ideal);

    if ( !internal_pssm || !sbp || !std_probs )
        return PSIERR_BADPARAM;

    freq_ratios = _PSIMatrixFrequencyRatiosNew(sbp->name);

    /* Each column is a position in the query */
    for (i = 0; i < internal_pssm->ncols; i++) {

        /* True if all frequencies in column i are zero */
        Boolean is_unaligned_column = TRUE;
        const Uint4 query_res = query[i];

        for (j = 0; j < (Uint4) sbp->alphabet_size; j++) {

            double qOverPEstimate = 0.0;

            /* Division compensates for multiplication in previous function */
            if (std_probs[j] > kEpsilon) {
                qOverPEstimate = 
                    internal_pssm->res_freqs[i][j] / std_probs[j];
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

            if ( (j == X || j == Star) &&
                 (sbp->matrix[query_res][X] != BLAST_SCORE_MIN) ) {
                internal_pssm->scaled_pssm[i][j] = 
                    sbp->matrix[query_res][j] * kPSIScaleFactor;
            }
        }

        if (is_unaligned_column) {
            for (j = 0; j < (Uint4) sbp->alphabet_size; j++) {

                internal_pssm->pssm[i][j] = sbp->matrix[query_res][j];

                if (sbp->matrix[query_res][j] != BLAST_SCORE_MIN) {
                    double tmp = 
                        kPSIScaleFactor * freq_ratios->bit_scale_factor *
                        log(freq_ratios->data[query_res][j])/NCBIMATH_LN2;

                    internal_pssm->scaled_pssm[i][j] = BLAST_Nint(tmp);
                } else {
                    internal_pssm->scaled_pssm[i][j] = BLAST_SCORE_MIN;
                }
            }
        }
    }

    freq_ratios = _PSIMatrixFrequencyRatiosFree(freq_ratios);

    /* Set the last column of the matrix to BLAST_SCORE_MIN (why?) *
    for (j = 0; j < (Uint4) sbp->alphabet_size; j++) {
        internal_pssm->scaled_pssm[internal_pssm->ncols-1][j] = BLAST_SCORE_MIN;
    }
    */

    return PSI_SUCCESS;
}

/****************************************************************************/
/************************* Scaling of PSSM stage ****************************/

/**
 * @param initial_lambda_guess value to be used when calculating lambda if this
 * is not null [in]
 * @param sbp Score block structure where the calculated lambda and K will be
 * returned
 */
void
_PSIUpdateLambdaK(const int** pssm,              /* [in] */
                  const Uint1* query,            /* [in] */
                  Uint4 query_length,            /* [in] */
                  const double* std_probs,       /* [in] */
                  double* initial_lambda_guess,  /* [in] */
                  BlastScoreBlk* sbp);           /* [in|out] */

/* FIXME: change so that only lambda is calculated inside the loop that scales
   the matrix and kappa is calculated before returning from this function.
   Scaling factor should be optional argument to accomodate kappa.c's needs?
*/
int
_PSIScaleMatrix(const Uint1* query,              /* [in] */
                Uint4 query_length,              /* [in] */
                const double* std_probs,         /* [in] */
                double* scaling_factor,          /* [in] */
                _PSIInternalPssmData* internal_pssm,         /* [in|out] */
                BlastScoreBlk* sbp)              /* [in|out] */
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

    const double kPositPercent = 0.05;
    const Uint4 kPositNumIterations = 10;
    Boolean too_high = TRUE;

    if ( !internal_pssm || !sbp || !query || !std_probs )
        return PSIERR_BADPARAM;

    ASSERT(sbp->kbp_psi[0]);
    ASSERT(sbp->kbp_ideal);

    scaled_pssm = internal_pssm->scaled_pssm;
    pssm = internal_pssm->pssm;
    ideal_lambda = sbp->kbp_ideal->Lambda;

    /* FIXME: need to take scaling_factor into account */

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

        if (scaling_factor) {
            double init_lambda_guess = 
                sbp->kbp_psi[0]->Lambda / *scaling_factor;
            _PSIUpdateLambdaK((const int**)pssm, query, query_length, 
                              std_probs, &init_lambda_guess, sbp);
        } else {
            _PSIUpdateLambdaK((const int**)pssm, query, query_length, 
                              std_probs, NULL, sbp);
        }

        new_lambda = sbp->kbp_psi[0]->Lambda;

        if (new_lambda > ideal_lambda) {
            if (first_time) {
                factor_high = 1.0 + kPositPercent;
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
                factor_low = 1.0 - kPositPercent;
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

    /* Binary search for kPositNumIterations times */
    for (index = 0; index < kPositNumIterations; index++) {
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

        if (scaling_factor) {
            double init_lambda_guess = 
                sbp->kbp_psi[0]->Lambda / *scaling_factor;
            _PSIUpdateLambdaK((const int**)pssm, query, query_length, 
                              std_probs, &init_lambda_guess, sbp);
        } else {
            _PSIUpdateLambdaK((const int**)pssm, query, query_length, 
                              std_probs, NULL, sbp);
        }

        new_lambda = sbp->kbp_psi[0]->Lambda;

        if (new_lambda > ideal_lambda) {
            factor_low = factor;
        } else {
            factor_high = factor;
        }
    }

    return PSI_SUCCESS;
}

Uint4
_PSISequenceLengthWithoutX(const Uint1* seq, Uint4 length)
{
    const Uint1 X = AMINOACID_TO_NCBISTDAA['X'];
    Uint4 retval = 0;       /* the return value */
    Uint4 i = 0;            /* loop index */

    ASSERT(seq);

    for(i = 0; i < length; i++) {
        if (seq[i] != X) {
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
    const Uint1 X = AMINOACID_TO_NCBISTDAA['X'];
    Uint1 aa_alphabet[BLASTAA_SIZE];            /* ncbistdaa alphabet */
    Uint4 alphabet_size = 0;                    /* number of elements populated
                                                   in array above */
    Uint4 effective_length = 0;                 /* length of query w/o Xs */
    Uint4 p = 0;                                /* index on positions */
    Uint4 r = 0;                                /* index on residues */
    int s = 0;                                  /* index on scores */
    int min_score = 0;                          /* minimum score in pssm */
    int max_score = 0;                          /* maximum score in pssm */
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

    score_freqs = Blast_ScoreFreqNew(min_score, max_score);
    if ( !score_freqs ) {
        return NULL;
    }

    score_freqs->obs_min = min_score;
    score_freqs->obs_max = max_score;
    for (p = 0; p < query_length; p++) {
        if (query[p] == X) {
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
                  double* initial_lambda_guess,  /* [in] */
                  BlastScoreBlk* sbp)            /* [in|out] */
{
    Blast_ScoreFreq* score_freqs = 
        _PSIComputeScoreProbabilities(pssm, query, query_length, 
                                      std_probs, sbp);

    if (initial_lambda_guess) {
        sbp->kbp_psi[0]->Lambda = Blast_KarlinLambdaNR(score_freqs, 
                                                       *initial_lambda_guess);

    } else {
        /* Calculate lambda and K */
        Blast_KarlinBlkCalc(sbp->kbp_psi[0], score_freqs);

        /* Shouldn't this be in a function? */
        ASSERT(sbp->kbp_ideal);
        ASSERT(sbp->kbp_psi[0]);
        ASSERT(sbp->kbp_gap_std[0]);
        ASSERT(sbp->kbp_gap_psi[0]);

        sbp->kbp_gap_psi[0]->K = 
            sbp->kbp_psi[0]->K * sbp->kbp_gap_std[0]->K / sbp->kbp_ideal->K;
        sbp->kbp_gap_psi[0]->logK = log(sbp->kbp_gap_psi[0]->K);
    }

    score_freqs = Blast_ScoreFreqDestruct(score_freqs);
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

    if (!msa)
        return PSIERR_BADPARAM;

    /* Cannot remove the query sequence from multiple alignment data or
       bad index FIXME "exclude query" option for structure group */
    if (seq_index == kQueryIndex || 
        seq_index > msa->dimensions->num_seqs + 1 ||
        stop > msa->dimensions->query_length)
        return PSIERR_BADPARAM;


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
double*
_PSIGetStandardProbabilities(const BlastScoreBlk* sbp)
{
    Blast_ResFreq* standard_probabilities = NULL;
    Uint4 i = 0;
    double* retval = NULL;

    retval = (double*) malloc(sbp->alphabet_size * sizeof(double));
    if ( !retval ) {
        return NULL;
    }

    standard_probabilities = Blast_ResFreqNew(sbp);
    Blast_ResFreqStdComp(sbp, standard_probabilities);

    for (i = 0; i < (Uint4) sbp->alphabet_size; i++) {
        retval[i] = standard_probabilities->prob[i];
    }

    Blast_ResFreqDestruct(standard_probabilities);
    return retval;
}

int
_PSISaveDiagnostics(const _PSIMsa* msa,
                    const _PSIAlignedBlock* aligned_block,
                    const _PSISequenceWeights* seq_weights,
                    const _PSIInternalPssmData* internal_pssm,
                    PSIDiagnosticsResponse* diagnostics)
{
    Uint4 p = 0;                  /* index on positions */
    Uint4 r = 0;                  /* index on residues */
    Uint4 s = 0;                  /* index on sequences */

    if ( !diagnostics || !msa || !aligned_block || !seq_weights ||
         !internal_pssm ) {
        return PSIERR_BADPARAM;
    }

    ASSERT(msa->dimensions->query_length ==
           diagnostics->dimensions->query_length);
    ASSERT(msa->dimensions->num_seqs == 
           diagnostics->dimensions->num_seqs);

    if (diagnostics->information_content) {
        double* info = _PSICalculateInformationContentFromResidueFreqs(
                internal_pssm->res_freqs, seq_weights->std_prob,
                diagnostics->dimensions->query_length, 
                diagnostics->alphabet_size);
        if ( !info ) {
            return PSIERR_OUTOFMEM;
        }
        for (p = 0; p < diagnostics->dimensions->query_length; p++) {
            diagnostics->information_content[p] = info[p];
        }
        sfree(info);
    }

    if (diagnostics->residue_frequencies) {
        for (p = 0; p < diagnostics->dimensions->query_length; p++) {
            for (r = 0; r < diagnostics->alphabet_size; r++) {
                diagnostics->residue_frequencies[p][r] =
                    internal_pssm->res_freqs[p][r];
            }
        }
    }

    /* FIXME: should save seq_weights->match_weights instead? */
    if (diagnostics->raw_residue_counts) {
        for (p = 0; p < diagnostics->dimensions->query_length; p++) {
            for (r = 0; r < diagnostics->alphabet_size; r++) {
                diagnostics->raw_residue_counts[p][r] =
                    msa->residue_counts[p][r];
            }
        }
    }
    
    if (diagnostics->sequence_weights) {
        for (s = 0; s < msa->dimensions->num_seqs + 1; s++) {
            diagnostics->sequence_weights[s] =
                seq_weights->norm_seq_weights[s];
        }
    }

    if (diagnostics->gapless_column_weights) {
        for (p = 0; p < msa->dimensions->query_length; p++) {
            diagnostics->gapless_column_weights[p] =
                seq_weights->gapless_column_weights[p];
        }
    }

    return PSI_SUCCESS;
}

/*
 * ===========================================================================
 * $Log$
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
 * 3. Cleaned up PSIComputeResidueFrequencies.
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
