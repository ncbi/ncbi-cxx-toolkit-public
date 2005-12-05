/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
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
 */

/** @file blast_extend.c
 * Functions to initialize structures used for BLAST extension
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_lookup.h>
#include <algo/blast/core/mb_lookup.h>
#include <algo/blast/core/blast_util.h> /* for NCBI2NA_UNPACK_BASE macros */
#include "blast_inline.h"
#include "blast_extend_priv.h"

/** Minimal size of an array of initial word hits, allocated up front. */
#define MIN_INIT_HITLIST_SIZE 100

/* Description in blast_extend.h */
BlastInitHitList* BLAST_InitHitListNew(void)
{
   BlastInitHitList* init_hitlist = (BlastInitHitList*)
      calloc(1, sizeof(BlastInitHitList));

   init_hitlist->allocated = MIN_INIT_HITLIST_SIZE;

   init_hitlist->init_hsp_array = (BlastInitHSP*)
      malloc(MIN_INIT_HITLIST_SIZE*sizeof(BlastInitHSP));

   return init_hitlist;
}

void BlastInitHitListReset(BlastInitHitList* init_hitlist)
{
   Int4 index;

   for (index = 0; index < init_hitlist->total; ++index)
      sfree(init_hitlist->init_hsp_array[index].ungapped_data);
   init_hitlist->total = 0;
}

BlastInitHitList* BLAST_InitHitListFree(BlastInitHitList* init_hitlist)
{
   if (init_hitlist == NULL)
       return NULL;

   BlastInitHitListReset(init_hitlist);
   sfree(init_hitlist->init_hsp_array);
   sfree(init_hitlist);
   return NULL;
}

/** Callback for sorting an array of initial HSP structures (not pointers to
 * structures!) by score. 
 */
static int
score_compare_match(const void* v1, const void* v2)

{
    BlastInitHSP* h1,* h2;
    int result = 0;

    h1 = (BlastInitHSP*) v1;
    h2 = (BlastInitHSP*) v2;

    /* Check if ungapped_data substructures are initialized. If not, move
       those array elements to the end. In reality this should never happen.*/
    if (h1->ungapped_data == NULL && h2->ungapped_data == NULL)
        return 0;
    else if (h1->ungapped_data == NULL)
        return 1;
    else if (h2->ungapped_data == NULL)
        return -1;

    if (0 == (result = BLAST_CMP(h2->ungapped_data->score,
                                 h1->ungapped_data->score)) &&
        0 == (result = BLAST_CMP(h1->ungapped_data->s_start,
                                 h2->ungapped_data->s_start)) &&
        0 == (result = BLAST_CMP(h2->ungapped_data->length,
                                 h1->ungapped_data->length)) &&
        0 == (result = BLAST_CMP(h1->ungapped_data->q_start,
                                 h2->ungapped_data->q_start))) {
        result = BLAST_CMP(h2->ungapped_data->length,
                           h1->ungapped_data->length);
    }

    return result;
}

void Blast_InitHitListSortByScore(BlastInitHitList* init_hitlist) 
{
    qsort(init_hitlist->init_hsp_array, init_hitlist->total,
          sizeof(BlastInitHSP), score_compare_match);
}

Boolean Blast_InitHitListIsSortedByScore(BlastInitHitList* init_hitlist)
{
    Int4 index;
    BlastInitHSP* init_hsp_array = init_hitlist->init_hsp_array;

    for (index = 0; index < init_hitlist->total - 1; ++index) {
        if (score_compare_match(&init_hsp_array[index],
                                &init_hsp_array[index+1]) > 0)
            return FALSE;
    }
    return TRUE;
}

BLAST_DiagTable*
BlastDiagTableNew (Int4 qlen, Boolean multiple_hits, Int4 window_size)

{
        BLAST_DiagTable* diag_table;
        Int4 diag_array_length;

        diag_table= (BLAST_DiagTable*) calloc(1, sizeof(BLAST_DiagTable));

        if (diag_table)
        {
                diag_array_length = 1;
                /* What power of 2 is just longer than the query? */
                while (diag_array_length < (qlen+window_size))
                {
                        diag_array_length = diag_array_length << 1;
                }
                /* These are used in the word finders to shift and mask
                rather than dividing and taking the remainder. */
                diag_table->diag_array_length = diag_array_length;
                diag_table->diag_mask = diag_array_length-1;
                diag_table->multiple_hits = multiple_hits;
                diag_table->offset = window_size;
                diag_table->window = window_size;
        }
        return diag_table;
}

/* Description in blast_extend.h */
Int2 BlastExtendWordNew(const LookupTableWrap* lookup_wrap, Uint4 query_length,
                        const BlastInitialWordParameters* word_params,
                        Uint4 subject_length, Blast_ExtendWord** ewp_ptr)
{
   Blast_ExtendWord* ewp;
   Int4 index;
   Boolean kDiscMb = (lookup_wrap->lut_type == MB_LOOKUP_TABLE && 
                      ((BlastMBLookupTable*)lookup_wrap->lut)->discontiguous);

   *ewp_ptr = ewp = (Blast_ExtendWord*) calloc(1, sizeof(Blast_ExtendWord));

   if (!ewp) {
      return -1;
   }

   if (word_params->container_type == eWordStacks) {
      double search_space;
      Int4 stack_size, num_stacks;
      BLAST_StackTable* stack_table;

      ewp->stack_table = stack_table = 
         (BLAST_StackTable*) calloc(1, sizeof(BLAST_StackTable));

      search_space = 
         ((double) query_length) * subject_length;
      num_stacks = MIN(1 + (Int4) (sqrt(search_space)/100), 500);
      stack_size = 5000/num_stacks;
      stack_table->stack_index = (Int4*) calloc(num_stacks, sizeof(Int4));
      stack_table->stack_size = (Int4*) malloc(num_stacks*sizeof(Int4));

      stack_table->bn_stack_array = 
            (BlastnStack**) malloc(num_stacks*sizeof(BlastnStack*));
      for (index=0; index<num_stacks; index++) {
         stack_table->bn_stack_array[index] = 
            (BlastnStack*) malloc(stack_size*sizeof(BlastnStack));
         stack_table->stack_size[index] = stack_size;
      }
      stack_table->num_stacks = num_stacks;
   } else /* container_type == eDiagArray */ {
      Boolean multiple_hits = (word_params->options->window_size > 0);
      BLAST_DiagTable* diag_table;
      const Boolean kIsNa = (lookup_wrap->lut_type == MB_LOOKUP_TABLE || 
                             lookup_wrap->lut_type == NA_LOOKUP_TABLE);

      ewp->diag_table = diag_table = 
         BlastDiagTableNew(query_length, multiple_hits, 
                            word_params->options->window_size);
      /* Allocate the buffer to be used for diagonal array. */
      if (!kIsNa || 
          (word_params->extension_method == eUpdateDiag && !kDiscMb)) {
         diag_table->hit_level_array = (DiagStruct*)
            calloc(diag_table->diag_array_length, sizeof(DiagStruct));
         if (!diag_table->hit_level_array)	{
            sfree(ewp);
            return -1;
         }
      } else {
         diag_table->last_hit_array = 
            (Uint4*) calloc(diag_table->diag_array_length, sizeof(Uint4));
         if (!diag_table->last_hit_array)	{
            sfree(ewp);
            return -1;
         }
      }
   }
   *ewp_ptr = ewp;
   
   return 0;
}

