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

File name: blast_hits.c

Author: Ilya Dondoshansky

Contents: BLAST functions

Detailed Contents: 

        - BLAST functions for saving hits after the (preliminary) gapped 
          alignment

******************************************************************************
 * $Revision$
 * */

static char const rcsid[] = "$Id$";

#include <blast_options.h>
#include <blast_extend.h>
#include <blast_hits.h>
#include <blast_util.h>

void 
BLAST_AdjustQueryOffsets(Uint1 program_number, BlastHSPListPtr hsp_list, 
   BlastQueryInfoPtr query_info, Boolean is_ooframe)
{
   BlastHSPPtr hsp;
   Int4 context;
   Int4 index;
   Int4 offset_shift, extra_length = 0;

   if (!hsp_list)
      return;

   for (index=0; index<hsp_list->hspcnt; index++) {
      hsp = hsp_list->hsp_array[index];
      context = hsp->context = 
         BSearchInt4(hsp->query.gapped_start, 
            query_info->context_offsets, 
            (Int4) (query_info->last_context+1));   
      if (is_ooframe && program_number == blast_type_blastx) {
         offset_shift = query_info->context_offsets[context-context%3];
         /* Query offset is in mixed-frame coordinates */
         hsp->query.frame = hsp->query.offset % 3 + 1;
         if ((context % 6) >= 3)
            hsp->query.frame = -hsp->query.frame;
      } else {
         hsp->query.frame = BLAST_ContextToFrame(program_number, context);
         offset_shift = query_info->context_offsets[context];
      }
      hsp->query.offset -= offset_shift;
      hsp->query.gapped_start -= offset_shift;
      
      /* Check if this HSP is crossing the boundary on the left */
      if (hsp->query.offset < 0) {
         hsp->subject.offset -= hsp->query.offset;
         hsp->query.offset = 0;
      }

      /* Check if this HSP is crossing the boundary on the right */
      if (!is_ooframe) {
         extra_length = 
            hsp->query.end - query_info->context_offsets[context+1] + 1;
      }
      hsp->query.end -= offset_shift;

      if (extra_length > 0) {
         hsp->subject.end -= extra_length;
         hsp->query.end -= extra_length;
      }
      /* Correct offsets in the edit block too */
      if (hsp->gap_info) {
         hsp->gap_info->frame1 = hsp->query.frame;
         hsp->gap_info->frame2 = hsp->subject.frame;
         hsp->gap_info->start1 -= offset_shift;
         hsp->gap_info->length1 = 
            query_info->context_offsets[context+1] - offset_shift - 1;
      }
   }
}

BlastHSPPtr BlastHSPFree(BlastHSPPtr hsp)
{
   if (!hsp)
      return NULL;
   hsp->gap_info = GapEditBlockDelete(hsp->gap_info);
   sfree(hsp);
   return NULL;
}

Int2 BLAST_GetNonSumStatsEvalue(Uint1 program, BlastQueryInfoPtr query_info,
        BlastHSPListPtr hsp_list, BlastHitSavingOptionsPtr hit_options, 
        BLAST_ScoreBlkPtr sbp)
{
   BlastHSPPtr hsp;
   BlastHSPPtr PNTR hsp_array;
   BLAST_KarlinBlkPtr PNTR kbp;
   Int4 hsp_cnt;
   Int4 index;
   Uint1 factor;
   
   if (hsp_list == NULL)
      return 1;
   
   if (hit_options->is_gapped && program != blast_type_blastn)
      kbp = sbp->kbp_gap_std;
   else
      kbp = sbp->kbp_std;
   
   factor = ((program == blast_type_blastn) ? 2 : 1);

   hsp_cnt = hsp_list->hspcnt;
   hsp_array = hsp_list->hsp_array;
   for (index=0; index<hsp_cnt; index++) {
      hsp = hsp_array[index];

      ASSERT(hsp != NULL);

      /* Get effective search space from the score block, or from the 
         query information block, in order of preference */
      if (sbp->effective_search_sp) {
         hsp->evalue = BlastKarlinStoE_simple(hsp->score, kbp[hsp->context],
                          (FloatHi)sbp->effective_search_sp);
      } else {
         hsp->evalue = 
            BlastKarlinStoE_simple(hsp->score, kbp[hsp->context],
               (FloatHi)query_info->eff_searchsp_array[hsp->context/factor]);
      }
   }
   
   return 0;
}

Int2 BLAST_ReapHitlistByEvalue(BlastHSPListPtr hsp_list, 
        BlastHitSavingOptionsPtr hit_options)
{
   BlastHSPPtr hsp;
   BlastHSPPtr PNTR hsp_array;
   Int4 hsp_cnt = 0;
   Int4 index;
   Nlm_FloatHi cutoff;
   
   if (hsp_list == NULL)
      return 1;

   cutoff = hit_options->expect_value;

   hsp_array = hsp_list->hsp_array;
   for (index = 0; index < hsp_list->hspcnt; index++) {
      hsp = hsp_array[index];

      ASSERT(hsp != NULL);
      
      if (hsp->evalue > cutoff) {
         hsp_array[index] = BlastHSPFree(hsp_array[index]);
      } else {
         if (index > hsp_cnt)
            hsp_array[hsp_cnt] = hsp_array[index];
         hsp_cnt++;
      }
   }
      
   hsp_list->hspcnt = hsp_cnt;

   return 0;
}

/** Cleans out the NULLed out HSP's from the HSP array,
 *	moving the BLAST_HSPPtr's up to fill in the gaps.
 *	returns the number of valid HSP's.
*/

Int4
BlastHSPArrayPurge (BlastHSPPtr PNTR hsp_array, Int4 hspcnt)

{
	Int4 index, index1;

	if (hspcnt == 0 || hsp_array == NULL)
		return 0;

	index1 = 0;
	for (index=0; index<hspcnt; index++)
	{
		if (hsp_array[index] != NULL)
		{
			hsp_array[index1] = hsp_array[index];
			index1++;
		}
	}

	for (index=index1; index<hspcnt; index++)
	{
		hsp_array[index] = NULL;
	}

	hspcnt = index1;

	return index1;
}

/** Calculate number of identities in an HSP.
 * @param query The query sequence [in]
 * @param subject The uncompressed subject sequence [in]
 * @param hsp All information about the HSP [in]
 * @param is_gapped Is this a gapped search? [in]
 * @param num_ident_ptr Number of identities [out]
 * @param align_length_ptr The alignment length, including gaps [out]
 */
