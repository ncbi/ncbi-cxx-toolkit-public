/* $Id$
 * ===========================================================================
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
 */

/** @file blast_aalookup.c
 * Functions interacting with the protein BLAST lookup table.
 */

#include <algo/blast/core/blast_aalookup.h>
#include <algo/blast/core/lookup_util.h>
#include <algo/blast/core/blast_encoding.h>

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif                          /* SKIP_DOXYGEN_PROCESSING */

/** Structure containing information needed for adding neighboring words. 
 */
typedef struct NeighborInfo {
    BlastAaLookupTable *lookup; /**< Lookup table */
    Uint1 *query_word;   /**< the word whose neighbors we are computing */
    Uint1 *subject_word; /**< the computed neighboring word */
    Int4 alphabet_size;  /**< number of letters in the alphabet */
    Int4 wordsize;       /**< number of residues in a word */
    Int4 charsize;       /**< number of bits in a residue */
    Int4 **matrix;       /**< the substitution matrix */
    Int4 *row_max;       /**< maximum possible score for each row of the matrix */
    Int4 *offset_list;   /**< list of offsets where the word occurs in the query */
    Int4 threshold;      /**< the score threshold for neighboring words */
    Int4 query_bias;     /**< bias all stored offsets for multiple queries */
} NeighborInfo;

/**
 * Index a query sequence; i.e. fill a lookup table with the offsets
 * of query words
 *
 * @param lookup the lookup table [in/modified]
 * @param matrix the substitution matrix [in]
 * @param query the query sequence [in]
 * @param query_bias number added to each offset put into lookup table
 *                      (ordinarily 0; a nonzero value allows a succession of
 *                      query sequences to update the same lookup table)
 * @param location the list of ranges of query offsets to examine 
 *                 for indexing [in]
 */
static void s_AddNeighboringWords(BlastAaLookupTable * lookup, Int4 ** matrix,
                                  BLAST_SequenceBlk * query, Int4 query_bias,
                                  BlastSeqLoc * location);

/**
 * A position-specific version of AddNeighboringWords. Note that
 * only the score matrix matters for indexing purposes, so an
 * actual query sequence is unneccessary
 *
 * @param lookup the lookup table [in/modified]
 * @param matrix the substitution matrix [in]
 * @param query_bias number added to each offset put into lookup table
 *                      (ordinarily 0; a nonzero value allows a succession of
 *                      query sequences to update the same lookup table)
 * @param location the list of ranges of query offsets to examine for indexing
 */
static void s_AddPSSMNeighboringWords(BlastAaLookupTable * lookup, 
                                      Int4 ** matrix, Int4 query_bias, 
                                      BlastSeqLoc * location);

/** Add neighboring words to the lookup table.
 * @param lookup Pointer to the lookup table.
 * @param matrix Pointer to the substitution matrix.
 * @param query Pointer to the query sequence.
 * @param offset_list list of offsets where the word occurs in the query
 * @param query_bias bias all stored offsets for multiple queries
 * @param row_max maximum possible score for each row of the matrix
 */
static void s_AddWordHits(BlastAaLookupTable * lookup,
                          Int4 ** matrix, Uint1 * query,
                          Int4 * offset_list, Int4 query_bias, 
                          Int4 * row_max);

/** Add neighboring words to the lookup table using NeighborInfo structure.
 * @param info Pointer to the NeighborInfo structure.
 * @param score The partial sum of the score.
 * @param current_pos The current offset.
 */
static void s_AddWordHitsCore(NeighborInfo * info, Int4 score, 
                              Int4 current_pos);

/** Add neighboring words to the lookup table in case of a position-specific 
 * matrix.
 * @param lookup Pointer to the lookup table.
 * @param matrix The position-specific matrix.
 * @param query_bias bias all stored offsets for multiple queries
 * @param row_max maximum possible score for each row of the matrix
 */
static void s_AddPSSMWordHits(BlastAaLookupTable * lookup,
                            Int4 ** matrix, Int4 query_bias, Int4 * row_max);

/** Add neighboring words to the lookup table in case of a position-specific 
 * matrix, using NeighborInfo structure.
 * @param info Pointer to the NeighborInfo structure.
 * @param score The partial sum of the score.
 * @param current_pos The current offset.
 */
static void s_AddPSSMWordHitsCore(NeighborInfo * info,
                             Int4 score, Int4 current_pos);


Int2 RPSLookupTableNew(const BlastRPSInfo * info, BlastRPSLookupTable * *lut)
{
    Int4 i;
    BlastRPSLookupFileHeader *lookup_header;
    BlastRPSProfileHeader *profile_header;
    BlastRPSLookupTable *lookup = *lut =
        (BlastRPSLookupTable *) calloc(1, sizeof(BlastRPSLookupTable));
    Int4 *pssm_start;
    Int4 num_pssm_rows;
    PV_ARRAY_TYPE *pv;

    ASSERT(info != NULL);

    /* Fill in the lookup table information. */

    lookup_header = info->lookup_header;
    if (lookup_header->magic_number != RPS_MAGIC_NUM &&
        lookup_header->magic_number != RPS_MAGIC_NUM_28)
        return -1;

    /* set the alphabet size. Use hardwired numbers, since we cannot rely on
       #define'd constants matching up to the sizes implicit in disk files */
    if (lookup_header->magic_number == RPS_MAGIC_NUM)
        lookup->alphabet_size = 26;
    else
        lookup->alphabet_size = 28;

    lookup->wordsize = BLAST_WORDSIZE_PROT;
    lookup->charsize = ilog2(lookup->alphabet_size) + 1;
    lookup->backbone_size = 1 << (lookup->wordsize * lookup->charsize);
    lookup->mask = lookup->backbone_size - 1;
    lookup->rps_backbone = (RPSBackboneCell *) ((Uint1 *) lookup_header +
                                                lookup_header->
                                                start_of_backbone);
    lookup->overflow =
        (Int4 *) ((Uint1 *) lookup_header + lookup_header->start_of_backbone +
                  (lookup->backbone_size + 1) * sizeof(RPSBackboneCell));
    lookup->overflow_size = lookup_header->overflow_hits;

    /* fill in the pv_array */

    pv = lookup->pv = (PV_ARRAY_TYPE *) calloc(
                            (lookup->backbone_size >> PV_ARRAY_BTS),
                            sizeof(PV_ARRAY_TYPE));

    for (i = 0; i < lookup->backbone_size; i++) {
        if (lookup->rps_backbone[i].num_used > 0) {
            PV_SET(pv, i, PV_ARRAY_BTS);
        }
    }

    /* Fill in the PSSM information */

    profile_header = info->profile_header;
    if (profile_header->magic_number != RPS_MAGIC_NUM &&
        profile_header->magic_number != RPS_MAGIC_NUM_28)
        return -2;

    lookup->rps_seq_offsets = profile_header->start_offsets;
    lookup->num_profiles = profile_header->num_profiles;
    num_pssm_rows = lookup->rps_seq_offsets[lookup->num_profiles];
    lookup->rps_pssm = (Int4 **) malloc((num_pssm_rows + 1) * sizeof(Int4 *));
    pssm_start = profile_header->start_offsets + lookup->num_profiles + 1;

    for (i = 0; i < num_pssm_rows + 1; i++) {
        lookup->rps_pssm[i] = pssm_start;
        pssm_start += lookup->alphabet_size;
    }

    /* divide the concatenated database into regions of size RPS_BUCKET_SIZE. 
       bucket_array will then be used to organize offsets retrieved from the
       lookup table in order to increase cache reuse */

    lookup->num_buckets = num_pssm_rows / RPS_BUCKET_SIZE + 1;
    lookup->bucket_array = (RPSBucket *) malloc(lookup->num_buckets *
                                                sizeof(RPSBucket));
    for (i = 0; i < lookup->num_buckets; i++) {
        RPSBucket *bucket = lookup->bucket_array + i;
        bucket->num_filled = 0;
        bucket->num_alloc = 1000;
        bucket->offset_pairs = (BlastOffsetPair *) malloc(bucket->num_alloc *
                                                          sizeof
                                                          (BlastOffsetPair));
    }

    return 0;
}