Boolean BLAST_SaveInitialHit(BlastInitHitList* init_hitlist, 
                             Int4 q_off, Int4 s_off, 
                             BlastUngappedData* ungapped_data) 
{
   BlastInitHSP* match_array;
   Int4 num, num_avail;

   num = init_hitlist->total;
   num_avail = init_hitlist->allocated;

   match_array = init_hitlist->init_hsp_array;
   if (num>=num_avail) {
      if (init_hitlist->do_not_reallocate) 
         return FALSE;
      num_avail *= 2;
      match_array = (BlastInitHSP*) 
         realloc(match_array, num_avail*sizeof(BlastInitHSP));
      if (!match_array) {
         init_hitlist->do_not_reallocate = TRUE;
         return FALSE;
      } else {
         init_hitlist->allocated = num_avail;
         init_hitlist->init_hsp_array = match_array;
      }
   }

   match_array[num].offsets.qs_offsets.q_off = q_off;
   match_array[num].offsets.qs_offsets.s_off = s_off;
   match_array[num].ungapped_data = ungapped_data;

   init_hitlist->total++;
   return TRUE;
}

/** Perform ungapped extension of a word hit
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param matrix The scoring matrix [in]
 * @param q_off The offset of a word in query [in]
 * @param s_off The offset of a word in subject [in]
 * @param X The drop-off parameter for the ungapped extension [in]
 * @param ungapped_data The ungapped extension information [out]
 * @return Status: 0 on success, -1 if memory allocation failed.
 */
static Int2
s_NuclUngappedExtend(BLAST_SequenceBlk* query, 
   BLAST_SequenceBlk* subject, Int4** matrix, 
   Int4 q_off, Int4 s_off, Int4 X, 
   BlastUngappedData* ungapped_data)
{
   Uint1* q;
   Int4 sum, score;
   Uint1 ch;
   Uint1* subject0,* sf,* q_beg,* q_end,* s_end,* s,* start;
   Int2 remainder, base;
   Int4 q_avail, s_avail;
   
   base = 3 - (s_off % 4);
   
   subject0 = subject->sequence;
   q_avail = query->length - q_off;
   s_avail = subject->length - s_off;

   q = q_beg = q_end = query->sequence + q_off;
   s = s_end = subject0 + s_off/COMPRESSION_RATIO;
   if (q_off < s_off) {
      start = subject0 + (s_off-q_off)/COMPRESSION_RATIO;
      remainder = 3 - ((s_off-q_off)%COMPRESSION_RATIO);
   } else {
      start = subject0;
      remainder = 3;
   }
   
   /* Find where positive scoring starts & ends within the word hit */
   score = 0;
   sum = 0;

   /* extend to the left */
   while ((s > start) || (s == start && base < remainder)) {
      if (base == 3) {
         s--;
         base = 0;
      } else {
         ++base;
      }
      ch = *s;
      if ((sum += matrix[*--q][NCBI2NA_UNPACK_BASE(ch, base)]) > 0) {
         q_beg = q;
         score += sum;
         sum = 0;
      } else if (sum < X) {
         break;
      }
   }
   
   ungapped_data->q_start = q_beg - query->sequence;
   ungapped_data->s_start = s_off - (q_off - ungapped_data->q_start);

   if (q_avail < s_avail) {
      sf = subject0 + (s_off + q_avail)/COMPRESSION_RATIO;
      remainder = 3 - ((s_off + q_avail)%COMPRESSION_RATIO);
   } else {
      sf = subject0 + (subject->length)/COMPRESSION_RATIO;
      remainder = 3 - ((subject->length)%COMPRESSION_RATIO);
   }
	
   /* extend to the right */
   q = q_end;
   s = s_end;
   sum = 0;
   base = 3 - (s_off % COMPRESSION_RATIO);
   
   while (s < sf || (s == sf && base > remainder)) {
      ch = *s;
      if ((sum += matrix[*q++][NCBI2NA_UNPACK_BASE(ch, base)]) > 0) {
         q_end = q;
         score += sum;
         sum = 0;
      } else if (sum < X)
         break;
      if (base == 0) {
         base = 3;
         s++;
      } else
         base--;
   }
   
   ungapped_data->length = q_end - q_beg;
   ungapped_data->score = score;
   
   return 0;
}

/** Mask for encoding in one integer a hit offset and whether that hit has 
 * already been extended and saved. The latter is put in the top bit of the
 * integer.
 */
#define LAST_HIT_MASK 0x7fffffff