static Int2
BlastHSPGetNumIdentical(Uint1Ptr query, Uint1Ptr subject, 
   BlastHSPPtr hsp, Boolean is_gapped, Int4Ptr num_ident_ptr, 
   Int4Ptr align_length_ptr)
{
   Int4 i, num_ident, align_length, q_off, s_off;
   Int2 context;
   Uint1Ptr q, s;
   GapEditBlockPtr gap_info;
   GapEditScriptPtr esp;

   gap_info = hsp->gap_info;

   if (!gap_info && is_gapped)
      return -1;

   context = hsp->context;
   q_off = hsp->query.offset;
   s_off = hsp->subject.offset;

   if (!subject || !query)
      return -1;

   q = &query[q_off];
   s = &subject[s_off];

   num_ident = 0;
   align_length = 0;


   if (!gap_info) {
      /* Ungapped case */
      align_length = hsp->query.length; 
      for (i=0; i<align_length; i++) {
         if (*q++ == *s++)
            num_ident++;
      }
   } else {
      for (esp = gap_info->esp; esp; esp = esp->next) {
         align_length += esp->num;
         switch (esp->op_type) {
         case GAPALIGN_SUB:
            for (i=0; i<esp->num; i++) {
               if (*q++ == *s++)
                  num_ident++;
            }
            break;
         case GAPALIGN_DEL:
            s += esp->num;
            break;
         case GAPALIGN_INS:
            q += esp->num;
            break;
         default: 
            s += esp->num;
            q += esp->num;
            break;
         }
      }
   }

   *align_length_ptr = align_length;
   *num_ident_ptr = num_ident;
   return 0;
}

/** Comparison callback function for sorting HSPs by e-value */
static int
evalue_compare_hsps(const void* v1, const void* v2)
{
   BlastHSPPtr h1, h2;
   
   h1 = *((BlastHSPPtr PNTR) v1);
   h2 = *((BlastHSPPtr PNTR) v2);

   if (h1->evalue < h2->evalue)
      return -1;
   else if (h1->evalue > h2->evalue)
      return 1;
   if (h1->score > h2->score)
      return -1;
   else if (h1->score < h2->score)
      return 1;
   return 0;
}

/** Comparison callback function for sorting HSPs by diagonal and removing
 * the HSPs contained in or identical to other HSPs.
*/
static int
diag_uniq_compare_hsps(const void* v1, const void* v2)
{
   BlastHSPPtr h1, h2;
   BlastHSPPtr PNTR hp1, PNTR hp2;
   
   hp1 = (BlastHSPPtr PNTR) v1;
   hp2 = (BlastHSPPtr PNTR) v2;
   h1 = *hp1;
   h2 = *hp2;
   
   if (h1==NULL && h2==NULL) return 0;
   else if (h1==NULL) return 1;
   else if (h2==NULL) return -1;
   
   /* Separate different queries and/or strands */
   if (h1->context < h2->context)
      return -1;
   else if (h1->context > h2->context)
      return 1;
   
   /* If the two HSP's have same coordinates, they are equal */
   if (h1->query.offset == h2->query.offset && 
       h1->query.end == h2->query.end && 
       h1->subject.offset == h2->subject.offset &&
       h1->subject.end == h2->subject.end)
      return 0;
   
   /* Check if one HSP is contained in the other, if so, 
      leave only the longer one, given it has lower evalue */
   if (h1->query.offset >= h2->query.offset && 
       h1->query.end <= h2->query.end &&  
       h1->subject.offset >= h2->subject.offset && 
       h1->subject.end <= h2->subject.end && 
       h1->evalue >= h2->evalue) { 
      *hp1 = BlastHSPFree(*hp1);
      return 1; 
   } else if (h1->query.offset <= h2->query.offset &&  
              h1->query.end >= h2->query.end &&  
              h1->subject.offset <= h2->subject.offset && 
              h1->subject.end >= h2->subject.end && 
              h1->evalue <= h2->evalue) { 
      *hp2 = BlastHSPFree(*hp2);
      return -1; 
   }
   
   return (h1->query.offset - h1->subject.offset) -
      (h2->query.offset - h2->subject.offset);
}

/** Are the two HSPs within a given diagonal distance of each other? */
#define MB_HSP_CLOSE(q1, q2, s1, s2, c) (ABS(((q1)-(s1)) - ((q2)-(s2))) < (c))

/** Sort the HSPs in an HSP list by diagonal and remove redundant HSPs */
static Int2
BlastSortUniqHspArray(BlastHSPListPtr hsp_list)
{
   Int4 index, new_hspcnt, index1, q_off, s_off, q_end, s_end, index2;
   BlastHSPPtr PNTR hsp_array = hsp_list->hsp_array;
   Boolean shift_needed = FALSE;
   Int2 context;
   FloatHi evalue;

   qsort(hsp_list->hsp_array, hsp_list->hspcnt, sizeof(BlastHSPPtr), 
            diag_uniq_compare_hsps);
   for (index=1, new_hspcnt=0; index<hsp_list->hspcnt; index++) {
      if (hsp_array[index]==NULL) 
	 continue;
      q_off = hsp_array[index]->query.offset;
      s_off = hsp_array[index]->subject.offset;
      q_end = hsp_array[index]->query.end;
      s_end = hsp_array[index]->subject.end;
      evalue = hsp_array[index]->evalue;
      context = hsp_array[index]->context;
      for (index1 = new_hspcnt; index1 >= 0 && 
           hsp_array[index1]->context == context && new_hspcnt-index1 < 10 && 
           MB_HSP_CLOSE(q_off, hsp_array[index1]->query.offset,
                        s_off, hsp_array[index1]->subject.offset, 
                        2*MB_DIAG_NEAR);
           index1--) {
         if (q_off >= hsp_array[index1]->query.offset && 
             s_off >= hsp_array[index1]->subject.offset && 
             q_end <= hsp_array[index1]->query.end && 
             s_end <= hsp_array[index1]->subject.end &&
             evalue >= hsp_array[index1]->evalue) {
            hsp_array[index] = BlastHSPFree(hsp_array[index]);
            break;
         } else if (q_off <= hsp_array[index1]->query.offset && 
             s_off <= hsp_array[index1]->subject.offset && 
             q_end >= hsp_array[index1]->query.end && 
             s_end >= hsp_array[index1]->subject.end &&
             evalue <= hsp_array[index1]->evalue) {
            hsp_array[index1] = BlastHSPFree(hsp_array[index1]);
            shift_needed = TRUE;
         }
      }
      
      if (shift_needed) {
         while (index1 >= 0 && !hsp_array[index1])
            index1--;
         for (index2 = ++index1; index1 <= new_hspcnt; index1++) {
            if (hsp_array[index1])
               hsp_array[index2++] = hsp_array[index1];
         }
         new_hspcnt = index2 - 1;
         shift_needed = FALSE;
      }
      if (hsp_array[index] != NULL)
         hsp_array[++new_hspcnt] = hsp_array[index];
   }
   /* Set all HSP pointers that will not be used any more to NULL */
   memset(&hsp_array[new_hspcnt+1], 0, 
	  (hsp_list->hspcnt - new_hspcnt - 1)*sizeof(BlastHSPPtr));
   hsp_list->hspcnt = new_hspcnt + 1;

   return 0;
}

