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

File name: blast_traceback.c

Author: Ilya Dondoshansky

Contents: Traceback functions

Detailed Contents: 

        - Functions responsible for the traceback stage of the BLAST algorithm

******************************************************************************/

#include <blast_traceback.h>
#include <blast_util.h>

static char const rcsid[] = "$Id$";

extern Int4 HspArrayPurge (BlastHSPPtr PNTR hsp_array, 
                       Int4 hspcnt, Boolean clear_num);

#define WINDOW_SIZE 20
static FloatHi 
SumHSPEvalue(Uint1 program_number, BLAST_ScoreBlkPtr sbp, 
   BlastQueryInfoPtr query_info, BLAST_SequenceBlkPtr subject, 
   BlastHitSavingParametersPtr hit_params, 
   BlastHSPPtr head_hsp, BlastHSPPtr hsp, Int4Ptr sumscore)
{
   FloatHi gap_prob, gap_decay_rate, sum_evalue, score_prime;
   Int4 gap_size, num;
   Int4 subject_eff_length, query_eff_length, length_adjustment;
   Int4 context = head_hsp->context;
   FloatHi eff_searchsp;

   gap_size = hit_params->gap_size;
   gap_prob = hit_params->gap_prob;
   gap_decay_rate = hit_params->gap_decay_rate;

   num = head_hsp->num + hsp->num;

   length_adjustment = query_info->length_adjustments[context];

   subject_eff_length = 
      MAX((subject->length - length_adjustment), 1);
   if (program_number == blast_type_tblastn) 
      subject_eff_length /= 3;
   subject_eff_length = MAX(subject_eff_length, 1);
	
   query_eff_length = 
      MAX(BLAST_GetQueryLength(query_info, context) - length_adjustment, 1);
   
   *sumscore = MAX(hsp->score, hsp->sumscore) + 
      MAX(head_hsp->score, head_hsp->sumscore);
   score_prime = *sumscore * sbp->kbp_gap[context]->Lambda;

   sum_evalue =  
      BlastUnevenGapSumE(sbp->kbp_gap[context], 2*WINDOW_SIZE, 
         hit_params->options->longest_intron + WINDOW_SIZE, 
         gap_prob, gap_decay_rate, num, score_prime, 
         query_eff_length, subject_eff_length);

   eff_searchsp = ((FloatHi) subject_eff_length) * query_eff_length;
   
   sum_evalue *= 
      ((FloatHi) query_info->eff_searchsp_array[context]) / eff_searchsp;

   return sum_evalue;
}

/** Sort the HSP's by starting position of the query.  Called by qsort.  
 *	The first function sorts in forward, the second in reverse order.
*/
static int
fwd_compare_hsps(VoidPtr v1, VoidPtr v2)

{
	BlastHSPPtr h1, h2;
	BlastHSPPtr PNTR hp1, PNTR hp2;

	hp1 = (BlastHSPPtr PNTR) v1;
	hp2 = (BlastHSPPtr PNTR) v2;
	h1 = *hp1;
	h2 = *hp2;

	if (SIGN(h1->query.frame) != SIGN(h2->query.frame))
	{
		if (h1->query.frame < h2->query.frame)
			return 1;
		else
			return -1;
	}
	if (h1->query.offset < h2->query.offset) 
		return -1;
	if (h1->query.offset > h2->query.offset) 
		return 1;
	/* Necessary in case both HSP's have the same query offset. */
	if (h1->subject.offset < h2->subject.offset) 
		return -1;
	if (h1->subject.offset > h2->subject.offset) 
		return 1;

	return 0;
}

/* Comparison function based on end position in the query */
static int
end_compare_hsps(VoidPtr v1, VoidPtr v2)

{
	BlastHSPPtr h1, h2;
	BlastHSPPtr PNTR hp1, PNTR hp2;

	hp1 = (BlastHSPPtr PNTR) v1;
	hp2 = (BlastHSPPtr PNTR) v2;
	h1 = *hp1;
	h2 = *hp2;

	if (SIGN(h1->query.frame) != SIGN(h2->query.frame))
	{
		if (h1->query.frame < h2->query.frame)
			return 1;
		else
			return -1;
	}
	if (h1->query.end < h2->query.end) 
		return -1;
	if (h1->query.end > h2->query.end) 
		return 1;
	/* Necessary in case both HSP's have the same query end. */
	if (h1->subject.end < h2->subject.end) 
		return -1;
	if (h1->subject.end > h2->subject.end) 
		return 1;

	return 0;
}

static int
sumscore_compare_hsps(VoidPtr v1, VoidPtr v2)

{
	BlastHSPPtr h1, h2;
	BlastHSPPtr PNTR hp1, PNTR hp2;
   Int4 score1, score2;

	hp1 = (BlastHSPPtr PNTR) v1;
	hp2 = (BlastHSPPtr PNTR) v2;
	h1 = *hp1;
	h2 = *hp2;

	if (h1 == NULL || h2 == NULL)
		return 0;

   score1 = MAX(h1->sumscore, h1->score);
   score2 = MAX(h2->sumscore, h2->score);

	if (score1 < score2) 
		return 1;
	if (score1 > score2)
		return -1;

	return 0;
}


/* The following function should be used only for new tblastn. 
   Current implementation does not allow its use in two sequences BLAST */