/** Perform ungapped extension given an offset pair, and save the initial 
 * hit information if the hit qualifies. This function assumes that the
 * exact match has already been extended to the word size parameter. It also
 * supports a two-hit version, where extension is performed only after two hits
 * are detected within a window.
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param min_step Distance at which new word hit lies within the previously
 *                 extended hit. Non-zero only when ungapped extension is not
 *                 performed, e.g. for contiguous megablast. [in]
 * @param template_length specifies overlap of two words in two-hit model [in]
 * @param word_params The parameters related to initial word extension [in]
 * @param matrix the substitution matrix for ungapped extension [in]
 * @param diag_table Structure containing diagonal table with word extension 
 *                   information [in]
 * @param q_off The offset in the query sequence [in]
 * @param s_end The offset in the subject sequence where this hit ends [in]
 * @param s_off The offset in the subject sequence [in]
 * @param init_hitlist The structure containing information about all 
 *                     initial hits [in] [out]
 * @return Has this hit been extended? 
 */
static Boolean
s_BlastnDiagExtendInitialHit(BLAST_SequenceBlk* query, 
   BLAST_SequenceBlk* subject, Uint4 min_step,
   Uint1 template_length,
   const BlastInitialWordParameters* word_params, 
   Int4** matrix, BLAST_DiagTable* diag_table, Int4 q_off, Int4 s_end,
   Int4 s_off, BlastInitHitList* init_hitlist)
{
   Int4 diag, real_diag;
   Int4 s_pos;
   BlastUngappedData* ungapped_data;
   BlastUngappedData dummy_ungapped_data;
   Int4 window_size = word_params->options->window_size;
   Boolean hit_ready;
   Boolean new_hit = FALSE, second_hit = FALSE;
   Int4 step;
   Uint4 last_hit;
   Uint4 hit_saved;
   Uint4* last_hit_array;

   last_hit_array = diag_table->last_hit_array;
   ASSERT(last_hit_array);

   diag = s_off + diag_table->diag_array_length - q_off;
   real_diag = diag & diag_table->diag_mask;
   last_hit = last_hit_array[real_diag] & LAST_HIT_MASK;
   hit_saved = last_hit_array[real_diag] & ~LAST_HIT_MASK;
   s_pos = s_end + diag_table->offset;
   step = s_pos - last_hit;

   if (step <= 0)
      /* This is a hit on a diagonal that has already been explored 
         further down */
      return 0;

   if (window_size == 0 || hit_saved) {
      /* Single hit version or previous hit was already a second hit */
      new_hit = (step > (Int4)min_step);
   } else {
      /* Previous hit was the first hit */
      new_hit = (step > window_size);
      second_hit = (step >= template_length && step < window_size);
   }

   hit_ready = ((window_size == 0) && new_hit) || second_hit;

   if (hit_ready) {
      if (word_params->options->ungapped_extension) {
         /* Perform ungapped extension */
         ungapped_data = &dummy_ungapped_data;
         s_NuclUngappedExtend(query, subject, matrix, q_off, s_off, 
                              -word_params->x_dropoff, ungapped_data);
      
         last_hit = ungapped_data->length + ungapped_data->s_start 
            + diag_table->offset;
      } else {
         ungapped_data = NULL;
         last_hit = s_pos;
      }
      if (ungapped_data == NULL) {
         BLAST_SaveInitialHit(init_hitlist, q_off, s_off, ungapped_data);
         /* Set the "saved" flag for this hit */
         hit_saved = ~LAST_HIT_MASK;
      } else if (ungapped_data->score >= word_params->cutoff_score) {
         BlastUngappedData *final_data = (BlastUngappedData *)malloc(
                                                 sizeof(BlastUngappedData));
         *final_data = *ungapped_data;
         BLAST_SaveInitialHit(init_hitlist, q_off, s_off, final_data);
         /* Set the "saved" flag for this hit */
         hit_saved = ~LAST_HIT_MASK;
      } else {
         /* Unset the "saved" flag for this hit */
         hit_saved = 0;
      }
   } else {
      /* First hit in the 2-hit case or a direct extension of the previous 
         hit - update the last hit information only */
      if (step > window_size)
         last_hit = s_pos;
      if (new_hit)
         hit_saved = 0;
   }

   last_hit_array[real_diag] = last_hit | hit_saved;

   return hit_ready;
}

/** Discontiguous megablast initial word extension using diagonal array.
 * @param query Query sequence block [in]
 * @param subject Subject sequence block [in]
 * @param lookup Lookup table [in]
 * @param word_params Parameters for finding and extending initial words [in]
 * @param matrix Matrix for ungapped extension [in]
 * @param diag_table Structure containing diagonal array [in]
 * @param num_hits Number of lookup table hits [in]
 * @param offset_pairs Array of query and subject offsets of lookup table 
 *                     hits [in]
 * @param init_hitlist Structure where to save hits when they are 
 *                     ready [in] [out]
 * @return Number of hits extended.
 */
static Int4
s_DiscMbDiagExtendInitialHits(BLAST_SequenceBlk* query, 
    BLAST_SequenceBlk* subject, LookupTableWrap* lookup,
    const BlastInitialWordParameters* word_params, 
    Int4** matrix, BLAST_DiagTable* diag_table, Int4 num_hits,
    const BlastOffsetPair* offset_pairs, BlastInitHitList* init_hitlist)
{
   BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup->lut;
   Int4 index;
   Int4 hits_extended = 0;

   for (index = 0; index < num_hits; ++index) {
       if (s_BlastnDiagExtendInitialHit(query, subject, mb_lt->scan_step, mb_lt->template_length,
               word_params, matrix, diag_table, offset_pairs[index].qs_offsets.q_off, 
               offset_pairs[index].qs_offsets.s_off, offset_pairs[index].qs_offsets.s_off, 
               init_hitlist))
           ++hits_extended;
   }

   return hits_extended;

}

/** Perform ungapped extension given an offset pair, and save the initial 
 * hit information if the hit qualifies. This function assumes that the
 * exact match has already been extended to the word size parameter.
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param min_step Distance at which new word hit lies within the previously
 *                 extended hit. Non-zero only when ungapped extension is not
 *                 performed, e.g. for contiguous megablast. [in]
 * @param template_length specifies overlap of two words in two-hit model [in]
 * @param word_params The parameters related to initial word extension [in]
 * @param matrix the substitution matrix for ungapped extension [in]
 * @param stack_table Structure containing stacks of initial hits [in] [out]
 * @param q_off The offset in the query sequence [in]
 * @param s_end The offset in the subject sequence where this hit ends [in]
 * @param s_off The offset in the subject sequence [in]
 * @param init_hitlist The structure containing information about all 
 *                     initial hits [in] [out]
 * @return Has this hit been extended? 
 */