/** Reevaluate the HSPs in an HSP list, using ambiguity information. 
 * This is/can only done either for an ungapped search, or if traceback is 
 * already available.
 * Subject sequence is uncompressed and saved here. Number of identities is
 * calculated for each HSP along the way. 
 * @param hsp_list The list of HSPs for one subject sequence [in] [out]
 * @param query_blk The query sequence [in]
 * @param subject_blk The subject sequence [in] [out]
 * @param hit_options The options related to saving hits [in]
 * @param query_info Auxiliary query information [in]
 * @param sbp The statistical information [in]
 * @param score_options The scoring options [in]
 * @param rdfp The BLAST database structure (for retrieving uncompressed
 *             sequence) [in]
 */
static Int2 
BLAST_ReevaluateWithAmbiguities(BlastHSPListPtr hsp_list,
   BLAST_SequenceBlkPtr query_blk, BLAST_SequenceBlkPtr subject_blk, 
   BlastHitSavingOptionsPtr hit_options, BlastQueryInfoPtr query_info, 
   BLAST_ScoreBlkPtr sbp, BlastScoringOptionsPtr score_options, 
   const BlastSeqSrcPtr bssp)
{
   Int4 sum, score, gap_open, gap_extend;
   Int4Ptr PNTR matrix;
   BlastHSPPtr PNTR hsp_array, hsp;
   Uint1Ptr query, subject, query_start, subject_start = NULL;
   Uint1Ptr new_q_start, new_s_start, new_q_end, new_s_end;
   Int4 index, context, hspcnt, i;
   Int2 factor = 1;
   Uint1 mask = 0x0f;
   GapEditScriptPtr esp, last_esp, prev_esp, first_esp;
   Boolean purge, delete_hsp;
   FloatHi searchsp_eff;
   Int4 last_esp_num;
   Int2 status = 0;
   Int4 align_length;
   BLAST_KarlinBlkPtr PNTR kbp;
   GetSeqArg seq_arg;

   if (!hsp_list)
      return status;

   hspcnt = hsp_list->hspcnt;
   hsp_array = hsp_list->hsp_array;
   memset((void*) &seq_arg, 0, sizeof(seq_arg));

   /* In case of no traceback, return without doing anything */
   if (!hsp_list->traceback_done && hit_options->is_gapped) {
      return status;
   }

   if (hsp_list->hspcnt == 0)
      /* All HSPs have been deleted */
      return status;

   /* NB: this function is called only for BLASTn, so we know where the 
      Karlin block is */
   kbp = sbp->kbp_std;

   if (score_options->gap_open == 0 && score_options->gap_extend == 0) {
      if (score_options->reward % 2 == 1) 
         factor = 2;
      gap_open = 0;
      gap_extend = 
         (score_options->reward - 2*score_options->penalty) * factor / 2;
   } else {
      gap_open = score_options->gap_open;
      gap_extend = score_options->gap_extend;
   }

   matrix = sbp->matrix;

   /* Retrieve the unpacked subject sequence and save it in the 
      sequence_start element of the subject structure.
      NB: for the BLAST 2 Sequences case, the uncompressed sequence must 
      already be there */
   if (bssp) {
      seq_arg.oid = subject_blk->oid;
      seq_arg.encoding = BLASTNA_ENCODING;
      BLASTSeqSrcGetSequence(bssp, (void*) &seq_arg);

#if 0
      readdb_get_sequence_ex(rdfp, subject_blk->oid, 
         &subject_blk->sequence_start, &buf_len, TRUE);
      subject_blk->sequence = subject_blk->sequence_start + 1;
      subject_blk->sequence_start_allocated = TRUE;
#endif
      subject_blk->sequence_start = seq_arg.seq->sequence_start;
      subject_blk->sequence = seq_arg.seq->sequence_start + 1;
   }
   /* The sequence in blastna encoding is now stored in sequence_start */
   subject_start = subject_blk->sequence_start + 1;

   purge = FALSE;
   for (index=0; index<hspcnt; index++) {
      if (hsp_array[index] == NULL)
         continue;
      else
         hsp = hsp_array[index];

      context = hsp->context;

      query_start = query_blk->sequence + query_info->context_offsets[context];
      searchsp_eff = (FloatHi)query_info->eff_searchsp_array[context/2];
      
      query = query_start + hsp->query.offset; 
      subject = subject_start + hsp->subject.offset;
      score = 0;
      sum = 0;
      new_q_start = new_q_end = query;
      new_s_start = new_s_end = subject;
      i = 0;

      esp = hsp->gap_info->esp;
      prev_esp = NULL;
      last_esp = first_esp = esp;
      last_esp_num = 0;

      while (esp) {
         if (esp->op_type == GAPALIGN_SUB) {
            sum += factor*matrix[*query & mask][*subject];
            query++;
            subject++;
            i++;
         } else if (esp->op_type == GAPALIGN_DEL) {
            sum -= gap_open + gap_extend * esp->num;
            subject += esp->num;
            i += esp->num;
         } else if (esp->op_type == GAPALIGN_INS) {
            sum -= gap_open + gap_extend * esp->num;
            query += esp->num;
            i += esp->num;
         }
         
         if (sum < 0) {
            if (BlastKarlinStoE_simple(score, kbp[context],
                   searchsp_eff) > hit_options->expect_value) {
               /* Start from new offset */
               new_q_start = query;
               new_s_start = subject;
               score = sum = 0;
               if (i < esp->num) {
                  esp->num -= i;
                  first_esp = esp;
                  i = 0;
               } else {
                  first_esp = esp->next;
               }
               /* Unlink the bad part of the esp chain 
                  so it can be freed later */
               if (prev_esp)
                  prev_esp->next = NULL;
               last_esp = NULL;
            } else {
               /* Stop here */
               break;
            }
         } else if (sum > score) {
            /* Remember this point as the best scoring end point */
            score = sum;
            last_esp = esp;
            last_esp_num = i;
            new_q_end = query;
            new_s_end = subject;
         }
         if (i >= esp->num) {
            i = 0;
            prev_esp = esp;
            esp = esp->next;
         }
      }

      score /= factor;

      delete_hsp = FALSE;
      hsp->score = score;
      hsp->evalue = 
         BlastKarlinStoE_simple(score, kbp[context], searchsp_eff);
      if (hsp->evalue > hit_options->expect_value) {
         delete_hsp = TRUE;
      } else {
         hsp->query.length = new_q_end - new_q_start;
         hsp->subject.length = new_s_end - new_s_start;
         hsp->query.offset = new_q_start - query_start;
         hsp->query.end = hsp->query.offset + hsp->query.length;
         hsp->subject.offset = new_s_start - subject_start;
         hsp->subject.end = hsp->subject.offset + hsp->subject.length;
         /* Make corrections in edit block and free any parts that
            are no longer needed */
         if (first_esp != hsp->gap_info->esp) {
            GapEditScriptDelete(hsp->gap_info->esp);
            hsp->gap_info->esp = first_esp;
         }
         if (last_esp->next != NULL) {
            GapEditScriptDelete(last_esp->next);
            last_esp->next = NULL;
         }
         last_esp->num = last_esp_num;
         BlastHSPGetNumIdentical(query_start, subject_start, hsp, 
            hit_options->is_gapped, &hsp->num_ident, &align_length);
         /* Check if this HSP passes the percent identity test */
         if (((FloatHi)hsp->num_ident) / align_length * 100 < 
             hit_options->percent_identity)
            delete_hsp = TRUE;
      }

      if (delete_hsp) { /* This HSP is now below the cutoff */
         if (first_esp != NULL && first_esp != hsp->gap_info->esp)
            GapEditScriptDelete(first_esp);
         hsp_array[index] = BlastHSPFree(hsp_array[index]);
         purge = TRUE;
      }
   }
    
   if (purge) {
      index = BlastHSPArrayPurge(hsp_array, hspcnt);
      hsp_list->hspcnt = index;	
   }
   
   /* Check for HSP inclusion once more */
   if (hsp_list->hspcnt > 1)
      status = BlastSortUniqHspArray(hsp_list);

   BlastSequenceBlkFree(seq_arg.seq);
   subject_blk->sequence = NULL;

   return status;
}


