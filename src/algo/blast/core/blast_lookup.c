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

/** @file blast_lookup.c
 * Functions that provide generic services for BLAST lookup tables
 */

#include <algo/blast/core/blast_lookup.h>

void BlastLookupAddWordHit(Int4 **backbone, Int4 wordsize,
                           Int4 charsize, Uint1* seq,
                           Int4 query_offset)
{
    Int4 index;
    Int4 *chain = NULL;
    Int4 chain_size = 0;        /* total number of elements in the chain */
    Int4 hits_in_chain = 0;     /* number of occupied elements in the chain,
                                   not including the zeroth and first
                                   positions */

    /* compute the backbone cell to update */

    index = ComputeTableIndex(wordsize, charsize, seq);

    /* if backbone cell is null, initialize a new chain */
    if (backbone[index] == NULL) {
        chain_size = 8;
        hits_in_chain = 0;
        chain = (Int4 *) malloc(chain_size * sizeof(Int4));
        ASSERT(chain != NULL);
        chain[0] = chain_size;
        chain[1] = hits_in_chain;
        backbone[index] = chain;
    } else {
        /* otherwise, use the existing chain */
        chain = backbone[index];
        chain_size = chain[0];
        hits_in_chain = chain[1];
    }

    /* if the chain is full, allocate more room */
    if ((hits_in_chain + 2) == chain_size) {
        chain_size = chain_size * 2;
        chain = (Int4 *) realloc(chain, chain_size * sizeof(Int4));
        ASSERT(chain != NULL);

        backbone[index] = chain;
        chain[0] = chain_size;
    }

    /* add the hit */
    chain[chain[1] + 2] = query_offset;
    chain[1]++;
}

void BlastLookupIndexQueryExactMatches(Int4 **backbone,
                                      Int4 word_length,
                                      Int4 charsize,
                                      Int4 lut_word_length,
                                      BLAST_SequenceBlk * query,
                                      BlastSeqLoc * locations)
{
    BlastSeqLoc *loc;
    Int4 offset;
    Uint1 *seq;
    Uint1 *word_target;
    Uint1 invalid_mask = 0xff << charsize;

    for (loc = locations; loc; loc = loc->next) {
        Int4 from = loc->ssr->left;
        Int4 to = loc->ssr->right;

        /* if this location is too small to fit a complete word, skip the
           location */

        if (word_length > to - from + 1)
            continue;

        /* Indexing proceeds from the start point to the last offset
           such that a full lookup table word can be created. word_target
           points to the letter beyond which indexing is allowed */
        seq = query->sequence + from;
        word_target = seq + lut_word_length;

        for (offset = from; offset <= to; offset++, seq++) {

            if (seq >= word_target) {
                BlastLookupAddWordHit(backbone, 
                                      lut_word_length, charsize,
                                      seq - lut_word_length, 
                                      offset - lut_word_length);
            }

            /* if the current word contains an ambiguity, skip all the
               words that would contain that ambiguity */
            if (*seq & invalid_mask)
                word_target = seq + lut_word_length + 1;
        }

        /* handle the last word, without loading *seq */
        if (seq >= word_target) {
            BlastLookupAddWordHit(backbone, 
                                  lut_word_length, charsize,
                                  seq - lut_word_length, 
                                  offset - lut_word_length);
        }

    }
}


BackboneCell* BackboneCellFree(BackboneCell* cell)
{
    BackboneCell* b = cell;
    while (b) {
        BackboneCell* next = b->next;
        if (b->offsets) {
            free(b->offsets);
        }
        sfree(b);
        b = next;
    }

    return NULL;
}

BackboneCell* BackboneCellNew(Uint4 word, Int4 offset, Int4 size)
{
    BackboneCell* cell = calloc(1, sizeof(BackboneCell));
    if (!cell) {
        BackboneCellFree(cell);
        return NULL;
    }

    cell->offsets = malloc(size * sizeof(Int4));
    if (!cell->offsets) {
        BackboneCellFree(cell);
        return NULL;
    }

    cell->word = word;
    cell->offsets[0] = offset;
    cell->num_offsets = 1;
    cell->allocated = size;

    return cell;
}


