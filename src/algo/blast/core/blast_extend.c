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
* ===========================================================================*/

/*****************************************************************************

File name: blast_extend.c

Author: Ilya Dondoshansky

Contents: Functions to initialize structures used for BLAST extension

******************************************************************************
 * $Revision$
 * */

#include <blast_extend.h>
#include <blast_options.h>
#include <aa_lookup.h>
#include <na_lookup.h>
#include <mb_lookup.h>

#define MIN_INIT_HITLIST_SIZE 100

/* Description in blast_extend.h */
BlastInitHitListPtr BLAST_InitHitListNew(void)
{
   BlastInitHitListPtr init_hitlist = (BlastInitHitListPtr)
      MemNew(sizeof(BlastInitHitList));

   init_hitlist->allocated = MIN_INIT_HITLIST_SIZE;

   init_hitlist->init_hsp_array = (BlastInitHSPPtr)
      Malloc(MIN_INIT_HITLIST_SIZE*sizeof(BlastInitHSP));

   return init_hitlist;
}

BlastInitHitListPtr BLAST_InitHitListDestruct(BlastInitHitListPtr init_hitlist)
{
  MemFree(init_hitlist->init_hsp_array);
  MemFree(init_hitlist);
  return NULL;
}


/** Allocates memory for the BLAST_DiagTablePtr. This function also 
 * sets many of the parametes such as diag_array_length etc.
 * @param qlen Length of the query [in]
 * @param multiple_hits Specifies whether multiple hits method is used [in]
 * @param window_size The max. distance between two hits that are extended [in]
 * @return The allocated BLAST_DiagTable structure
*/
static BLAST_DiagTablePtr
BLAST_DiagTableNew (Int4 qlen, Boolean multiple_hits, Int4 window_size)