/** This is a copy of a static function from ncbimisc.c.
 * Turns array into a heap with respect to a given comparison function.
 */
static void
heapify (CharPtr base0, CharPtr base, CharPtr lim, CharPtr last, size_t width, int (*compar )(const void*, const void* ))
{
   size_t i;
   char   ch;
   CharPtr left_son, large_son;
   
   left_son = base0 + 2*(base-base0) + width;
   while (base <= lim) {
      if (left_son == last)
         large_son = left_son;
      else
         large_son = (*compar)(left_son, left_son+width) >= 0 ?
            left_son : left_son+width;
      if ((*compar)(base, large_son) < 0) {
         for (i=0; i<width; ++i) {
            ch = base[i];
            base[i] = large_son[i];
            large_son[i] = ch;
         }
         base = large_son;
         left_son = base0 + 2*(base-base0) + width;
      } else
         break;
   }
}

/** The first part of qsort: create heap only, without sorting it
 * @param b An array [in] [out]
 * @param nel Number of elements in b [in]
 * @param width The size of each element [in]
 */
static void CreateHeap (VoidPtr b, size_t nel, size_t width, 
   int (*compar )(const void*, const void* ))   
{
   CharPtr    base = (CharPtr)b;
   size_t i;
   CharPtr    base0 = (CharPtr)base, lim, basef;
   
   if (nel < 2)
      return;
   
   lim = &base[((nel-2)/2)*width];
   basef = &base[(nel-1)*width];
   i = nel/2;
   for (base = &base0[(i - 1)*width]; i > 0; base = base - width) {
      heapify(base0, base, lim, basef, width, compar);
      i--;
   }
}

/** Callback for sorting hsp lists by their best evalue/score;
 * It is assumed that the HSP arrays in each hit list are already sorted by 
 * e-value/score.
*/
static int
evalue_compare_hsp_lists(const void* v1, const void* v2)
{
   BlastHSPListPtr h1, h2;
   
   h1 = *(BlastHSPListPtr PNTR) v1;
   h2 = *(BlastHSPListPtr PNTR) v2;
   
   /* If any of the HSP lists is empty, it is considered "worse" than the 
      other */
   if (h1->hspcnt == 0)
      return 1;
   else if (h2->hspcnt == 0)
      return -1;

   if (h1->hsp_array[0]->evalue < h2->hsp_array[0]->evalue)
      return -1;
   if (h1->hsp_array[0]->evalue > h2->hsp_array[0]->evalue)
      return 1;
   
   if (h1->hsp_array[0]->score > h2->hsp_array[0]->score)
      return -1;
   if (h1->hsp_array[0]->score < h2->hsp_array[0]->score)
      return 1;
   
   /* In case of equal best E-values and scores, order will be determined
      by ordinal ids of the subject sequences */
   if (h1->oid > h2->oid)
      return -1;
   if (h1->oid < h2->oid)
      return 1;

   return 0;
}

#define MIN_HSP_ARRAY_SIZE 100
BlastHSPListPtr BlastHSPListNew()
{
   BlastHSPListPtr hsp_list = (BlastHSPListPtr) calloc(1, sizeof(BlastHSPList));
   hsp_list->hsp_array = (BlastHSPPtr PNTR) 
      calloc(MIN_HSP_ARRAY_SIZE, sizeof(BlastHSPPtr));
   hsp_list->hsp_max = INT4_MAX;
   hsp_list->allocated = MIN_HSP_ARRAY_SIZE;

   return hsp_list;
}

BlastHSPListPtr BlastHSPListFree(BlastHSPListPtr hsp_list)
{
   Int4 index;

   for (index = 0; index < hsp_list->hspcnt; ++index) {
      BlastHSPFree(hsp_list->hsp_array[index]);
   }
   sfree(hsp_list->hsp_array);

   sfree(hsp_list);
   return NULL;
}

/** Given a BlastHitListPtr with a heapified HSP list array, remove
 *  the worst scoring HSP list and insert the new HSP list in the heap
 * @param hit_list Contains all HSP lists for a given query [in] [out]
 * @param hsp_list A new HSP list to be inserted into the hit list [in]
 */
static void InsertBlastHSPListInHeap(BlastHitListPtr hit_list, 
                                     BlastHSPListPtr hsp_list)
{
      BlastHSPListFree(hit_list->hsplist_array[0]);
      hit_list->hsplist_array[0] = hsp_list;
      if (hit_list->hsplist_count >= 2) {
         heapify((CharPtr)hit_list->hsplist_array, (CharPtr)hit_list->hsplist_array, 
                 (CharPtr)&hit_list->hsplist_array[(hit_list->hsplist_count-1)/2],
                 (CharPtr)&hit_list->hsplist_array[hit_list->hsplist_count-1],
                 sizeof(BlastHSPListPtr), evalue_compare_hsp_lists);
      }
      hit_list->worst_evalue = 
         hit_list->hsplist_array[0]->hsp_array[0]->evalue;
}