static Boolean
s_BlastnStacksExtendInitialHit(BLAST_SequenceBlk* query, 
   BLAST_SequenceBlk* subject, Uint4 min_step,
   Uint1 template_length,
   const BlastInitialWordParameters* word_params, 
   Int4** matrix, BLAST_StackTable* stack_table, Int4 q_off, Int4 s_end,
   Int4 s_off, BlastInitHitList* init_hitlist)
{
   Int4 index, diag;
   BlastnStack* stack;
   Int4 stack_top;
   Int4 window_size;
   Boolean hit_ready = FALSE, two_hits;
   BlastUngappedData dummy_ungapped_data;
   BlastUngappedData* ungapped_data = NULL;

   window_size = word_params->options->window_size;

   two_hits = (window_size > 0);

   /* Find the stack index */
   diag = (s_off - q_off) % stack_table->num_stacks;
   if (diag<0)
      diag += stack_table->num_stacks;
   stack = stack_table->bn_stack_array[diag];
   stack_top = stack_table->stack_index[diag] - 1;
   
   /* Find previous hit on this diagonal in the stack entries */
   for (index = 0; index <= stack_top; ) {
      Int4 last_hit = stack[index].level & LAST_HIT_MASK;
      Int4 step = s_end - last_hit;
      Boolean new_hit = FALSE;
      if (stack[index].diag == s_off - q_off) {
         Int4 hit_saved = stack[index].level & ~LAST_HIT_MASK;
         Boolean second_hit = FALSE;

         if (step <= 0)
            /* This is a hit on a diagonal that has already been explored 
               further down */
            return 0;
         if (!two_hits || hit_saved) {
            /* Single hit version or previous hit was already a second hit */
            new_hit = (step > (Int4)min_step);
         } else {
            /* Previous hit was the first hit */
            new_hit = (step > window_size);
            second_hit = (step >= template_length && step < window_size);
         }

         hit_ready = (!two_hits && new_hit) || second_hit;

         if (hit_ready) {
            if (word_params->options->ungapped_extension) {
               /* Perform ungapped extension */
               ungapped_data = &dummy_ungapped_data;
               s_NuclUngappedExtend(query, subject, matrix, q_off, s_off, 
                                    -word_params->x_dropoff, ungapped_data);
      
               last_hit = ungapped_data->length + ungapped_data->s_start;
            } else {
               ungapped_data = NULL;
               last_hit = s_end;
            }
            if (ungapped_data == NULL) {
               BLAST_SaveInitialHit(init_hitlist, q_off, s_off, ungapped_data);
               /* Set the "saved" flag for this hit */
               hit_saved = ~LAST_HIT_MASK;
            } else if (ungapped_data->score >= word_params->cutoff_score) {
               BlastUngappedData *final_data = (BlastUngappedData *)malloc(
                                                 sizeof(BlastUngappedData));
               *final_data = *ungapped_data;
               BLAST_SaveInitialHit(init_hitlist, q_off, s_off, final_data);
               /* Set the "saved" flag for this hit */
               hit_saved = ~LAST_HIT_MASK;
            } else {
               /* Unset the "saved" flag for this hit */
               hit_saved = 0;
            }
         } else {
            /* First hit in the 2-hit case or a direct extension of the previous 
               hit - update the last hit information only */
            if (step > window_size)
               last_hit = s_end;
            if (new_hit)
               hit_saved = 0;
         }
         stack[index].level = last_hit | hit_saved;
         return hit_ready;
      } else if (step <= (Int4)min_step || (two_hits && step <= window_size)) {
         /* Hit from a different diagonal, and it can be continued */
         index++;
      } else {
         /* Hit from a different diagonal that does not continue: remove
            it from the stack */
         stack[index] = stack[stack_top];
         --stack_top;
      } /* End of processing stack entry */
   } /* End of loop over stack entries */

   /* If we got to this place in the code, it means no previous hit was found 
      in the stack. Hence we need an extra slot on the stack for this hit */
   if (++stack_top >= stack_table->stack_size[diag]) {
      /* Stack about to overflow - reallocate memory */
      BlastnStack* ptr;
      if (!(ptr = (BlastnStack*) 
            realloc(stack, 
                    2*stack_table->stack_size[diag]*sizeof(BlastnStack)))) {
         return FALSE;
      } else {
         stack_table->stack_size[diag] *= 2;
         stack = stack_table->bn_stack_array[diag] = ptr;
      }
   }
   /* Start a new hit */
   stack[stack_top].diag = s_off - q_off;
   stack[stack_top].level = s_end;
   stack_table->stack_index[diag] = stack_top + 1;
   /* Save the hit if it already qualifies */
   if (!two_hits) {
      hit_ready = TRUE;
      if (word_params->options->ungapped_extension) {
         /* Perform ungapped extension */
         ungapped_data = &dummy_ungapped_data;
         s_NuclUngappedExtend(query, subject, matrix, q_off, s_off, 
                              -word_params->x_dropoff, ungapped_data);
         stack[stack_top].level = 
            (ungapped_data->length + ungapped_data->s_start);
      } else {
         ungapped_data = NULL;
      }
      if (ungapped_data == NULL) {
         BLAST_SaveInitialHit(init_hitlist, q_off, s_off, ungapped_data);
         stack[stack_top].level |= ~LAST_HIT_MASK;
      } else if (ungapped_data->score >= word_params->cutoff_score) {
         BlastUngappedData *final_data = (BlastUngappedData *)malloc(
                                                 sizeof(BlastUngappedData));
         *final_data = *ungapped_data;
         BLAST_SaveInitialHit(init_hitlist, q_off, s_off, final_data);
         stack[stack_top].level |= ~LAST_HIT_MASK;
      } else {
         /* Set hit length back to 0 after ungapped extension 
            failure */
         stack[stack_top].level &= LAST_HIT_MASK;
      }
   }

   return hit_ready;
}