BlastRPSLookupTable *RPSLookupTableDestruct(BlastRPSLookupTable * lookup)
{
    /* The following will only free memory that was allocated by
       RPSLookupTableNew. */
    Int4 i;
    for (i = 0; i < lookup->num_buckets; i++)
        sfree(lookup->bucket_array[i].offset_pairs);
    sfree(lookup->bucket_array);

    sfree(lookup->rps_pssm);
    sfree(lookup->pv);
    sfree(lookup);
    return NULL;
}

Int4 BlastAaLookupTableNew(const LookupTableOptions * opt,
                           BlastAaLookupTable * *lut)
{
    Int4 i;
    BlastAaLookupTable *lookup = *lut =
        (BlastAaLookupTable *) calloc(1, sizeof(BlastAaLookupTable));

    ASSERT(lookup != NULL);

    lookup->charsize = ilog2(BLASTAA_SIZE) + 1;
    lookup->word_length = opt->word_size;

    for (i = 0; i < lookup->word_length; i++)
        lookup->backbone_size |= (BLASTAA_SIZE - 1) << (i * lookup->charsize);
    lookup->backbone_size++;

    lookup->mask = (1 << (opt->word_size * lookup->charsize)) - 1;
    lookup->alphabet_size = BLASTAA_SIZE;
    lookup->threshold = (Int4)opt->threshold;
    lookup->thin_backbone =
        (Int4 **) calloc(lookup->backbone_size, sizeof(Int4 *));
    ASSERT(lookup->thin_backbone != NULL);

    lookup->thick_backbone = NULL;
    lookup->overflow = NULL;
    lookup->pv = NULL;
    return 0;
}


BlastAaLookupTable *BlastAaLookupTableDestruct(BlastAaLookupTable * lookup)
{
    sfree(lookup->thick_backbone);
    sfree(lookup->overflow);
    sfree(lookup->pv);
    sfree(lookup);
    return NULL;
}