/** Insert a new HSP list into the hit list.
 * Before capacity of the hit list is reached, just add to the end;
 * After that, store in a heap, to ensure efficient insertion and deletion.
 * The heap order is reverse, with worst e-value on top, for convenience
 * of deletion.
 * @param hit_list Contains all HSP lists saved so far [in] [out]
 * @param hsp_list A new HSP list to be inserted into the hit list [in]
 * @param thr_info Thread information, containing the relevant mutexes [in]
*/
static Int2 BLAST_UpdateHitList(BlastHitListPtr hit_list, 
                                BlastHSPListPtr hsp_list, 
                                BlastThrInfoPtr thr_info)
{
#ifdef THREADS_IMPLEMENTED
   /* For MP BLAST we check that no other thread is attempting to insert 
      results. */
   if (thr_info && thr_info->results_mutex)
      NlmMutexLock(thr_info->results_mutex);
#endif
   if (hit_list->hsplist_count < hit_list->hsplist_max) {
      /* If the array of HSP lists for this query is not yet allocated, 
         do it here */
      if (!hit_list->hsplist_array)
         hit_list->hsplist_array = (BlastHSPListPtr PNTR)
            malloc(hit_list->hsplist_max*sizeof(BlastHSPListPtr));
      /* Just add to the end; sort later */
      hit_list->hsplist_array[hit_list->hsplist_count++] = hsp_list;
      hit_list->worst_evalue = 
         MAX(hsp_list->hsp_array[0]->evalue, hit_list->worst_evalue);
   } else if (hsp_list->hsp_array[0]->evalue >= hit_list->worst_evalue) {
      /* This hit list is less significant than any of those already saved;
         discard it */
      BlastHSPListFree(hsp_list);
   } else {
      if (!hit_list->heapified) {
         CreateHeap(hit_list->hsplist_array, hit_list->hsplist_count, 
                    sizeof(BlastHSPListPtr), evalue_compare_hsp_lists);
         hit_list->heapified = TRUE;
      }
      InsertBlastHSPListInHeap(hit_list, hsp_list);
   }

#ifdef THREADS_IMPLEMENTED
   if (thr_info && thr_info->results_mutex)
      NlmMutexUnlock(thr_info->results_mutex);
#endif

   return 0;
}

/** Allocate memory for a hit list of a given size.
 * @param hitlist_size Size of the hit list (number of HSP lists) [in]
 */
static BlastHitListPtr BLAST_HitListNew(Int4 hitlist_size)
{
   BlastHitListPtr new_hitlist = (BlastHitListPtr) 
      calloc(1, sizeof(BlastHitList));
   new_hitlist->hsplist_max = hitlist_size;
   return new_hitlist;
}

/** Deallocate memory for the hit list */
static BlastHitListPtr BLAST_HitListFree(BlastHitListPtr hitlist)
{
   Int4 index;

   if (!hitlist)
      return NULL;

   for (index = 0; index < hitlist->hsplist_count; ++index)
      BlastHSPListFree(hitlist->hsplist_array[index]);
   sfree(hitlist->hsplist_array);
   sfree(hitlist);
   return NULL;
}

static BlastHSPListPtr BlastHSPListDup(BlastHSPListPtr hsp_list)
{
   BlastHSPListPtr new_hsp_list = (BlastHSPListPtr) 
      MemDup(hsp_list, sizeof(BlastHSPList));
   new_hsp_list->hsp_array = (BlastHSPPtr PNTR) 
      MemDup(hsp_list->hsp_array, hsp_list->hspcnt*sizeof(BlastHSPPtr));
   new_hsp_list->allocated = hsp_list->hspcnt;

   return new_hsp_list;
}

/* Documentation in blast_hits.h */
Int2 BLAST_SaveHitlist(Uint1 program, BLAST_SequenceBlkPtr query,
        BLAST_SequenceBlkPtr subject, BlastResultsPtr results, 
        BlastHSPListPtr hsp_list, BlastHitSavingParametersPtr hit_parameters, 
        BlastQueryInfoPtr query_info, BLAST_ScoreBlkPtr sbp, 
        BlastScoringOptionsPtr score_options, const BlastSeqSrcPtr bssp,
        BlastThrInfoPtr thr_info)
{
   Int2 status;
   BlastHSPListPtr PNTR hsp_list_array;
   BlastHSPPtr hsp;
   Int4 index;
   BlastHitSavingOptionsPtr hit_options = hit_parameters->options;
   Uint1 context_factor;

   if (!hsp_list)
      return 0;

   if (program == blast_type_blastn) {
      status = BLAST_ReevaluateWithAmbiguities(hsp_list, query, subject, 
                  hit_options, query_info, sbp, score_options, bssp);
      context_factor = 2;
   } else if (program == blast_type_blastx || 
              program == blast_type_tblastx) {
      context_factor = 6;
   } else {
      context_factor = 1;
   }

   /* Sort the HSPs by e-value */
   if (hsp_list->hspcnt > 1) {
      qsort(hsp_list->hsp_array, hsp_list->hspcnt, sizeof(BlastHSPPtr), 
               evalue_compare_hsps);
   }

   /* *******************************************************************
    * IF THERE WILL BE A CALLBACK TO FORMAT RESULTS ON THE FLY, IT SHOULD
    * BE CALLED HERE.
    * *******************************************************************/
#if 0
   /* FIXME: this is not implemented yet! */
   if (hit_options->handle_results_method != 0) {
      hit_parameters->handle_results(query, subject, hsp_list, hit_options,
                                     query_info, sbp, rdfp);
   }
#endif

   /* Rearrange HSPs into multiple hit lists if more than one query */
   if (results->num_queries > 1) {
      BlastHSPListPtr tmp_hsp_list;
      hsp_list_array = calloc(results->num_queries, sizeof(BlastHSPListPtr));
      for (index = 0; index < hsp_list->hspcnt; index++) {
         hsp = hsp_list->hsp_array[index];
         tmp_hsp_list = hsp_list_array[hsp->context/context_factor];

         if (!tmp_hsp_list) {
            hsp_list_array[hsp->context/context_factor] = tmp_hsp_list = 
               BlastHSPListNew();
            tmp_hsp_list->oid = hsp_list->oid;
            tmp_hsp_list->traceback_done = hsp_list->traceback_done;
         }

         if (!tmp_hsp_list || tmp_hsp_list->do_not_reallocate) {
            tmp_hsp_list = NULL;
         } else if (tmp_hsp_list->hspcnt >= tmp_hsp_list->allocated) {
            BlastHSPPtr PNTR new_hsp_array;
            Int4 new_size = 
               MIN(2*tmp_hsp_list->allocated, tmp_hsp_list->hsp_max);
            if (new_size == tmp_hsp_list->hsp_max)
               tmp_hsp_list->do_not_reallocate = TRUE;
            
            new_hsp_array = realloc(tmp_hsp_list->hsp_array, 
                                    new_size*sizeof(BlastHSPPtr));
            if (!new_hsp_array) {
               tmp_hsp_list->allocated = tmp_hsp_list->hspcnt;
               tmp_hsp_list->do_not_reallocate = TRUE;
               tmp_hsp_list = NULL;
            } else {
               tmp_hsp_list->hsp_array = new_hsp_array;
               tmp_hsp_list->allocated = new_size;
            }
         }
         if (tmp_hsp_list) {
            tmp_hsp_list->hsp_array[tmp_hsp_list->hspcnt++] = hsp;
         } else {
            /* Cannot add more HSPs; free the memory */
            hsp_list->hsp_array[index] = BlastHSPFree(hsp);
         }
      }

      /* Insert the hit list(s) into the appropriate places in the results 
         structure */
      for (index = 0; index < results->num_queries; index++) {
         if (hsp_list_array[index]) {
            if (!results->hitlist_array[index]) {
               results->hitlist_array[index] = 
                  BLAST_HitListNew(hit_options->hitlist_size);
            }
            BLAST_UpdateHitList(results->hitlist_array[index], 
                                hsp_list_array[index], thr_info);
         }
      }
      sfree(hsp_list_array);
      /* All HSPs from the hsp_list structure are now moved to the results 
         structure, so set the HSP count back to 0 */
      hsp_list->hspcnt = 0;
      BlastHSPListFree(hsp_list);
   } else if (hsp_list->hspcnt > 0) {
      /* Single query; save the HSP list directly into the results 
         structure */
      if (!results->hitlist_array[0]) {
         results->hitlist_array[0] = 
            BLAST_HitListNew(hit_options->hitlist_size);
      }
      BLAST_UpdateHitList(results->hitlist_array[0], 
                          hsp_list, thr_info);
   }
   
   return 0; 
}