{
        BLAST_DiagTablePtr diag_table;
        Int4 diag_array_length;

        diag_table= MemNew(sizeof(BLAST_DiagTable));

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
Int2 BLAST_ExtendWordInit(BLAST_SequenceBlkPtr query,
   BlastInitialWordOptionsPtr word_options,
   Int8 dblen, Int4 dbseq_num, BLAST_ExtendWordPtr PNTR ewp_ptr)
{
   BLAST_ExtendWordPtr ewp;
   Int4 index, i;

   *ewp_ptr = ewp = (BLAST_ExtendWordPtr) MemNew(sizeof(BLAST_ExtendWord));

   if (!ewp) {
      return -1;
   }

   if (word_options->extend_word_method & EXTEND_WORD_MB_STACKS) {
      Int8 av_search_space;
      Int4 stack_size, num_stacks;
      MB_StackTablePtr stack_table;

      ewp->stack_table = stack_table = 
         (MB_StackTablePtr) Malloc(sizeof(MB_StackTable));

      av_search_space = 
         ((Int8) query->length) * dblen / dbseq_num;
      num_stacks = MIN(1 + (Int4) sqrt(av_search_space)/100, 500);
      stack_size = 5000/num_stacks;
      stack_table->stack_index = (Int4Ptr) MemNew(num_stacks*sizeof(Int4));
      stack_table->stack_size = (Int4Ptr) Malloc(num_stacks*sizeof(Int4));
      stack_table->estack = 
         (MbStackPtr PNTR) Malloc(num_stacks*sizeof(MbStackPtr));
      for (index=0; index<num_stacks; index++) {
         stack_table->estack[index] = 
            (MbStackPtr) Malloc(stack_size*sizeof(MbStack));
         stack_table->stack_size[index] = stack_size;
      }
      stack_table->num_stacks = num_stacks;
   } else {
      Boolean multiple_hits = (word_options->window_size > 0);
      Int4Ptr buffer;
      BLAST_DiagTablePtr diag_table;

      ewp->diag_table = diag_table = 
         BLAST_DiagTableNew(query->length, multiple_hits, 
                            word_options->window_size);
      /* Allocate the buffer to be used for diagonal array. */
      buffer = 
         (Int4Ptr) MemNew(diag_table->diag_array_length*sizeof(DiagStruct));
      
      if (buffer == NULL)	{
         ewp = MemFree(ewp);
         return -1;
      }
         
      diag_table->diag_array= (DiagStructPtr) buffer;
      for(i=0;i<diag_table->diag_array_length;i++){
         diag_table->diag_array[i].diag_level=0;
         diag_table->diag_array[i].last_hit = -diag_table->window;
      }
   }

   *ewp_ptr = ewp;
   
   return 0;
}

Boolean BLAST_SaveInitialHit(BlastInitHitListPtr init_hitlist, 
                  Int4 q_off, Int4 s_off, BlastUngappedDataPtr ungapped_data) 
{
   BlastInitHSPPtr match_array;
   Int4 num, num_avail;

   num = init_hitlist->total;
   num_avail = init_hitlist->allocated;

   match_array = init_hitlist->init_hsp_array;
   if (num>=num_avail) {
      if (init_hitlist->do_not_reallocate) 
         return FALSE;
      num_avail *= 2;
      match_array = (BlastInitHSPPtr) 
         Realloc(match_array, num_avail*sizeof(BlastInitHSP));
      if (!match_array) {
         init_hitlist->do_not_reallocate = TRUE;
         return FALSE;
      } else {
         init_hitlist->allocated = num_avail;
         init_hitlist->init_hsp_array = match_array;
      }
   }

   match_array[num].q_off = q_off;
   match_array[num].s_off = s_off;
   match_array[num].ungapped_data = ungapped_data;

   init_hitlist->total++;
   return TRUE;
}

/* Documentation in blast_extend.h */
Int4
MB_ExtendInitialHit(BlastInitialWordOptionsPtr word_options, 
             Uint1 scan_step, Int4 hit_length, Int4 word_length, 
             BLAST_ExtendWordPtr ewp, Int4 q_off, Int4 s_off,
             BlastInitHitListPtr init_hitlist) {
   Int4 index, index1, step;
   MbStackPtr estack;
   Int4 diag, stack_top;
   register Int4 window;
   Boolean one_word, hit_ready, two_hits;
   MB_StackTablePtr stack_table = ewp->stack_table;
   BLAST_DiagTablePtr diag_table = ewp->diag_table;

   window = word_options->window_size;
   two_hits = (window > 0);

   /* For short query/db stacks are not used; 
      instead use a diagonal array */
   if (diag_table) {
      DiagStructPtr diag_array = diag_table->diag_array;
      DiagStructPtr diag_array_elem;
      register Int4 diag_mask;
      Int4 offset, s_pos;

      diag_mask = diag_table->diag_mask;
      offset = diag_table->offset;

      s_pos = s_off + offset;
      diag = (s_off - q_off) & diag_mask;
      diag_array_elem = &diag_array[diag];
      step = s_pos - diag_array_elem->last_hit;

      one_word = (diag_array_elem->diag_level >= word_length);
      hit_ready = ((!two_hits && one_word) || 
                    diag_array_elem->diag_level > word_length);

      if (step == scan_step || 
               (step > scan_step && step <= window 
                && one_word && !hit_ready)) {
         /* We had a hit on this diagonal that is extended by current hit */
         diag_array_elem->last_hit = s_pos;
         diag_array_elem->diag_level += step;
         /* Save the hit immediately if:
               For single-hit case - it reached the word size
               For double-hit case - hit is longer than word size, or 
               2 words have already been found, and there is another gap. 
            If hit is longer than the required minimum, it must have already 
            been saved earlier.
         */                  
         if (!hit_ready && ((diag_array_elem->diag_level > word_length) ||
              (!two_hits && diag_array_elem->diag_level == word_length)))
         {
            BLAST_SaveInitialHit(init_hitlist, q_off, s_off, NULL);
         }
      } else {
         /* Start the new hit, overwriting the diagonal array entry */
         diag_array_elem->last_hit = s_pos;
         diag_array_elem->diag_level = hit_length;
         /* Save the hit if it already qualifies */
         if (!two_hits && (hit_length == word_length))
            BLAST_SaveInitialHit(init_hitlist, q_off, s_off, NULL);
      }
   } else {
      /* Use stacks instead of the diagonal array */
      index1 = (s_off - q_off) % stack_table->num_stacks;
      if (index1<0)
         index1 += stack_table->num_stacks;
      estack = stack_table->estack[index1];
      stack_top = stack_table->stack_index[index1] - 1;

      for (index=0; index<=stack_top; ) {
         step = s_off - estack[index].level;
         one_word = (estack[index].length >= word_length);
         if (estack[index].diag == s_off - q_off) {
            hit_ready = ((!two_hits && one_word) || 
                         estack[index].length > word_length);
            if (step == scan_step || 
                (step > scan_step && step <= window && 
                 one_word && !hit_ready)) {
               estack[index].length += step;
               /* We had a hit on this diagonal that is extended by current 
                  hit. Save the hit if:
                  For single-hit case - it has just reached the word size
                  For double-hit case - second hit has just been found, 
                  after the first reached the word size.
                  Otherwise, just update the hit length and offset.
               */                  
               if (!hit_ready && ((estack[index].length > word_length) || 
                   (!two_hits && estack[index].length == word_length))) {
                  BLAST_SaveInitialHit(init_hitlist, q_off, s_off, NULL);
               }    
               estack[index].level = s_off;
            } else {
               /* Start the new hit, overwriting the previous one */
               estack[index].length = hit_length;
               estack[index].level = s_off;
               /* Save the hit if it already qualifies */
               if (!two_hits && (hit_length == word_length))
                  BLAST_SaveInitialHit(init_hitlist, q_off, s_off, NULL);
            }
            /* In case the size of this stack changed */
            stack_table->stack_index[index1] = stack_top + 1;	 
            return 0;
         } else if (step <= scan_step || (step <= window && one_word)) {
            /* Hit from a different diagonal, and it can continue */
            index++;
         } else {
            /* Hit from a different diagonal that does not continue: remove it 
               from the stack */
            estack[index] = estack[stack_top--];
         }
      }

      /* Need an extra slot on the stack for this hit */
      if (++stack_top >= stack_table->stack_size[index1]) {
         /* Stack about to overflow - reallocate memory */
         MbStackPtr ptr;
         if (!(ptr = (MbStackPtr)Realloc(estack,
                     2*stack_table->stack_size[index1]*sizeof(MbStack)))) {
            ErrPostEx(SEV_WARNING, 0, 0, 
                      "Unable to allocate %ld extra spaces for stack %d",
                      2*stack_table->stack_size[index1], index1);
            return 1;
         } else {
            stack_table->stack_size[index1] *= 2;
            estack = stack_table->estack[index1] = ptr;
         }
      }
      /* Start a new hit */
      estack[stack_top].diag = s_off - q_off;
      estack[stack_top].level = s_off;
      estack[stack_top].length = hit_length;
      stack_table->stack_index[index1] = stack_top + 1;
      /* Save the hit if it already qualifies */
      if (!two_hits && (hit_length >= word_length))
         BLAST_SaveInitialHit(init_hitlist, q_off, s_off, NULL);
   }

   return 0;
}

/** Update the word extension structure after scanning of each subject sequence
 * @param ewp The structure holding word extension information [in] [out]
 * @param subject_length The length of the subject sequence that has just been
 *        processed [in]
 */
static Int2 BlastNaExtendWordExit(BLAST_ExtendWordPtr ewp, Int4 subject_length)
{
   BLAST_DiagTablePtr diag_table;
   Int4 diag_array_length, i;

   if (!ewp)
      return -1;

   diag_table = ewp->diag_table;
      
   if (diag_table->offset >= INT4_MAX/2) {
      diag_array_length = diag_table->diag_array_length;
      for(i=0;i<diag_array_length;i++) {
	 diag_table->diag_array[i].diag_level=0;
	 diag_table->diag_array[i].last_hit = -diag_table->window;
      }
   }
      
   if (diag_table->offset < INT4_MAX/2)	{
      diag_table->offset += subject_length + diag_table->window;
   } else {
      diag_table->offset = 0;
   }

   return 0;
}

/** Update the MegaBlast word extension structure after scanning of each 
 *  subject sequence.
 * @param ewp The structure holding word extension information [in] [out]
 * @param subject_length The length of the subject sequence that has just been
 *        processed [in]
 */
static Int2 MB_ExtendWordExit(BLAST_ExtendWordPtr ewp, Int4 subject_length)
{
   if (!ewp)
      return -1;

   if (ewp->diag_table) {
      return BlastNaExtendWordExit(ewp, subject_length);
   } else { 
      MemSet(ewp->stack_table->stack_index, 0, 
             ewp->stack_table->num_stacks*sizeof(Int4));
   }

   return 0;
}

/** Pack 4 sequence bytes into a one byte integer, assuming sequence contains
 *  no ambiguities.
 */
#define	PACK_WORD(q) ((q[0]<<6) + (q[1]<<4) + (q[2]<<2) + q[3]) 

/** Compare a given number of bytes of an compressed subject sequence with
 *  the non-compressed query sequence.
 * @param q Pointer to the first byte to be compared in the query sequence [in]
 * @param s Pointer to the first byte to be compared in the subject 
 *        sequence [in]
 * @param extra_bytes Number of compressed bytes to compare [in]
 * @return TRUE if sequences are identical, FALSE if mismatch is found.
 */
static inline Boolean BlastNaCompareExtraBytes(Uint1Ptr q, Uint1Ptr s, 
                                               Int4 extra_bytes)
{
   Int4 index;
   
   for (index = 0; index < extra_bytes; ++index) {
      if (*s++ != PACK_WORD(q))
	 return FALSE;
      q += COMPRESSION_RATIO;
   }
   return TRUE;
}

/** Perform mini extension (up to max_left <= 4 bases) to the left; 
 * @param q Pointer to the query base right after the ones to be extended [in]
 * @param s Pointer to a byte in the compressed subject sequence that is to be 
 *        tested for extension [in]
 * @param max_left Maximal number of bits to compare [in]
 * @return Number of matched bases
*/
static inline Uint1 
BlastNaMiniExtendLeft(Uint1Ptr q, Uint1Ptr s, Uint1 max_left)
{
   Uint1 left = 0;
   Uint1 s_val = *s;

   for (left = 0; left < max_left; ++left) {
      if (READDB_UNPACK_BASE_N(s_val, left) != *--q) {
	 break;
      }
   }
   return left;
}

/** Perform mini extension (up to max_right <= 4 bases) to the right; 
 * @param q Pointer to the start of the extension in the query [in]
 * @param s Pointer to a byte in the compressed subject sequence that is to be 
 *        tested for extension [in]
 * @param max_right Maximal number of bits to compare [in]
 * @return Number of matched bases
*/
static inline Uint1 
BlastNaMiniExtendRight(Uint1Ptr q, Uint1Ptr s, Uint1 max_right)
{
   Uint1 right;
   Uint1 s_val = *s;
   Uint1 index = 3;
   
   for (right = 0; right < max_right; ++right, --index) {
      if (READDB_UNPACK_BASE_N(s_val, index) != *q++) {
	 break;
      }
   }
   return right;
}

/* Description in blast_extend.h */
Boolean
BlastnWordUngappedExtend(BLAST_SequenceBlkPtr query, 
   BLAST_SequenceBlkPtr subject, Int4Ptr PNTR matrix, 
   Int4 q_off, Int4 s_off, Int4 cutoff, Int4 X, 
   BlastUngappedDataPtr PNTR ungapped_data)
{
	register Uint1Ptr q;
	register Int4 sum, score;
	Uint1 ch;
	Uint1Ptr subject0, sf, q_beg, q_end, s_end, s, start;
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
	do {
           if (base == 3) {
              s--;
              base = 0;
           } else
              base++;
           ch = *s;
           if ((sum += matrix[*--q][READDB_UNPACK_BASE_N(ch, base)]) > 0) {
              q_beg = q;
              score += sum;
              sum = 0;
           } else if (sum < X)
              break;
	} while ((s > start) || (s == start && base < remainder));

        if (score >= cutoff && !ungapped_data) 
           return FALSE;

	if (ungapped_data) {
	   (*ungapped_data)->q_start = q_beg - query->sequence;
	   (*ungapped_data)->s_start = 
	      s_off - (q_off - (*ungapped_data)->q_start);
	}

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

	while (s < sf || (s == sf && base >= remainder)) {
           ch = *s;
           if ((sum += matrix[*q++][READDB_UNPACK_BASE_N(ch, base)]) > 0) {
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
        
	if (ungapped_data) {
	   (*ungapped_data)->length = q_end - q_beg;
	   (*ungapped_data)->score = score;
           (*ungapped_data)->frame = 0;
	}

        return (score < cutoff);
}

/** Perform ungapped extension given an offset pair, and save the initial 
 * hit information if the hit qualifies. This function assumes that the
 * exact match has already been extended to the word size parameter.
 * @param query The query sequence [in]
 * @param subject The subject sequence [in]
 * @param word_params The parameters related to initial word extension [in]
 * @param matrix the substitution matrix [in]
 * @param ewp The structure containing word extension information [in]
 * @param q_off The offset in the query sequence [in]
 * @param s_start The start of this hit in the subject sequence [in]
 * @param s_off The offset in the subject sequence [in]
 * @param init_hitlist The structure containing information about all 
 *                     initial hits [in] [out]
 */
static Int2
BlastnExtendInitialHit(BLAST_SequenceBlkPtr query, 
   BLAST_SequenceBlkPtr subject, BlastInitialWordParametersPtr word_params, 
   Int4Ptr PNTR matrix, BLAST_ExtendWordPtr ewp, Int4 q_off, Int4 s_start,
   Int4 s_off, BlastInitHitListPtr init_hitlist)
{
   Int4 diag, real_diag;
   Int4 s_pos, last_hit;
   BLAST_DiagTablePtr     diag_table;
   BlastUngappedDataPtr ungapped_data;
   BlastInitialWordOptionsPtr word_options = word_params->options;
   Int4 window_size = word_options->window_size;
   Boolean hit_ready;
   Boolean new_hit, second_hit;

   diag_table = ewp->diag_table;

   diag = s_off + diag_table->diag_array_length - q_off;
   real_diag = diag & diag_table->diag_mask;
   s_pos = s_off + diag_table->offset;
   last_hit = diag_table->diag_array[real_diag].last_hit;

   /* Hit is new if it starts more than the window size after the previous 
      hit ended */
   new_hit = ((s_start + diag_table->offset) > last_hit + window_size);

   /* A hit is a second hit if it ends after the end of the previous hit, but
      by less than the window size; the previous hit should not have been a 
      second hit itself in this case, since we don't want to connect more 
      than 2 hits into one */
   second_hit = diag_table->diag_array[real_diag].diag_level == 0 && 
      (s_pos > last_hit) && (s_pos <= last_hit + window_size); 

   hit_ready = ((window_size == 0) && new_hit) || second_hit;

   if (hit_ready) {
      if (word_options->extend_word_method & EXTEND_WORD_UNGAPPED) {
         ungapped_data = (BlastUngappedDataPtr) 
            Malloc(sizeof(BlastUngappedData));
         /* Perform ungapped extension */
         BlastnWordUngappedExtend(query, subject, matrix, q_off, s_off, 
            word_params->cutoff_score, -word_params->x_dropoff, 
            &ungapped_data);
      
         diag_table->diag_array[real_diag].last_hit = 
            ungapped_data->length - 1 + ungapped_data->s_start 
            + diag_table->offset;
      } else {
         ungapped_data = NULL;
         diag_table->diag_array[real_diag].last_hit = s_pos;
      }
      /* diag_level serves as a boolean variable indicating that this hit 
         has already been saved */
      diag_table->diag_array[real_diag].diag_level = 1;
      BLAST_SaveInitialHit(init_hitlist, q_off, s_off, ungapped_data);
   } else if (new_hit) {
      /* First hit in the 2-hit case - update the last hit information only */
      diag_table->diag_array[real_diag].last_hit = s_pos;
      diag_table->diag_array[real_diag].diag_level = 0;
   }
   return 0;
}

/* Description in blast_extend.h */
Int4 BlastNaWordFinder(BLAST_SequenceBlkPtr subject, 
		       BLAST_SequenceBlkPtr query,
		       LookupTableWrapPtr lookup_wrap,
		       Int4Ptr PNTR matrix,
		       BlastInitialWordParametersPtr word_params,
		       BLAST_ExtendWordPtr ewp,
		       Uint4Ptr q_offsets,
		       Uint4Ptr s_offsets,
		       Int4 max_hits,
		       BlastInitHitListPtr init_hitlist)
{
   BlastInitialWordOptionsPtr word_options = word_params->options;
   LookupTablePtr lookup = (LookupTablePtr) lookup_wrap->lut;
   Uint1Ptr s_start = subject->sequence;
   Uint1Ptr abs_start = s_start;
   Int4 i;
   Uint1Ptr s, s_end;
   Uint1Ptr q_start = query->sequence;
   Uint1Ptr q_end = query->sequence + query->length;
   Int4 hitsfound, total_hits = 0;
   Uint4 compressed_wordsize, reduced_word_length;
   Uint4 extra_bytes_needed;
   Uint2 extra_bases, left, right;
   Uint1Ptr q;
   Int4 start_offset, last_start, next_start, last_end;
   Int4 max_bases;
   Int4 bases_in_last_byte;

   last_start = subject->length - lookup->word_length;
   s_end = subject->sequence + subject->length/COMPRESSION_RATIO;
   start_offset = 0;
   bases_in_last_byte = subject->length % COMPRESSION_RATIO;

   compressed_wordsize = lookup->reduced_wordsize;
   extra_bytes_needed = lookup->wordsize - compressed_wordsize;
   reduced_word_length = COMPRESSION_RATIO*compressed_wordsize;
   extra_bases = lookup->word_length - COMPRESSION_RATIO*lookup->wordsize;
   last_end = subject->length - (lookup->word_length - reduced_word_length);

   while (start_offset <= last_start) {
      /* Pass the last word ending offset */
      next_start = last_end;
      hitsfound = BlastNaScanSubject(lookup_wrap, subject, start_offset, 
                     q_offsets, s_offsets, max_hits, &next_start); 
      
      total_hits += hitsfound;
      for (i = 0; i < hitsfound; ++i) {
         q = q_start + q_offsets[i];
         s = abs_start + s_offsets[i]/COMPRESSION_RATIO;
	    
	 /* Check for extra bytes if required for longer words. */
	 if (extra_bytes_needed && 
	     !BlastNaCompareExtraBytes(q, s, extra_bytes_needed))
	    continue;
         if (s_offsets[i] > reduced_word_length) {
            /* mini extension to the left */
            max_bases = 
               MIN(4, q_offsets[i] - reduced_word_length);
            left = BlastNaMiniExtendLeft(q-reduced_word_length, 
                      s-compressed_wordsize-1, max_bases);
         } else {
            left = 0;
         }
         s += extra_bytes_needed;
         if (s <= s_end) {
            /* mini extension to the right */
            q += COMPRESSION_RATIO*extra_bytes_needed;
            max_bases = MIN(4, q_end - q);
            if (s == s_end)
               max_bases = MIN(max_bases, bases_in_last_byte);
            right = BlastNaMiniExtendRight(q, s, max_bases);
         } else {
            right = 0;
         }

	 if (left + right >= extra_bases) {
	    /* Check if this diagonal has already been explored. */
	    BlastnExtendInitialHit(query, subject, word_params, matrix, ewp,
               q_offsets[i], s_offsets[i] - reduced_word_length - left, 
               s_offsets[i], init_hitlist);
         }
	 
      }
      
      start_offset = next_start;
   }

   BlastNaExtendWordExit(ewp, subject->length);
   return total_hits;
} 

/** Extend an exact match in both directions up to the provided 
 * maximal length. 
 * @param q_start Pointer to the start of the extension in query [in]
 * @param s_start Pointer to the start of the extension in subject [in]
 * @param max_bases_left At most how many bases to extend to the left [in]
 * @param max_bases_right At most how many bases to extend to the right [in]
 * @param max_length The length of the required exact match [in]
 * @param extend_partial_byte Should partial byte extension be perfomed? [in]
 * @param extended_left How far has the left extension succeeded? [out]
 * @return TRUE if extension successful 
 */
static Boolean BlastNaExactMatchExtend(Uint1Ptr q_start, Uint1Ptr s_start, 
        Int4 max_bases_left, Int4 max_bases_right, Int4 max_length, 
        Boolean extend_partial_byte, Int4Ptr extended_left)
{
   Int4 length = 0;
   Uint1Ptr q, s;

   /* Extend to the left; start with previous byte */
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
   *extended_left = length;
   if (length >= max_length)
      return TRUE;
   if (extend_partial_byte && max_bases_left > 0) {
      length += BlastNaMiniExtendLeft(q+COMPRESSION_RATIO, s, 
                   MIN(max_bases_left, COMPRESSION_RATIO));
   }
   if (length >= max_length)
      return TRUE;

   /* Extend to the right; start after the end of the word */
   max_bases_right = MIN(max_bases_right, max_length - length);
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
      if (length >= max_length)
         return TRUE;
      if (max_bases_right > 0) {
         length += BlastNaMiniExtendRight(q, s, 
                      MIN(max_bases_right, COMPRESSION_RATIO));
      }
   }
   return (length >= max_length);
}

/* Description in blast_extend.h */
Int4 MB_WordFinder(BLAST_SequenceBlkPtr subject,
		   BLAST_SequenceBlkPtr query,
		   LookupTableWrapPtr lookup,
		   Int4Ptr PNTR matrix, 
		   BlastInitialWordParametersPtr word_params,
		   BLAST_ExtendWordPtr ewp,
		   Uint4Ptr q_offsets,
		   Uint4Ptr s_offsets,
		   Int4 max_hits,
		   BlastInitHitListPtr init_hitlist)
{
   BlastInitialWordOptionsPtr word_options = word_params->options;
   /* Pointer to the beginning of the first word of the subject sequence */
   MBLookupTablePtr mb_lt = (MBLookupTablePtr) lookup->lut;
   Uint1Ptr s_start, q_start, q, s;
   Int4 hitsfound=0;
   Int4 hit_counter=0, i;
   Int4 start_offset, next_start, last_start, last_end;
   Int4 min_hit_length = COMPRESSION_RATIO*(mb_lt->compressed_wordsize);
   Uint4 word_length, reduced_word_length;
   Uint4 max_bases_left, max_bases_right;
   Int4 query_length = query->length;
   Boolean ag_blast, variable_wordsize;
   Int4 extended_left;

   s_start = subject->sequence;
   q_start = query->sequence;
   word_length = mb_lt->word_length;
   reduced_word_length = COMPRESSION_RATIO*mb_lt->compressed_wordsize;
   ag_blast = (Boolean) (word_options->extend_word_method & EXTEND_WORD_AG);
   variable_wordsize = (Boolean) 
      (word_options->extend_word_method & EXTEND_WORD_VARIABLE_SIZE);

   start_offset = 0;
   if (mb_lt->discontiguous) {
      last_start = subject->length - mb_lt->template_length;
      last_end = last_start + mb_lt->word_length;
   } else {
      last_end = subject->length;
      if (ag_blast)
         last_start = last_end - reduced_word_length;
      else
         last_start = last_end - mb_lt->word_length;
   }

   /* start_offset points to the beginning of the word */
   while ( start_offset <= last_start ) {
      /* Set the last argument's value to the end of the last word,
         without the extra bases for the discontiguous case */
      next_start = last_end;
      if (mb_lt->discontiguous) {
         hitsfound = MB_DiscWordScanSubject(lookup, subject, start_offset,
            q_offsets, s_offsets, max_hits, &next_start);
      } else if (!ag_blast) {
         hitsfound = MB_ScanSubject(lookup, subject, start_offset, 
	    q_offsets, s_offsets, max_hits, &next_start);
      } else {
         hitsfound = MB_AG_ScanSubject(lookup, subject, start_offset, 
	    q_offsets, s_offsets, max_hits, &next_start);
      }

      if (ag_blast) {
         for (i = 0; i < hitsfound; ++i) {
            q = q_start + q_offsets[i] - s_offsets[i]%COMPRESSION_RATIO;
            s = s_start + s_offsets[i]/COMPRESSION_RATIO;
	    
            max_bases_left = MIN(word_length, MIN(q_offsets[i], s_offsets[i]));

            max_bases_right = MIN(word_length, 
               MIN(query_length-q_offsets[i], last_end-s_offsets[i]));
                                                
            if (BlastNaExactMatchExtend(q, s, max_bases_left, max_bases_right,
                   word_length, !variable_wordsize, &extended_left)) {
               /* Check if this diagonal has already been explored and save
                  the hit if needed. */
               BlastnExtendInitialHit(query, subject, word_params, matrix, 
                  ewp, q_offsets[i], s_offsets[i] - extended_left, 
                  s_offsets[i], init_hitlist);
            }
         }
      } else {
         for (i = 0; i < hitsfound; ++i) {
            MB_ExtendInitialHit(word_options, mb_lt->scan_step, 
               min_hit_length, mb_lt->word_length, ewp, q_offsets[i], 
               s_offsets[i], init_hitlist);
         }
      }
      /* next_start returned from the ScanSubject points to the beginning
         of the word */
      start_offset = next_start;
      hit_counter += hitsfound;
   }

   MB_ExtendWordExit(ewp, subject->length);

   return hit_counter;
}

/* Description in blast_extend.h */
Int4 BlastNaWordFinder_AG(BLAST_SequenceBlkPtr subject, 
			  BLAST_SequenceBlkPtr query,
			  LookupTableWrapPtr lookup_wrap, 
			  Int4Ptr PNTR matrix,
			  BlastInitialWordParametersPtr word_params,
			  BLAST_ExtendWordPtr ewp,
			  Uint4Ptr q_offsets,
			  Uint4Ptr s_offsets,
			  Int4 max_hits,
			  BlastInitHitListPtr init_hitlist)
{
   BlastInitialWordOptionsPtr word_options = word_params->options;
   LookupTablePtr lookup = (LookupTablePtr) lookup_wrap->lut;
   Uint1Ptr s_start = subject->sequence;
   Int4 i;
   Uint1Ptr s, s_end;
   Uint1Ptr q_start = query->sequence;
   Int4 query_length = query->length;
   Int4 subject_length = subject->length;
   Int4 hitsfound, total_hits = 0;
   Uint4 word_length, reduced_word_length;
   Uint1Ptr q;
   Int4 start_offset, end_offset, next_start;
   Uint4 max_bases_left, max_bases_right;
   Int4 bases_in_last_byte;
   Boolean variable_wordsize = (Boolean) 
      (word_options->extend_word_method & EXTEND_WORD_VARIABLE_SIZE);
   Int4 extended_left;

   s_end = subject->sequence + subject_length/COMPRESSION_RATIO;
   bases_in_last_byte = subject_length % COMPRESSION_RATIO;

   word_length = lookup->word_length;
   reduced_word_length = COMPRESSION_RATIO*lookup->reduced_wordsize;
   start_offset = 0;
   end_offset = subject_length - reduced_word_length;

   /* start_offset points to the beginning of the word; end_offset is the
      beginning of the last word */
   while (start_offset <= end_offset) {
      hitsfound = BlastNaScanSubject_AG(lookup_wrap, subject, start_offset, 
                     q_offsets, s_offsets, max_hits, &next_start); 
      
      total_hits += hitsfound;
      for (i = 0; i < hitsfound; ++i) {
         q = q_start + q_offsets[i] - s_offsets[i]%COMPRESSION_RATIO;
         s = s_start + s_offsets[i]/COMPRESSION_RATIO;
	    
         max_bases_left = MIN(word_length, MIN(q_offsets[i], s_offsets[i]));

         max_bases_right = MIN(word_length, 
            MIN(query_length-q_offsets[i], subject_length-s_offsets[i]));
                                                
         if (BlastNaExactMatchExtend(q, s, max_bases_left, max_bases_right, 
                word_length, !variable_wordsize, &extended_left)) {
	    /* Check if this diagonal has already been explored. */
	    BlastnExtendInitialHit(query, subject, word_params, matrix, ewp,
               q_offsets[i], s_offsets[i] - extended_left, s_offsets[i], 
               init_hitlist);
         }
	 
      }
      
      start_offset = next_start;
   }

   BlastNaExtendWordExit(ewp, subject_length);
   return total_hits;
} 