Int4 BlastAaLookupFinalize(BlastAaLookupTable * lookup, EBoneType bone_type)
{
    Int4 i,j;
    Int4 overflow_cells_needed = 0;
    Int4 overflow_cursor = 0;
    Int4 longest_chain = 0;
    PV_ARRAY_TYPE *pv;
    AaLookupBackboneCell  *bbc;
    AaLookupSmallboneCell *sbc;
#ifdef LOOKUP_VERBOSE
    Int4 backbone_occupancy = 0;
    Int4 thick_backbone_occupancy = 0;
    Int4 num_overflows = 0;
#endif

    /* find out how many cells need the overflow array */
    for (i = 0; i < lookup->backbone_size; i++) {
        if (lookup->thin_backbone[i]) {
#ifdef LOOKUP_VERBOSE
            backbone_occupancy++;
#endif
            if (lookup->thin_backbone[i][1] > AA_HITS_PER_CELL){
#ifdef LOOKUP_VERBOSE
                ++num_overflows;
#endif
                overflow_cells_needed += lookup->thin_backbone[i][1];
            }
            if (lookup->thin_backbone[i][1] > longest_chain)
                longest_chain = lookup->thin_backbone[i][1];
        }
    }
    lookup->overflow_size = overflow_cells_needed;
    lookup->longest_chain = longest_chain;

#ifdef LOOKUP_VERBOSE
    thick_backbone_occupancy =  backbone_occupancy - num_overflows;
    printf("backbone size: %d\n", lookup->backbone_size);
    printf("backbone occupancy: %d (%f%%)\n", backbone_occupancy,
           100.0 * backbone_occupancy / lookup->backbone_size);
    printf("thick_backbone occupancy: %d (%f%%)\n",
           thick_backbone_occupancy,
           100.0 * thick_backbone_occupancy / lookup->backbone_size);
    printf("num_overflows: %d\n", num_overflows);
    printf("overflow size: %d\n", overflow_cells_needed);
    printf("longest chain: %d\n", longest_chain);
    printf("exact matches: %d\n", lookup->exact_matches);
    printf("neighbor matches: %d\n", lookup->neighbor_matches);
#endif

    /* bone-dependent lookup table filling-up */
    lookup->bone_type = bone_type;

    /* backbone using Int4 as storage unit */
    if(bone_type==eBackbone){
      /* allocate new lookup table */
      lookup->thick_backbone = 
            calloc(lookup->backbone_size, sizeof(AaLookupBackboneCell));
      ASSERT(lookup->thick_backbone != NULL);
      bbc = (AaLookupBackboneCell *) lookup->thick_backbone;
      /* allocate the pv_array */
      pv = lookup->pv = (PV_ARRAY_TYPE *) calloc(
            (lookup->backbone_size >> PV_ARRAY_BTS) + 1,
             sizeof(PV_ARRAY_TYPE));
      ASSERT(pv != NULL);
      /* allocate the overflow array */
      if (overflow_cells_needed > 0) {
          lookup->overflow =
                 calloc(overflow_cells_needed, sizeof(Int4));
          ASSERT(lookup->overflow != NULL);
      }
      /* fill the lookup table */
      for (i = 0; i < lookup->backbone_size; i++) {
        /* if there are hits there, */
        if (lookup->thin_backbone[i] ) {
            Int4 * dest = NULL;
            /* set the corresponding bit in the pv_array */
            PV_SET(pv, i, PV_ARRAY_BTS);
            bbc[i].num_used = lookup->thin_backbone[i][1];
            /* if there are three or fewer hits, */
            if (lookup->thin_backbone[i][1] <= AA_HITS_PER_CELL)
                /* copy them into the thick_backbone cell */
                dest = bbc[i].payload.entries;
            else /* more than three hits; copy to overflow array */
            {
                bbc[i].payload.overflow_cursor = overflow_cursor;
                dest = (Int4 *) lookup->overflow;
                dest += overflow_cursor;
                overflow_cursor += lookup->thin_backbone[i][1];
            }
            for (j=0; j <lookup->thin_backbone[i][1]; j++) 
                dest[j] = lookup->thin_backbone[i][j + 2];
            /* done with this chain- free it */
            sfree(lookup->thin_backbone[i]);
            lookup->thin_backbone[i] = NULL;
        }
        else /* no hits here */
            bbc[i].num_used = 0;
      }                           /* end for */
    } /* end of original scheme*/
    /* Smallbone, using Uint2 as storage unit */
    else{
      /* allocate new lookup table */
      lookup->thick_backbone = 
            calloc(lookup->backbone_size, sizeof(AaLookupSmallboneCell));
      ASSERT(lookup->thick_backbone != NULL);
      sbc = (AaLookupSmallboneCell *) lookup->thick_backbone;
      /* allocate the pv_array */
      pv = lookup->pv = (PV_ARRAY_TYPE *) calloc(
            (lookup->backbone_size >> PV_ARRAY_BTS) + 1,
             sizeof(PV_ARRAY_TYPE));
      ASSERT(pv != NULL);
      /* allocate the overflow array */
      if (overflow_cells_needed > 0) {
          lookup->overflow =
                 calloc(overflow_cells_needed, sizeof(Uint2));
          ASSERT(lookup->overflow != NULL);
      }
      /* fill the lookup table */
      for (i = 0; i < lookup->backbone_size; i++) {
        if (lookup->thin_backbone[i] ) {
            Uint2 * dest = NULL;
            PV_SET(pv, i, PV_ARRAY_BTS);
            sbc[i].num_used = lookup->thin_backbone[i][1];
            if ((lookup->thin_backbone[i])[1] <= AA_HITS_PER_CELL)
                 dest=sbc[i].payload.entries;
            else{
                sbc[i].payload.overflow_cursor = overflow_cursor;
                dest=((Uint2 *) (lookup->overflow))+overflow_cursor;
                overflow_cursor += lookup->thin_backbone[i][1];
            }
            for (j=0; j <lookup->thin_backbone[i][1]; j++) 
                dest[j] = lookup->thin_backbone[i][j + 2];
            sfree(lookup->thin_backbone[i]);
            lookup->thin_backbone[i] = NULL;
        }
        else sbc[i].num_used = 0;
      }                           /* end for */
    }  /* end of the small backbone */

    /* done copying hit info- free the backbone */
    sfree(lookup->thin_backbone);
    lookup->thin_backbone = NULL;

    return 0;
}

void BlastAaLookupIndexQuery(BlastAaLookupTable * lookup,
                             Int4 ** matrix,
                             BLAST_SequenceBlk * query,
                             BlastSeqLoc * location, 
                             Int4 query_bias)
{
    if (lookup->use_pssm) {
        s_AddPSSMNeighboringWords(lookup, matrix, query_bias, location);
    }
    else {
        ASSERT(query != NULL);
        s_AddNeighboringWords(lookup, matrix, query, query_bias, location);
    }
}

static void s_AddNeighboringWords(BlastAaLookupTable * lookup, Int4 ** matrix,
                                  BLAST_SequenceBlk * query, Int4 query_bias,
                                  BlastSeqLoc * location)
{
    Int4 i, j;
    Int4 **exact_backbone;
    Int4 row_max[BLASTAA_SIZE];

    ASSERT(lookup->alphabet_size <= BLASTAA_SIZE);

    /* Determine the maximum possible score for each row of the score matrix */

    for (i = 0; i < lookup->alphabet_size; i++) {
        row_max[i] = matrix[i][0];
        for (j = 1; j < lookup->alphabet_size; j++)
            row_max[i] = MAX(row_max[i], matrix[i][j]);
    }

    /* create an empty backbone */

    exact_backbone = (Int4 **) calloc(lookup->backbone_size, sizeof(Int4 *));

    /* find all the exact matches, grouping together all offsets of identical 
       query words. The query bias is not used here, since the next stage
       will need real offsets into the query sequence */

    BlastLookupIndexQueryExactMatches(exact_backbone, lookup->word_length,
                                      lookup->charsize, lookup->word_length,
                                      query, location);

    /* walk though the list of exact matches previously computed. Find
       neighboring words for entire lists at a time */

    for (i = 0; i < lookup->backbone_size; i++) {
        if (exact_backbone[i] != NULL) {
            s_AddWordHits(lookup, matrix, query->sequence,
                          exact_backbone[i], query_bias, row_max);
            sfree(exact_backbone[i]);
        }
    }

    sfree(exact_backbone);
}