static Int2 s_AddWordHit(BackboneCell** backbone, Int4 wordsize,
                         Int4 charsize, Uint1* seq, Int4 offset,
                         TNaLookupHashFunction hash_func, Uint4 mask,
                         PV_ARRAY_TYPE* pv_array)
{
    Uint4 large_index;
    Int8 index;
    Int4 i;
    Int4 num_collisions = 0;

    /* convert a sequence from 4NA to 2NA */
    large_index = 0;
    for (i = 0;i < wordsize;i++) {
        large_index = (large_index << charsize) | seq[i];
    }

    /* if filtering by database word count, then do not add words
       that do not appear in the database or appear to many times */
    if (pv_array && !PV_TEST(pv_array, large_index, PV_ARRAY_BTS)) {
        return 0;
    }

    index = (Int8)hash_func((Uint1*)&large_index, mask);

    /* if the hash table entry is emtpy, create a new cell */
    if (!backbone[index]) {
        Int4 size = 8;
        backbone[index] = BackboneCellNew(large_index, offset, size);
        if (!backbone[index]) {
            return -1;
        }
    }
    else {
        /* otherwiose check if the word was already added */

        BackboneCell* b = backbone[index];
        while (b->next && b->word != large_index) {
            b = b->next;
        }

        /* if word was already added, add the new offset to an existing cell */
        if (b->word == large_index) {
            if (b->num_offsets >= b->allocated) {
                Int4 new_size = b->allocated * 2;
                b->offsets = realloc(b->offsets, new_size * sizeof(Int4));
                if (!b->offsets) {
                    return -1;
                }
                b->allocated = new_size;
            }
            b->offsets[b->num_offsets++] = offset;
        }
        else {
            /* otherwise creare a new cell */

            Int4 size = 8;
            ASSERT(!b->next);
            b->next = BackboneCellNew(large_index, offset, size);
            if (!b->next) {
                return -1;
            }

            num_collisions++;
        }
    }

    return 0;
}

void BlastHashLookupIndexQueryExactMatches(BackboneCell **backbone,
                                           Int4 word_length,
                                           Int4 charsize,
                                           Int4 lut_word_length,
                                           BLAST_SequenceBlk* query,
                                           BlastSeqLoc* locations,
                                           TNaLookupHashFunction hash_func,
                                           Uint4 mask,
                                           PV_ARRAY_TYPE* pv_array)
{
    BlastSeqLoc *loc;
    Int4 offset;
    Uint1 *seq;
    Uint1 *word_target;
    Uint1 invalid_mask = 0xff << charsize;

    for (loc = locations; loc; loc = loc->next) {
        Int4 from = loc->ssr->left;
        Int4 to = loc->ssr->right;

        /* if this location is too small to fit a complete word, skip the
           location */

        if (word_length > to - from + 1)
            continue;

        /* Indexing proceeds from the start point to the last offset
           such that a full lookup table word can be created. word_target
           points to the letter beyond which indexing is allowed */
        seq = query->sequence + from;
        word_target = seq + lut_word_length;

        for (offset = from; offset <= to; offset++, seq++) {

            if (seq >= word_target) {
                s_AddWordHit(backbone,
                             lut_word_length, charsize,
                             seq - lut_word_length,
                             offset - lut_word_length,
                             hash_func, mask,
                             pv_array);
            }

            /* if the current word contains an ambiguity, skip all the
               words that would contain that ambiguity */
            if (*seq & invalid_mask)
                word_target = seq + lut_word_length + 1;
        }

        /* handle the last word, without loading *seq */
        if (seq >= word_target) {
            s_AddWordHit(backbone, 
                         lut_word_length, charsize,
                         seq - lut_word_length, 
                         offset - lut_word_length,
                         hash_func, mask,
                         pv_array);
        }
        
    }
}