#define MAX_SPLICE_DIST 5
static Boolean
FindSpliceJunction(Uint1Ptr subject_seq, BlastHSPPtr hsp1, 
                   BlastHSPPtr hsp2)
{
   Boolean found = FALSE;
   Int4 overlap, length, i;
   Uint1Ptr nt_seq;
   Uint1 g = 4, t = 8, a = 1; /* ncbi4na values for G, T, A respectively */

   overlap = hsp1->query.end - hsp2->query.offset;

   if (overlap >= 0) {
      length = 3*overlap + 2;
      nt_seq = &subject_seq[hsp1->subject.end - 3*overlap];
   } else {
      length = MAX_SPLICE_DIST;
      nt_seq = &subject_seq[hsp1->subject.end];
   }

   for (i=0; i<length-1; i++) {
      if (nt_seq[i] == g && nt_seq[i+1] == t) {
         found = TRUE;
         break;
      }
   }

   if (!found) 
      return FALSE;
   else
      found = FALSE;

   if (overlap >= 0) 
      nt_seq = &subject_seq[hsp2->subject.offset - 2];
   else 
      nt_seq = &subject_seq[hsp2->subject.offset - MAX_SPLICE_DIST];

   for (i=0; i<length-1; i++) {
      if (nt_seq[i] == a && nt_seq[i+1] == g) {
         found = TRUE;
         break;
      }
   }
   return found;
}

/* Find an HSP with offset closest, but not smaller/larger than a given one.
 */
static Int4 hsp_binary_search(BlastHSPPtr PNTR hsp_array, Int4 size, 
                              Int4 offset, Boolean right)
{
   Int4 index, begin, end, coord;
   
   begin = 0;
   end = size;
   while (begin < end) {
      index = (begin + end) / 2;
      if (right) 
         coord = hsp_array[index]->query.offset;
      else
         coord = hsp_array[index]->query.end;
      if (coord >= offset) 
         end = index;
      else
         begin = index + 1;
   }

   return end;
}

static Int2
link_hsps(Uint1 program_number, BlastHSPListPtr hsp_list, 
   BlastQueryInfoPtr query_info, BLAST_SequenceBlkPtr subject,
   BLAST_ScoreBlkPtr sbp, BlastHitSavingParametersPtr hit_params)
{

   return 0;
}