static void s_AddWordHits(BlastAaLookupTable * lookup, Int4 ** matrix,
                        Uint1 * query, Int4 * offset_list,
                        Int4 query_bias, Int4 * row_max)
{
    Uint1 *w;
    Uint1 s[32];   /* larger than any possible wordsize */
    Int4 score;
    Int4 i;
    NeighborInfo info;

#ifdef LOOKUP_VERBOSE
    lookup->exact_matches += offset_list[1];
#endif

    /* All of the offsets in the list refer to the same query word. Thus,
       neighboring words only have to be found for the first offset in the
       list (since all other offsets would have the same neighbors) */

    w = query + offset_list[2];

    /* Compute the self-score of this word */

    score = matrix[w[0]][w[0]];
    for (i = 1; i < lookup->word_length; i++)
        score += matrix[w[i]][w[i]];

    /* If the self-score is above the threshold, then the neighboring
       computation will automatically add the word to the lookup table.
       Otherwise, either the score is too low or neighboring is not done at
       all, so that all of these exact matches must be explicitly added to
       the lookup table */

    if (lookup->threshold == 0 || score < lookup->threshold) {
        for (i = 0; i < offset_list[1]; i++) {
            BlastLookupAddWordHit(lookup->thin_backbone, lookup->word_length,
                                  lookup->charsize, w,
                                  query_bias + offset_list[i + 2]);
        }
    } else {
#ifdef LOOKUP_VERBOSE
        lookup->neighbor_matches -= offset_list[1];
#endif
    }

    /* check if neighboring words need to be found */

    if (lookup->threshold == 0)
        return;

    /* Set up the structure of information to be used during the recursion */

    info.lookup = lookup;
    info.query_word = w;
    info.subject_word = s;
    info.alphabet_size = lookup->alphabet_size;
    info.wordsize = lookup->word_length;
    info.charsize = lookup->charsize;
    info.matrix = matrix;
    info.row_max = row_max;
    info.offset_list = offset_list;
    info.threshold = lookup->threshold;
    info.query_bias = query_bias;

    /* compute the largest possible score that any neighboring word can have; 
       this maximum will gradually be replaced by exact scores as subject
       words are built up */

    score = row_max[w[0]];
    for (i = 1; i < lookup->word_length; i++)
        score += row_max[w[i]];

    s_AddWordHitsCore(&info, score, 0);
}

static void s_AddWordHitsCore(NeighborInfo * info, Int4 score, 
                              Int4 current_pos)
{
    Int4 alphabet_size = info->alphabet_size;
    Int4 threshold = info->threshold;
    Uint1 *query_word = info->query_word;
    Uint1 *subject_word = info->subject_word;
    Int4 *row;
    Int4 i;

    /* remove the maximum score of letters that align with the query letter
       at position 'current_pos'. Later code will align the entire alphabet
       with this letter, and compute the exact score each time. Also point to 
       the row of the score matrix corresponding to the query letter at
       current_pos */

    score -= info->row_max[query_word[current_pos]];
    row = info->matrix[query_word[current_pos]];

    if (current_pos == info->wordsize - 1) {

        /* The recursion has bottomed out, and we can produce complete
           subject words. Pass the entire alphabet through the last position
           in the subject word, then save the list of query offsets in all
           positions corresponding to subject words that yield a high enough
           score */

        Int4 *offset_list = info->offset_list;
        Int4 query_bias = info->query_bias;
        Int4 wordsize = info->wordsize;
        Int4 charsize = info->charsize;
        BlastAaLookupTable *lookup = info->lookup;
        Int4 j;

        for (i = 0; i < alphabet_size; i++) {
            if (score + row[i] >= threshold) {
                subject_word[current_pos] = i;
                for (j = 0; j < offset_list[1]; j++) {
                    BlastLookupAddWordHit(lookup->thin_backbone, wordsize,
                                          charsize, subject_word,
                                          query_bias + offset_list[j + 2]);
                }
#ifdef LOOKUP_VERBOSE
                lookup->neighbor_matches += offset_list[1];
#endif
            }
        }
        return;
    }

    /* Otherwise, pass the entire alphabet through position current_pos of
       the subject word, and recurse on all words that could possibly exceed
       the threshold later */

    for (i = 0; i < alphabet_size; i++) {
        if (score + row[i] >= threshold) {
            subject_word[current_pos] = i;
            s_AddWordHitsCore(info, score + row[i], current_pos + 1);
        }
    }
}

static void s_AddPSSMNeighboringWords(BlastAaLookupTable * lookup, 
                                      Int4 ** matrix, Int4 query_bias, 
                                      BlastSeqLoc * location)
{
    Int4 offset;
    Int4 i, j;
    BlastSeqLoc *loc;
    Int4 *row_max;
    Int4 wordsize = lookup->word_length;

    /* for PSSMs, we only have to track the maximum score of 'wordsize'
       matrix columns */

    row_max = (Int4 *) malloc(lookup->word_length * sizeof(Int4));
    ASSERT(row_max != NULL);

    for (loc = location; loc; loc = loc->next) {
        Int4 from = loc->ssr->left;
        Int4 to = loc->ssr->right - wordsize + 1;
        Int4 **row = matrix + from;

        /* prepare to start another run of adjacent query words. Find the
           maximum possible score for the first wordsize-1 rows of the PSSM */

        if (to >= from) {
            for (i = 0; i < wordsize - 1; i++) {
                row_max[i] = row[i][0];
                for (j = 1; j < lookup->alphabet_size; j++)
                    row_max[i] = MAX(row_max[i], row[i][j]);
            }
        }

        for (offset = from; offset <= to; offset++, row++) {
            /* find the maximum score of the next PSSM row */

            row_max[wordsize - 1] = row[wordsize - 1][0];
            for (i = 1; i < lookup->alphabet_size; i++)
                row_max[wordsize - 1] = MAX(row_max[wordsize - 1],
                                            row[wordsize - 1][i]);

            /* find all neighboring words */

            s_AddPSSMWordHits(lookup, row, offset + query_bias, row_max);

            /* shift the list of maximum scores over by one, to make room for 
               the next maximum in the next loop iteration */

            for (i = 0; i < wordsize - 1; i++)
                row_max[i] = row_max[i + 1];
        }
    }

    sfree(row_max);
}

static void s_AddPSSMWordHits(BlastAaLookupTable * lookup, Int4 ** matrix,
                              Int4 offset, Int4 * row_max)
{
    Uint1 s[32];   /* larger than any possible wordsize */
    Int4 score;
    Int4 i;
    NeighborInfo info;

    /* Set up the structure of information to be used during the recursion */

    info.lookup = lookup;
    info.query_word = NULL;
    info.subject_word = s;
    info.alphabet_size = lookup->alphabet_size;
    info.wordsize = lookup->word_length;
    info.charsize = lookup->charsize;
    info.matrix = matrix;
    info.row_max = row_max;
    info.offset_list = NULL;
    info.threshold = lookup->threshold;
    info.query_bias = offset;

    /* compute the largest possible score that any neighboring word can have; 
       this maximum will gradually be replaced by exact scores as subject
       words are built up */

    score = row_max[0];
    for (i = 1; i < lookup->word_length; i++)
        score += row_max[i];

    s_AddPSSMWordHitsCore(&info, score, 0);
}

