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
 * ===========================================================================
 *
 * Author: Ilya Dondoshansky
 *
 */

/** @file blast_traceback.c
 * Functions responsible for the traceback stage of the BLAST algorithm
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/link_hsps.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_kappa.h>
#include "blast_gapalign_priv.h"
#include "blast_psi_priv.h"

/** TRUE if c is between a and b; f between d and f.  Determines if the
 * coordinates are already in an HSP that has been evaluated. 
*/
#define CONTAINED_IN_HSP(a,b,c,d,e,f) (((a <= c && b >= c) && (d <= f && e >= f)) ? TRUE : FALSE)

/** Compares HSPs in an HSP array and deletes those whose ranges are completely
 * included in other HSP ranges. The HSPs must be sorted by e-value/score 
 * coming into this function. Null HSPs must be purged after this function is
 * called.
 * @param hsp_array Array of BlastHSP's [in]
 * @param hspcnt Size of the array [in]
 * @param is_ooframe Is this a search with out-of-frame gapping? [in]
 */
static void 
s_BlastCheckHSPInclusion(BlastHSP* *hsp_array, Int4 hspcnt, 
                                   Boolean is_ooframe)
{
   Int4 index, index1;
   BlastHSP* hsp,* hsp1;
   
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
                     hsp_array[index] = Blast_HSPFree(hsp_array[index]);
                     break;
                  }
               }
            }
         }
      }
   }
   return;
}

/** Window size used to scan HSP for highest score region, where gapped
 * extension starts. 
 */
#define HSP_MAX_WINDOW 11

/** Function to check that the highest scoring region in an HSP still gives a 
 * positive score. This value was originally calcualted by 
 * BlastGetStartForGappedAlignment but it may have changed due to the 
 * introduction of ambiguity characters. Such a change can lead to 'strange' 
 * results from ALIGN. 
 * @param hsp An HSP structure [in]
 * @param query Query sequence buffer [in]
 * @param subject Subject sequence buffer [in]
 * @param sbp Scoring block containing matrix [in]
 * @return TRUE if region aroung starting offsets gives a positive score
*/
Boolean
BLAST_CheckStartForGappedAlignment(const BlastHSP* hsp, const Uint1* query, 
                                   const Uint1* subject, const BlastScoreBlk* sbp)
{
    Int4 index, score, start, end, width;
    Uint1* query_var,* subject_var;
    Boolean positionBased = (sbp->posMatrix != NULL);
    
    width = MIN((hsp->query.gapped_start-hsp->query.offset), HSP_MAX_WINDOW/2);
    start = hsp->query.gapped_start - width;
    end = MIN(hsp->query.end, hsp->query.gapped_start + width);
    /* Assures that the start of subject is above zero. */
    if ((hsp->subject.gapped_start + start - hsp->query.gapped_start) < 0) {
        start -= hsp->subject.gapped_start + (start - hsp->query.gapped_start);
    }
    query_var = ((Uint1*) query) + start;
    subject_var = ((Uint1*) subject) + hsp->subject.gapped_start + 
        (start - hsp->query.gapped_start);
    score=0;
    for (index = start; index < end; index++) {
        if (!positionBased)
            score += sbp->matrix[*query_var][*subject_var];
        else
            score += sbp->posMatrix[index][*subject_var];
        query_var++; subject_var++;
    }
    if (score <= 0) {
        return FALSE;
    }
    
    return TRUE;
}

/** Retrieves the pattern length information from a BlastHSP structure.
 * @param hsp HSP structure [in]
 */
static Int4 
s_GetPatternLengthFromBlastHSP(BlastHSP* hsp)
{
   return hsp->pattern_length;
}

/** Saves pattern length in a gapped alignment structure, in a PHI BLAST search.
 * @param length Pattern length [in]
 * @param gap_align Gapped alignment structure. [in] [out]
 */
static void 
s_SavePatternLengthInGapAlignStruct(Int4 length,
               BlastGapAlignStruct* gap_align)
{
   /* Kludge: save length in an output structure member, to avoid introducing 
      a new structure member. Probably should be changed??? */
   gap_align->query_stop = length;
}


/** Check whether an HSP is already contained within another higher scoring HSP.
 * "Containment" is defined by the macro CONTAINED_IN_HSP.  
 * the implicit assumption here is that HSP's are sorted by score
 * The main goal of this routine is to eliminate double gapped extensions of HSP's.
 *
 * @param hsp_array Full Array of all HSP's found so far. [in]
 * @param hsp HSP to be compared to other HSP's [in]
 * @param max_index Compare above HSP to all HSP's in hsp_array up to 
 *                  max_index [in]
 * @param is_ooframe TRUE if out-of-frame gapped alignment 
 *                   (blastx and tblastn only). [in]
 */