static Int2
new_link_hsps(Uint1 program_number, BlastHSPListPtr hsp_list, 
   BlastQueryInfoPtr query_info, BLAST_SequenceBlkPtr subject,
   BLAST_ScoreBlkPtr sbp, BlastHitSavingParametersPtr hit_params)
{
   BlastHSPPtr PNTR hsp_array;
   BlastHSPPtr PNTR score_hsp_array, PNTR offset_hsp_array, PNTR end_hsp_array;
   BlastHSPPtr hsp, head_hsp, best_hsp, var_hsp;
   Int4 hspcnt, index, index1, i;
   FloatHi best_evalue, evalue;
   Int4 sumscore, best_sumscore;
   Boolean reverse_link;
   Uint1Ptr subject_seq = NULL;
   Int4 longest_intron = hit_params->options->longest_intron;

   hspcnt = hsp_list->hspcnt;
   hsp_array = hsp_list->hsp_array;

   for (index=0; index<hspcnt; index++) {
      hsp_array[index]->num = 1;
      hsp_array[index]->linked_set = FALSE;
      hsp_array[index]->ordering_method = 3;
   }

   score_hsp_array = (BlastHSPPtr PNTR) Malloc(hspcnt*sizeof(BlastHSPPtr));
   offset_hsp_array = (BlastHSPPtr PNTR) Malloc(hspcnt*sizeof(BlastHSPPtr));
   end_hsp_array = (BlastHSPPtr PNTR) Malloc(hspcnt*sizeof(BlastHSPPtr));

   MemCpy(score_hsp_array, hsp_array, hspcnt*sizeof(BlastHSPPtr));
   MemCpy(offset_hsp_array, hsp_array, hspcnt*sizeof(BlastHSPPtr));
   MemCpy(end_hsp_array, hsp_array, hspcnt*sizeof(BlastHSPPtr));
   qsort(offset_hsp_array, hspcnt, sizeof(BlastHSPPtr), fwd_compare_hsps);
   qsort(end_hsp_array, hspcnt, sizeof(BlastHSPPtr), end_compare_hsps);

   qsort(score_hsp_array, hspcnt, sizeof(BlastHSPPtr), 
            sumscore_compare_hsps);
      
   head_hsp = NULL;
   for (index = 0; index < hspcnt && score_hsp_array[index]; ) {
      if (!head_hsp) {
         while (index<hspcnt && score_hsp_array[index] && 
                score_hsp_array[index]->linked_set)
            index++;
         if (index==hspcnt)
            break;
         head_hsp = score_hsp_array[index];
      }
      best_evalue = head_hsp->evalue;
      best_hsp = NULL;
      reverse_link = FALSE;
      
      if (head_hsp->linked_set)
         for (var_hsp = head_hsp, i=1; i<head_hsp->num; 
              var_hsp = var_hsp->next, i++);
      else
         var_hsp = head_hsp;

      index1 = hsp_binary_search(offset_hsp_array, hspcnt,
                                 var_hsp->query.end - WINDOW_SIZE, TRUE);
      while (index1 < hspcnt && offset_hsp_array[index1]->query.offset < 
             var_hsp->query.end + WINDOW_SIZE) {
         hsp = offset_hsp_array[index1++];
         /* If this is already part of a linked set, disregard it */
         if (hsp == var_hsp || hsp == head_hsp || 
             (hsp->linked_set && !hsp->start_of_chain))
            continue;
         /* Check if the subject coordinates are consistent with query */
         if (hsp->subject.offset < var_hsp->subject.end - WINDOW_SIZE || 
             hsp->subject.offset > var_hsp->subject.end + longest_intron)
            continue;
         /* Check if the e-value for the combined two HSPs is better than for
            one of them */
         if ((evalue = SumHSPEvalue(program_number, sbp, query_info, subject, 
                                    hit_params, head_hsp, hsp, &sumscore)) < 
             MIN(best_evalue, hsp->evalue)) {
            best_hsp = hsp;
            best_evalue = evalue;
            best_sumscore = sumscore;
         }
      }
      index1 = hsp_binary_search(end_hsp_array, hspcnt,
                                 head_hsp->query.offset - WINDOW_SIZE, FALSE);
      while (index1 < hspcnt && end_hsp_array[index1]->query.end < 
             head_hsp->query.offset + WINDOW_SIZE) {
         hsp = end_hsp_array[index1++];

         /* Check if the subject coordinates are consistent with query */
         if (hsp == head_hsp || 
             hsp->subject.end > head_hsp->subject.offset + WINDOW_SIZE || 
             hsp->subject.end < head_hsp->subject.offset - longest_intron)
            continue;
         if (hsp->linked_set) {
            for (var_hsp = hsp, i=1; var_hsp->start_of_chain == FALSE; 
                 var_hsp = var_hsp->prev, i++);
            if (i<var_hsp->num || var_hsp == head_hsp)
               continue;
         } else
            var_hsp = hsp;
         /* Check if the e-value for the combined two HSPs is better than for
            one of them */
         if ((evalue = SumHSPEvalue(program_number, sbp, query_info, subject, 
                          hit_params, var_hsp, head_hsp, &sumscore)) < 
             MIN(var_hsp->evalue, best_evalue)) {
            best_hsp = hsp;
            best_evalue = evalue;
            best_sumscore = sumscore;
            reverse_link = TRUE;
         }
      }
         
      /* Link these HSPs together */
      if (best_hsp != NULL) {
         if (!reverse_link) {
            head_hsp->start_of_chain = TRUE;
            head_hsp->sumscore = best_sumscore;
            head_hsp->evalue = best_evalue;
            best_hsp->start_of_chain = FALSE;
            if (head_hsp->linked_set) 
               for (var_hsp = head_hsp, i=1; i<head_hsp->num; 
                    var_hsp = var_hsp->next, i++);
            else 
               var_hsp = head_hsp;
            var_hsp->next = best_hsp;
            best_hsp->prev = var_hsp;
            head_hsp->num += best_hsp->num;
         } else {
            best_hsp->next = head_hsp;
            head_hsp->prev = best_hsp;
            if (best_hsp->linked_set) {
               for (var_hsp = best_hsp; 
                    var_hsp->start_of_chain == FALSE; 
                    var_hsp = var_hsp->prev);
            } else
                  var_hsp = best_hsp;
            var_hsp->start_of_chain = TRUE;
            var_hsp->sumscore = best_sumscore;
            var_hsp->evalue = best_evalue;
            var_hsp->num += head_hsp->num;
            head_hsp->start_of_chain = FALSE;
         }
         
         head_hsp->linked_set = best_hsp->linked_set = TRUE;
         if (reverse_link)
            head_hsp = var_hsp;
      } else {
         head_hsp = NULL;
         index++;
      }
   }
   
   qsort(score_hsp_array, hspcnt, sizeof(BlastHSPPtr), sumscore_compare_hsps);
   /* Get the nucleotide subject sequence in Seq_code_ncbi4na */
   subject_seq = subject->sequence;

   hsp = head_hsp = score_hsp_array[0];
   for (index=0; index<hspcnt; hsp = hsp->next) {
      if (hsp->linked_set) {
         index1 = hsp->num;
         for (i=1; i < index1; i++, hsp = hsp->next) {
            hsp->next->evalue = hsp->evalue; 
            hsp->next->num = hsp->num;
            hsp->next->sumscore = hsp->sumscore;
            if (FindSpliceJunction(subject_seq, hsp, hsp->next)) {
               /* Kludge: ordering_method here would indicate existence of
                  splice junctions(s) */
               hsp->ordering_method++;
               hsp->next->ordering_method++;
            } else {
               hsp->ordering_method--;
               hsp->next->ordering_method--;
            }
         }
      } 
      while (++index < hspcnt)
         if (!score_hsp_array[index]->linked_set ||
             score_hsp_array[index]->start_of_chain)
            break;
      if (index == hspcnt) {
         hsp->next = NULL;
         break;
      }
      hsp->next = score_hsp_array[index];
   }

   MemFree(subject_seq);
   MemFree(score_hsp_array);
   MemFree(offset_hsp_array);
   MemFree(end_hsp_array);

   for (index=0; index<hsp_list->hspcnt; index++) {
      hsp_list->hsp_array[index] = head_hsp;
      head_hsp = head_hsp->next;
   }

   return 0;
}

/** Link HSPs using sum statistics.
 * NOT IMPLEMENTED YET!!!!!!!!!!!!!!
 */
static Int2 
BlastLinkHsps(Uint1 program_number, BlastHSPListPtr hsp_list, 
   BlastQueryInfoPtr query_info, BLAST_SequenceBlkPtr subject, 
   BLAST_ScoreBlkPtr sbp, BlastHitSavingParametersPtr hit_params)
{
	if (hsp_list && hsp_list->hspcnt > 0)
	{
      /* Link up the HSP's for this hsp_list. */
      if ((program_number != blast_type_tblastn &&
           program_number != blast_type_psitblastn) || 
          hit_params->options->longest_intron <= 0)
      {
         link_hsps(program_number, hsp_list, query_info, subject, sbp, 
                   hit_params);
         /* The HSP's may be in a different order than they were before, 
            but hsp contains the first one. */
      } else {
         new_link_hsps(program_number, hsp_list, query_info, subject, sbp, 
                       hit_params);
      }
	}

	return 0;
}

/** Comparison function for sorting HSPs by score. 
 * Ties are broken based on subject sequence offsets.
 */