static void s_AddPSSMWordHitsCore(NeighborInfo * info, Int4 score,
                                  Int4 current_pos)
{
    Int4 alphabet_size = info->alphabet_size;
    Int4 threshold = info->threshold;
    Uint1 *subject_word = info->subject_word;
    Int4 *row;
    Int4 i;

    /* remove the maximum score of letters that align with the query letter
       at position 'current_pos'. Later code will align the entire alphabet
       with this letter, and compute the exact score each time. Also point to 
       the row of the score matrix corresponding to the query letter at
       current_pos */

    score -= info->row_max[current_pos];
    row = info->matrix[current_pos];

    if (current_pos == info->wordsize - 1) {

        /* The recursion has bottomed out, and we can produce complete
           subject words. Pass the entire alphabet through the last position
           in the subject word, then save the query offset in all lookup
           table positions corresponding to subject words that yield a high
           enough score */

        Int4 offset = info->query_bias;
        Int4 wordsize = info->wordsize;
        Int4 charsize = info->charsize;
        BlastAaLookupTable *lookup = info->lookup;

        for (i = 0; i < alphabet_size; i++) {
            if (score + row[i] >= threshold) {
                subject_word[current_pos] = i;
                BlastLookupAddWordHit(lookup->thin_backbone, wordsize,
                                      charsize, subject_word, offset);
#ifdef LOOKUP_VERBOSE
                lookup->neighbor_matches++;
#endif
            }
        }
        return;
    }

    /* Otherwise, pass the entire alphabet through position current_pos of
       the subject word, and recurse on all words that could possibly exceed
       the threshold later */

    for (i = 0; i < alphabet_size; i++) {
        if (score + row[i] >= threshold) {
            subject_word[current_pos] = i;
            s_AddPSSMWordHitsCore(info, score + row[i], current_pos + 1);
        }
    }
}

/** Fetch next vacant cell from a bank.
 * @param[in] lookup compressed protein lookup table
 * @return pointer to reserved cell
 */
static CompressedOverflowCell* 
s_CompressedListGetNewCell(BlastCompressedAaLookupTable * lookup)
{
    if (lookup->curr_overflow_cell == 
                        COMPRESSED_OVERFLOW_CELLS_IN_BANK) {
        /* need a new bank */
        Int4 bank_idx = lookup->curr_overflow_bank + 1;
        lookup->overflow_banks[bank_idx] = (CompressedOverflowCell*) malloc(
                                           COMPRESSED_OVERFLOW_CELLS_IN_BANK *
                                           sizeof(CompressedOverflowCell));
        ASSERT(bank_idx < COMPRESSED_OVERFLOW_MAX_BANKS);
        ASSERT(lookup->overflow_banks[bank_idx]);
        lookup->curr_overflow_bank++;
        lookup->curr_overflow_cell = 0;
    }

    return lookup->overflow_banks[lookup->curr_overflow_bank] +
                               lookup->curr_overflow_cell++;
}

/** Add a single query offset to the compressed
 * alphabet protein lookup table
 * @param lookup The lookup table [in]
 * @param index The hashtable index into which the query offset goes [in]
 * @param query_offset Query offset to add [in]
 */
static void s_CompressedLookupAddWordHit(
                                  BlastCompressedAaLookupTable * lookup,
                                  Int4 index,
                                  Int4 query_offset)
{
    CompressedLookupBackboneCell *backbone_cell = lookup->backbone + index;
    CompressedOverflowCell * new_cell;
    Int4 num_entries = backbone_cell->num_used;

    if (num_entries < COMPRESSED_HITS_PER_BACKBONE_CELL) {
        backbone_cell->payload.query_offsets[num_entries] = query_offset;
    } 
    else if (num_entries == COMPRESSED_HITS_PER_BACKBONE_CELL) {

        /* need to create new overflow list */
  
        Int4 i;
        Int4 tmp_offsets[COMPRESSED_HITS_PER_BACKBONE_CELL-1];
  
        /* fetch next vacant cell */
        new_cell = s_CompressedListGetNewCell(lookup);

        /* this cell is always the end of the list */
        new_cell->next = NULL; 
  
        /* store the last element of the original backbone cell */
        new_cell->query_offsets[0] = backbone_cell->payload.query_offsets[
                                        COMPRESSED_HITS_PER_BACKBONE_CELL-1]; 
  
        /* store this new offset too */
        new_cell->query_offsets[1] = query_offset; 
  
        /* save offsets from being overwritten (the list of query
           offsets must be copied to a struct that aliases the current
           list in memory) */
        for (i = 0; i < COMPRESSED_HITS_PER_BACKBONE_CELL - 1; i++) {
            tmp_offsets[i] = backbone_cell->payload.query_offsets[i];
        }
        
        /* repopulate */
        for (i = 0; i < COMPRESSED_HITS_PER_BACKBONE_CELL - 1; i++) {
            backbone_cell->payload.overflow_list.query_offsets[i] = 
                                                        tmp_offsets[i];
        }
        
        /* make backbone point to this new, one-cell long list */
        backbone_cell->payload.overflow_list.head = new_cell;
    } 
    else { /* continue with existing overflow list */
      
        /* find the index into the current overflow cell; we
           do not store the current index in every cell, to
           save space */
        Int4 cell_index = (num_entries - 
                           COMPRESSED_HITS_PER_BACKBONE_CELL + 1) %
                           COMPRESSED_HITS_PER_OVERFLOW_CELL;

        if (cell_index == 0 ) { /* can't be empty => it's full  */

            /* fetch next vacant cell */
            new_cell = s_CompressedListGetNewCell(lookup);

            /* shuffle the pointers */
            new_cell->next = backbone_cell->payload.overflow_list.head;
            backbone_cell->payload.overflow_list.head = new_cell;
        }
        
        /* head always points to a cell with free space */
        backbone_cell->payload.overflow_list.head->query_offsets[
                                              cell_index] = query_offset;
    }

    backbone_cell->num_used++;
}

/** Add a single query offset to the compressed lookup table.
 * The index is computed using the letters in w[], which is 
 * assumed to already be converted to the compressed alphabet
 * @param lookup Pointer to the lookup table. [in][out]
 * @param w Word to add [in]
 * @param query_offset The offset in the query where the word occurs [in]
 */
