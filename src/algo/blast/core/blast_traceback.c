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

******************************************************************************
 * $Revision$
 * */

#include <blast_traceback.h>
#include <blast_util.h>

extern Int4 LIBCALL HspArrayPurge PROTO((BlastHSPPtr PNTR hsp_array, 
                       Int4 hspcnt, Boolean clear_num));

/** Link HSPs using sum statistics.
 * NOT IMPLEMENTED YET!!!!!!!!!!!!!!
 */
static Int2 BlastLinkHsps(BlastHSPListPtr hsp_list)
{
   return 0;
}


/** This should be defined somewhere else!!!!!!!!!!!!! */
#define BLAST_SMALL_GAPS 0

/** Given a BlastHSP structure, create a score set to be inserted into the 
 *  SeqAlign.
 */
static ScorePtr 
GetScoreSetFromBlastHsp(Uint1 program, BlastHSPPtr hsp,  
   BLAST_ScoreBlkPtr sbp, BlastHitSavingOptionsPtr hit_options)
{
   ScorePtr	score_set=NULL;
   Nlm_FloatHi	prob;
   Int4		score;
   CharPtr	scoretype;
   BLAST_KarlinBlkPtr kbp;

   score = hsp->score;
   if (score > 0)
      MakeBlastScore(&score_set, "score", 0.0, score);

   score = hsp->num;
   scoretype = "sum_n";
   
   if (score > 1)
      MakeBlastScore(&score_set, scoretype, 0.0, score);
   
   prob = hsp->evalue;
   if (hsp->num == 1) {
      scoretype = "e_value";
   } else {
      scoretype = "sum_e";
   }
   if (prob >= 0.) {
      if (prob < 1.0e-180)
         prob = 0.0;
      MakeBlastScore(&score_set, scoretype, prob, 0);
   }

   if (program == blast_type_blastn || !hit_options->is_gapped) {
      kbp = sbp->kbp[hsp->context];
   } else {
      kbp = sbp->kbp_gap[hsp->context];
   }

   /* Calculate bit score from the raw score */
   prob = ((hsp->score*kbp->Lambda) - kbp->logK)/NCBIMATH_LN2;
   if (prob >= 0.)
      MakeBlastScore(&score_set, "bit_score", prob, 0);
   
   if (hsp->num_ident > 0)
      MakeBlastScore(&score_set, "num_ident", 0.0, hsp->num_ident);
   
   if (hsp->num > 1 && hsp->ordering_method == BLAST_SMALL_GAPS) {
      MakeBlastScore(&score_set, "small_gap", 0.0, 1);
   } else if (hsp->ordering_method > 3) { 
      /* In new tblastn this means splice junction was found */
      MakeBlastScore(&score_set, "splice_junction", 0.0, 1);
   }

   return score_set;
}

/** Add information on what gis to use to the score set */
static Int2 AddGiListToScoreSet(ScorePtr score_set, ValNodePtr gi_list)

{
   while (gi_list) {
      MakeBlastScore(&score_set, "use_this_gi", 0.0, gi_list->data.intvalue);
      gi_list = gi_list->next;
   }
   
   return 0;
}