/** Discontiguous megablast initial word extension using stacks.
 * Finds previous hit on this hit's diagonal among the hits saved in the 
 * stacks and updates that hit information, or adds new hit to the stack.
 * Saves hit if it has reached the word size and/or it is a second
 * hit in case of multiple-hit option.
 * @param query Query sequence block [in]
 * @param subject Subject sequence block [in]
 * @param lookup Lookup table [in]
 * @param word_params Parameters for finding and extending initial words [in]
 * @param matrix Matrix for ungapped extension [in]
 * @param stack_table Structure containing stacks with word hits [in]
 * @param num_hits Number of lookup table hits [in]
 * @param offset_pairs Array of query and subject offsets of lookup table 
 *                     hits [in]
 * @param init_hitlist Structure where to save hits when they are 
 *                     ready [in] [out]
 * @return Number of hits extended.
 */
static Int4
s_DiscMbStacksExtendInitialHits(BLAST_SequenceBlk* query, 
   BLAST_SequenceBlk* subject, LookupTableWrap* lookup,
   const BlastInitialWordParameters* word_params, 
   Int4** matrix, BLAST_StackTable* stack_table, Int4 num_hits,
   const BlastOffsetPair* offset_pairs, BlastInitHitList* init_hitlist)
{
   BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup->lut;
   Int4 index;
   Int4 hits_extended = 0;

   for (index = 0; index < num_hits; ++index) {
       if (s_BlastnStacksExtendInitialHit(query, subject, mb_lt->scan_step, mb_lt->template_length,
               word_params, matrix, stack_table, offset_pairs[index].qs_offsets.q_off, 
               offset_pairs[index].qs_offsets.s_off, offset_pairs[index].qs_offsets.s_off, 
               init_hitlist))
           ++hits_extended;
   }

   return hits_extended;
}

Boolean
DiscMB_ExtendInitialHits(const BlastOffsetPair* offset_pairs,
                         Int4 num_hits, BLAST_SequenceBlk* query, 
                         BLAST_SequenceBlk* subject, LookupTableWrap* lookup,
                         const BlastInitialWordParameters* word_params, 
                         Int4** matrix, Blast_ExtendWord* ewp, 
                         BlastInitHitList* init_hitlist) 
{
   ASSERT(lookup->lut_type == MB_LOOKUP_TABLE &&
         ((BlastMBLookupTable*)lookup->lut)->discontiguous);

   if (word_params->container_type == eDiagArray) {
       return s_DiscMbDiagExtendInitialHits(query, subject, lookup, 
                                            word_params, matrix, 
                                            ewp->diag_table, num_hits,
                                            offset_pairs, init_hitlist);
   } else if (word_params->container_type == eWordStacks) {
       return s_DiscMbStacksExtendInitialHits(query, subject, lookup, 
                                              word_params, matrix, 
                                              ewp->stack_table, num_hits,
                                              offset_pairs, init_hitlist);
   }

   return FALSE;
}

/** Update the word extension structure after scanning of each subject sequence
 * @param ewp The structure holding word extension information [in] [out]
 * @param subject_length The length of the subject sequence that has just been
 *        processed [in]
 */
static Int2 
s_BlastNaExtendWordExit(Blast_ExtendWord* ewp, Int4 subject_length)
{
   if (!ewp)
      return -1;

   if (ewp->diag_table) {
      BLAST_DiagTable* diag_table;
      Int4 diag_array_length;

      diag_table = ewp->diag_table;
      
      if (diag_table->offset >= INT4_MAX/2) {
         diag_array_length = diag_table->diag_array_length;
         if (diag_table->hit_level_array) {
            memset(diag_table->hit_level_array, 0, 
                   diag_array_length*sizeof(DiagStruct));
         } else if (diag_table->last_hit_array) {
            memset(diag_table->last_hit_array, 0, 
                   diag_array_length*sizeof(Uint4));
         }
      }
      
      if (diag_table->offset < INT4_MAX/2)	{
         diag_table->offset += subject_length + diag_table->window;
      } else {
         diag_table->offset = diag_table->window;
      }
   } else if (ewp->stack_table) { 
      memset(ewp->stack_table->stack_index, 0, 
             ewp->stack_table->num_stacks*sizeof(Int4));
   }

   return 0;
}