Int2 BLAST_ResultsInit(Int4 num_queries, BlastResultsPtr PNTR results_ptr)
{
   BlastResultsPtr results;
   Int2 status = 0;

   results = (BlastResultsPtr) malloc(sizeof(BlastResults));

   results->num_queries = num_queries;
   results->hitlist_array = (BlastHitListPtr PNTR) 
      calloc(num_queries, sizeof(BlastHitListPtr));
   
   *results_ptr = results;
   return status;
}

BlastResultsPtr BLAST_ResultsFree(BlastResultsPtr results)
{
   Int4 index;
   for (index = 0; index < results->num_queries; ++index)
      BLAST_HitListFree(results->hitlist_array[index]);
   sfree(results->hitlist_array);
   sfree(results);
   return NULL;
}

Int2 BLAST_SortResults(BlastResultsPtr results)
{
   Int4 index;
   BlastHitListPtr hit_list;

   for (index = 0; index < results->num_queries; ++index) {
      hit_list = results->hitlist_array[index];
      if (hit_list && hit_list->hsplist_count > 1) {
         qsort(hit_list->hsplist_array, hit_list->hsplist_count, 
                  sizeof(BlastHSPListPtr), evalue_compare_hsp_lists);
      }
   }
   return 0;
}

void AdjustOffsetsInHSPList(BlastHSPListPtr hsp_list, Int4 offset)
{
   Int4 index;
   BlastHSPPtr hsp;

   for (index=0; index<hsp_list->hspcnt; index++) {
      hsp = hsp_list->hsp_array[index];
      hsp->subject.offset += offset;
      hsp->subject.end += offset;
      hsp->subject.gapped_start += offset;
      if (hsp->gap_info)
         hsp->gap_info->start2 += offset;
   }
}

/** Comparison callback for sorting HSPs by diagonal */
static int
diag_compare_hsps(const void* v1, const void* v2)
{
   BlastHSPPtr h1, h2;

   h1 = *((BlastHSPPtr PNTR) v1);
   h2 = *((BlastHSPPtr PNTR) v2);
   
   return (h1->query.offset - h1->subject.offset) - 
      (h2->query.offset - h2->subject.offset);
}