static int
score_compare_hsps(VoidPtr v1, VoidPtr v2)
{
   BlastHSPPtr h1, h2;
   BlastHSPPtr PNTR hp1, PNTR hp2;
   
   hp1 = (BlastHSPPtr PNTR) v1;
   hp2 = (BlastHSPPtr PNTR) v2;
   h1 = *hp1;
   h2 = *hp2;
   
   if (h1 == NULL || h2 == NULL)
      return 0;
   
   if (h1->score < h2->score) 
      return 1;
   if (h1->score > h2->score)
      return -1;
   
   if( h1->subject.offset < h2->subject.offset )
      return 1;
   if( h1->subject.offset > h2->subject.offset )
      return -1;
   
   if( h1->subject.end < h2->subject.end )
      return 1;
   if( h1->subject.end > h2->subject.end )
      return -1;
   
   return 0;
}

/** TRUE if c is between a and b; f between d and f.  Determines if the
 * coordinates are already in an HSP that has been evaluated. 
*/
#define CONTAINED_IN_HSP(a,b,c,d,e,f) (((a <= c && b >= c) && (d <= f && e >= f)) ? TRUE : FALSE)

static void BLASTCheckHSPInclusion(BlastHSPPtr *hsp_array, Int4 hspcnt, 
                                   Boolean is_ooframe)
{
   Int4 index, index1;
   BlastHSPPtr hsp, hsp1;
   
   for (index = 0; index < hspcnt; index++) {
      hsp = hsp_array[index];
      
      if (hsp == NULL)
         continue;
      
      for (index1 = 0; index1 < index; index1++) {
         hsp1 = hsp_array[index1];
         
         if (hsp1 == NULL)
            continue;
         
         if(is_ooframe) {
            if (SIGN(hsp1->query.frame) != SIGN(hsp->query.frame))
               continue;
         } else {
            if (hsp->context != hsp1->context)
               continue;
         }
         
         /* Check of the start point of this HSP */
         if (CONTAINED_IN_HSP(hsp1->query.offset, hsp1->query.end, 
                hsp->query.offset, hsp1->subject.offset, hsp1->subject.end, 
                hsp->subject.offset) == TRUE) {
            /* Check of the end point of this HSP */
            if (CONTAINED_IN_HSP(hsp1->query.offset, hsp1->query.end, 
                   hsp->query.end, hsp1->subject.offset, hsp1->subject.end, 
                   hsp->subject.end) == TRUE) {
               /* Now checking correct strand */
               if (SIGN(hsp1->query.frame) == SIGN(hsp->query.frame) &&
                   SIGN(hsp1->subject.frame) == SIGN(hsp->subject.frame)){
                  
                  /* If we come here through all these if-s - this
                     mean, that current HSP should be removed. */
                  
                  if(hsp_array[index] != NULL) {
                     hsp_array[index] = BlastHSPFree(hsp_array[index]);
                     break;
                  }
               }
            }
         }
      }
   }
   return;
}

/* Window size used to scan HSP for highest score region, where gapped
 * extension starts. 
 */
#define HSP_MAX_WINDOW 11

/** Function to check that the highest scoring region in an HSP still gives a 
 * positive score. This value was originally calcualted by 
 * GetStartForGappedAlignment but it may have changed due to the introduction of 
 * ambiguity characters. Such a change can lead to 'strange' results from ALIGN. 
*/
static Boolean
BLAST_CheckStartForGappedAlignment (BlastHSPPtr hsp, Uint1Ptr query, 
                                    Uint1Ptr subject, BLAST_ScoreBlkPtr sbp)
{
   Int4 index1, score, start, end, width;
   Uint1Ptr query_var, subject_var;
   Boolean positionBased = (sbp->posMatrix != NULL);
   
   width = MIN((hsp->query.gapped_start-hsp->query.offset), HSP_MAX_WINDOW/2);
   start = hsp->query.gapped_start - width;
   end = MIN(hsp->query.end, hsp->query.gapped_start + width);
   /* Assures that the start of subject is above zero. */
   if ((hsp->subject.gapped_start + start - hsp->query.gapped_start) < 0)
      start -= hsp->subject.gapped_start + (start - hsp->query.gapped_start);
   query_var = query + start;
   subject_var = subject + hsp->subject.gapped_start + 
      (start - hsp->query.gapped_start);
   score=0;
   for (index1=start; index1<end; index1++)
      {
         if (!positionBased)
	    score += sbp->matrix[*query_var][*subject_var];
         else
	    score += sbp->posMatrix[index1][*subject_var];
         query_var++; subject_var++;
      }
   if (score <= 0)
      return FALSE;
   
   return TRUE;
}

/** Function to look for the highest scoring window (of size HSP_MAX_WINDOW)
 * in an HSP and return the middle of this.  Used by the gapped-alignment
 * functions to start the gapped alignments.
*/
static Int4 
BLAST_GetStartForGappedAlignment (BlastHSPPtr hsp, Uint1Ptr query, 
                                  Uint1Ptr subject, BLAST_ScoreBlkPtr sbp)
{
   Int4 index1, max_offset, score, max_score, hsp_end;
   Uint1Ptr query_var, subject_var;
   Boolean positionBased = (sbp->posMatrix != NULL);
   
   if (hsp->query.length <= HSP_MAX_WINDOW) {
      max_offset = hsp->query.offset + hsp->query.length/2;
      return max_offset;
   }
   
   hsp_end = hsp->query.offset + HSP_MAX_WINDOW;
   query_var = query + hsp->query.offset;
   subject_var = subject + hsp->subject.offset;
   score=0;
   for (index1=hsp->query.offset; index1<hsp_end; index1++) {
      if (!positionBased)
         score += sbp->matrix[*query_var][*subject_var];
      else
         score += sbp->posMatrix[index1][*subject_var];
      query_var++; subject_var++;
   }
   max_score = score;
   max_offset = hsp_end - 1;
   hsp_end = hsp->query.end - 
      MAX(0, hsp->query.length - hsp->subject.length);
   for (index1=hsp->query.offset + HSP_MAX_WINDOW; index1<hsp_end; index1++) {
      if (!positionBased) {
         score -= 
         sbp->matrix[*(query_var-HSP_MAX_WINDOW)][*(subject_var-HSP_MAX_WINDOW)];
         score += sbp->matrix[*query_var][*subject_var];
      } else {
         score -= 
            sbp->posMatrix[index1-HSP_MAX_WINDOW][*(subject_var-HSP_MAX_WINDOW)];
         score += sbp->posMatrix[index1][*subject_var];
      }
      if (score > max_score) {
         max_score = score;
         max_offset = index1;
      }
      query_var++; subject_var++;
   }
   if (max_score > 0)
      max_offset -= HSP_MAX_WINDOW/2;
   else 
      max_offset = hsp->query.offset;
   
   return max_offset;
}