Int4
BlastNaExtendRight(const BlastOffsetPair* offset_pairs, Int4 num_hits, 
                   const BlastInitialWordParameters* word_params,
                   LookupTableWrap* lookup_wrap,
                   BLAST_SequenceBlk* query, BLAST_SequenceBlk* subject,
                   Int4** matrix, Blast_ExtendWord* ewp, 
                   BlastInitHitList* init_hitlist)
{
   Uint1* s_start = subject->sequence;
   Uint1* q_start = query->sequence;
   Int4 i;
   Uint1* s;
   Uint1* q;
   Uint4 word_length, compressed_wordsize, compressed_word_length;
   Uint4 reduced_word_length, reduced_wordsize;
   Uint4 extra_bytes_needed;
   Uint4 extra_bases, left, right;
   Int4 hits_extended = 0;
   Boolean extend_partial_byte = FALSE;
   Uint4 min_step = 0;
   Uint1 template_length = 0;

   if (lookup_wrap->lut_type == MB_LOOKUP_TABLE) {
      BlastMBLookupTable* mb_lt = (BlastMBLookupTable*)lookup_wrap->lut;
      word_length = mb_lt->word_length;
      reduced_wordsize = 
         (word_length - COMPRESSION_RATIO + 1)/COMPRESSION_RATIO;
      compressed_wordsize = mb_lt->compressed_wordsize;
      extend_partial_byte = !mb_lt->variable_wordsize;
      /* When ungapped extension is not performed, the hit will be new only if
         it more than scan_step away from the previous hit. */
      if(!word_params->options->ungapped_extension)
         min_step = mb_lt->scan_step;
      template_length = mb_lt->template_length;
   } else {
      BlastLookupTable* lookup = (BlastLookupTable*)lookup_wrap->lut;
      word_length = lookup->word_length;
      reduced_wordsize = lookup->wordsize;
      compressed_wordsize = lookup->reduced_wordsize;
      extend_partial_byte = !lookup->variable_wordsize;
      /* When ungapped extension is not performed, the hit will be new only if
         it more than scan_step away from the previous hit. */
      if(!word_params->options->ungapped_extension)
         min_step = lookup->scan_step;
   }

   extra_bytes_needed = reduced_wordsize - compressed_wordsize;

   if (!extend_partial_byte) {
      /* If partial bytes are not checked, we need to check one full extra byte
         at the end of the word. */
      ++extra_bytes_needed;
   }

   reduced_word_length = COMPRESSION_RATIO*reduced_wordsize;
   compressed_word_length = COMPRESSION_RATIO*compressed_wordsize;
   extra_bases = word_length - reduced_word_length;

   for (i = 0; i < num_hits; ++i) {
      Uint4 q_offset = offset_pairs[i].qs_offsets.q_off;
      Uint4 s_offset = offset_pairs[i].qs_offsets.s_off;
      /* Here it is guaranteed that subject offset is divisible by 4,
         because we only extend to the right, so scanning stride must be
         equal to 4. */
      s = s_start + s_offset/COMPRESSION_RATIO;
      q = q_start + q_offset;
      
      /* Check for extra bytes if required for longer words. */
      if (extra_bytes_needed && 
          !BlastNaCompareExtraBytes(q+compressed_word_length, 
              s+compressed_wordsize, extra_bytes_needed))
         continue;

      if (extend_partial_byte) {
         /* mini extension to the left */
         Uint1 max_bases = MIN(COMPRESSION_RATIO, MIN(q_offset, s_offset));
         left = BlastNaMiniExtendLeft(q, s-1, max_bases);
         
         /* mini extension to the right */
         max_bases =
            MIN(COMPRESSION_RATIO, 
                MIN(subject->length - s_offset - reduced_word_length,
                    query->length - q_offset - reduced_word_length));
         
         right = 0;
         if (max_bases > 0) {
            s += reduced_wordsize;
            q += reduced_word_length;
            right = BlastNaMiniExtendRight(q, s, max_bases);
         }
      } else {
         left = 0;
         right = COMPRESSION_RATIO;
      }

      if (left + right >= extra_bases) {
         Boolean hit_ready = FALSE;
         /* Check if this diagonal has already been explored. */
         if (word_params->container_type == eWordStacks) {
            hit_ready = 
               s_BlastnStacksExtendInitialHit(query, subject, min_step, template_length,
                  word_params, matrix, ewp->stack_table, q_offset, 
                  s_offset + reduced_word_length + right, 
                  s_offset, init_hitlist);
         } else {
            hit_ready = 
               s_BlastnDiagExtendInitialHit(query, subject, min_step, template_length,
                  word_params, matrix, ewp->diag_table, q_offset, 
                  s_offset + reduced_word_length + right, 
                  s_offset, init_hitlist);
         }
         if (hit_ready)
            ++hits_extended;
      }
   }
   return hits_extended;
}

/* Description in blast_extend.h */
Int2 BlastNaWordFinder(BLAST_SequenceBlk* subject, 
		       BLAST_SequenceBlk* query,
		       LookupTableWrap* lookup_wrap,
		       Int4** matrix,
		       const BlastInitialWordParameters* word_params,
		       Blast_ExtendWord* ewp,
                       BlastOffsetPair* offset_pairs,
		       Int4 max_hits,
		       BlastInitHitList* init_hitlist, 
             BlastUngappedStats* ungapped_stats)
{
   BlastLookupTable* lookup = (BlastLookupTable*) lookup_wrap->lut;
   Int4 hitsfound, total_hits = 0;
   Int4 hits_extended = 0;
   Int4 start_offset, last_start, next_start;

   last_start = subject->length - COMPRESSION_RATIO*lookup->wordsize;
   start_offset = 0;

   while (start_offset <= last_start) {
      /* Pass the last word ending offset */
      next_start = last_start;
      hitsfound = BlastNaScanSubject(lookup_wrap, subject, start_offset, 
                     offset_pairs, max_hits, &next_start); 
      
      total_hits += hitsfound;

      hits_extended += 
         BlastNaExtendRight(offset_pairs, hitsfound, word_params,
                            lookup_wrap, query, subject, matrix, ewp, 
                            init_hitlist);

      start_offset = next_start;
   }

   s_BlastNaExtendWordExit(ewp, subject->length);

   Blast_UngappedStatsUpdate(ungapped_stats, total_hits, hits_extended, 
                             init_hitlist->total);

   if (word_params->options->ungapped_extension)
       Blast_InitHitListSortByScore(init_hitlist);

   return 0;
} 

/** Extend an exact match in both directions up to the provided 
 * maximal length. 
 * @param q_start Pointer to the start of the extension in query [in]
 * @param s_start Pointer to the start of the extension in subject [in]
 * @param max_bases_left At most how many bases to extend to the left [in]
 * @param max_bases_right At most how many bases to extend to the right [in]
 * @param max_length The length of the required exact match [in]
 * @param extend_partial_byte Should partial byte extension be perfomed?[in]
 * @param extended_right How far has extension succeeded to the right? [out]
 * @return TRUE if extension successful 
 */