#define OVERLAP_DIAG_CLOSE 10
/** Merge the two HSPs if they intersect.
 * @param hsp1 The first HSP [in]
 * @param hsp2 The second HSP [in]
 * @param start The starting offset of the subject coordinates where the 
 *               intersection is possible [in]
*/
static Boolean
BLAST_MergeHsps(BlastHSPPtr hsp1, BlastHSPPtr hsp2, Int4 start)
{
   BLASTHSPSegmentPtr segments1, segments2, new_segment1, new_segment2;
   GapEditScriptPtr esp1, esp2, esp;
   Int4 end = start + DBSEQ_CHUNK_OVERLAP - 1;
   Int4 min_diag, max_diag, num1, num2, dist = 0, next_dist = 0;
   Int4 diag1_start, diag1_end, diag2_start, diag2_end;
   Int4 index;
   Uint1 intersection_found;
   Uint1 op_type = 0;

   if (!hsp1->gap_info || !hsp2->gap_info) {
      /* Assume that this is an ungapped alignment, hence simply compare 
         diagonals. Do not merge if they are on different diagonals */
      if (diag_compare_hsps(&hsp1, &hsp2) == 0 &&
          hsp1->query.end >= hsp2->query.offset) {
         hsp1->query.end = hsp2->query.end;
         hsp1->subject.end = hsp2->subject.end;
         hsp1->query.length = hsp1->query.end - hsp1->query.offset;
         hsp1->subject.length = hsp1->subject.end - hsp1->subject.offset;
         return TRUE;
      } else
         return FALSE;
   }
   /* Find whether these HSPs have an intersection point */
   segments1 = (BLASTHSPSegmentPtr) calloc(1, sizeof(BLASTHSPSegment));
   
   esp1 = hsp1->gap_info->esp;
   esp2 = hsp2->gap_info->esp;
   
   segments1->q_start = hsp1->query.offset;
   segments1->s_start = hsp1->subject.offset;
   while (segments1->s_start < start) {
      if (esp1->op_type == GAPALIGN_INS)
         segments1->q_start += esp1->num;
      else if (segments1->s_start + esp1->num < start) {
         if (esp1->op_type == GAPALIGN_SUB) { 
            segments1->s_start += esp1->num;
            segments1->q_start += esp1->num;
         } else if (esp1->op_type == GAPALIGN_DEL)
            segments1->s_start += esp1->num;
      } else 
         break;
      esp1 = esp1->next;
   }
   /* Current esp is the first segment within the overlap region */
   segments1->s_end = segments1->s_start + esp1->num - 1;
   if (esp1->op_type == GAPALIGN_SUB)
      segments1->q_end = segments1->q_start + esp1->num - 1;
   else
      segments1->q_end = segments1->q_start;
   
   new_segment1 = segments1;
   
   for (esp = esp1->next; esp; esp = esp->next) {
      new_segment1->next = (BLASTHSPSegmentPtr)
         calloc(1, sizeof(BLASTHSPSegment));
      new_segment1->next->q_start = new_segment1->q_end + 1;
      new_segment1->next->s_start = new_segment1->s_end + 1;
      new_segment1 = new_segment1->next;
      if (esp->op_type == GAPALIGN_SUB) {
         new_segment1->q_end += esp->num - 1;
         new_segment1->s_end += esp->num - 1;
      } else if (esp->op_type == GAPALIGN_INS) {
         new_segment1->q_end += esp->num - 1;
         new_segment1->s_end = new_segment1->s_start;
      } else {
         new_segment1->s_end += esp->num - 1;
         new_segment1->q_end = new_segment1->q_start;
      }
   }
   
   /* Now create the second segments list */
   
   segments2 = (BLASTHSPSegmentPtr) calloc(1, sizeof(BLASTHSPSegment));
   segments2->q_start = hsp2->query.offset;
   segments2->s_start = hsp2->subject.offset;
   segments2->q_end = segments2->q_start + esp2->num - 1;
   segments2->s_end = segments2->s_start + esp2->num - 1;
   
   new_segment2 = segments2;
   
   for (esp = esp2->next; esp && new_segment2->s_end < end; 
        esp = esp->next) {
      new_segment2->next = (BLASTHSPSegmentPtr)
         calloc(1, sizeof(BLASTHSPSegment));
      new_segment2->next->q_start = new_segment2->q_end + 1;
      new_segment2->next->s_start = new_segment2->s_end + 1;
      new_segment2 = new_segment2->next;
      if (esp->op_type == GAPALIGN_INS) {
         new_segment2->s_end = new_segment2->s_start;
         new_segment2->q_end = new_segment2->q_start + esp->num - 1;
      } else if (esp->op_type == GAPALIGN_DEL) {
         new_segment2->s_end = new_segment2->s_start + esp->num - 1;
         new_segment2->q_end = new_segment2->q_start;
      } else if (esp->op_type == GAPALIGN_SUB) {
         new_segment2->s_end = new_segment2->s_start + esp->num - 1;
         new_segment2->q_end = new_segment2->q_start + esp->num - 1;
      }
   }
   
   new_segment1 = segments1;
   new_segment2 = segments2;
   intersection_found = 0;
   num1 = num2 = 0;
   while (new_segment1 && new_segment2 && !intersection_found) {
      if (new_segment1->s_end < new_segment2->s_start || 
          new_segment1->q_end < new_segment2->q_start) {
         new_segment1 = new_segment1->next;
         num1++;
         continue;
      }
      if (new_segment2->s_end < new_segment1->s_start || 
          new_segment2->q_end < new_segment1->q_start) {
         new_segment2 = new_segment2->next;
         num2++;
         continue;
      }
      diag1_start = new_segment1->s_start - new_segment1->q_start;
      diag2_start = new_segment2->s_start - new_segment2->q_start;
      diag1_end = new_segment1->s_end - new_segment1->q_end;
      diag2_end = new_segment2->s_end - new_segment2->q_end;
      
      if (diag1_start == diag1_end && diag2_start == diag2_end &&  
          diag1_start == diag2_start) {
         /* Both segments substitutions, on same diagonal */
         intersection_found = 1;
         dist = new_segment2->s_end - new_segment1->s_start + 1;
         break;
      } else if (diag1_start != diag1_end && diag2_start != diag2_end) {
         /* Both segments gaps - must intersect */
         intersection_found = 3;

         dist = new_segment2->s_end - new_segment1->s_start + 1;
         op_type = GAPALIGN_INS;
         next_dist = new_segment2->q_end - new_segment1->q_start - dist + 1;
         if (new_segment2->q_end - new_segment1->q_start < dist) {
            dist = new_segment2->q_end - new_segment1->q_start + 1;
            op_type = GAPALIGN_DEL;
            next_dist = new_segment2->s_end - new_segment1->s_start - dist + 1;
         }
         break;
      } else if (diag1_start != diag1_end) {
         max_diag = MAX(diag1_start, diag1_end);
         min_diag = MIN(diag1_start, diag1_end);
         if (diag2_start >= min_diag && diag2_start <= max_diag) {
            intersection_found = 2;
            dist = diag2_start - min_diag + 1;
            if (new_segment1->s_end == new_segment1->s_start)
               next_dist = new_segment2->s_end - new_segment1->s_end + 1;
            else
               next_dist = new_segment2->q_end - new_segment1->q_end + 1;
            break;
         }
      } else if (diag2_start != diag2_end) {
         max_diag = MAX(diag2_start, diag2_end);
         min_diag = MIN(diag2_start, diag2_end);
         if (diag1_start >= min_diag && diag1_start <= max_diag) {
            intersection_found = 2;
            next_dist = max_diag - diag1_start + 1;
            if (new_segment2->s_end == new_segment2->s_start)
               dist = new_segment2->s_start - new_segment1->s_start + 1;
            else
               dist = new_segment2->q_start - new_segment1->q_start + 1;
            break;
         }
      }
      if (new_segment1->s_end <= new_segment2->s_end) {
         new_segment1 = new_segment1->next;
         num1++;
      }
      if (new_segment1->s_end >= new_segment2->s_end) {
         new_segment2 = new_segment2->next;
         num2++;
      }
   }

   if (intersection_found) {
      esp = NULL;
      for (index = 0; index < num1-1; index++)
         esp1 = esp1->next;
      for (index = 0; index < num2-1; index++) {
         esp = esp2;
         esp2 = esp2->next;
      }
      if (intersection_found < 3) {
         if (num1 > 0)
            esp1 = esp1->next;
         if (num2 > 0) {
            esp = esp2;
            esp2 = esp2->next;
         }
      }
      switch (intersection_found) {
      case 1:
         esp1->num = dist;
         esp1->next = esp2->next;
         esp2->next = NULL;
         break;
      case 2:
         esp1->num = dist;
         esp2->num = next_dist;
         esp1->next = esp2;
         if (esp)
            esp->next = NULL;
         break;
      case 3:
         esp1->num += dist;
         esp2->op_type = op_type;
         esp2->num = next_dist;
         esp1->next = esp2;
         if (esp)
            esp->next = NULL;
         break;
      default: break;
      }
      hsp1->query.end = hsp2->query.end;
      hsp1->subject.end = hsp2->subject.end;
      hsp1->query.length = hsp1->query.end - hsp1->query.offset;
      hsp1->subject.length = hsp1->subject.end - hsp1->subject.offset;
   }

   return (Boolean) intersection_found;
}

/** TRUE if c is between a and b; f between d and f. Determines if the
 * coordinates are already in an HSP that has been evaluated. 
 */
#define CONTAINED_IN_HSP(a,b,c,d,e,f) (((a <= c && b >= c) && (d <= f && e >= f)) ? TRUE : FALSE)

/** Checks if the first HSP is contained in the second. */
static Boolean BLASTHspContained(BlastHSPPtr hsp1, BlastHSPPtr hsp2)
{
   Boolean hsp_start_is_contained=FALSE, hsp_end_is_contained=FALSE;

   if (hsp1->score > hsp2->score || 
       SIGN(hsp2->query.frame) != SIGN(hsp1->query.frame) || 
       SIGN(hsp2->subject.frame) != SIGN(hsp1->subject.frame))
      return FALSE;

   if (CONTAINED_IN_HSP(hsp2->query.offset, hsp2->query.end, hsp1->query.offset, hsp2->subject.offset, hsp2->subject.end, hsp1->subject.offset) == TRUE) {
      hsp_start_is_contained = TRUE;
   }
   if (CONTAINED_IN_HSP(hsp2->query.offset, hsp2->query.end, hsp1->query.end, hsp2->subject.offset, hsp2->subject.end, hsp1->subject.end) == TRUE) {
      hsp_end_is_contained = TRUE;
   }
   
   return (hsp_start_is_contained && hsp_end_is_contained);
}