static Boolean
s_HSPContainedInHSPCheck(BlastHSP** hsp_array, BlastHSP* hsp, Int4 max_index, Boolean is_ooframe)
{
      BlastHSP* hsp1;
      Boolean delete_hsp=FALSE;
      Boolean hsp_start_is_contained=FALSE;
      Boolean hsp_end_is_contained=FALSE;
      Int4 index;

      for (index=0; index<max_index; index++) {
         hsp_start_is_contained = FALSE;
         hsp_end_is_contained = FALSE;
         
         hsp1 = hsp_array[index];
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
            delete_hsp = TRUE;
            break;
         }
      }

      return delete_hsp;
}

/** Check whether an HSP is already contained within another higher scoring HSP.
 * This is done AFTER the gapped alignment has been performed
 *
 * @param hsp_array Full Array of all HSP's found so far. [in][out]
 * @param hsp HSP to be compared to other HSP's [in]
 * @param max_index compare above HSP to all HSP's in hsp_array up to max_index [in]
 */
static Boolean
s_HSPCheckForDegenerateAlignments(BlastHSP** hsp_array, BlastHSP* hsp, Int4 max_index)
{
            BlastHSP* hsp2;
            Boolean keep=TRUE;
            Int4 index;

            for (index=0; index<max_index; index++) {
               hsp2 = hsp_array[index];
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
                     hsp_array[index] = Blast_HSPFree(hsp2);
                  }
               }
            }

            return keep;
}

Int2
Blast_HSPUpdateWithTraceback(BlastGapAlignStruct* gap_align, BlastHSP* hsp, 
                             Uint4 query_length, Uint4 subject_length, 
                             EBlastProgramType program)
{
   if (!hsp || !gap_align)
     return -1;

   hsp->score = gap_align->score;
   hsp->query.offset = gap_align->query_start;
   hsp->subject.offset = gap_align->subject_start;
   hsp->query.length = gap_align->query_stop - gap_align->query_start;
   hsp->subject.length = gap_align->subject_stop - gap_align->subject_start;
   hsp->query.end = gap_align->query_stop;
   hsp->subject.end = gap_align->subject_stop;
   /* If editing block is non-NULL, transfer ownership to the HSP structure.
      Then fill the missing infomation in the editing block. */
   if (gap_align->edit_block) { 
      hsp->gap_info = gap_align->edit_block;
      gap_align->edit_block = NULL;

      hsp->gap_info->frame1 = hsp->query.frame;
      hsp->gap_info->frame2 = hsp->subject.frame;
      hsp->gap_info->original_length1 = query_length;
      hsp->gap_info->original_length2 = subject_length;
      if (program == eBlastTypeBlastx)
         hsp->gap_info->translate1 = TRUE;
      if (program == eBlastTypeTblastn || program == eBlastTypeRpsTblastn)
         hsp->gap_info->translate2 = TRUE;
   }

   return 0;
}

/** Calculates number of identities and alignment lengths of an HSP and 
 * determines whether this HSP should be kept or deleted. The num_ident
 * field of the BlastHSP structure is filled here.
 * @program_number Type of BLAST program [in]
 * @param hsp An HSP structure [in] [out]
 * @param query Query sequence [in]
 * @param subject Subject sequence [in]
 * @param score_options Scoring options, needed to distinguish the 
 *                      out-of-frame case. [in]
 * @param hit_options Hit saving options containing percent identity and
 *                    HSP length thresholds.
 * @return TRUE if HSP passes the test, FALSE if it should be deleted.
 */ 
static Boolean
s_HSPTestIdentityAndLength(EBlastProgramType program_number, 
                               BlastHSP* hsp, Uint1* query, Uint1* subject, 
                               const BlastScoringOptions* score_options,
                               const BlastHitSavingOptions* hit_options)
{
   Int4 align_length = 0;
   Boolean keep = TRUE;

   /* Calculate alignment length and number of identical letters. 
      Do not get the number of identities if the query is not available */
   if (query != NULL) {
      if (score_options->is_ooframe) {
         Blast_HSPGetOOFNumIdentities(query, subject, hsp, program_number, 
                                      &hsp->num_ident, &align_length);
      } else {
         Blast_HSPGetNumIdentities(query, subject, hsp, 
                                   &hsp->num_ident, &align_length);
      }
   }
      
   /* Check whether this HSP passes the percent identity and minimal hit 
      length criteria, and delete it if it does not. */
   if ((hsp->num_ident * 100 < 
        align_length * hit_options->percent_identity) ||
       align_length < hit_options->min_hit_length) {
      keep = FALSE;
   }

   return keep;
}

