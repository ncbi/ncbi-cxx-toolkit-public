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

/****************************************************************************/
/* Constants */
const unsigned int kQueryIndex = 0;
const double kEpsilon = 0.0001;
const double kDefaultEvalueForPosition = 1.0;
const int kPsiScaleFactor = 200;

/****************************************************************************/

void**
_PSIAllocateMatrix(unsigned int ncols, unsigned int nrows, 
                   unsigned int data_type_sz)
{
    void** retval = NULL;
    unsigned int i = 0;

    if ( !(retval = (void**) malloc(sizeof(void*) * ncols)))
        return NULL;

    for (i = 0; i < ncols; i++) {
        if ( !(retval[i] = (void*) calloc(nrows, data_type_sz))) {
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
_PSICopyMatrix(double** dest, const double** src, 
               unsigned int ncols, unsigned int nrows)
{
    unsigned int i = 0;
    unsigned int j = 0;

    for (i = 0; i < ncols; i++) {
        for (j = 0; j < nrows; j++) {
            dest[i][j] = src[i][j];
        }
    }
}

/****************************************************************************/

PsiAlignedBlock*
_PSIAlignedBlockNew(Uint4 num_positions)
{
    PsiAlignedBlock* retval = NULL;
    Uint4 i = 0;

    if ( !(retval = (PsiAlignedBlock*) calloc(1, sizeof(PsiAlignedBlock)))) {
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

PsiAlignedBlock*
_PSIAlignedBlockFree(PsiAlignedBlock* aligned_blocks)
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

PsiSequenceWeights*
_PSISequenceWeightsNew(const PsiInfo* info, const BlastScoreBlk* sbp)
{
    PsiSequenceWeights* retval = NULL;

    ASSERT(info);

    retval = (PsiSequenceWeights*) calloc(1, sizeof(PsiSequenceWeights));
    if ( !retval ) {
        return NULL;
    }

    retval->row_sigma = (double*) calloc(info->num_seqs + 1, sizeof(double));
    if ( !retval->row_sigma ) {
        return _PSISequenceWeightsFree(retval);
    }

    retval->norm_seq_weights = (double*) calloc(info->num_seqs + 1, 
                                                sizeof(double));
    if ( !retval->norm_seq_weights ) {
        return _PSISequenceWeightsFree(retval);
    }

    retval->sigma = (double*) calloc(info->num_seqs + 1, sizeof(double));
    if ( !retval->sigma ) {
        return _PSISequenceWeightsFree(retval);
    }

    retval->match_weights = (double**) _PSIAllocateMatrix(info->query_sz + 1,
                                                          PSI_ALPHABET_SIZE,
                                                          sizeof(double));
    retval->match_weights_size = info->query_sz + 1;
    if ( !retval->match_weights ) {
        return _PSISequenceWeightsFree(retval);
    }

    retval->std_prob = _PSIGetStandardProbabilities(sbp);
    if ( !retval->std_prob ) {
        return _PSISequenceWeightsFree(retval);
    }

    retval->info_content = (double*) calloc(info->query_sz, sizeof(double));
    if ( !retval->info_content ) {
        return _PSISequenceWeightsFree(retval);
    }

    return retval;
}

PsiSequenceWeights*
_PSISequenceWeightsFree(PsiSequenceWeights* seq_weights)
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

    if (seq_weights->info_content) {
        sfree(seq_weights->info_content);
    }

    sfree(seq_weights);

    return NULL;
}

/****************************************************************************/
/* Function prototypes */

/* Purges any aligned segments which are identical to the query sequence */
static void
_PSIPurgeIdenticalAlignments(PsiAlignmentData* alignment);

/* Keeps only one copy of any aligned sequences which are >PSI_NEAR_IDENTICAL%
 * identical to one another */
static void
_PSIPurgeNearIdenticalAlignments(PsiAlignmentData* alignment);
static void
_PSIUpdatePositionCounts(PsiAlignmentData* alignment);

/* FIXME: needs more descriptive name */
static void
_PSIPurgeSimilarAlignments(PsiAlignmentData* alignment,
                           Uint4 seq_index1,
                           Uint4 seq_index2,
                           double max_percent_identity);
/****************************************************************************/

/**************** PurgeMatches stage of PSSM creation ***********************/
int
PSIPurgeBiasedSegments(PsiAlignmentData* alignment)
{
    if ( !alignment ) {
        return PSIERR_BADPARAM;
    }

    _PSIPurgeIdenticalAlignments(alignment);
    _PSIPurgeNearIdenticalAlignments(alignment);
    _PSIUpdatePositionCounts(alignment);

    return PSI_SUCCESS;
}

/** Remove those sequences which are identical to the query sequence 
 * FIXME: Rename to _PSIPurgeSelfHits() ?
 */
static void
_PSIPurgeIdenticalAlignments(PsiAlignmentData* alignment)
{
    Uint4 s = 0;        /* index on sequences */

    ASSERT(alignment);

    for (s = 0; s < alignment->dimensions->num_seqs + 1; s++) {
        _PSIPurgeSimilarAlignments(alignment, kQueryIndex, s, PSI_IDENTICAL);
    }
}

static void
_PSIPurgeNearIdenticalAlignments(PsiAlignmentData* alignment)
{
    Uint4 i = 0;
    Uint4 j = 0;

    ASSERT(alignment);

    for (i = 1; i < alignment->dimensions->num_seqs + 1; i++) { 
        for (j = 1; (i + j) < alignment->dimensions->num_seqs + 1; j++) {
            _PSIPurgeSimilarAlignments(alignment, i, (i + j), 
                                       PSI_NEAR_IDENTICAL);
        }
    }
}

/** Counts the number of sequences matching the query per query position
 * (columns of the multiple alignment) as well as the number of residues
 * present in each position of the query.
 * Should be called after multiple alignment data has been purged from biased
 * sequences.
 */
static void
_PSIUpdatePositionCounts(PsiAlignmentData* alignment)
{
    Uint4 s = 0;        /* index on aligned sequences */
    Uint4 p = 0;        /* index on positions */

    ASSERT(alignment);

    for (s = kQueryIndex + 1; s < alignment->dimensions->num_seqs + 1; s++) {

        if ( !alignment->use_sequences[s] )
            continue;

        for (p = 0; p < alignment->dimensions->query_sz; p++) {
            if (alignment->desc_matrix[s][p].used) {
                const Uint1 res = alignment->desc_matrix[s][p].letter;
                if (res >= PSI_ALPHABET_SIZE) {
                    continue;
                }
                alignment->res_counts[p][res]++;
                alignment->match_seqs[p]++;
            }
        }
    }
}

/** This function compares the sequences in the alignment->desc_matrix
 * structure indexed by sequence_index1 and seq_index2. If it finds aligned 
 * regions that have a greater percent identity than max_percent_identity, 
 * it removes the sequence identified by seq_index2.
 */
static void
_PSIPurgeSimilarAlignments(PsiAlignmentData* alignment,
                           Uint4 seq_index1,
                           Uint4 seq_index2,
                           double max_percent_identity)
{
    Uint4 i = 0;

    /* Nothing to do if sequences are the same or not selected for further
       processing */
    if ( seq_index1 == seq_index2 ||
         !alignment->use_sequences[seq_index1] ||
         !alignment->use_sequences[seq_index2] ) {
        return;
    }

    for (i = 0; i < alignment->dimensions->query_sz; i++) {

        const PsiDesc* seq1 = alignment->desc_matrix[seq_index1];
        const PsiDesc* seq2 = alignment->desc_matrix[seq_index2];


        /* starting index of the aligned region */
        Uint4 align_start = i;    
        /* length of the aligned region */
        Uint4 align_length = 0;   
        /* # of identical residues in aligned region */
        Uint4 nidentical = 0;     

        /* both positions in the sequences must be used */
        if ( !(seq1[i].used && seq2[i].used) ) {
            continue;
        }

        /* examine the aligned region (FIXME: should we care about Xs?) */
        while ( (i < alignment->dimensions->query_sz) && 
                (seq1[i].used && seq2[i].used)) {
            if (seq1[i].letter == seq2[i].letter)
                nidentical++;
            align_length++;
            i++;
        }
        ASSERT(align_length != 0);

        /* percentage of similarity of an aligned region between seq1 and 
           seq2 */
        {
            double percent_identity = (double) nidentical / align_length;

                fprintf(stderr, "%f%% for seqs %d and %d\n",
                        percent_identity, seq_index1, seq_index2);
            if (percent_identity >= max_percent_identity) {
                int rv = _PSIPurgeAlignedRegion(alignment, seq_index2, 
                                                align_start, 
                                                align_start+align_length);
                ASSERT(rv == PSI_SUCCESS);
            }
        }
    }
}

/****************************************************************************/
/* Function prototypes */
static void
_PSIGetLeftExtents(const PsiAlignmentData* alignment, Uint4 seq_index);
static void
_PSIGetRightExtents(const PsiAlignmentData* alignment, Uint4 seq_index);

static void
_PSIComputePositionExtents(const PsiAlignmentData* alignment, 
                           Uint4 seq_index,
                           PsiAlignedBlock* aligned_blocks);
static void
_PSIComputeAlignedRegionLengths(const PsiAlignmentData* alignment,
                                PsiAlignedBlock* aligned_blocks);

/****************************************************************************/
/******* Compute alignment extents stage of PSSM creation *******************/
/* posComputeExtents in posit.c */
int
PSIComputeAlignmentBlocks(const PsiAlignmentData* alignment,        /* [in] */
                          PsiAlignedBlock* aligned_blocks)          /* [out] */
{
    Uint4 s = 0;     /* index on aligned sequences */

    if ( !alignment || !aligned_blocks ) {
        return PSIERR_BADPARAM;
    }

    /* no need to compute extents for query sequence */
    for (s = kQueryIndex + 1; s < alignment->dimensions->num_seqs + 1; s++) {
        if ( !alignment->use_sequences[s] )
            continue;

        _PSIGetLeftExtents(alignment, s);
        _PSIGetRightExtents(alignment, s);
        _PSIComputePositionExtents(alignment, s, aligned_blocks);
    }

    _PSIComputeAlignedRegionLengths(alignment, aligned_blocks);

    return PSI_SUCCESS;
}

static void
_PSIGetLeftExtents(const PsiAlignmentData* alignment, Uint4 seq_index)
{
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA['-'];
    PsiDesc* sequence_position = NULL;
    Uint4 prev = 0;  /* index for the first and previous position */
    Uint4 curr = 0;  /* index for the current position */

    ASSERT(alignment);
    ASSERT(seq_index < alignment->dimensions->num_seqs + 1);
    ASSERT(alignment->use_sequences[seq_index]);

    sequence_position = alignment->desc_matrix[seq_index];

    if (sequence_position[prev].used && sequence_position[prev].letter != GAP) {
        sequence_position[prev].extents.left = prev;
    }

    for (curr = prev + 1; curr < alignment->dimensions->query_sz; 
         curr++, prev++) {

        if ( !sequence_position[curr].used ) {
            continue;
        }

        if (sequence_position[prev].used) {
            sequence_position[curr].extents.left =
                sequence_position[prev].extents.left;
        } else {
            sequence_position[curr].extents.left = curr;
        }
    }
}

static void
_PSIGetRightExtents(const PsiAlignmentData* alignment, Uint4 seq_index)
{
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA['-'];
    PsiDesc* sequence_position = NULL;
    Uint4 last = 0;      /* index for the last position */
    Int4 curr = 0;      /* index for the current position */

    ASSERT(alignment);
    ASSERT(seq_index < alignment->dimensions->num_seqs + 1);
    ASSERT(alignment->use_sequences[seq_index]);

    sequence_position = alignment->desc_matrix[seq_index];
    last = alignment->dimensions->query_sz - 1;

    if (sequence_position[last].used && sequence_position[last].letter != GAP) {
        sequence_position[last].extents.right = last;
    }

    for (curr = last - 1; curr >= 0; curr--, last--) {

        if ( !sequence_position[curr].used ) {
            continue;
        }

        if (sequence_position[last].used) {
            sequence_position[curr].extents.right =
                sequence_position[last].extents.right;
        } else {
            sequence_position[curr].extents.right = curr;
        }
    }
}

static void
_PSIComputePositionExtents(const PsiAlignmentData* alignment, 
                           Uint4 seq_index,
                           PsiAlignedBlock* aligned_blocks)
{
#ifdef PSI_IGNORE_GAPS_IN_COLUMNS
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA['-'];
#endif
    PsiDesc* sequence_position = NULL;
    Uint4 i = 0;

    ASSERT(aligned_blocks);
    ASSERT(alignment);
    ASSERT(seq_index < alignment->dimensions->num_seqs + 1);
    ASSERT(alignment->use_sequences[seq_index]);

    sequence_position = alignment->desc_matrix[seq_index];

    for (i = 0; i < alignment->dimensions->query_sz; i++) {
#ifdef PSI_IGNORE_GAPS_IN_COLUMNS
        if (sequence_position[i].used && 
            sequence_position[i].letter != GAP) {
#else
        if (sequence_position[i].used) {
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
_PSIComputeAlignedRegionLengths(const PsiAlignmentData* alignment,
                                PsiAlignedBlock* aligned_blocks)
{
    const Uint1 X = AMINOACID_TO_NCBISTDAA['X'];
    PsiDesc* query_seq = NULL;
    Uint4 i = 0;
    
    ASSERT(alignment);
    ASSERT(aligned_blocks);

    for (i = 0; i < alignment->dimensions->query_sz; i++) {
        aligned_blocks->size[i] = aligned_blocks->pos_extnt[i].right - 
                                   aligned_blocks->pos_extnt[i].left;
    }

    query_seq = alignment->desc_matrix[kQueryIndex];

    /* Do not include X's in aligned region lengths */
    for (i = 0; i < alignment->dimensions->query_sz; i++) {

        if (query_seq[i].letter == X) {

            Uint4 idx = 0;
            for (idx = 0; idx < i; idx++) {
                if ((Uint4)aligned_blocks->pos_extnt[idx].right >= i &&
                    query_seq[idx].letter != X) {
                    aligned_blocks->size[idx]--;
                }
            }
            for (idx = alignment->dimensions->query_sz - 1; idx > i; idx--) {
                if ((Uint4)aligned_blocks->pos_extnt[idx].left <= i &&
                    query_seq[idx].letter != X) {
                    aligned_blocks->size[idx]--;
                }
            }
        }

    }
}

/****************************************************************************/
static Uint4
_PSIGetAlignedSequencesForPosition(
    const PsiAlignmentData* alignment, 
    Uint4 position,
    Uint4* aligned_sequences);

static void
_PSICalculatePositionWeightsAndIntervalSigmas(
    const PsiAlignmentData* alignment,     /* [in] */
    const PsiAlignedBlock* aligned_blocks, /* [in] */
    Uint4 position,                        /* [in] */
    const Uint4* aligned_seqs,             /* [in] */
    Uint4 num_aligned_seqs,                /* [in] */
    PsiSequenceWeights* seq_weights,       /* [out] */
    double* sigma,                         /* [out] */
    double* interval_sigma);               /* [out] */

static void
_PSICalculateNormalizedSequenceWeights(
    const PsiAlignedBlock* aligned_blocks, /* [in] */
    Uint4 position,                        /* [in] */
    const Uint4* aligned_seqs,             /* [in] */
    Uint4 num_aligned_seqs,                /* [in] */
    double sigma,                          /* [in] */
    PsiSequenceWeights* seq_weights);      /* [out] */

static void
_PSICalculateMatchWeights(
    const PsiAlignmentData* alignment,  /* [in] */
    Uint4 position,                     /* [in] */
    const Uint4* aligned_seqs,          /* [in] */
    Uint4 num_aligned_seqs,             /* [in] */
    PsiSequenceWeights* seq_weights);   /* [out] */

static int
_PSICheckSequenceWeights(
    const PsiAlignmentData* alignment,         /* [in] */
    PsiSequenceWeights* seq_weights);          /* [in] */

/****************************************************************************/
/******* Calculate sequence weights stage of PSSM creation ******************/
/* Needs the PsiAlignedBlock structure calculated in previous stage as well
 * as PsiAlignmentData structure */

int
PSIComputeSequenceWeights(const PsiAlignmentData* alignment,        /* [in] */
                          const PsiAlignedBlock* aligned_blocks,    /* [in] */
                          PsiSequenceWeights* seq_weights)          /* [out] */
{
    Uint4* aligned_seqs = NULL;     /* list of indices of sequences
                                              which participate in an
                                              aligned position */
    Uint4 pos = 0;                  /* position index */

    ASSERT(alignment);
    ASSERT(aligned_blocks);
    ASSERT(seq_weights);

    aligned_seqs = (Uint4*) calloc(alignment->dimensions->num_seqs + 1,
                                          sizeof(Uint4));
    if ( !aligned_seqs ) {
        return PSIERR_OUTOFMEM;
    }

    for (pos = 0; pos < alignment->dimensions->query_sz; pos++) {

        Uint4 num_aligned_seqs = 0;
        double sigma = 0.0; /**< number of different characters
                                  occurring in matches within a multiple
                                  alignment block, excluding identical
                                  columns */
        double interval_sigma = 0.0; /**< same as sigma but includes identical
                                  columns */

        /* ignore positions of no interest */
        if (aligned_blocks->size[pos] == 0 || alignment->match_seqs[pos] <= 1) {
            continue;
        }

        memset((void*)aligned_seqs, 0, 
               sizeof(Uint4) * (alignment->dimensions->num_seqs + 1));

        num_aligned_seqs = _PSIGetAlignedSequencesForPosition(alignment, pos,
                                                              aligned_seqs);
        if (num_aligned_seqs <= 1) {
            continue;
        }

        /* Skipping optimization about redundant sets */

        /* if (newSequenceSet) in posit.c */
        memset((void*)seq_weights->norm_seq_weights, 0, 
               sizeof(double)*(alignment->dimensions->num_seqs+1));
        memset((void*)seq_weights->row_sigma, 0,
               sizeof(double)*(alignment->dimensions->num_seqs+1));
        _PSICalculatePositionWeightsAndIntervalSigmas(alignment, 
                                     aligned_blocks, pos, aligned_seqs, 
                                     num_aligned_seqs, seq_weights, 
                                     &sigma, &interval_sigma);

        seq_weights->sigma[pos] = interval_sigma;

        /* Populates norm_seq_weights */
        _PSICalculateNormalizedSequenceWeights(aligned_blocks, pos, 
                                 aligned_seqs, num_aligned_seqs, sigma,
                                 seq_weights);


        /* Uses seq_weights->norm_seq_weights to populate match_weights */
        _PSICalculateMatchWeights(alignment, pos, aligned_seqs,
                                  num_aligned_seqs, seq_weights);
    }

    sfree(aligned_seqs);

    /* Check that the sequence weights add up to 1 in each column */
    _PSICheckSequenceWeights(alignment, seq_weights);

    /* Return seq_weights->match_weigths, should free others? FIXME: need to
     * keep sequence weights for diagnostics for structure group */
    return PSI_SUCCESS;
}

/* Calculates the position based weights using a modified version of the
 * Henikoff's algorithm presented in "Position-based sequence weights".
 * More documentation pending */
static void
_PSICalculatePositionWeightsAndIntervalSigmas(
    const PsiAlignmentData* alignment,     /* [in] */
    const PsiAlignedBlock* aligned_blocks, /* [in] */
    Uint4 position,                        /* [in] */
    const Uint4* aligned_seqs,             /* [in] */
    Uint4 num_aligned_seqs,                /* [in] */
    PsiSequenceWeights* seq_weights,       /* [out] */
    double* sigma,                         /* [out] */
    double* interval_sigma)                /* [out] */
{
    /** keeps track of how many occurrences of each residue was found at a
     * given position */
    Uint4 residue_counts[PSI_ALPHABET_SIZE];
    Uint4 num_distinct_residues = 0; /**< number of distinct 
                                              residues found at a given 
                                              position */

    Uint4 i = 0;         /**< index into aligned block for requested
                                   position */

    ASSERT(seq_weights);
    ASSERT(sigma);
    ASSERT(interval_sigma);

    *sigma = 0.0;
    *interval_sigma = 0.0;

    for (i = 0; i < sizeof(residue_counts); i++) {
        residue_counts[i] = 0;
    }

    for (i = (Uint4) aligned_blocks->pos_extnt[position].left; 
         i <= (Uint4) aligned_blocks->pos_extnt[position].right; i++) {

        Uint4 asi = 0;   /**< index into array of aligned sequences */

        /**** Count number of occurring residues at a position ****/
        for (asi = 0; asi < num_aligned_seqs; asi++) {
            const Uint4 seq_idx = aligned_seqs[asi];
            const Uint1 residue = 
                alignment->desc_matrix[seq_idx][i].letter;

            if (residue_counts[residue] == 0) {
                num_distinct_residues++;
            }
            residue_counts[residue]++;
        }
        /**** END: Count number of occurring residues at a position ****/

        /* FIXME: see Alejandro's email about this */
        (*interval_sigma) += num_distinct_residues;
        if (num_distinct_residues > 1) {    /* if this is not 1 */
            (*sigma) += num_distinct_residues;
        }

        /* Calculate row_sigma */
        for (asi = 0; asi < num_aligned_seqs; asi++) {
            const Uint4 seq_idx = aligned_seqs[asi];
            const Uint1 residue =
                alignment->desc_matrix[seq_idx][i].letter;

            seq_weights->row_sigma[seq_idx] += 
                (1.0 / (double) 
                 (residue_counts[residue] * num_distinct_residues) );
        }
    }

    return;
}

/** Calculates the normalized sequence weights for the requested position */
static void
_PSICalculateNormalizedSequenceWeights(
    const PsiAlignedBlock* aligned_blocks, /* [in] */
    Uint4 position,                        /* [in] */
    const Uint4* aligned_seqs,             /* [in] */
    Uint4 num_aligned_seqs,                /* [in] */
    double sigma,                          /* [in] */
    PsiSequenceWeights* seq_weights)       /* [out] */
{
    Uint4 asi = 0;   /**< index into array of aligned sequences */

    if (sigma > 0) {
        double weight_sum = 0.0;

        for (asi = 0; asi < num_aligned_seqs; asi++) {
            const Uint4 seq_idx = aligned_seqs[asi];
            seq_weights->norm_seq_weights[seq_idx] = 
                seq_weights->row_sigma[seq_idx] / 
                (aligned_blocks->pos_extnt[position].right -
                 aligned_blocks->pos_extnt[position].left + 1);
#ifndef PSI_IGNORE_GAPS_IN_COLUMNS
            weight_sum += seq_weights->norm_seq_weights[seq_idx];
#endif
        }

        for (asi = 0; asi < num_aligned_seqs; asi++) {
            const Uint4 seq_idx = aligned_seqs[asi];
            seq_weights->norm_seq_weights[seq_idx] /= weight_sum;
        }

    } else {
        for (asi = 0; asi < num_aligned_seqs; asi++) {
            const Uint4 seq_idx = aligned_seqs[asi];
            seq_weights->norm_seq_weights[seq_idx] = 
                (1.0/(double) num_aligned_seqs);
        }
    }

}

static void
_PSICalculateMatchWeights(
    const PsiAlignmentData* alignment,  /* [in] */
    Uint4 position,                     /* [in] */
    const Uint4* aligned_seqs,          /* [in] */
    Uint4 num_aligned_seqs,             /* [in] */
    PsiSequenceWeights* seq_weights)    /* [out] */
{
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA['-'];
    Uint4 asi = 0;   /**< index into array of aligned sequences */

    for (asi = 0; asi < num_aligned_seqs; asi++) {
        const Uint4 seq_idx = aligned_seqs[asi];
        const Uint1 residue =
            alignment->desc_matrix[seq_idx][position].letter;

        seq_weights->match_weights[position][residue] += 
            seq_weights->norm_seq_weights[seq_idx];

        /* FIXME: this field is populated but never used */
        if (residue == GAP) {
            /*seq_weights->gapless_column_weights[position] +=
             * seq_weights->a[seq_idx]; */
            ;
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
_PSIGetAlignedSequencesForPosition(const PsiAlignmentData* alignment, 
                                   Uint4 position,
                                   Uint4* aligned_sequences)
{
#ifdef PSI_IGNORE_GAPS_IN_COLUMNS
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA['-'];
#endif
    Uint4 retval = 0;
    Uint4 i = 0;

    ASSERT(alignment);
    ASSERT(position < alignment->dimensions->query_sz);
    ASSERT(aligned_sequences);

    for (i = 0; i < alignment->dimensions->num_seqs + 1; i++) {

        if ( !alignment->use_sequences[i] ) {
            continue;
        }

#ifdef PSI_IGNORE_GAPS_IN_COLUMNS
        if (alignment->desc_matrix[i][position].used &&
            alignment->desc_matrix[i][position].letter != GAP) {
#else
        if (alignment->desc_matrix[i][position].used) {
#endif
            aligned_sequences[retval++] = i;
        }
    }

    return retval;
}

/* The second parameter is not really const, it's updated! */
static int
_PSICheckSequenceWeights(const PsiAlignmentData* alignment,
                         PsiSequenceWeights* seq_weights)
{
    const Uint1 GAP = AMINOACID_TO_NCBISTDAA['-'];
    const Uint1 X = AMINOACID_TO_NCBISTDAA['X'];
    Uint4 pos = 0;   /* residue position (ie: column number) */
    Uint4 res = 0;   /* residue */

    ASSERT(alignment);
    ASSERT(seq_weights);

    for (pos = 0; pos < alignment->dimensions->query_sz; pos++) {

        double running_total = 0.0;

        if (alignment->match_seqs[pos] <= 1 ||
            alignment->desc_matrix[kQueryIndex][pos].letter == X) {
            continue;
        }

        for (res = 0; res < PSI_ALPHABET_SIZE; res++) {
            running_total += seq_weights->match_weights[pos][res];
        }
        ASSERT(running_total < 0.99 || running_total > 1.01);

#ifndef PSI_IGNORE_GAPS_IN_COLUMNS
        /* Disperse method of spreading the gap weights */
        for (res = 0; res < PSI_ALPHABET_SIZE; res++) {
            if (seq_weights->std_prob[res] > kEpsilon) {
                seq_weights->match_weights[pos][res] += 
                    (seq_weights->match_weights[pos][GAP] * 
                     seq_weights->std_prob[res]);
            }
        }
#endif
        seq_weights->match_weights[pos][GAP] = 0.0;
        running_total = 0.0;
        for (res = 0; res < PSI_ALPHABET_SIZE; res++) {
            running_total += seq_weights->match_weights[pos][res];
        }

        if (running_total < 0.99 || running_total > 1.01) {
            return PSIERR_BADSEQWEIGHTS;
        }
    }

    return PSI_SUCCESS;
}

/****************************************************************************/
/******* Compute residue frequencies stage of PSSM creation *****************/
/* port of posComputePseudoFreqs */
int
PSIComputeResidueFrequencies(const PsiAlignmentData* alignment,     /* [in] */
                             const PsiSequenceWeights* seq_weights, /* [in] */
                             const BlastScoreBlk* sbp,              /* [in] */
                             const PsiAlignedBlock* aligned_blocks, /* [in] */
                             const PSIBlastOptions* opts,           /* [in] */
                             PsiMatrix* score_matrix)               /* [out] */
{
    const Uint1 X = AMINOACID_TO_NCBISTDAA['X'];
    Uint4 i = 0;             /* loop index into query positions */
    SFreqRatios* freq_ratios;       /* matrix-specific frequency ratios */

    if ( !alignment || !seq_weights || !sbp || 
         !aligned_blocks || !opts || !score_matrix ) {
        return PSIERR_BADPARAM;
    }

    freq_ratios = _PSIMatrixFrequencyRatiosNew(sbp->name);

    for (i = 0; i < alignment->dimensions->query_sz; i++) {

        Uint4 j = 0;     /* loop index into alphabet */
        double info_sum = 0.0;  /* for information content - FIXME calculate
                                   separately */

        /* If there is an 'X' in the query sequence at position i... */
        if (alignment->desc_matrix[kQueryIndex][i].letter == X) {

            for (j = 0; j < (Uint4) sbp->alphabet_size; j++) {
                score_matrix->res_freqs[i][j] = 0.0;
            }

        } else {

            for (j = 0; j < (Uint4) sbp->alphabet_size; j++) {

                if (seq_weights->std_prob[j] > kEpsilon) {
                    Uint4 interval_size = 0; /* length of a block */
                    Uint4 idx = 0;  /* loop index */
                    double sigma = 0.0;    /* number of chars in an interval */

                    double pseudo = 0.0;            /* intermediate term */
                    double numerator = 0.0;         /* intermediate term */
                    double denominator = 0.0;       /* intermediate term */
                    double qOverPEstimate = 0.0;    /* intermediate term */

                    /* changed to matrix specific ratios here May 2000 */
                    for (idx = 0; idx < (Uint4) sbp->alphabet_size; idx++) {
                        if (sbp->matrix[j][idx] != BLAST_SCORE_MIN) {
                            pseudo += (seq_weights->match_weights[i][idx] *
                                       freq_ratios->data[j][idx]);
                        }
                    }
                    pseudo *= opts->pseudo_count;

                    /* FIXME: document where this formula is coming from
                     * (probably 2001 paper, p 2996) */
                    sigma = seq_weights->sigma[i];
                    interval_size = aligned_blocks->size[i];

                    numerator = pseudo + 
                        ((sigma/interval_size-1) * 
                         seq_weights->match_weights[i][j] / 
                         seq_weights->std_prob[j]);

                    denominator = (sigma/interval_size-1) +
                        opts->pseudo_count;

                    qOverPEstimate = numerator/denominator;

                    /* Note artificial multiplication by standard probability
                     * to normalize */
                    score_matrix->res_freqs[i][j] = qOverPEstimate *
                        seq_weights->std_prob[j];

                    if ( qOverPEstimate != 0.0 &&
                         (seq_weights->std_prob[j] > kEpsilon) ) {
                        info_sum += qOverPEstimate * seq_weights->std_prob[j] *
                            log(qOverPEstimate)/NCBIMATH_LN2;
                    }

                } else {
                    score_matrix->res_freqs[i][j] = 0.0;
                } /* END: if (seq_weights->std_prob[j] > kEpsilon) { */
            } /* END: for (j = 0; j < sbp->alphabet_size; j++) */

        }
        /* FIXME: Should move out the calculation of information content to its
         * own function (see posFreqsToInformation)! */
        seq_weights->info_content[i] = info_sum;
    }

    freq_ratios = _PSIMatrixFrequencyRatiosFree(freq_ratios);

    return PSI_SUCCESS;
}

/****************************************************************************/
/**************** Convert residue frequencies to PSSM stage *****************/

/* FIXME: Answer questions
   FIXME: need ideal_labmda, regular scoring matrix, length of query
*/
int
PSIConvertResidueFreqsToPSSM(PsiMatrix* score_matrix,           /* [in|out] */
                             const Uint1* query,                /* [in] */
                             const BlastScoreBlk* sbp,          /* [in] */
                             const double* std_probs)           /* [in] */
{
    const Uint4 X = AMINOACID_TO_NCBISTDAA['X'];
    const Uint4 Star = AMINOACID_TO_NCBISTDAA['*'];
    Uint4 i = 0;
    Uint4 j = 0;
    SFreqRatios* std_freq_ratios = NULL;    /* only needed when there are not
                                               residue frequencies for a given 
                                               column */
    double ideal_lambda;

    if ( !score_matrix || !sbp || !std_probs )
        return PSIERR_BADPARAM;

    std_freq_ratios = _PSIMatrixFrequencyRatiosNew(sbp->name);
    ideal_lambda = sbp->kbp_ideal->Lambda;

    /* Each column is a position in the query */
    for (i = 0; i < score_matrix->ncols; i++) {

        /* True if all frequencies in column i are zero */
        Boolean is_unaligned_column = TRUE;
        const Uint4 query_res = query[i];

        for (j = 0; j < (Uint4) sbp->alphabet_size; j++) {

            double qOverPEstimate = 0.0;

            /* Division compensates for multiplication in previous function */
            if (std_probs[j] > kEpsilon) {
                qOverPEstimate = 
                    score_matrix->res_freqs[i][j] / std_probs[j];
            }

            if (is_unaligned_column && qOverPEstimate != 0.0) {
                is_unaligned_column = FALSE;
            }

            /* Populate scaled matrix */
            if (qOverPEstimate == 0.0 || std_probs[j] < kEpsilon) {
                score_matrix->scaled_pssm[i][j] = BLAST_SCORE_MIN;
            } else {
                double tmp = log(qOverPEstimate)/ideal_lambda;
                score_matrix->scaled_pssm[i][j] = (int)
                    BLAST_Nint(kPsiScaleFactor * tmp);
            }

            if ( (j == X || j == Star) &&
                 (sbp->matrix[query_res][X] != BLAST_SCORE_MIN) ) {
                score_matrix->scaled_pssm[i][j] = 
                    sbp->matrix[query_res][j] * kPsiScaleFactor;
            }
        }

        if (is_unaligned_column) {
            for (j = 0; j < (Uint4) sbp->alphabet_size; j++) {

                score_matrix->pssm[i][j] = sbp->matrix[query_res][j];

                if (sbp->matrix[query_res][j] != BLAST_SCORE_MIN) {
                    double tmp = 
                        kPsiScaleFactor * std_freq_ratios->bit_scale_factor *
                        log(std_freq_ratios->data[query_res][j])/NCBIMATH_LN2;

                    score_matrix->scaled_pssm[i][j] = BLAST_Nint(tmp);
                } else {
                    score_matrix->scaled_pssm[i][j] = BLAST_SCORE_MIN;
                }
            }
        }
    }

    std_freq_ratios = _PSIMatrixFrequencyRatiosFree(std_freq_ratios);

    /* Set the last column of the matrix to BLAST_SCORE_MIN (why?) */
    for (j = 0; j < (Uint4) sbp->alphabet_size; j++) {
        score_matrix->scaled_pssm[score_matrix->ncols-1][j] = BLAST_SCORE_MIN;
    }

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
PSIScaleMatrix(const Uint1* query,              /* [in] */
               Uint4 query_length,              /* [in] */
               const double* std_probs,         /* [in] */
               double* scaling_factor,          /* [in] */
               PsiMatrix* score_matrix,         /* [in|out] */
               BlastScoreBlk* sbp)              /* [in|out] */
{
    Boolean first_time = TRUE;
    Uint4 index = 0;     /* loop index */
    int** scaled_pssm = NULL;
    int** pssm = NULL;
    double factor;
    double factor_low = 0.0;
    double factor_high = 0.0;
    double new_lambda;      /* Karlin-Altschul parameter */

    const double kPositPercent = 0.05;
    const Uint4 kPositNumIterations = 10;
    Boolean too_high = TRUE;
    double ideal_lambda;

    if ( !score_matrix || !sbp || !query || !std_probs )
        return PSIERR_BADPARAM;

    ASSERT(sbp->kbp_psi[0]);

    scaled_pssm = score_matrix->scaled_pssm;
    pssm = score_matrix->pssm;
    ideal_lambda = sbp->kbp_ideal->Lambda;

    /* FIXME: need to take scaling_factor into account */

    factor = 1.0;
    while (1) {
        Uint4 i = 0;
        Uint4 j = 0;

        for (i = 0; i < score_matrix->ncols; i++) {
            for (j = 0; j < (Uint4) sbp->alphabet_size; j++) {
                if (scaled_pssm[i][j] != BLAST_SCORE_MIN) {
                    pssm[i][j] = 
                        BLAST_Nint(factor*scaled_pssm[i][j]/kPsiScaleFactor);
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

        for (i = 0; i < score_matrix->ncols; i++) {
            for (j = 0; j < (Uint4) sbp->alphabet_size; j++) {
                if (scaled_pssm[i][j] != BLAST_SCORE_MIN) {
                    pssm[i][j] = 
                        BLAST_Nint(factor*scaled_pssm[i][j]/kPsiScaleFactor);
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

    /* FIXME: Why is this needed? */
    for (index = 0; index < (Uint4) sbp->alphabet_size; index++) {
        pssm[score_matrix->ncols-1][index] = BLAST_SCORE_MIN;
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
    Uint4 effective_length = 0;                 /* length of query w/o Xs */
    Uint4 p = 0;                                /* index on positions */
    Uint4 c = 0;                                /* index on characters */
    int s = 0;                                  /* index on scores */
    int min_score = 0;                          /* minimum score in pssm */
    int max_score = 0;                          /* maximum score in pssm */
    short rv = 0;                               /* temporary return value */
    Blast_ScoreFreq* score_freqs = NULL;        /* score frequencies */

    ASSERT(pssm);
    ASSERT(query);
    ASSERT(std_probs);
    ASSERT(sbp);
    ASSERT(sbp->alphabet_code == BLASTAA_SEQ_CODE);

    rv = Blast_GetStdAlphabet(sbp->alphabet_code, aa_alphabet, BLASTAA_SIZE);
    if (rv <= 0) {
        return NULL;
    }
    ASSERT(rv == sbp->alphabet_size);

    effective_length = _PSISequenceLengthWithoutX(query, query_length);

    /* Get the minimum and maximum scores */
    for (p = 0; p < query_length; p++) {
        for (c = 0; c < (Uint4) sbp->alphabet_size; c++) {
            const int kScore = pssm[p][aa_alphabet[c]];

            if (kScore <= BLAST_SCORE_MIN || kScore >= BLAST_SCORE_MAX) {
                continue;
            }
            max_score = MAX(kScore, max_score);
            min_score = MIN(kScore, min_score);
        }
    }

    if ( !(score_freqs = Blast_ScoreFreqNew(min_score, max_score)))
        return NULL;

    score_freqs->obs_min = min_score;
    score_freqs->obs_max = max_score;
    for (p = 0; p < query_length; p++) {
        if (query[p] == X) {
            continue;
        }

        for (c = 0; c < (Uint4) sbp->alphabet_size; c++) {
            const int kScore = pssm[p][aa_alphabet[c]];

            if (kScore <= BLAST_SCORE_MIN || kScore >= BLAST_SCORE_MAX) {
                continue;
            }

            /* Increment the weight for the score in position p, residue c */
            score_freqs->sprob[kScore] += 
                (std_probs[aa_alphabet[c]]/effective_length);
        }
    }

    for (s = min_score; s < max_score; s++) {
        score_freqs->score_avg += (s * score_freqs->sprob[s]);
    }

    return score_freqs;
}

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
        sbp->kbp_gap_psi[0]->K = 
            sbp->kbp_psi[0]->K * sbp->kbp_gap_std[0]->K / sbp->kbp_ideal->K;
        sbp->kbp_gap_psi[0]->logK = log(sbp->kbp_gap_psi[0]->K);
    }

    score_freqs = Blast_ScoreFreqDestruct(score_freqs);
}


/****************************************************************************/
/* Function definitions for auxiliary functions for the stages above */
int
_PSIPurgeAlignedRegion(PsiAlignmentData* alignment,
                       unsigned int seq_index,
                       unsigned int start,
                       unsigned int stop)
{
    PsiDesc* sequence_position = NULL;
    unsigned int i = 0;

    if (!alignment)
        return PSIERR_BADPARAM;

    /* Cannot remove the query sequence from multiple alignment data or
       bad index */
    if (seq_index == kQueryIndex || 
        seq_index > alignment->dimensions->num_seqs + 1 ||
        stop > alignment->dimensions->query_sz)
        return PSIERR_BADPARAM;


    sequence_position = alignment->desc_matrix[seq_index];
    fprintf(stderr, "NEW purge %d (%d-%d)\n", seq_index, start, stop);
    for (i = start; i < stop; i++) {
        sequence_position[i].letter = (unsigned char) -1;
        sequence_position[i].used = FALSE;
        sequence_position[i].e_value = kDefaultEvalueForPosition;
        sequence_position[i].extents.left = (unsigned int) -1;
        sequence_position[i].extents.right = alignment->dimensions->query_sz;
    }

    _PSIDiscardIfUnused(alignment, seq_index);

    return PSI_SUCCESS;
}

/* Check if we still need this sequence */
void
_PSIDiscardIfUnused(PsiAlignmentData* alignment, unsigned int seq_index)
{
    Boolean contains_aligned_regions = FALSE;
    unsigned int i = 0;

    for (i = 0; i < alignment->dimensions->query_sz; i++) {
        if (alignment->desc_matrix[seq_index][i].used) {
            contains_aligned_regions = TRUE;
            break;
        }
    }

    if ( !contains_aligned_regions ) {
        alignment->use_sequences[seq_index] = FALSE;
        fprintf(stderr, "NEW Removing sequence %d\n", seq_index);
    }
}

/****************************************************************************/
double*
_PSIGetStandardProbabilities(const BlastScoreBlk* sbp)
{
    Blast_ResFreq* standard_probabilities = NULL;
    Uint4 i = 0;
    double* retval = NULL;

    if ( !(retval = (double*) malloc(sbp->alphabet_size * sizeof(double))))
        return NULL;

    standard_probabilities = Blast_ResFreqNew(sbp);
    Blast_ResFreqStdComp(sbp, standard_probabilities);

    for (i = 0; i < (Uint4) sbp->alphabet_size; i++) {
        retval[i] = standard_probabilities->prob[i];
    }

    Blast_ResFreqDestruct(standard_probabilities);
    return retval;
}

PsiDiagnostics*
_PSISaveDiagnostics(const PsiAlignmentData* alignment,
                    const PsiAlignedBlock* aligned_block,
                    const PsiSequenceWeights* seq_weights)
{
    /* _PSICalculateInformationContent(seq_weights); */
    abort();
    return NULL;
}


/*
 * ===========================================================================
 * $Log$
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
 * + _PSICopyMatrix
 *
 * Revision 1.2  2004/04/07 22:08:37  kans
 * needed to include blast_def.h for sfree prototype
 *
 * Revision 1.1  2004/04/07 19:11:17  camacho
 * Initial revision
 *
 * ===========================================================================
 */