static Int2 
BLAST_GapInfoToSeqAlign(Uint1 program, BlastHSPListPtr hsp_list, 
   SeqIdPtr query_id, SeqIdPtr subject_id, BLAST_ScoreBlkPtr sbp, 
   BlastHitSavingOptionsPtr hit_options, SeqAlignPtr PNTR seqalign)
{
   Int2 status = 0;
   BlastHSPPtr PNTR hsp_array;
   SeqAlignPtr seqalign_var = NULL;
   Int4 index;

   hsp_array = hsp_list->hsp_array;

   for (index=0; index<hsp_list->hspcnt; index++) { 
      if (index==0) {
         *seqalign = seqalign_var =
            GapXEditBlockToSeqAlign(hsp_array[index]->gap_info, 
                                    subject_id, query_id);
      } else {
         seqalign_var->next = 
            GapXEditBlockToSeqAlign(hsp_array[index]->gap_info, 
                                    subject_id, query_id); 
         
	 seqalign_var = seqalign_var->next;
      }
      seqalign_var->score = 
         GetScoreSetFromBlastHsp(program, hsp_array[index], sbp, 
                                 hit_options);
      /* The following can only happen for translated BLAST */
      if (seqalign_var->segtype == 3) {
	 StdSegPtr ssp = seqalign_var->segs;
	 while (ssp) {
	    ssp->scores = GetScoreSetFromBlastHsp(program, hsp_array[index],
                             sbp, hit_options);
	    ssp = ssp->next;
	 }
      } else if (seqalign_var->segtype == 5) { /* Discontinuous */
	 SeqAlignPtr sap = (SeqAlignPtr) seqalign_var->segs;
	 for(;sap != NULL; sap = sap->next) {
	    sap->score = GetScoreSetFromBlastHsp(program, hsp_array[index],
                            sbp, hit_options);
	 }
      }
   }

   return status;
}

/** Comparison function for sorting HSPs by score. 
 * Ties are broken based on subject sequence offsets.
 */
static int LIBCALLBACK
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
                     hsp_array[index]->gap_info = 
                        GapXEditBlockDelete(hsp_array[index]->gap_info);
                     hsp_array[index] = MemFree(hsp_array[index]);
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
   GapXEditBlockPtr gap_info;
   GapXEditScriptPtr esp;

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

/** Compute gapped alignment with traceback for all HSPs from a single
 * query/subject sequence pair.
 * @param hsp_list List of HSPs [in]
 * @param query_blk The query sequence [in]
 * @param subject_blk The subject sequence [in]
 * @param query_info Query information, needed to get pointer to the
 *        start of this query within the concatenated sequence [in]
 * @param gap_align Auxiliary structure used for gapped alignment [in]
 * @param sbp Statistical parameters [in]
 * @param score_options Scoring parameters [in]
 * @param hit_params Hit saving parameters [in]
 * @param head_seqalign Pointer to the resulting seqalign list [out]
 */