static Boolean 
s_BlastNaExactMatchExtend(Uint1* q_start, Uint1* s_start, 
   Uint4 max_bases_left, Uint4 max_bases_right, Uint4 max_length, 
   Boolean extend_partial_byte, Uint4* extended_right)
{
   Uint4 length;
   Uint1* q,* s;
   
   *extended_right = 0;

   length = 0;

   /* Extend to the right; start from the firstt byte (it must be the 
      first one that's guaranteed to match by the lookup table hit). */

   q = q_start;
   s = s_start;
   while (length < max_length && max_bases_right >= COMPRESSION_RATIO) {
      if (*s != PACK_WORD(q))
         break;
      length += COMPRESSION_RATIO;
      ++s;
      q += COMPRESSION_RATIO;
      max_bases_right -= COMPRESSION_RATIO;
   }
   if (extend_partial_byte) {
      if (max_bases_right > 0) {
         length += BlastNaMiniExtendRight(q, s, 
                      (Uint1) MIN(max_bases_right, COMPRESSION_RATIO));
      }
   }

   *extended_right = length;

   if (length >= max_length)
      return TRUE;

   if (max_bases_left < max_length - length)
      return FALSE;
   else
      max_bases_left = max_length - length;

   /* Extend to the left; start with the byte just before the first. */
   q = q_start - COMPRESSION_RATIO;
   s = s_start - 1;
   while (length < max_length && max_bases_left >= COMPRESSION_RATIO) {
      if (*s != PACK_WORD(q))
         break;
      length += COMPRESSION_RATIO;
      --s;
      q -= COMPRESSION_RATIO;
      max_bases_left -= COMPRESSION_RATIO;
   }
   if (length >= max_length)
      return TRUE;
   if (extend_partial_byte && max_bases_left > 0) {
      length += BlastNaMiniExtendLeft(q+COMPRESSION_RATIO, s, 
                   (Uint1) MIN(max_bases_left, COMPRESSION_RATIO));
   }

   return (length >= max_length);
}

Int4 
BlastNaExtendRightAndLeft(const BlastOffsetPair* offset_pairs, Int4 num_hits, 
                          const BlastInitialWordParameters* word_params,
                          LookupTableWrap* lookup_wrap,
                          BLAST_SequenceBlk* query, BLAST_SequenceBlk* subject,
                          Int4** matrix, Blast_ExtendWord* ewp, 
                          BlastInitHitList* init_hitlist)
{
   Int4 index;
   Uint4 query_length = query->length;
   Uint4 subject_length = subject->length;
   Uint1* q_start = query->sequence;
   Uint1* s_start = subject->sequence;
   Uint4 word_length = 0;
   Uint4 q_off, s_off;
   Uint4 max_bases_left, max_bases_right;
   Uint4 extended_right;
   Uint4 shift;
   Uint1* q, *s;
   Uint4 min_step = 0;
   Boolean variable_wordsize = FALSE;
   Int4 hits_extended = 0;
   Uint1 template_length = 0;

   if (lookup_wrap->lut_type == MB_LOOKUP_TABLE) {
      BlastMBLookupTable* lut = (BlastMBLookupTable*)lookup_wrap->lut;
      word_length = lut->word_length;
      /* When ungapped extension is not performed, the hit will be new only if
         it more than scan_step away from the previous hit. */
      if(!word_params->options->ungapped_extension)
         min_step = lut->scan_step;
      variable_wordsize = lut->variable_wordsize;
      template_length = lut->template_length;
   } else {
      BlastLookupTable* lut = (BlastLookupTable*)lookup_wrap->lut;
      word_length = lut->word_length;
      variable_wordsize = lut->variable_wordsize;
      if(!word_params->options->ungapped_extension)
         min_step = lut->scan_step;
   }

   for (index = 0; index < num_hits; ++index) {
      Uint4 s_offset = offset_pairs[index].qs_offsets.s_off;
      Uint4 q_offset = offset_pairs[index].qs_offsets.q_off;
      /* Adjust offsets to the start of the next full byte in the
         subject sequence */
      shift = (COMPRESSION_RATIO - s_offset%COMPRESSION_RATIO)
         % COMPRESSION_RATIO;
      q_off = q_offset + shift;
      s_off = s_offset + shift;
      s = s_start + s_off/COMPRESSION_RATIO;
      q = q_start + q_off;
      
      max_bases_left = 
         MIN(word_length, MIN(q_off, s_off));
      max_bases_right = MIN(word_length, 
                            MIN(query_length-q_off, subject_length-s_off));
      
      if (s_BlastNaExactMatchExtend(q, s, max_bases_left, 
                                  max_bases_right, word_length, 
                                  (Boolean) !variable_wordsize, &extended_right)) 
      {
         /* Check if this diagonal has already been explored and save
            the hit if needed. */
         Boolean hit_ready = FALSE;
         /* Check if this diagonal has already been explored. */
         if (word_params->container_type == eWordStacks) {
            hit_ready = 
               s_BlastnStacksExtendInitialHit(query, subject, min_step, template_length,
                  word_params, matrix, ewp->stack_table, q_offset, 
                  s_off + extended_right, s_offset, init_hitlist);
         } else {
            hit_ready = 
               s_BlastnDiagExtendInitialHit(query, subject, min_step, template_length,
                  word_params, matrix, ewp->diag_table, q_offset, 
                  s_off + extended_right, s_offset, init_hitlist);
         }
         if (hit_ready)
            ++hits_extended;
      }
   }
   return hits_extended;
}