static void s_CompressedLookupAddEncoded(
                                     BlastCompressedAaLookupTable * lookup,
                                     Uint1* w,
                                     Int4 query_offset)
{
    Int4 index;

    static const Int4 W7p1[] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90};
    static const Int4 W7p2[] = { 0, 100, 200, 300, 400, 500, 600, 700, 800,
                                 900};
    static const Int4 W7p3[] = { 0, 1000, 2000, 3000, 4000, 5000, 6000,
                                 7000, 8000, 9000};
    static const Int4 W7p4[] = { 0, 10000, 20000, 30000, 40000, 50000, 60000,
                                 70000, 80000, 90000};
    static const Int4 W7p5[] = { 0, 100000, 200000, 300000, 400000, 500000,
                                 600000, 700000, 800000, 900000};
    static const Int4 W7p6[] = { 0, 1000000, 2000000, 3000000, 4000000,
                                 5000000, 6000000, 7000000, 8000000, 9000000};
    
    static const Int4 W6p1[] = { 0, 15, 30, 45, 60, 75, 90, 105, 120, 135,
                                 150, 165, 180, 195, 210};
    static const Int4 W6p2[] = { 0, 225, 450, 675, 900, 1125, 1350, 1575,
                                 1800, 2025, 2250, 2475, 2700, 2925, 3150};
    static const Int4 W6p3[] = { 0, 3375, 6750, 10125, 13500, 16875, 20250,
                                 23625, 27000, 30375, 33750, 37125, 40500,
                                 43875, 47250};
    static const Int4 W6p4[] = { 0, 50625, 101250, 151875, 202500, 253125,
                                 303750, 354375, 405000, 455625, 506250,
                                 556875, 607500, 658125, 708750};
    static const Int4 W6p5[] = { 0, 759375, 1518750, 2278125, 3037500,
                                 3796875, 4556250, 5315625, 6075000, 6834375,
                                 7593750, 8353125, 9112500, 9871875, 10631250};

    if(lookup->word_length == 7)
        index =  w[0] + W7p1[w[1]] + W7p2[w[2]] + W7p3[w[3]] +
                        W7p4[w[4]] + W7p5[w[5]] + W7p6[w[6]];
    else
        index = w[0] + W6p1[w[1]] + W6p2[w[2]] + W6p3[w[3]] +
                       W6p4[w[4]] + W6p5[w[5]];

    s_CompressedLookupAddWordHit(lookup, index, query_offset);
}

/** Add a single query offset to the compressed lookup table.
 * The index is computed using the letters in w[], which is 
 * assumed to be in the standard alphabet (i.e. not compressed)
 * @param lookup Pointer to the lookup table. [in][out]
 * @param w word to add [in]
 * @param query_offset the offset in the query where the word occurs [in]
 */
static void s_CompressedLookupAddUnencoded(
                               BlastCompressedAaLookupTable * lookup,
                               Uint1* w,
                               Int4 query_offset)
{
    Int4 skip = 0;
    Int4 index = s_ComputeCompressedIndex(lookup->word_length, w, 
                                          lookup->compressed_alphabet_size,
                                          &skip, lookup);
    if (skip == 0)
        s_CompressedLookupAddWordHit(lookup, index, query_offset);
}

/** Structure containing information needed for adding neighboring words 
 * (specific to compressed lookup table)
 */
typedef struct CompressedNeighborInfo {
    BlastCompressedAaLookupTable *lookup; /**< Lookup table */
    Uint1 *query_word;   /**< the word whose neighbors we are computing */
    Uint1 *subject_word; /**< the computed neighboring word */
    Int4 compressed_alphabet_size;  /**< for use with compressed alphabet */
    Int4 wordsize;       /**< number of residues in a word */
    Int4 **matrix;       /**< the substitution matrix */
    Int4 row_max[BLASTAA_SIZE]; /**< maximum possible score for each
                                     row of the matrix */
    Int4 query_offset;   /**< a single query offset to index */
    Int4 threshold;      /**< the score threshold for neighboring words */
    Int4 matrixSorted[BLASTAA_SIZE][BLASTAA_SIZE]; /**< version of substitution
                                                       matrix whose rows are
                                                       sorted by score */
    Uint1 matrixSortedChar[BLASTAA_SIZE][BLASTAA_SIZE];/**< matrix with
                                              the letters permuted identically
                                              to that of matrixSorted */
} CompressedNeighborInfo;

/** Structure used as a helper for sorting matrix according to substitution
 * score
 */
typedef struct LetterAndScoreDifferencePair{
    Int4  diff;  /**< score difference from row maximum */
    Uint1 letter; /**< given protein letter */
} LetterAndScoreDifferencePair;

/** callback for the "sort" */
static int ScoreDifferenceSort(const void * a, const void *b ){
    return (((LetterAndScoreDifferencePair*)a)->diff - 
            ((LetterAndScoreDifferencePair*)b)->diff);
}

/** Prepare "score sorted" version of the substitution matrix"
 * @param info Pointer to the NeighborInfo structure.
 */
static void s_loadSortedMatrix(CompressedNeighborInfo* info) {

    LetterAndScoreDifferencePair sortTable[BLASTAA_SIZE];
    Int4 i;
    Int4 longChar, shortChar;

    for (longChar = 0; longChar < BLASTAA_SIZE; longChar++) {
        for (shortChar = 0; shortChar < 
                        info->compressed_alphabet_size; shortChar++) {
      
            sortTable[shortChar].diff = info->row_max[longChar] -
                                        info->matrix[longChar][shortChar];
            sortTable[shortChar].letter = shortChar;
        }

        qsort(sortTable, info->compressed_alphabet_size,
              sizeof(LetterAndScoreDifferencePair), ScoreDifferenceSort);

        for (i = 0; i < info->compressed_alphabet_size; i++) {
            Uint1 letter = sortTable[i].letter;

            info->matrixSorted[longChar][i] = info->matrix[longChar][letter];
            info->matrixSortedChar[longChar][i] = letter;
        }
    }
}

/** Very similar to s_AddWordHitsCore
 * @param info Pointer to the NeighborInfo structure.
 * @param score The partial sum of the score.
 * @param current_pos The current offset.
 */