static Int2
BlastHSPListGetTraceback(BlastHSPListPtr hsp_list, 
   BLAST_SequenceBlkPtr query_blk, BLAST_SequenceBlkPtr subject_blk, 
   BlastQueryInfoPtr query_info,
   BlastGapAlignStructPtr gap_align, BLAST_ScoreBlkPtr sbp, 
   BlastScoringOptionsPtr score_options, 
   BlastHitSavingParametersPtr hit_params,
   SeqAlignPtr PNTR head_seqalign)
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
   Int4 min_score_to_keep = hit_options->cutoff_score;
   Int4 align_length;
   /** THE FOLLOWING HAS TO BE PASSED IN THE ARGUMENTS!!!!! 
       DEFINED INSIDE TEMPORARILY, ONLY TO ALLOW COMPILATION */
   FloatHi scalingFactor = 1.0;
   Int4 new_hspcnt = 0;
   SeqAlignPtr last_seqalign = NULL, seqalign;
   Boolean is_ooframe = score_options->is_ooframe;
   Uint1 program = gap_align->program;
   Int4 context_offset;
   Boolean translate_subject = 
      (gap_align->program == blast_type_tblastn ||
       gap_align->program == blast_type_psitblastn);   
   Uint1Ptr PNTR translated_sequence = NULL, PNTR translated_sequence_orig;
   Int4Ptr translated_length = 0, translated_length_orig;
   Uint1Ptr nucl_sequence = NULL, nucl_sequence_rev = NULL;
   CharPtr genetic_code = NULL;

   *head_seqalign = NULL;

   if (hsp_list->hspcnt == 0) {
      return 0;
   }

   hsp_array = hsp_list->hsp_array;
   
   if (!is_ooframe) {
      query = query_blk->sequence;
      query_length = query_blk->length;
      if (!translate_subject) {
         subject_start = subject_blk->sequence;
         subject_length_orig = subject_blk->length;
      } else {
         ValNodePtr vnp;
         GeneticCodePtr gcp;
         /** THIS SHOULD BE CHANGED!!! Database genetic code must be passed
            in one of the options structures */
         gcp = GeneticCodeFind(1, NULL);
         for (vnp = (ValNodePtr)gcp->data.ptrvalue; vnp != NULL;
              vnp = vnp->next) {
            if (vnp->choice == 3) {  /* ncbieaa */
               genetic_code = (CharPtr)vnp->data.ptrvalue;
               break;
            }
         }
         nucl_sequence = subject_blk->sequence_start;
         GetReverseNuclSequence(nucl_sequence, subject_blk->length, 
                                &nucl_sequence_rev);
      }
   } else {
      /* For out-of-frame gapping, swith query and subject sequences */
      subject_start = subject = query_blk->sequence;
      subject_length_orig = query_blk->length;
      query = subject_blk->sequence;
      query_length = subject_blk->length;
   }
   
   for (index=0; index < hsp_list->hspcnt; index++) {
      hsp_start_is_contained = FALSE;
      hsp_end_is_contained = FALSE;
      hsp = hsp_array[index];
      context_offset = query_info->context_offsets[hsp->context];
      query = &(query_blk->sequence[context_offset]);
      query_length =
         query_info->context_offsets[hsp->context+1] - context_offset - 1;
      
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
      
      if (translate_subject) {
         if (translated_sequence == NULL) {
            translated_sequence_orig = 
               (Uint1Ptr PNTR) MemNew(8*sizeof(Uint1Ptr));
            translated_sequence = translated_sequence_orig + 3;
            translated_length_orig = MemNew(8*sizeof(Int4));
            translated_length = translated_length_orig + 3;
         }
         if (translated_sequence[hsp->subject.frame] == NULL) {
            translated_sequence[hsp->subject.frame] =
               BLAST_GetTranslation(nucl_sequence, nucl_sequence_rev, 
                  subject_blk->length, hsp->subject.frame, 
                  &translated_length[hsp->subject.frame], genetic_code);
         }
         subject_start = translated_sequence[hsp->subject.frame] + 1;
         subject_length_orig = translated_length[hsp->subject.frame];
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
               /* Code above should be investigated for possible
                  optimization for OOF */
               q_start = hsp->subject.gapped_start;
               s_start = hsp->query.gapped_start;
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
         BLAST_GappedAlignmentWithTraceback(query, subject, gap_align,
            score_options, q_start, s_start, query_length, subject_length);
         
         if (gap_align->score >= min_score_to_keep) {
            if(is_ooframe) {
               hsp->query.offset = gap_align->subject_start + start_shift;
               hsp->subject.offset = gap_align->query_start;
               /* The end is one further for BLAST than for the gapped align */
               hsp->query.end = gap_align->subject_stop + 1 + start_shift;
               hsp->subject.end = gap_align->query_stop + 1;
            } else {
               hsp->query.offset = gap_align->query_start;
               hsp->subject.offset = gap_align->subject_start + start_shift;
               /* The end is one further for BLAST than for the gapped align */
               hsp->query.end = gap_align->query_stop + 1;
               hsp->subject.end = gap_align->subject_stop + 1 + start_shift;
            }
            
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
               if (is_ooframe) {
                  hsp->gap_info->frame2 = hsp->query.frame;
                  hsp->gap_info->frame1 = hsp->subject.frame;
               } else {
                  hsp->gap_info->frame1 = hsp->query.frame;
                  hsp->gap_info->frame2 = hsp->subject.frame;
               }
               if (program == blast_type_blastx)
                  hsp->gap_info->translate1 = TRUE;
               if (program == blast_type_tblastn)
                  hsp->gap_info->translate2 = TRUE;
            }

            keep = TRUE;
            if (program == blast_type_blastp ||
                program == blast_type_blastn) {
               hsp->evalue = 
                  BlastKarlinStoE_simple(hsp->score, sbp->kbp[hsp->context],
                     query_info->eff_searchsp_array[hsp->context]);
               if (hsp->evalue > hit_options->expect_value) 
                  /* put in for comp. based stats. */
                  keep = FALSE;
            }

            BlastHSPGetNumIdentical(query, subject, hsp, 
               score_options->gapped_calculation, &hsp->num_ident, 
               &align_length);
            if (hsp->num_ident * 100 < 
                align_length * hit_options->percent_identity) {
               keep = FALSE;
            }
            
            if (scalingFactor != 0.0 && scalingFactor != 1.0) {
               /* Scale down score for blastp and tblastn. */
               hsp->score = (hsp->score+(0.5*scalingFactor))/scalingFactor;
            }
            /* only one alignment considered for blast[np]. */
            /* This may be changed by LinkHsps for blastx or tblastn. */
            hsp->num = 1;
            if ((program == blast_type_tblastn ||
                 program == blast_type_psitblastn) && 
                hit_options->longest_intron > 0) {
               hsp->evalue = 
                  BlastKarlinStoE_simple(hsp->score, sbp->kbp[hsp->context],
                     query_info->eff_searchsp_array[hsp->context]);
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
                     hsp_array[index2]->gap_info = 
                        GapXEditBlockDelete(hsp_array[index2]->gap_info);
                     hsp_array[index2] = MemFree(hsp_array[index2]);
                  }
               }
            }
            
            if (keep) {
               new_hspcnt++;
            } else {
               hsp_array[index] = MemFree(hsp_array[index]);
               gap_align->edit_block = GapXEditBlockDelete(gap_align->edit_block);
            }
         } else {	/* Should be kept? */
                hsp_array[index] = MemFree(hsp_array[index]);
                gap_align->edit_block = GapXEditBlockDelete(gap_align->edit_block);
         }
      } else { /* Contained within another HSP, delete. */
         hsp_array[index]->gap_info = GapXEditBlockDelete(hsp_array[index]->gap_info);
         hsp_array[index] = MemFree(hsp_array[index]);
      }
   } /* End loop on HSPs */

    /* Now try to detect simular alignments */

    BLASTCheckHSPInclusion(hsp_array, hsp_list->hspcnt, is_ooframe);
    
    /* Make up fake hitlist, relink and rereap. */

    if (program == blast_type_blastx ||
        program == blast_type_tblastn ||
        program == blast_type_psitblastn) {
        hsp_list->hspcnt = HspArrayPurge(hsp_array, hsp_list->hspcnt, FALSE);
        
        if (hit_options->do_sum_stats == TRUE) {
           BlastLinkHsps(hsp_list);
        } else {
           BLAST_GetNonSumStatsEvalue(program, query_info, hsp_list, 
                                      hit_options, sbp);
        }
        
        BLAST_ReapHitlistByEvalue(hsp_list, hit_options);
    }
    
    new_hspcnt = HspArrayPurge(hsp_array, hsp_list->hspcnt, FALSE);
    
    HeapSort(hsp_array,new_hspcnt,sizeof(BlastHSPPtr), score_compare_hsps);
    
    /* Remove extra HSPs if there is a user proveded limit on the number 
       of HSPs per database sequence */
    if (hit_options->hsp_num_max > new_hspcnt) {
       for (index=new_hspcnt; index<hit_options->hsp_num_max; ++index) {
          hsp_array[index]->gap_info = 
             GapXEditBlockDelete(hsp_array[index]->gap_info);
          hsp_array[index] = MemFree(hsp_array[index]);
       }
       new_hspcnt = MIN(new_hspcnt, hit_options->hsp_num_max);
    }

    for (index=0; index<new_hspcnt; index++) {
       hsp = hsp_array[index];

       if(is_ooframe) {
          hsp->gap_info->original_length1 = subject_blk->length;
          hsp->gap_info->original_length2 = query_blk->length;
          seqalign = 
             OOFGapXEditBlockToSeqAlign(hsp->gap_info, subject_blk->seqid, 
                                        query_blk->seqid, query_blk->length);
       } else {
          hsp->gap_info->original_length1 = query_blk->length;
          hsp->gap_info->original_length2 = subject_blk->length;
          seqalign = 
             GapXEditBlockToSeqAlign(hsp->gap_info, subject_blk->seqid, 
                                     query_blk->seqid); 
       }
                
       seqalign->score = GetScoreSetFromBlastHsp(program, hsp, sbp, 
                                                 hit_options);
                
       if (seqalign) {
          if (!last_seqalign) {
             *head_seqalign = last_seqalign = seqalign;
          } else {
             last_seqalign->next = seqalign;
             last_seqalign = last_seqalign->next;
          }
       }
    }
        
    return 0;
}