static void 
s_HSPListRescaleScores(BlastHSPList* hsp_list, double scale_factor)
{
   Int4 index;

   for (index = 0; index < hsp_list->hspcnt; ++index) {
      BlastHSP* hsp = hsp_list->hsp_array[index];
      
      /* Remove any scaling of the calculated score */
      hsp->score = 
         (Int4) ((hsp->score+(0.5*scale_factor)) / scale_factor);
   }

   /* Sort HSPs by score again, as the expected order might change after
      rescaling. */
   Blast_HSPListSortByScore(hsp_list);
}

/** Simple macro to swap two numbers. */
#define SWAP(a, b) {tmp = (a); (a) = (b); (b) = tmp; }

/** Swaps query and subject information in an HSP structure for RPS BLAST 
 * search. This is necessary because query and subject are switched during the 
 * traceback alignment, and must be switched back.
 * @param hsp HSP structure to fix. [in] [out]
 */
static void 
s_BlastHSPRPSUpdate(BlastHSP *hsp)
{
   Int4 tmp;
   GapEditBlock *gap_info = hsp->gap_info;
   GapEditScript *esp;

   if (gap_info == NULL)
      return;

   SWAP(gap_info->start1, gap_info->start2);
   SWAP(gap_info->length1, gap_info->length2);
   SWAP(gap_info->original_length1, gap_info->original_length2);
   SWAP(gap_info->frame1, gap_info->frame2);
   SWAP(gap_info->translate1, gap_info->translate2);

   esp = gap_info->esp;
   while (esp != NULL) {
      if (esp->op_type == eGapAlignIns)
          esp->op_type = eGapAlignDel;
      else if (esp->op_type == eGapAlignDel)
          esp->op_type = eGapAlignIns;

      esp = esp->next;
   }
}

/** Switches back the query and subject in all HSPs in an HSP list; also
 * reassigns contexts to indicate query context, needed to pick correct
 * Karlin block later in the code.
 * @param program Program type: RPS or RPS tblastn [in]
 * @param hsplist List of HSPs [in] [out]
 */
static void 
s_BlastHSPListRPSUpdate(EBlastProgramType program, BlastHSPList *hsplist)
{
   Int4 i;
   BlastHSP **hsp;
   BlastSeg tmp;

   /* If this is not an RPS BLAST search, do not do anything. */
   if (program != eBlastTypeRpsBlast && program != eBlastTypeRpsTblastn)
      return;

   hsp = hsplist->hsp_array;
   for (i = 0; i < hsplist->hspcnt; i++) {

      /* switch query and subject offsets (which are
         already in local coordinates) */

      tmp = hsp[i]->query;
      hsp[i]->query = hsp[i]->subject;
      hsp[i]->subject = tmp;

      /* Change the traceback information to reflect the
         query and subject sequences getting switched */

      s_BlastHSPRPSUpdate(hsp[i]);

      /* If query was nucleotide, set context, because it is needed in order 
	 to pick correct Karlin block for calculating bit scores. There are 
         6 contexts corresponding to each nucleotide query sequence. */
      if (program == eBlastTypeRpsTblastn) {
	 hsp[i]->context = FrameToContext(hsp[i]->query.frame);
      }
   }
}

static Int2
s_HSPListPostTracebackUpdate(EBlastProgramType program_number, 
   BlastHSPList* hsp_list, BlastQueryInfo* query_info, 
   const BlastScoringParameters* score_params, 
   const BlastHitSavingParameters* hit_params, 
   BlastScoreBlk* sbp, Int4 subject_length)
{
   BlastScoringOptions* score_options = score_params->options;
   const Boolean kGapped = score_options->gapped_calculation;

   /* Now try to detect simular alignments */
   s_BlastCheckHSPInclusion(hsp_list->hsp_array, hsp_list->hspcnt, 
                            score_options->is_ooframe);
   
   Blast_HSPListPurgeNullHSPs(hsp_list);
   
   /* Revert query and subject to their traditional meanings. 
      This involves switching the offsets around and reversing
      any traceback information */
   s_BlastHSPListRPSUpdate(program_number, hsp_list);
   
   /* Relink and rereap the HSP list, if needed. */
   if (hit_params->link_hsp_params) {
      BLAST_LinkHsps(program_number, hsp_list, query_info, subject_length,
                     sbp, hit_params->link_hsp_params, kGapped);
   } else if (hit_params->options->phi_align) {
      Blast_HSPListPHIGetEvalues(hsp_list, sbp);
   } else {
      /* Only use the scaling factor from parameters structure for RPS BLAST, 
       * because for other programs either there is no scaling at all, or, in
       * case of composition based statistics, Lambda is scaled as well as 
       * scores, and hence scaling factor should not be used for e-value 
       * computations. 
       */
      double scale_factor = 
         (program_number == eBlastTypeRpsBlast || 
          program_number == eBlastTypeRpsTblastn) ? 
         score_params->scale_factor : 1.0;
      Blast_HSPListGetEvalues(query_info, hsp_list, kGapped, sbp, 0,
                              scale_factor);
   }

   Blast_HSPListReapByEvalue(hsp_list, hit_params->options);
   
   /* Rescale the scores by scaling factor, if necessary. This rescaling
    * should be done for all programs where scaling factor is not 1.
    */
   s_HSPListRescaleScores(hsp_list, score_params->scale_factor);
   
   /* Calculate and fill the bit scores. This is the only time when 
      they are calculated. */
   Blast_HSPListGetBitScores(hsp_list, kGapped, sbp);

   return 0;
}