static void s_CompressedAddWordHitsCore(CompressedNeighborInfo * info, 
                                        Int4 score, Int4 current_pos)
{
    Int4 compressed_alphabet_size = info->compressed_alphabet_size;
    Int4 threshold = info->threshold;
    Int4 wordsize = info->wordsize;
    Uint1 *query_word = info->query_word;
    Uint1 *subject_word = info->subject_word;
    Int4 i;
    Int4 *rowSorted;
    Uint1 *charSorted;
    Int4 currQueryChar = query_word[current_pos]; 
    
    /* remove the maximum score of letters that align with the query letter
       at position 'current_pos'. Later code will align the entire alphabet
       with this letter, and compute the exact score each time. Also point to 
       the row of the score matrix corresponding to the query letter at
       current_pos */

    score -= info->row_max[currQueryChar];
    rowSorted = info->matrixSorted[currQueryChar];
    charSorted = info->matrixSortedChar[currQueryChar];

    if (current_pos == wordsize - 1) {
        
        /* The recursion has bottomed out, and we can produce complete
           subject words. Pass (a portion of) the alphabet through the 
           last position in the subject word, then saving the query offset 
           in the lookup table position corresponding to subject word i */

        BlastCompressedAaLookupTable *lookup = info->lookup;
        Int4 query_offset = info->query_offset;
        
        for (i = 0; i < compressed_alphabet_size && 
               (score + rowSorted[i] >= threshold); i++) {
            subject_word[current_pos] = charSorted[i];
            s_CompressedLookupAddEncoded(lookup, subject_word, 
                                         query_offset);
#ifdef LOOKUP_VERBOSE
            lookup->neighbor_matches++;
#endif
        }
        return;
    }

    /* Otherwise, pass (a portion of) the alphabet through position 
       current_pos of the subject word, and recurse on all words that 
       could possibly exceed the threshold later */

    for (i = 0; i < compressed_alphabet_size && 
           (score + rowSorted[i] >= threshold); i++) {
        subject_word[current_pos] = charSorted[i];
        s_CompressedAddWordHitsCore(info, score + rowSorted[i], 
                                    current_pos + 1);
    }
}

/** Add neighboring words to the lookup table (compressed alphabet).
 * @param info Pointer to the NeighborInfo structure.
 * @param query Pointer to the query sequence.
 * @param query_offset offset where the word occurs in the query
 */
static void s_CompressedAddWordHits(CompressedNeighborInfo * info,
                                    Uint1 * query, Int4 query_offset)
{
    Uint1 *w = query + query_offset;
    Uint1 s[32];   /* larger than any possible wordsize */
    Int4 score;
    Int4 i;
    BlastCompressedAaLookupTable * lookup = info->lookup;

#ifdef LOOKUP_VERBOSE
    lookup->exact_matches++;
#endif

    /* Compute the self-score of the query word */

    score = 0;
    for (i = 0; i < lookup->word_length; i++) {
        int c = lookup->compress_table[w[i]]; 

        if (c >= lookup->compressed_alphabet_size) /* "non-20 aa": skip it*/
            return;

        score += info->matrix[w[i]][c];
    }

    /* If the self-score is above the threshold, then the neighboring
       computation will automatically add the word to the lookup table.
       Otherwise, either the score is too low or neighboring is not done at
       all, so that all of these exact matches must be explicitly added to
       the lookup table */

    if (lookup->threshold == 0 || score < lookup->threshold) {
        s_CompressedLookupAddUnencoded(lookup, w, query_offset);
    } 
    else {
#ifdef LOOKUP_VERBOSE
        lookup->neighbor_matches--;
#endif
    }

    /* check if neighboring words need to be found */

    if (lookup->threshold == 0)
        return;

    /* Set up the structure of information to be used during the recursion */

    info->query_word = w;
    info->subject_word = s;
    info->query_offset = query_offset;

    /* compute the largest possible score that any neighboring word can have; 
       this maximum will gradually be replaced by exact scores as subject
       words are built up */

    score = info->row_max[w[0]];
    for (i = 1; i < lookup->word_length; i++)
        score += info->row_max[w[i]];

    s_CompressedAddWordHitsCore(info, score, 0);
}

/**
 * Index a query sequence; i.e. fill a lookup table with the offsets
 * of query words
 *
 * @param lookup The lookup table [in/modified]
 * @param compressed_matrix The substitution matrix [in]
 * @param query The query sequence [in]
 * @param location List of ranges of query offsets to examine 
 *                 for indexing [in]
 */
static void s_CompressedAddNeighboringWords(
                                  BlastCompressedAaLookupTable * lookup,
                                  Int4 ** compressed_matrix,
                                  BLAST_SequenceBlk * query, 
                                  BlastSeqLoc * location)
{
    Int4 i, j;
    CompressedNeighborInfo info;
    BlastSeqLoc *loc;
    Int4 offset;

    ASSERT(lookup->alphabet_size <= BLASTAA_SIZE);

    /* Determine the maximum possible score for each 
       row of the score matrix */

    for (i = 0; i < lookup->alphabet_size; i++) {
        info.row_max[i] = compressed_matrix[i][0];
        for (j = 1; j < lookup->compressed_alphabet_size; j++)
            info.row_max[i] = MAX(info.row_max[i], compressed_matrix[i][j]);
    }

    /* Set up the structure of information to be used during the recursion */
    info.lookup = lookup;
    info.compressed_alphabet_size = lookup->compressed_alphabet_size;
    info.wordsize = lookup->word_length;
    info.matrix = compressed_matrix;
    info.threshold = lookup->threshold;

    s_loadSortedMatrix(&info); 


    /* Walk through the query and index all the words */

    for (loc = location; loc; loc = loc->next){
        Int4 from = loc->ssr->left;
        Int4 to = loc->ssr->right - lookup->word_length + 1;

        for (offset = from; offset <= to; offset++){
            s_CompressedAddWordHits(&info, query->sequence, offset);
        }
    }
}

/** Complete the construction of a compressed protein lookup table
 * @param lookup The lookup table [in][out]
 * @return Always 0
 */