Int2 MergeHSPLists(BlastHSPListPtr hsp_list, 
        BlastHSPListPtr PNTR combined_hsp_list_ptr, Int4 start,
        Boolean merge_hsps, Boolean append)
{
   BlastHSPListPtr combined_hsp_list = *combined_hsp_list_ptr;
   BlastHSPPtr hsp, PNTR hspp1, PNTR hspp2;
   Int4 index, index1, hspcnt1, hspcnt2, new_hspcnt = 0;
   BlastHSPPtr PNTR new_hsp_array;
   Int4Ptr index_array;
   
   if (!hsp_list || hsp_list->hspcnt == 0)
      return 0;

   /* If no previous HSP list, just return the new one or its copy */
   if (!combined_hsp_list) {
      if (!append) {
         *combined_hsp_list_ptr = BlastHSPListDup(hsp_list);
         hsp_list->hspcnt = 0;
      } else {
         *combined_hsp_list_ptr = hsp_list;
      }
      return 0;
   }

   if (append) {
      /* Just append new list to the end of the old list, in case of 
         multiple frames of the subject sequence */
      new_hspcnt = MIN(combined_hsp_list->hspcnt + hsp_list->hspcnt, 
                       combined_hsp_list->hsp_max);
      if (new_hspcnt > combined_hsp_list->allocated) {
         Int4 new_allocated = MIN(2*new_hspcnt, combined_hsp_list->hsp_max);
         new_hsp_array = (BlastHSPPtr PNTR) 
            realloc(combined_hsp_list->hsp_array, 
                    new_allocated*sizeof(BlastHSPPtr));

         if (new_hsp_array) {
            combined_hsp_list->allocated = new_allocated;
            combined_hsp_list->hsp_array = new_hsp_array;
         } else {
            combined_hsp_list->do_not_reallocate = TRUE;
            return -1;
         }
      }
      
      memcpy(&combined_hsp_list->hsp_array[combined_hsp_list->hspcnt], 
             hsp_list->hsp_array, 
             (new_hspcnt - combined_hsp_list->hspcnt)*sizeof(BlastHSPPtr));
      for (index = new_hspcnt - combined_hsp_list->hspcnt; 
           index < hsp_list->hspcnt; ++index)
         BlastHSPFree(hsp_list->hsp_array[index]);

      combined_hsp_list->hspcnt = new_hspcnt;

      return 1;
   }

   /* Merge the two HSP lists for successive chunks of the subject sequence */
   hspcnt1 = hspcnt2 = 0;
   hspp1 = (BlastHSPPtr PNTR) 
      calloc(combined_hsp_list->hspcnt, sizeof(BlastHSPPtr));
   hspp2 = (BlastHSPPtr PNTR) calloc(hsp_list->hspcnt, sizeof(BlastHSPPtr));
   index_array = (Int4Ptr) calloc(combined_hsp_list->hspcnt, sizeof(Int4));

   for (index=0; index<combined_hsp_list->hspcnt; index++) {
      hsp = combined_hsp_list->hsp_array[index];
      if (hsp->subject.end > start) {
         index_array[hspcnt1] = index;
         hspp1[hspcnt1++] = hsp;
         combined_hsp_list->hsp_array[index] = NULL;
      } else
         new_hspcnt++;
   }
   for (index=0; index<hsp_list->hspcnt; index++) {
      hsp = hsp_list->hsp_array[index];
      if (hsp->subject.offset < start + DBSEQ_CHUNK_OVERLAP) {
         hspp2[hspcnt2++] = hsp;
         hsp_list->hsp_array[index] = NULL;
      } else
         new_hspcnt++;
   }

   qsort(hspp1, hspcnt1, sizeof(BlastHSPPtr), diag_compare_hsps);
   qsort(hspp2, hspcnt2, sizeof(BlastHSPPtr), diag_compare_hsps);

   for (index=0; index<hspcnt1; index++) {
      for (index1=0; index1<hspcnt2; index1++) {
         if (hspp2[index1] && 
             hspp2[index1]->query.frame == hspp1[index]->query.frame &&
             hspp2[index1]->subject.frame == hspp1[index]->subject.frame &&
             ABS(diag_compare_hsps(&hspp1[index], &hspp2[index1])) < 
             OVERLAP_DIAG_CLOSE) {
            if (merge_hsps) {
               if (BLAST_MergeHsps(hspp1[index], hspp2[index1], start)) {
                  /* Remove the second HSP, as it has been merged with the
                     first */
                  hspp2[index1] = BlastHSPFree(hspp2[index1]);
                  break;
               }
            } else { /* No gap information available */
               if (BLASTHspContained(hspp1[index], hspp2[index1])) {
                  hspp1[index] = BlastHSPFree(hspp1[index]);
                  hspp1[index] = hspp2[index1];
                  hspp2[index1] = NULL;
               } else if (BLASTHspContained(hspp2[index1], hspp1[index]))
                  hspp2[index1] = BlastHSPFree(hspp2[index1]);

            }
         }
      }
   }

   new_hspcnt += hspcnt1;
   for (index=0; index<hspcnt2; index++) {
      if (hspp2[index] != NULL)
         new_hspcnt++;
   }
   
   if (new_hspcnt >= combined_hsp_list->allocated-1 && 
       combined_hsp_list->do_not_reallocate == FALSE) {
      new_hsp_array = (BlastHSPPtr PNTR) 
         realloc(combined_hsp_list->hsp_array, 
                 new_hspcnt*2*sizeof(BlastHSPPtr));
      if (new_hsp_array == NULL) {
         combined_hsp_list->do_not_reallocate = TRUE; 
      } else {
         combined_hsp_list->hsp_array = new_hsp_array;
         combined_hsp_list->allocated = 2*new_hspcnt;
      }
   }

   for (index=0; index<hspcnt1; index++) 
      combined_hsp_list->hsp_array[index_array[index]] = hspp1[index];
   for (index=combined_hsp_list->hspcnt, index1=0; 
        index1<hsp_list->hspcnt && index<combined_hsp_list->allocated; 
        index1++) {
      if (hsp_list->hsp_array[index1] != NULL)
         combined_hsp_list->hsp_array[index++] = hsp_list->hsp_array[index1];
   }
   for (index1=0; index1<hspcnt2 && index<combined_hsp_list->allocated;
        index1++) {
      if (hspp2[index1] != NULL)
         combined_hsp_list->hsp_array[index++] = hspp2[index1];
   }
   combined_hsp_list->hspcnt = index;

   /* Free the remaining HSPs in the new list, if reallocation failed */
   if (combined_hsp_list->do_not_reallocate) {
      for ( ; index1 < hsp_list->hspcnt; ++index1)
         BlastHSPFree(hsp_list->hsp_array[index]);
   }

   sfree(hspp1);
   sfree(hspp2);
   sfree(index_array);
   
   return 1;
}