Int2 BLAST_ComputeTraceback(BlastResultsPtr results, 
        BLAST_SequenceBlkPtr query, BlastQueryInfoPtr query_info, 
        ReadDBFILEPtr rdfp, BLAST_SequenceBlkPtr subject, 
        BlastGapAlignStructPtr gap_align,
        BlastScoringOptionsPtr score_options, 
        BlastHitSavingParametersPtr hit_params,  
        SeqAlignPtr PNTR head_seqalign)
{
   Int2 status = 0;
   Int4 query_index, subject_index;
   BlastHitListPtr hit_list;
   BlastHSPListPtr hsp_list;
   SeqAlignPtr seqalign, last_seqalign = NULL;
   SeqIdPtr query_id, subject_id;
   Int8 searchsp_eff;
   BLAST_ScoreBlkPtr sbp;
   Uint1 num_frames;
   Uint1 encoding;

   *head_seqalign = NULL;

   if (!results || !query_info || (!rdfp && !subject)) {
      return 0;
   }
   
   sbp = gap_align->sbp;

   if (!rdfp && subject) {
      /* The BLAST 2 sequences case */
      subject_id = SeqIdDup(subject->seqid);
   }
   
   switch (gap_align->program) {
   case blast_type_blastn:
      num_frames = 2;
      encoding = BLASTNA_ENCODING;
      break;
   case blast_type_blastp:
      num_frames = 1;
      encoding = BLASTP_ENCODING;
      break;
   case blast_type_blastx:
      num_frames = 6;
      encoding = BLASTP_ENCODING;
      break;
   case blast_type_tblastn:
      num_frames = 1;
      encoding = NCBI4NA_ENCODING;
      break;
   case blast_type_tblastx:
      num_frames = 6;
      encoding = NCBI4NA_ENCODING;
      break;
   default:
      break;
   }

   for (query_index = 0; query_index < results->num_queries; ++query_index) {
      hit_list = results->hitlist_array[query_index];
      query_id = query_info->qid_array[query_index];

      query->seqid = query_id;

      if (!hit_list)
         continue;
      for (subject_index = 0; subject_index < hit_list->hsplist_count;
           ++subject_index) {
         hsp_list = hit_list->hsplist_array[subject_index];
         if (!hsp_list)
            continue;

         if (hsp_list->traceback_done) {
            if (rdfp) {
               readdb_get_descriptor(rdfp, hsp_list->oid, &subject_id, NULL); 
            }
            BLAST_GapInfoToSeqAlign(gap_align->program, hsp_list, query_id,
               subject_id, sbp, hit_params->options, &seqalign);
         } else {
            if (rdfp) {
               MakeBlastSequenceBlk(rdfp, &subject, hsp_list->oid, 
                                    encoding);
               readdb_get_descriptor(rdfp, hsp_list->oid, &subject->seqid, 
                                     NULL);
            }

            BlastHSPListGetTraceback(hsp_list, query, subject, query_info, 
               gap_align, sbp, score_options, hit_params, &seqalign);

            if (rdfp)
               subject = BLAST_SequenceBlkDestruct(subject);
         }
         
         if (seqalign) {
            if (!last_seqalign) {
               *head_seqalign = last_seqalign = seqalign;
            } else {
               last_seqalign->next = seqalign;
            }
            for ( ; last_seqalign->next; 
                  last_seqalign = last_seqalign->next);
         }
      }
      /* Re-sort the hit lists according to their best e-values, because
         they could have changed */
      
   }

   return status;
}