/*
    Comments in blast_traceback.h
 */
Int2
Blast_TracebackFromHSPList(EBlastProgramType program_number, 
   BlastHSPList* hsp_list, BLAST_SequenceBlk* query_blk, 
   BLAST_SequenceBlk* subject_blk, BlastQueryInfo* query_info_in,
   BlastGapAlignStruct* gap_align, BlastScoreBlk* sbp, 
   const BlastScoringParameters* score_params,
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingParameters* hit_params, const Uint1* gen_code_string)
{
   Int4 index;
   BlastHSP* hsp;
   Uint1* query,* subject;
   Int4 query_length, query_length_orig, subject_length_orig;
   Int4 subject_length=0;
   BlastHSP** hsp_array;
   Int4 q_start, s_start;
   BlastHitSavingOptions* hit_options = hit_params->options;
   BlastScoringOptions* score_options = score_params->options;
   Int4 context_offset;
   Uint1* translation_buffer = NULL;
   Int4 * frame_offsets   = NULL;
   Int4 * frame_offsets_a = NULL;
   Boolean partial_translation = FALSE;
   const Boolean kIsOutOfFrame = score_options->is_ooframe;
   const Boolean kGreedyTraceback = (ext_options->eTbackExt == eGreedyTbck);
   const Boolean kTranslateSubject = 
      (program_number == eBlastTypeTblastn ||
       program_number == eBlastTypeRpsTblastn); 
   BlastQueryInfo* query_info = query_info_in;
   Int4 offsets[2];
   
   if (hsp_list->hspcnt == 0) {
      return 0;
   }

   hsp_array = hsp_list->hsp_array;

   if (kTranslateSubject) {
      if (!gen_code_string && program_number != eBlastTypeRpsTblastn)
         return -1;

      if (kIsOutOfFrame) {
         Blast_SetUpSubjectTranslation(subject_blk, gen_code_string,
                                       NULL, NULL, &partial_translation);
         subject = subject_blk->oof_sequence + CODON_LENGTH;
         /* Mixed-frame sequence spans all 6 frames, i.e. both strands
            of the nucleotide sequence. However its start will also be 
            shifted by 3.*/
         subject_length = 2*subject_blk->length - 1;
      } else if (program_number == eBlastTypeRpsTblastn) {
	 translation_buffer = subject_blk->sequence - 1;
	 frame_offsets_a = frame_offsets =
             ContextOffsetsToOffsetArray(query_info_in);
      } else {
         Blast_SetUpSubjectTranslation(subject_blk, gen_code_string,
            &translation_buffer, &frame_offsets, &partial_translation);
         frame_offsets_a = frame_offsets;
         /* subject and subject_length will be set later, for each HSP. */
      }
   } else {
      /* Subject is not translated */
      subject = subject_blk->sequence;
      subject_length = subject_blk->length;
   }

   if (program_number == eBlastTypeRpsBlast || 
       program_number == eBlastTypeRpsTblastn) {
      /* Create a local BlastQueryInfo structure for this subject sequence
	 that has been switched with the query. */
      query_info = BlastMemDup(query_info_in, sizeof(BlastQueryInfo));
      query_info->first_context = query_info->last_context = 0;
      query_info->num_queries = 1;
      offsets[0] = 0;
      offsets[1] = query_blk->length + 1;
      OffsetArrayToContextOffsets(query_info, offsets, program_number);
   }

   /* Make sure the HSPs in the HSP list are sorted by score, as they should 
      be. */
   ASSERT(Blast_HSPListIsSortedByScore(hsp_list));

   for (index=0; index < hsp_list->hspcnt; index++) {
      hsp = hsp_array[index];
      if (program_number == eBlastTypeBlastx || 
          program_number == eBlastTypeTblastx) {
         Int4 end_of_frame_set = 0;
         Int4 context = hsp->context - hsp->context % 3;
         context_offset = query_info->contexts[context].query_offset;
         
         end_of_frame_set =
             query_info->contexts[context+2].query_offset +
             query_info->contexts[context+2].query_length;
         
         query_length_orig = end_of_frame_set - context_offset;
         
         if (kIsOutOfFrame) {
            query = query_blk->oof_sequence + CODON_LENGTH + context_offset;
            query_length = query_length_orig;
         } else {
            query = query_blk->sequence + 
               query_info->contexts[hsp->context].query_offset;
            query_length = query_info->contexts[hsp->context].query_length;
         }
      } else {
          query = query_blk->sequence + 
              query_info->contexts[hsp->context].query_offset;
          query_length = query_length_orig = 
              query_info->contexts[hsp->context].query_length;
      }
      
      subject_length_orig = subject_blk->length;
      /* For RPS tblastn the "subject" length should be the original 
	 nucleotide query length, which can be calculated from the 
	 context offsets in query_info_in. */
      if (program_number == eBlastTypeRpsTblastn) {
	 if (hsp->subject.frame > 0) {
	    subject_length_orig = 
	       query_info_in->contexts[NUM_FRAMES/2].query_offset - 1;
	 } else {
            Int4 frame_set_end =
                query_info_in->contexts[NUM_FRAMES-1].query_offset +
                query_info_in->contexts[NUM_FRAMES-1].query_length;
            
            subject_length_orig = frame_set_end -
                query_info_in->contexts[NUM_FRAMES/2].query_offset;
	 }
      }

      /* preliminary RPS blast alignments have not had
         the composition-based correction applied yet, so
         we cannot reliably check whether an HSP is contained
         within another */

      if (program_number == eBlastTypeRpsBlast ||
          !s_HSPContainedInHSPCheck(hsp_array, hsp, index, kIsOutOfFrame)) {

         Int4 start_shift = 0;
         Int4 adjusted_s_length;
         Uint1* adjusted_subject;

         if (kTranslateSubject) {
            if (!kIsOutOfFrame && !partial_translation) {
               Int4 context = FrameToContext(hsp->subject.frame);
               subject = translation_buffer + frame_offsets[context] + 1;
               subject_length = 
                  frame_offsets[context+1] - frame_offsets[context] - 1;
            } else if (partial_translation) {
               Blast_HSPGetPartialSubjectTranslation(subject_blk, hsp, 
                  kIsOutOfFrame, gen_code_string, &translation_buffer, &subject,
                  &subject_length, &start_shift);
            }
         }

         if (!hit_options->phi_align && !kIsOutOfFrame && 
             (((hsp->query.gapped_start == 0 && 
                hsp->subject.gapped_start == 0) ||
               !BLAST_CheckStartForGappedAlignment(hsp, query, 
                   subject, sbp)))) {
            Int4 max_offset = 
               BlastGetStartForGappedAlignment(query, subject, sbp,
                  hsp->query.offset, hsp->query.length,
                  hsp->subject.offset, hsp->subject.length);
            q_start = max_offset;
            s_start = 
               (hsp->subject.offset - hsp->query.offset) + max_offset;
            hsp->query.gapped_start = q_start;
            hsp->subject.gapped_start = s_start;
         } else {
            if(kIsOutOfFrame) {
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
         
         adjusted_s_length = subject_length;
         adjusted_subject = subject;

        /* Perform the gapped extension with traceback */
         if (hit_options->phi_align) {
            Int4 pat_length = s_GetPatternLengthFromBlastHSP(hsp);
            s_SavePatternLengthInGapAlignStruct(pat_length, gap_align);
            PHIGappedAlignmentWithTraceback(query, subject, gap_align, 
               score_params, q_start, s_start, query_length, subject_length);
         } else {
            if (!kTranslateSubject) {
               AdjustSubjectRange(&s_start, &adjusted_s_length, q_start, 
                                  query_length, &start_shift);
               adjusted_subject = subject + start_shift;
               /* Shift the gapped start in HSP structure, to compensate for 
                  a shift in the other direction later. */
               hsp->subject.gapped_start = s_start;
            }
            if (kGreedyTraceback) {
               BLAST_GreedyGappedAlignment(query, adjusted_subject, 
                  query_length, adjusted_s_length, gap_align, 
                  score_params, q_start, s_start, FALSE, TRUE);
            } else {
               BLAST_GappedAlignmentWithTraceback(program_number, query, 
                  adjusted_subject, gap_align, score_params, q_start, s_start, 
                  query_length, adjusted_s_length);
            }
         }

         if (gap_align->score >= hit_params->cutoff_score) {
            Boolean keep = FALSE;
            Blast_HSPUpdateWithTraceback(gap_align, hsp, query_length_orig,
                                         subject_length_orig, program_number);

            if (kGreedyTraceback) {
               /* Low level greedy algorithm ignores ambiguities, so the score
                  needs to be reevaluated. */
               Blast_HSPReevaluateWithAmbiguitiesGapped(hsp, query, 
                  adjusted_subject, hit_params, score_params, query_info, sbp);
            }
            
            /* Calculate number of identities and check if this HSP meets the
               percent identity and length criteria. */
            keep = s_HSPTestIdentityAndLength(program_number, hsp, 
                                                  query, adjusted_subject, 
                                                  score_options, hit_options);
            if (keep) {
               Blast_HSPAdjustSubjectOffset(hsp, subject_blk, kIsOutOfFrame, 
                                            start_shift);
               keep = s_HSPCheckForDegenerateAlignments(hsp_array, hsp, index);
            }
            if (!keep)
               hsp_array[index] = Blast_HSPFree(hsp);
         } else {
            /* Score is below threshold */
            gap_align->edit_block = GapEditBlockDelete(gap_align->edit_block);
            hsp_array[index] = Blast_HSPFree(hsp);
         }
      } else {
         /* Contained within another HSP, delete. */
         hsp_array[index] = Blast_HSPFree(hsp);
      }
   } /* End loop on HSPs */

   if (program_number != eBlastTypeRpsTblastn) {
      if (translation_buffer) {
         sfree(translation_buffer);
      }
   }
   
   if (frame_offsets_a) {
       sfree(frame_offsets_a);
   }
   
   /* Free the local query_info structure, if necessary (RPS tblastn only) */
   if (query_info != query_info_in)
      sfree(query_info);
   
   /* Sort HSPs by score again, as the scores might have changed. */
   Blast_HSPListSortByScore(hsp_list);

   s_HSPListPostTracebackUpdate(program_number, hsp_list, query_info_in, 
                                score_params, hit_params, sbp, 
                                subject_blk->length);
   
   
   return 0;
}

Uint1 Blast_TracebackGetEncoding(EBlastProgramType program_number) 
{
   Uint1 encoding;

   switch (program_number) {
   case eBlastTypeBlastn:
      encoding = BLASTNA_ENCODING;
      break;
   case eBlastTypeBlastp:
   case eBlastTypeRpsBlast:
   case eBlastTypeBlastx:
   case eBlastTypeRpsTblastn:
      encoding = BLASTP_ENCODING;
      break;
   case eBlastTypeTblastn:
   case eBlastTypeTblastx:
      encoding = NCBI4NA_ENCODING;
      break;
   default:
      encoding = ERROR_ENCODING;
      break;
   }
   return encoding;
}

/** Delete extra subject sequences hits, if after-traceback hit list size is
 * smaller than preliminary hit list size.
 * @param results All results after traceback, assumed already sorted by best
 *                e-value [in] [out]
 * @param hitlist_size Final hit list size [in]
 */
static void 
s_BlastPruneExtraHits(BlastHSPResults* results, Int4 hitlist_size)
{
   Int4 query_index, subject_index;
   BlastHitList* hit_list;

   for (query_index = 0; query_index < results->num_queries; ++query_index) {
      if (!(hit_list = results->hitlist_array[query_index]))
         continue;
      for (subject_index = hitlist_size;
           subject_index < hit_list->hsplist_count; ++subject_index) {
         hit_list->hsplist_array[subject_index] = 
            Blast_HSPListFree(hit_list->hsplist_array[subject_index]);
      }
      hit_list->hsplist_count = MIN(hit_list->hsplist_count, hitlist_size);
   }
}

Int2 
BLAST_ComputeTraceback(EBlastProgramType program_number, 
   BlastHSPStream* hsp_stream, BLAST_SequenceBlk* query, 
   BlastQueryInfo* query_info, const BlastSeqSrc* seq_src, 
   BlastGapAlignStruct* gap_align, BlastScoringParameters* score_params,
   const BlastExtensionParameters* ext_params,
   BlastHitSavingParameters* hit_params,
   BlastEffectiveLengthsParameters* eff_len_params,
   const BlastDatabaseOptions* db_options,
   const PSIBlastOptions* psi_options, BlastHSPResults** results_out)
{
   Int2 status = 0;
   BlastHSPResults* results = NULL;
   BlastHSPList* hsp_list = NULL;
   BlastScoreBlk* sbp;
   Uint1 encoding;
   GetSeqArg seq_arg;
   Uint1* gen_code_string = NULL;
 
   if (!query_info || !seq_src || !hsp_stream || !results_out) {
      return 0;
   }
   
   /* Set the raw X-dropoff value for the final gapped extension with 
      traceback */
   gap_align->gap_x_dropoff = ext_params->gap_x_dropoff_final;

   sbp = gap_align->sbp;
  
   if (db_options)
      gen_code_string = db_options->gen_code_string;
 
   encoding = Blast_TracebackGetEncoding(program_number);
   memset((void*) &seq_arg, 0, sizeof(seq_arg));

   results = Blast_HSPResultsNew(query_info->num_queries);

   if (program_number == eBlastTypeBlastp && 
         (ext_params->options->compositionBasedStats == TRUE || 
            ext_params->options->eTbackExt == eSmithWatermanTbck)) {
          Kappa_RedoAlignmentCore(program_number, query, query_info,
              sbp, hsp_stream, seq_src, gen_code_string,
              score_params, ext_params, hit_params, psi_options, results); 
   } else {
      Boolean perform_traceback = 
         (score_params->options->gapped_calculation && 
          (ext_params->options->ePrelimGapExt != eGreedyWithTracebackExt) &&
          (ext_params->options->eTbackExt != eSkipTbck));

      /* Retrieve all HSP lists from the HSPStream. */
      while (BlastHSPStreamRead(hsp_stream, &hsp_list) 
             != kBlastHSPStream_Eof) {

         /* Perform traceback here, if necessary. */
         if (perform_traceback) {
            seq_arg.oid = hsp_list->oid;
            seq_arg.encoding = encoding;
            BlastSequenceBlkClean(seq_arg.seq);
            if (BLASTSeqSrcGetSequence(seq_src, (void*) &seq_arg) < 0)
               continue;
            
            if (BLASTSeqSrcGetTotLen(seq_src) == 0) {
               /* This is not a database search, so effective search spaces
                * need to be recalculated based on this subject sequence 
                * length.
                * NB: The initial word parameters structure is not available 
                * here, so the small gap cutoff score for linking of HSPs will 
                * not be updated. Since by default linking is done with uneven 
                * gap statistics, this can only influence a corner non-default 
                * case, and is a tradeoff for a benefit of not having to deal 
                * with ungapped extension parameters in the traceback stage.
                */
               if ((status = BLAST_OneSubjectUpdateParameters(program_number, 
                                seq_arg.seq->length, score_params->options, 
                                query_info, sbp, hit_params, 
                                NULL, eff_len_params)) != 0)
                  return status;
            }

            Blast_TracebackFromHSPList(program_number, hsp_list, query, 
               seq_arg.seq, query_info, gap_align, sbp, score_params, 
               ext_params->options, hit_params, gen_code_string);

            BLASTSeqSrcRetSequence(seq_src, (void*)&seq_arg);
         }

         Blast_HSPResultsInsertHSPList(results, hsp_list, 
                                       hit_params->options->hitlist_size);
      }
   }

   /* Re-sort the hit lists according to their best e-values, because
      they could have changed. Only do this for a database search. */
   if (BLASTSeqSrcGetTotLen(seq_src) > 0)
      Blast_HSPResultsSortByEvalue(results);

   /* Eliminate extra hits from results, if preliminary hit list size is larger
      than the final hit list size */
   if (hit_params->options->hitlist_size < 
       hit_params->options->prelim_hitlist_size)
      s_BlastPruneExtraHits(results, hit_params->options->hitlist_size);

   BlastSequenceBlkFree(seq_arg.seq);

   *results_out = results;

   return status;
}

/** Factor to multiply the Karlin-Altschul K parameter by for RPS BLAST, to make
 * e-values more conservative.
 */
#define RPS_K_MULT 1.2

Int2 BLAST_RPSTraceback(EBlastProgramType program_number, 
        BlastHSPStream* hsp_stream, 
        const BlastSeqSrc* seq_src, BlastQueryInfo* concat_db_info, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        BlastGapAlignStruct* gap_align,
        const BlastScoringParameters* score_params,
        const BlastExtensionParameters* ext_params,
        BlastHitSavingParameters* hit_params,
        const double* karlin_k,
        BlastHSPResults** results_out)
{
   Int2 status = 0;
   BlastHSPList* hsp_list;
   BlastScoreBlk* sbp;
   Int4 **orig_pssm;
   Int4 db_seq_start;
   BlastHSPResults* results = NULL;
   Uint1 encoding;
   GetSeqArg seq_arg;
   BlastQueryInfo* one_query_info = NULL;
   BLAST_SequenceBlk* one_query = NULL;
   

   if (!hsp_stream || !concat_db_info || !seq_src || !results_out) {
      return 0;
   }
   
   /* Set the raw X-dropoff value for the final gapped extension with 
      traceback */
   gap_align->gap_x_dropoff = ext_params->gap_x_dropoff_final;

   sbp = gap_align->sbp;
   orig_pssm = gap_align->sbp->posMatrix;

   encoding = Blast_TracebackGetEncoding(program_number);
   memset((void*) &seq_arg, 0, sizeof(seq_arg));

   results = Blast_HSPResultsNew(query_info->num_queries);

   while (BlastHSPStreamRead(hsp_stream, &hsp_list) 
          != kBlastHSPStream_Eof) {
      if (!hsp_list)
         continue;

      /* If traceback should be skipped, just save the HSP list into the results
         structure. */
      if (ext_params->options->eTbackExt == eSkipTbck) {
         /* Save this HSP list in the results structure. */
         Blast_HSPResultsInsertHSPList(results, hsp_list, 
                                       hit_params->options->hitlist_size);
         continue;
      }

      /* Restrict the query sequence block and information structures 
         to the one query this HSP list corresponds to. */
      if (Blast_GetOneQueryStructs(&one_query_info, &one_query, 
                                   query_info, query, 
                                   hsp_list->query_index) != 0)
          return -1;

      /* Pick out one of the sequences from the concatenated DB (given by the 
         OID of this HSPList). The sequence length does not include the 
         trailing NULL. The sequence itself is only needed to calculate number
         of identities, since scoring is done with a portion of the PSSM 
         corresponding to this sequence. */
      seq_arg.oid = hsp_list->oid;
      seq_arg.encoding = encoding;
      if (BLASTSeqSrcGetSequence(seq_src, (void*) &seq_arg) < 0)
          continue;

      db_seq_start = concat_db_info->contexts[hsp_list->oid].query_offset;
      
      /* Update the statistics for this database sequence
         (if not a translated search) */
      
      if (program_number == eBlastTypeRpsTblastn) {
         sbp->posMatrix = orig_pssm + db_seq_start;
      } else {
         /* replace the PSSM and the Karlin values for this DB sequence
            and this query sequence. */
         sbp->posMatrix = 
            RPSCalculatePSSM(score_params->scale_factor,
                             one_query->length, one_query->sequence, 
                             seq_arg.seq->length,
                             orig_pssm + db_seq_start,
                             sbp->name);
         /* The composition of the query could have caused this one
            subject sequence to produce a bad PSSM. This should
            not be a fatal error, so just go on to the next subject
            sequence */
         if (sbp->posMatrix == NULL) {
            /** @todo FIXME Results should not be silently skipped,
             *        need a warning here
             */
            hsp_list = Blast_HSPListFree(hsp_list);
            BLASTSeqSrcRetSequence(seq_src, (void*)&seq_arg);
            continue;
         }
         
         sbp->kbp_gap[0]->K = RPS_K_MULT * karlin_k[hsp_list->oid];
         sbp->kbp_gap[0]->logK = log(RPS_K_MULT * karlin_k[hsp_list->oid]);
      }


      /* compute the traceback information and calculate E values
         for all HSPs in the list */
      
      Blast_TracebackFromHSPList(program_number, hsp_list, seq_arg.seq, 
         one_query, one_query_info, gap_align, sbp, score_params, 
         ext_params->options, hit_params, NULL);

      BLASTSeqSrcRetSequence(seq_src, (void*)&seq_arg);

      if (program_number != eBlastTypeRpsTblastn)
         _PSIDeallocateMatrix((void**)sbp->posMatrix, seq_arg.seq->length);

      if (hsp_list->hspcnt == 0) {
         hsp_list = Blast_HSPListFree(hsp_list);
         continue;
      }

      /* Save this HSP list in the results structure. */
      Blast_HSPResultsInsertHSPList(results, hsp_list, 
                                    hit_params->options->hitlist_size);
   }

   /* Free the sequence block allocated inside the loop */
   BlastSequenceBlkFree(seq_arg.seq);

   /* Free the single-query structures allocated inside the loop. */
   BlastQueryInfoFree(one_query_info);
   BlastSequenceBlkFree(one_query);

   /* The traceback calculated the E values, so it's safe
      to sort the results now */
   Blast_HSPResultsSortByEvalue(results);

   *results_out = results;

   gap_align->sbp->posMatrix = orig_pssm;
   return status;
}