static Int2
BlastHSPGetNumIdentical(Uint1Ptr query, Uint1Ptr subject, BlastHSPPtr hsp,
   Boolean gapped_calculation, Int4Ptr num_ident_ptr, Int4Ptr align_length_ptr)
{
   Int4 i, num_ident, align_length, q_off, s_off;
   Int2 context;
   Uint1Ptr q, s;
   GapEditBlockPtr gap_info;
   GapEditScriptPtr esp;

   gap_info = hsp->gap_info;

   if (!gap_info && gapped_calculation)
      return -1;

   context = hsp->context;
   q_off = hsp->query.offset;
   s_off = hsp->subject.offset;

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

static Int2
BlastOOFGetNumIdentical(Uint1Ptr query_seq, Uint1Ptr subject_seq, 
   BlastHSPPtr hsp, Int4Ptr num_ident_ptr, Int4Ptr align_length_ptr)
{
   Int4 i, num_ident, align_length, q_off, s_off;
   Int2 context;
   Uint1Ptr q, s;
   GapEditScriptPtr esp;

   if (!hsp->gap_info)
      return -1;

   context = hsp->context;
   s_off = hsp->query.offset;
   q_off = hsp->subject.offset;

   if (!subject_seq || !query_seq)
      return -1;

   q = &query_seq[q_off];
   s = &subject_seq[s_off];

   num_ident = 0;
   align_length = 0;


   for (esp = hsp->gap_info->esp; esp; esp = esp->next) {
      switch (esp->op_type) {
      case 3: /* Substitution */
         align_length += esp->num;
         for (i=0; i<esp->num; i++) {
            if (*q == *s)
               num_ident++;
            ++q;
            s += CODON_LENGTH;
         }
         break;
      case 6: /* Insertion */
         align_length += esp->num;
         s += esp->num * CODON_LENGTH;
         break;
      case 0: /* Deletion */
         align_length += esp->num;
         q += esp->num;
         break;
      case 1: /* Gap of two nucleotides. */
         s -= 2;
         break;
      case 2: /* Gap of one nucleotide. */
         s -= 1;
         break;
      case 4: /* Insertion of one nucleotide. */
         s += 1;
         break;
      case 5: /* Insertion of two nucleotides. */
         s += 2;
         break;
      default: 
         s += esp->num * CODON_LENGTH;
         q += esp->num;
         break;
      }
   }

   *align_length_ptr = align_length;
   *num_ident_ptr = num_ident;
   return 0;


}

/** Compute gapped alignment with traceback for all HSPs from a single
 * query/subject sequence pair.
 * @param program_number Type of BLAST program [in]
 * @param hsp_list List of HSPs [in]
 * @param query_blk The query sequence [in]
 * @param subject_blk The subject sequence [in]
 * @param query_info Query information, needed to get pointer to the
 *        start of this query within the concatenated sequence [in]
 * @param gap_align Auxiliary structure used for gapped alignment [in]
 * @param sbp Statistical parameters [in]
 * @param score_options Scoring parameters [in]
 * @param hit_params Hit saving parameters [in]
 * @param db_options Options containing database genetic code string [in]
 */
static Int2
BlastHSPListGetTraceback(Uint1 program_number, BlastHSPListPtr hsp_list, 
   BLAST_SequenceBlkPtr query_blk, BLAST_SequenceBlkPtr subject_blk, 
   BlastQueryInfoPtr query_info,
   BlastGapAlignStructPtr gap_align, BLAST_ScoreBlkPtr sbp, 
   BlastScoringOptionsPtr score_options,
   BlastHitSavingParametersPtr hit_params,
   const BlastDatabaseOptionsPtr db_options)
{
   Int4 index, index1, index2;
   Boolean hsp_start_is_contained, hsp_end_is_contained, do_not_do;
   BlastHSPPtr hsp, hsp1=NULL, hsp2;
   Uint1Ptr query, subject, subject_start = NULL;
   Int4 query_length, subject_length, subject_length_orig=0;
   Int4 max_start = MAX_DBSEQ_LEN / 2, start_shift;
   BlastHSPPtr PNTR hsp_array;
   Int4 q_start, s_start, max_offset;
   Boolean keep;
   BlastHitSavingOptionsPtr hit_options = hit_params->options;
   Int4 min_score_to_keep = hit_params->cutoff_score;
   Int4 align_length;
   /** THE FOLLOWING HAS TO BE PASSED IN THE ARGUMENTS!!!!! 
       DEFINED INSIDE TEMPORARILY, ONLY TO ALLOW COMPILATION */
   FloatHi scalingFactor = 1.0;
   Int4 new_hspcnt = 0;
   Boolean is_ooframe = score_options->is_ooframe;
   Int4 context_offset;
   Boolean translate_subject = 
      (program_number == blast_type_tblastn ||
       program_number == blast_type_psitblastn);   
   Uint1Ptr translation_buffer;
   Int4Ptr frame_offsets;
   Uint1Ptr nucl_sequence = NULL;
   BLAST_KarlinBlkPtr PNTR kbp;

   if (hsp_list->hspcnt == 0) {
      return 0;
   }

   hsp_array = hsp_list->hsp_array;
   
   if (translate_subject) {
      if (!db_options)
         return -1;
      nucl_sequence = subject_blk->sequence_start;
      if (is_ooframe) {
         BLAST_GetAllTranslations(nucl_sequence, NCBI4NA_ENCODING,
            subject_blk->length, db_options->gen_code_string,
            &translation_buffer, &frame_offsets, &subject_blk->oof_sequence);
      } else {
         BLAST_GetAllTranslations(nucl_sequence, NCBI4NA_ENCODING,
            subject_blk->length, db_options->gen_code_string, 
            &translation_buffer, &frame_offsets, NULL);
      }
   }

   if (!is_ooframe) {
      query = query_blk->sequence;
      query_length = query_blk->length;
      if (!translate_subject) {
         subject_start = subject_blk->sequence;
         subject_length_orig = subject_blk->length;
      }
   } else {
      /* Out-of-frame gapping: need to use a mixed-frame sequence */
      if (program_number == blast_type_blastx) {
         query = query_blk->oof_sequence + CODON_LENGTH;
         subject = subject_start = subject_blk->sequence;
      } else {
         query = query_blk->sequence;
         subject = subject_start = subject_blk->oof_sequence + CODON_LENGTH;
      }
      query_length = query_blk->length;
      subject_length_orig = subject_blk->length;
   }
   
   for (index=0; index < hsp_list->hspcnt; index++) {
      hsp_start_is_contained = FALSE;
      hsp_end_is_contained = FALSE;
      hsp = hsp_array[index];
      if (is_ooframe && program_number == blast_type_blastx) {
         Int4 context = hsp->context - hsp->context % 3;
         context_offset = query_info->context_offsets[context];
         query = query_blk->oof_sequence + CODON_LENGTH + context_offset;
         query_length = 
            query_info->context_offsets[context+3] - context_offset;
       } else {
         context_offset = query_info->context_offsets[hsp->context];
         query = query_blk->sequence + context_offset;
         query_length = BLAST_GetQueryLength(query_info, hsp->context);
      }
      
      for (index1=0; index1<index; index1++) {
         hsp_start_is_contained = FALSE;
         hsp_end_is_contained = FALSE;
         
         hsp1 = hsp_array[index1];
         if (hsp1 == NULL)
            continue;
         
         if(is_ooframe) {
            if (SIGN(hsp1->query.frame) != SIGN(hsp->query.frame))
               continue;
         } else {
            if (hsp->context != hsp1->context)
               continue;
         }
         
         if (CONTAINED_IN_HSP(hsp1->query.offset, hsp1->query.end, hsp->query.offset, hsp1->subject.offset, hsp1->subject.end, hsp->subject.offset) == TRUE) {
            if (SIGN(hsp1->query.frame) == SIGN(hsp->query.frame) &&
                SIGN(hsp1->subject.frame) == SIGN(hsp->subject.frame))
               hsp_start_is_contained = TRUE;
         }
         if (CONTAINED_IN_HSP(hsp1->query.offset, hsp1->query.end, hsp->query.end, hsp1->subject.offset, hsp1->subject.end, hsp->subject.end) == TRUE) {
            if (SIGN(hsp1->query.frame) == SIGN(hsp->query.frame) &&
                SIGN(hsp1->subject.frame) == SIGN(hsp->subject.frame))
               hsp_end_is_contained = TRUE;
         }
         if (hsp_start_is_contained && hsp_end_is_contained && hsp->score <= hsp1->score) {
            break;
         }
      }
      
      if (translate_subject && !is_ooframe) {
         Int2 context = FrameToContext(hsp->subject.frame);
         subject_start = translation_buffer + frame_offsets[context] + 1;
         subject_length_orig = 
            frame_offsets[context+1] - frame_offsets[context] - 1;
      }

      do_not_do = FALSE;
      
      if (do_not_do == FALSE && (hsp_start_is_contained == FALSE || 
           hsp_end_is_contained == FALSE || hsp->score > hsp1->score)) {
         if (!is_ooframe && 
             (((hsp->query.gapped_start == 0 && 
                hsp->subject.gapped_start == 0) ||
               !BLAST_CheckStartForGappedAlignment(hsp, query, 
                   subject_start, sbp)))) {
            max_offset = BLAST_GetStartForGappedAlignment(hsp, query, 
                            subject_start, sbp);
            q_start = max_offset;
            s_start = 
               (hsp->subject.offset - hsp->query.offset) + max_offset;
            hsp->query.gapped_start = q_start;
            hsp->subject.gapped_start = s_start;
         } else {
            if(is_ooframe) {
               /* Code below should be investigated for possible
                  optimization for OOF */
               s_start = hsp->subject.gapped_start;
               q_start = hsp->query.gapped_start;
               gap_align->subject_start = 0;
               gap_align->query_start = 0;
            } else {
               q_start = hsp->query.gapped_start;
               s_start = hsp->subject.gapped_start;
            }
         }
         
         /* Shift the subject sequence if the offset is very large */
         if (s_start > max_start) {
            start_shift = (s_start / max_start) * max_start;
            s_start %= max_start;
         } else {
            start_shift = 0;
         }
         subject = subject_start + start_shift;

         subject_length = MIN(subject_length_orig - start_shift, 
                              s_start + hsp->subject.length + max_start);

         /* Perform the gapped extension with traceback */
         BLAST_GappedAlignmentWithTraceback(program_number, query, subject, 
            gap_align, score_options, q_start, s_start, query_length, 
            subject_length);
         
         if (gap_align->score >= min_score_to_keep) {
            hsp->subject.offset = gap_align->subject_start + start_shift;
            hsp->query.offset = gap_align->query_start;
            hsp->subject.end = gap_align->subject_stop + start_shift;
            hsp->query.end = gap_align->query_stop;
            
            if (gap_align->edit_block && start_shift > 0) {
               gap_align->edit_block->start2 += start_shift;
               gap_align->edit_block->length2 += start_shift;
            }
            hsp->query.length = hsp->query.end - hsp->query.offset;
            hsp->subject.length = hsp->subject.end - hsp->subject.offset;
            hsp->score = gap_align->score;
            
            hsp->gap_info = gap_align->edit_block;
            gap_align->edit_block = NULL;
            
            if (hsp->gap_info) {
               hsp->gap_info->frame1 = hsp->query.frame;
               hsp->gap_info->frame2 = hsp->subject.frame;
               hsp->gap_info->original_length1 = query_length;
               hsp->gap_info->original_length2 = subject_blk->length;
               if (program_number == blast_type_blastx)
                  hsp->gap_info->translate1 = TRUE;
               if (program_number == blast_type_tblastn)
                  hsp->gap_info->translate2 = TRUE;
            }

            keep = TRUE;
            if (is_ooframe) {
               BlastOOFGetNumIdentical(query, subject, hsp, 
                                       &hsp->num_ident, &align_length);
               /* Adjust subject offsets for negative frames */
               if (hsp->subject.frame < 0) {
                  Int4 strand_start = subject_blk->length + 1;
                  hsp->subject.offset -= strand_start;
                  hsp->subject.end -= strand_start;
                  hsp->subject.gapped_start -= strand_start;
                  hsp->gap_info->start2 -= strand_start;
               }
            } else {
               BlastHSPGetNumIdentical(query, subject, hsp, 
                  score_options->gapped_calculation, &hsp->num_ident, 
                  &align_length);
            }

            if (hsp->num_ident * 100 < 
                align_length * hit_options->percent_identity) {
               keep = FALSE;
            }
            
               

            if (program_number == blast_type_blastp ||
                program_number == blast_type_blastn) {
               if (hit_options->is_gapped && 
                   program_number != blast_type_blastn)
                  kbp = sbp->kbp_gap;
               else
                  kbp = sbp->kbp;

               hsp->evalue = 
                  BlastKarlinStoE_simple(hsp->score, kbp[hsp->context],
                     (FloatHi)query_info->eff_searchsp_array[hsp->context]);
               if (hsp->evalue > hit_options->expect_value) 
                  /* put in for comp. based stats. */
                  keep = FALSE;
            }

            if (scalingFactor != 0.0 && scalingFactor != 1.0) {
               /* Scale down score for blastp and tblastn. */
               hsp->score = (Int4)
                  (hsp->score+(0.5*scalingFactor))/scalingFactor;
            }
            /* only one alignment considered for blast[np]. */
            /* This may be changed by LinkHsps for blastx or tblastn. */
            hsp->num = 1;
            if ((program_number == blast_type_tblastn ||
                 program_number == blast_type_psitblastn) && 
                hit_options->longest_intron > 0) {
               hsp->evalue = 
                  BlastKarlinStoE_simple(hsp->score, sbp->kbp_gap[hsp->context],
                     (FloatHi) query_info->eff_searchsp_array[hsp->context]);
            }

            for (index2=0; index2<index && keep == TRUE; index2++) {
               hsp2 = hsp_array[index2];
               if (hsp2 == NULL)
                  continue;
               
               /* Check if both HSP's start or end on the same diagonal 
                  (and are on same strands). */
               if (((hsp->query.offset == hsp2->query.offset &&
                     hsp->subject.offset == hsp2->subject.offset) ||
                    (hsp->query.end == hsp2->query.end &&
                     hsp->subject.end == hsp2->subject.end))  &&
                   hsp->context == hsp2->context &&
                   hsp->subject.frame == hsp2->subject.frame) {
                  if (hsp2->score > hsp->score) {
                     keep = FALSE;
                     break;
                  } else {
                     new_hspcnt--;
                     hsp_array[index2] = BlastHSPFree(hsp2);
                  }
               }
            }
            
            if (keep) {
               new_hspcnt++;
            } else {
               hsp_array[index] = BlastHSPFree(hsp);
            }
         } else {
            /* Score is below threshold */
            gap_align->edit_block = GapEditBlockDelete(gap_align->edit_block);
            hsp_array[index] = BlastHSPFree(hsp);
         }
      } else { 
         /* Contained within another HSP, delete. */
         hsp_array[index] = BlastHSPFree(hsp);
      }
   } /* End loop on HSPs */

    /* Now try to detect simular alignments */

    BLASTCheckHSPInclusion(hsp_array, hsp_list->hspcnt, is_ooframe);
    
    /* Make up fake hitlist, relink and rereap. */

    if (program_number == blast_type_blastx ||
        program_number == blast_type_tblastn ||
        program_number == blast_type_psitblastn) {
        hsp_list->hspcnt = HspArrayPurge(hsp_array, hsp_list->hspcnt, FALSE);
        
        if (hit_options->do_sum_stats == TRUE) {
           BlastLinkHsps(program_number, hsp_list, query_info, subject_blk,
                         sbp, hit_params);
        } else {
           BLAST_GetNonSumStatsEvalue(program_number, query_info, hsp_list, 
                                      hit_options, sbp);
        }
        
        BLAST_ReapHitlistByEvalue(hsp_list, hit_options);
    }
    
    new_hspcnt = HspArrayPurge(hsp_array, hsp_list->hspcnt, FALSE);
    
    qsort(hsp_array,new_hspcnt,sizeof(BlastHSPPtr), score_compare_hsps);
    
    /* Remove extra HSPs if there is a user proveded limit on the number 
       of HSPs per database sequence */
    if (hit_options->hsp_num_max > new_hspcnt) {
       for (index=new_hspcnt; index<hit_options->hsp_num_max; ++index) {
          hsp_array[index] = BlastHSPFree(hsp_array[index]);
       }
       new_hspcnt = MIN(new_hspcnt, hit_options->hsp_num_max);
    }

    hsp_list->hspcnt = new_hspcnt;

    return 0;
}

static Uint1 GetTracebackEncoding(Uint1 program_number) 
{
   Uint1 encoding;

   switch (program_number) {
   case blast_type_blastn:
      encoding = BLASTNA_ENCODING;
      break;
   case blast_type_blastp:
      encoding = BLASTP_ENCODING;
      break;
   case blast_type_blastx:
      encoding = BLASTP_ENCODING;
      break;
   case blast_type_tblastn:
      encoding = NCBI4NA_ENCODING;
      break;
   case blast_type_tblastx:
      encoding = NCBI4NA_ENCODING;
      break;
   default:
      encoding = ERROR_ENCODING;
      break;
   }
   return encoding;
}

Int2 BLAST_ComputeTraceback(Uint1 program_number, BlastResultsPtr results, 
        BLAST_SequenceBlkPtr query, BlastQueryInfoPtr query_info, 
        const BlastSeqSrcPtr bssp, BlastGapAlignStructPtr gap_align,
        BlastScoringOptionsPtr score_options,
        BlastExtensionParametersPtr ext_params,
        BlastHitSavingParametersPtr hit_params,
        const BlastDatabaseOptionsPtr db_options)
{
   Int2 status = 0;
   Int4 query_index, subject_index;
   BlastHitListPtr hit_list;
   BlastHSPListPtr hsp_list;
   BLAST_ScoreBlkPtr sbp;
   Uint1 encoding;
   GetSeqArg seq_arg;
   
   if (!results || !query_info || !bssp) {
      return 0;
   }
   
   /* Set the raw X-dropoff value for the final gapped extension with 
      traceback */
   gap_align->gap_x_dropoff = ext_params->gap_x_dropoff_final;
   /* For traceback, dynamic programming structure will be allocated on
      the fly, proportionally to the subject length */
   gap_align->dyn_prog = MemFree(gap_align->dyn_prog);

   sbp = gap_align->sbp;
   
   encoding = GetTracebackEncoding(program_number);
   MemSet((void*) &seq_arg, 0, sizeof(seq_arg));

   for (query_index = 0; query_index < results->num_queries; ++query_index) {
      hit_list = results->hitlist_array[query_index];

      if (!hit_list)
         continue;
      for (subject_index = 0; subject_index < hit_list->hsplist_count;
           ++subject_index) {
         hsp_list = hit_list->hsplist_array[subject_index];
         if (!hsp_list)
            continue;

         if (!hsp_list->traceback_done) {
            seq_arg.oid = hsp_list->oid;
            seq_arg.encoding = encoding;
            BlastSequenceBlkClean(seq_arg.seq);
            if (BLASTSeqSrcGetSequence(bssp, (void*) &seq_arg) < 0)
                continue;

            BlastHSPListGetTraceback(program_number, hsp_list, query, 
               seq_arg.seq, query_info, gap_align, sbp, score_options, 
               hit_params, db_options);
         }
      }
   }

   /* Re-sort the hit lists according to their best e-values, because
      they could have changed */
   BLAST_SortResults(results);
   BlastSequenceBlkFree(seq_arg.seq);

   return status;
}

Int2 BLAST_TwoSequencesTraceback(Uint1 program_number, 
        BlastResultsPtr results, 
        BLAST_SequenceBlkPtr query, BlastQueryInfoPtr query_info, 
        BLAST_SequenceBlkPtr subject, 
        BlastGapAlignStructPtr gap_align,
        BlastScoringOptionsPtr score_options,
        BlastExtensionParametersPtr ext_params,
        BlastHitSavingParametersPtr hit_params,
        const BlastDatabaseOptionsPtr db_options)
{
   Int2 status = 0;
   Int4 query_index;
   BlastHitListPtr hit_list;
   BlastHSPListPtr hsp_list;
   BLAST_ScoreBlkPtr sbp;
   Uint1 encoding=ERROR_ENCODING;
   Boolean db_is_na;
   
   if (!results || !query_info || !subject) {
      return 0;
   }
   
   db_is_na = (program_number != blast_type_blastp && 
               program_number != blast_type_blastx);

   /* Set the raw X-dropoff value for the final gapped extension with 
      traceback */
   gap_align->gap_x_dropoff = ext_params->gap_x_dropoff_final;
   /* For traceback, dynamic programming structure will be allocated on
      the fly, proportionally to the subject length */
   gap_align->dyn_prog = MemFree(gap_align->dyn_prog);

   sbp = gap_align->sbp;

   encoding = GetTracebackEncoding(program_number);

   if (db_is_na) {
      /* Two sequences case: free the compressed sequence */
      MemFree(subject->sequence);
      subject->sequence = subject->sequence_start + 1;
      subject->sequence_allocated = FALSE;
   }

   for (query_index = 0; query_index < results->num_queries; ++query_index) {
      hit_list = results->hitlist_array[query_index];

      if (!hit_list)
         continue;
      hsp_list = *hit_list->hsplist_array;
      if (!hsp_list)
         continue;

      if (!hsp_list->traceback_done) {
         BlastHSPListGetTraceback(program_number, hsp_list, query, subject, 
            query_info, gap_align, sbp, score_options, hit_params, db_options);

      }
   }

   return status;
}