/* Description in blast_extend.h */
Int2 MB_WordFinder(BLAST_SequenceBlk* subject,
		   BLAST_SequenceBlk* query,
		   LookupTableWrap* lookup_wrap,
		   Int4** matrix, 
		   const BlastInitialWordParameters* word_params,
		   Blast_ExtendWord* ewp,
                   BlastOffsetPair* offset_pairs,
		   Int4 max_hits,
		   BlastInitHitList* init_hitlist, 
         BlastUngappedStats* ungapped_stats)
{
   /* Pointer to the beginning of the first word of the subject sequence */
   BlastMBLookupTable* mb_lt = (BlastMBLookupTable*) lookup_wrap->lut;
   Int4 hitsfound=0;
   Int4 total_hits=0;
   Int4 start_offset, next_start, last_start=0, last_end=0;
   Int4 subject_length = subject->length;
   Int4 hits_extended = 0;

   start_offset = 0;
   if (mb_lt->discontiguous) {
      last_start = subject_length - mb_lt->template_length;
      last_end = subject_length;
   } else {
      last_end = subject_length;
      switch (word_params->extension_method) {
      case eRightAndLeft: case eUpdateDiag:
         /* Look for matches all the way to the end of the sequence. */
         last_start = 
            last_end - COMPRESSION_RATIO*mb_lt->compressed_wordsize;
         break;
      case eRight:
         /* Need to leave word_length to the end of the sequence for extension
            to the right. */
         if (mb_lt->variable_wordsize)
            last_end -= last_end%COMPRESSION_RATIO;
         last_start = last_end - mb_lt->word_length;
         last_end = last_start + mb_lt->compressed_wordsize*COMPRESSION_RATIO;
         break;
      default: 
         abort();  /* All the relevant cases should be handled above, otherwise variables may not be properly set
                      for the rest of the funciton. */ 
         break;
      }
   }

   /* start_offset points to the beginning of the word */
   while ( start_offset <= last_start ) {
      /* Set the last argument's value to the end of the last word,
         without the extra bases for the discontiguous case */
      next_start = last_end;
      if (mb_lt->discontiguous) {
         hitsfound = MB_DiscWordScanSubject(lookup_wrap, subject, start_offset,
                        offset_pairs, max_hits, &next_start);
      } else if (word_params->extension_method == eRightAndLeft) {
         hitsfound = MB_AG_ScanSubject(lookup_wrap, subject, start_offset, 
                        offset_pairs, max_hits, &next_start);
      } else {
         hitsfound = MB_ScanSubject(lookup_wrap, subject, start_offset, 
	                     offset_pairs, max_hits, &next_start);
      }

      switch (word_params->extension_method) {
      case eRightAndLeft:
         hits_extended += 
            BlastNaExtendRightAndLeft(offset_pairs, hitsfound, word_params,
                                      lookup_wrap, query, subject, 
                                      matrix, ewp, init_hitlist);
         break;
      case eRight:
         hits_extended += 
            BlastNaExtendRight(offset_pairs, hitsfound, word_params, 
                               lookup_wrap, query, subject, 
                               matrix, ewp, init_hitlist);
         break;
      case eUpdateDiag:
          hits_extended += 
              DiscMB_ExtendInitialHits(offset_pairs, hitsfound, query, subject, 
                                       lookup_wrap, word_params, matrix,
                                       ewp, init_hitlist);
         break;
      default: break;
      }
      /* next_start returned from the ScanSubject points to the beginning
         of the word */
      start_offset = next_start;
      total_hits += hitsfound;
   }

   s_BlastNaExtendWordExit(ewp, subject_length);

   Blast_UngappedStatsUpdate(ungapped_stats, total_hits, hits_extended, 
                             init_hitlist->total);

   if (word_params->options->ungapped_extension)
       Blast_InitHitListSortByScore(init_hitlist);


   return 0;
}

/* Description in blast_extend.h */
Int2 BlastNaWordFinder_AG(BLAST_SequenceBlk* subject, 
			  BLAST_SequenceBlk* query,
			  LookupTableWrap* lookup_wrap, 
			  Int4** matrix,
			  const BlastInitialWordParameters* word_params,
			  Blast_ExtendWord* ewp,
                          BlastOffsetPair* offset_pairs,
			  Int4 max_hits,
			  BlastInitHitList* init_hitlist, 
           BlastUngappedStats* ungapped_stats)
{
   Int4 hitsfound, total_hits = 0;
   Int4 start_offset, end_offset, next_start;
   BlastLookupTable* lookup = (BlastLookupTable*) lookup_wrap->lut;
   Int4 hits_extended = 0;

   start_offset = 0;
   end_offset = subject->length - COMPRESSION_RATIO*lookup->reduced_wordsize;

   /* start_offset points to the beginning of the word; end_offset is the
      beginning of the last word */
   while (start_offset <= end_offset) {
      hitsfound = BlastNaScanSubject_AG(lookup_wrap, subject, start_offset, 
                     offset_pairs, max_hits, &next_start); 
      
      total_hits += hitsfound;

      hits_extended += 
         BlastNaExtendRightAndLeft(offset_pairs, hitsfound, word_params, 
                                   lookup_wrap, query, subject, 
                                   matrix, ewp, init_hitlist);

      start_offset = next_start;
   }

   s_BlastNaExtendWordExit(ewp, subject->length);

   Blast_UngappedStatsUpdate(ungapped_stats, total_hits, hits_extended, 
                             init_hitlist->total);

   if (word_params->options->ungapped_extension)
       Blast_InitHitListSortByScore(init_hitlist);

   return 0;
} 

/* Deallocate memory for the diagonal table structure */
BLAST_DiagTable* 
BlastDiagTableFree(BLAST_DiagTable* diag_table)
{
   if (diag_table) {
      sfree(diag_table->hit_level_array);
      sfree(diag_table->last_hit_array);
               
      sfree(diag_table);
   }
   return NULL;
}

/** Deallocate memory for the stack table structure.
 * @param stack_table The stack table structure to free. [in]
 * @return NULL.
 */
static BLAST_StackTable* 
s_BlastStackTableFree(BLAST_StackTable* stack_table)
{
   Int4 index;

   if (!stack_table)
      return NULL;

   if (stack_table->bn_stack_array) {
      for (index = 0; index < stack_table->num_stacks; ++index)
         sfree(stack_table->bn_stack_array[index]);
      sfree(stack_table->bn_stack_array);
   }
   sfree(stack_table->stack_index);
   sfree(stack_table->stack_size);
   sfree(stack_table);
   return NULL;
}

Blast_ExtendWord* BlastExtendWordFree(Blast_ExtendWord* ewp)
{

   if (ewp == NULL)
      return NULL;

   BlastDiagTableFree(ewp->diag_table);
   s_BlastStackTableFree(ewp->stack_table);
   sfree(ewp);
   return NULL;
}

void 
BlastSaveInitHsp(BlastInitHitList* ungapped_hsps, Int4 q_start, Int4 s_start, 
   Int4 q_off, Int4 s_off, Int4 len, Int4 score)
{
  BlastUngappedData* ungapped_data = NULL;

  ungapped_data = (BlastUngappedData*) malloc(sizeof(BlastUngappedData));

  ungapped_data->q_start = q_start;
  ungapped_data->s_start = s_start;
  ungapped_data->length  = len;
  ungapped_data->score   = score;

  BLAST_SaveInitialHit(ungapped_hsps, q_off, s_off, ungapped_data);

  return;
}