static Int4 s_CompressedLookupFinalize(BlastCompressedAaLookupTable * lookup)
{
    Int4 i;
    Int4 longest_chain = 0;
    Int4 count;
    PV_ARRAY_TYPE *pv;
    Int4 pv_array_bts;
    const Int4 kTargetPVBytes = 262144;
#ifdef LOOKUP_VERBOSE
#define HISTSIZE 30
    Int4 histogram[HISTSIZE] = {0};
    Int4 backbone_occupancy = 0;
    Int4 num_overflows = 0;
#endif
    
    /* count the number of nonempty cells in the backbone */

    for (i = count = 0; i < lookup->backbone_size; i++) {
        if (lookup->backbone[i].num_used)
            count++;
    }

/* fprintf(stderr, "SIZE: %ld %ld\n", (long) count, (long) (0.05 * lookup->backbone_size));
 */

    /* Compress the PV array if it would be large. Compress it
       more if the backbone is sparsely populated. Do not compress
       if the PV array is small enough already or the backbone is
       mostly full */

    pv_array_bts = PV_ARRAY_BTS;
    if (count <= 0.01 * lookup->backbone_size) {
        pv_array_bts += ilog2(lookup->backbone_size / (8 * kTargetPVBytes));
    }

    pv = lookup->pv = (PV_ARRAY_TYPE *)calloc(
                              (lookup->backbone_size >> pv_array_bts) + 1,
                              sizeof(PV_ARRAY_TYPE));
    lookup->pv_array_bts = pv_array_bts;
    ASSERT(pv != NULL);
    
    /* compute the longest chain size and initialize the PV array */

    for (i = 0; i < lookup->backbone_size; i++) {
        count = lookup->backbone[i].num_used;

        if (count > 0) {
            /* set the corresponding bit in the pv_array */
            PV_SET(pv, i, pv_array_bts);
            longest_chain = MAX(count, longest_chain);
            
#ifdef LOOKUP_VERBOSE
            if (count > COMPRESSED_HITS_PER_BACKBONE_CELL) {
                num_overflows++;
            }
            if (count >= HISTSIZE)
                count = HISTSIZE-1;
#endif
        }
        
#ifdef LOOKUP_VERBOSE
        histogram[count]++;
#endif
    }
    
    lookup->longest_chain = longest_chain;
    
#ifdef LOOKUP_VERBOSE
    backbone_occupancy = lookup->backbone_size - histogram[0];

    printf("backbone size: %d\n", lookup->backbone_size);
    printf("backbone occupancy: %d (%f%%)\n", backbone_occupancy,
                 100.0 * backbone_occupancy / lookup->backbone_size); 
    printf("num_overflows: %d\n", num_overflows);
    printf("longest chain: %d\n", longest_chain);
    printf("exact matches: %d\n", lookup->exact_matches);
    printf("neighbor matches: %d\n", lookup->neighbor_matches);
    printf("banks allocated: %d\n", lookup->curr_overflow_bank + 1);
    printf("PV array: %d entries per bit\n", 1 << (lookup->pv_array_bts -
                                                   PV_ARRAY_BTS));
    printf("Lookup table histogram:\n");
    for (i = 0; i < HISTSIZE; i++) {
        printf("%d\t%d\n", i, histogram[i]);
    }
#endif

    return 0;
}

Int4 BlastCompressedAaLookupTableNew(BLAST_SequenceBlk* query,
                                     BlastSeqLoc* locations,
                                     BlastCompressedAaLookupTable * *lut,
                                     const LookupTableOptions * opt,
                                     BlastScoreBlk *sbp)
{
    Int4 i;
    SCompressedAlphabet* new_alphabet;
    const double kMatrixScale = 100.0;
    Int4 word_size = opt->word_size;
    Int4 table_scale;
    BlastCompressedAaLookupTable *lookup = *lut =
              (BlastCompressedAaLookupTable *) calloc(1, 
                                  sizeof(BlastCompressedAaLookupTable));
    
    ASSERT(lookup != NULL);
    ASSERT(word_size == 6 || word_size == 7);

    /* set word size and threshold information. The reciprocals
       below are 2^32 / (compressed alphabet size) */

    lookup->word_length = word_size;
    lookup->threshold = (Int4)(kMatrixScale * opt->threshold);
    lookup->alphabet_size = BLASTAA_SIZE;
    if (word_size == 6) {
        lookup->compressed_alphabet_size = 15;
        lookup->reciprocal_alphabet_size = 286331154;
    }
    else {
        lookup->compressed_alphabet_size = 10;
        lookup->reciprocal_alphabet_size = 429496730;
    }

    /* compute a custom score matrix, for use only
       with lookup table creation. The matrix dimensions
       are BLASTAA_SIZE x compressed_alphabet_size, and
       the score entries are scaled up by kMatrixScale */

    new_alphabet = SCompressedAlphabetNew(sbp,
                                lookup->compressed_alphabet_size,
                                kMatrixScale);
    if (new_alphabet == NULL)
        return -1;

    /* allocate the backbone and overflow array */

    lookup->backbone_size = (Int4)pow(lookup->compressed_alphabet_size,
                                        word_size) + 1;
    lookup->backbone = (CompressedLookupBackboneCell* )calloc(
                                    lookup->backbone_size, 
                                    sizeof(CompressedLookupBackboneCell));
    lookup->overflow_banks = (CompressedOverflowCell **) calloc(
                                         COMPRESSED_OVERFLOW_MAX_BANKS,
                                         sizeof(CompressedOverflowCell *));
    ASSERT(lookup->backbone != NULL);
    ASSERT(lookup->overflow_banks != NULL);
    /* there is no 'current overflow cell' that was previously 
       allocated, so configure the allocator to start allocations
       immediately */
    lookup->curr_overflow_cell = COMPRESSED_OVERFLOW_CELLS_IN_BANK;
    lookup->curr_overflow_bank = -1;

    /* copy the mapping from protein to compressed 
       representation; also save a scaled version of the
       mapping, for use in the scanning phase */

    lookup->compress_table = (Uint1 *)malloc(BLASTAA_SIZE * sizeof(Uint1));
    lookup->scaled_compress_table = (Int4 *)malloc(
                                       BLASTAA_SIZE * sizeof(Int4));
    table_scale = iexp(lookup->compressed_alphabet_size, word_size - 1);
    for (i = 0; i < BLASTAA_SIZE; i++) {
        Uint1 letter = new_alphabet->compress_table[i];
        lookup->compress_table[i] = letter;
        
        if (letter >= lookup->compressed_alphabet_size)
            lookup->scaled_compress_table[i] = -1;
        else
            lookup->scaled_compress_table[i] = table_scale * letter;
    }

    /* index the query and finish up */

    s_CompressedAddNeighboringWords(lookup, new_alphabet->matrix->data, 
                                    query, locations);
    s_CompressedLookupFinalize(lookup);
    SCompressedAlphabetFree(new_alphabet);
    return 0;
}

BlastCompressedAaLookupTable *BlastCompressedAaLookupTableDestruct(
                                 BlastCompressedAaLookupTable * lookup)
{
    Int4 i;

    for (i = 0; i <= lookup->curr_overflow_bank; i++) {
        free(lookup->overflow_banks[i]);
    }

    sfree(lookup->compress_table);
    sfree(lookup->scaled_compress_table);
    sfree(lookup->backbone);
    sfree(lookup->overflow_banks);
    sfree(lookup->pv);
    sfree(lookup);
    return NULL;
}
