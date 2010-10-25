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
 *
 *
 */

/** @file blast_traceback.c
 * Functions responsible for the traceback stage of the BLAST algorithm.
 * <pre>
 * The hierarchy of function calls for performing traceback is:
 *    Blast_RunTracebackSearch 
 *        BLAST_GapAlignSetUp
 *        BLAST_ComputeTraceback
 *            if ( RPS BLAST ) 
 *                s_RPSComputeTraceback
 *                    for ( all HSP lists )
 *                        Blast_TracebackFromHSPList
 *            else if ( composition based statistics )
 *                Blast_RedoAlignmentCore
 *            else
 *                for ( all HSP lists )
 *                    if ( PHI BLAST) 
 *                        s_PHITracebackFromHSPList
 *                    else
 *                        Blast_TracebackFromHSPList
 * </pre>
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/link_hsps.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_kappa.h>
#include <algo/blast/core/blast_sw.h>
#include <algo/blast/core/phi_gapalign.h>
#include <algo/blast/core/gencode_singleton.h>
#include "blast_gapalign_priv.h"
#include "blast_psi_priv.h"
#include "blast_hits_priv.h"
#include "blast_itree.h"

/** Window size used to scan HSP for highest score region, where gapped
 * extension starts. 
 */
#define HSP_MAX_WINDOW 11

/**
 * Check whether the starting point for gapped alignment lies in
 * region that has positive score.  This routine is called after a
 * preliminary gapped alignment has been computed, but before the
 * traceback is computed.  The score of the region containing the
 * starting point may have changed due to the introduction of
 * ambiguity characters, further filtering of the sequences or the
 * application of composition based statistics.
 *
 * Usually, we check an ungapped alignment of length 11 about the
 * starting point: 5 characters to the left and 5 to the right.
 * However, the actual region checked is occassionally shorter because
 * we don't check characters before the start, or after the end, of
 * the preliminarily aligned regions in the query or subject.
 *
 * @param hsp An HSP structure [in]
 * @param query Query sequence buffer [in]
 * @param subject Subject sequence buffer [in]
 * @param sbp Scoring block containing matrix [in]
 * @return TRUE if region around starting offsets gives a positive score
*/
Boolean
BLAST_CheckStartForGappedAlignment(const BlastHSP* hsp, const Uint1* query,
                                   const Uint1* subject,
                                   const BlastScoreBlk* sbp)
{
    Int4 left, right;       /* Number of aligned characters to the
                               left and right of the starting point */
    Int4 score;             /* Score of the word alignment */
    const Uint1*   subject_var;   /* Current character in the subject sequence */
    const Uint1*   subject_right; /* Last character to be considered in the subject
                               sequence */
    Boolean positionBased = (sbp->psi_matrix != NULL);

    /* Compute the number of characters to the left of the start
       to include in the word */
    left = -HSP_MAX_WINDOW/2;
    if (left < hsp->query.offset - hsp->query.gapped_start) {
        left = hsp->query.offset - hsp->query.gapped_start;
    }
    if (left < hsp->subject.offset - hsp->subject.gapped_start) {
        left = hsp->subject.offset - hsp->subject.gapped_start;
    }

    /* Compute the number of characters to right to include in the word,
       including the starting point itself. */
    right = HSP_MAX_WINDOW/2 + 1;
    if (right > hsp->query.end - hsp->query.gapped_start) {
        right = hsp->query.end - hsp->query.gapped_start;
    }
    if (right > hsp->subject.end - hsp->subject.gapped_start) {
        right = hsp->subject.end - hsp->subject.gapped_start;
    }

    /* Calculate the score of the word */
    score = 0;
    subject_var   = subject + hsp->subject.gapped_start + left;
    subject_right = subject + hsp->subject.gapped_start + right;
    if ( !positionBased ) {
        const Uint1*   query_var;     /* Current character in the query */
        query_var = query + hsp->query.gapped_start + left;
        for ( ; subject_var < subject_right; subject_var++, query_var++) {
            ASSERT(*query_var < sbp->matrix->ncols);
            ASSERT(*subject_var < sbp->matrix->nrows);
           score += sbp->matrix->data[*query_var][*subject_var];
        }
    } else {
        Int4 query_index;       /* Current position in the query */
        query_index = hsp->query.gapped_start + left;
        for ( ;  subject_var < subject_right;  subject_var++, query_index++) {
            score += sbp->psi_matrix->pssm->data[query_index][*subject_var];
        }
    }
    if (score <= 0) {
        return FALSE;
    } else {
        return TRUE;
    }
}


Int2
Blast_HSPUpdateWithTraceback(BlastGapAlignStruct* gap_align, BlastHSP* hsp)
{
   if (!hsp || !gap_align)
     return -1;

   hsp->score = gap_align->score;
   hsp->query.offset = gap_align->query_start;
   hsp->subject.offset = gap_align->subject_start;
   hsp->query.end = gap_align->query_stop;
   hsp->subject.end = gap_align->subject_stop;
   /* If editing block is non-NULL, transfer ownership to the HSP structure.
      Then fill the missing infomation in the editing block. */
   if (gap_align->edit_script) { 
      hsp->gap_info = gap_align->edit_script;
      gap_align->edit_script = NULL;
   }

   return 0;
}

/** Remove scaling from scores previously calculated on the hsp_list.
 * @param hsp_list list of HPSs with the score field calculated [in|out]
 * @param scale_factor factor by which scores are scaled, for everything other
 * than RPS-BLAST this should be 1 [in] 
 * @todo rename to something which is more intention revealing, merge with
 * function of the same name in blast_kappa.c
 */
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

   /* Sort HSPs by score again because after the loop above scores that
    * were previously different can become equal, and then the order of HSPs
    * should be determined by the tie-breaking criteria 
    * (e.g.: subject offsets, ...) */
   Blast_HSPListSortByScore(hsp_list);
}

/** Swaps insertions and deletions in an edit script for RPS BLAST search. 
 * This is necessary because query and subject are switched during the 
 * traceback alignment, and must be switched back.
 * @param hsp HSP structure to fix. [in] [out]
 */
static void 
s_BlastHSPRPSUpdate(BlastHSP *hsp)
{
   GapEditScript *esp = hsp->gap_info;
   Int4 index;

   if (hsp->gap_info == NULL)
      return;

   for (index=0; index<esp->size; index++)
   {
      if (esp->op_type[index] == eGapAlignIns)
          esp->op_type[index] = eGapAlignDel;
      else if (esp->op_type[index] == eGapAlignDel)
          esp->op_type[index] = eGapAlignIns;
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
   if ( !Blast_ProgramIsRpsBlast(program))
      return;

   hsp = hsplist->hsp_array;
   for (i = 0; i < hsplist->hspcnt; i++) {

      /* switch query and subject (which are already in local coordinates) */
      tmp = hsp[i]->query;
      hsp[i]->query = hsp[i]->subject;
      hsp[i]->subject = tmp;

      /* Change the traceback information to reflect the query and subject 
         sequences getting switched */
      s_BlastHSPRPSUpdate(hsp[i]);

      /* If query was nucleotide, set context, because it is needed in order 
         to pick correct Karlin block for calculating bit scores. There are 
         6 contexts corresponding to each nucleotide query sequence. */
      if (program == eBlastTypeRpsTblastn) {
          hsp[i]->context = BLAST_FrameToContext(hsp[i]->query.frame, program);
      }
   }
   Blast_HSPListSortByScore(hsplist);
}

/** Updates the e-values after the traceback alignment. Also includes relinking
 * of HSPs in case of sum statistics and calculation of bit scores.
 * @param program_number Type of BLAST program [in]
 * @param hsp_list HSPList obtained after a traceback alignment [in] [out]
 * @param query_info Query information structure [in]
 * @param score_params Scoring parameters [in]
 * @param hit_params Hit saving parameters [in]
 * @param sbp Scoring block [in]
 * @param subject_length Length of the subject sequence - needed for linking 
 *                       HSPs [in]
 */
static Int2
s_HSPListPostTracebackUpdate(EBlastProgramType program_number, 
   BlastHSPList* hsp_list, BlastQueryInfo* query_info, 
   const BlastScoringParameters* score_params, 
   const BlastHitSavingParameters* hit_params, 
   BlastScoreBlk* sbp, Int4 subject_length)
{
   BlastScoringOptions* score_options = score_params->options;
   const Boolean kGapped = score_options->gapped_calculation;
   
   /* Revert query and subject to their traditional meanings. 
      This involves switching the offsets around and reversing
      any traceback information */
   s_BlastHSPListRPSUpdate(program_number, hsp_list);
   
   /* Relink and rereap the HSP list, if needed. */
   if (hit_params->link_hsp_params) {
      BLAST_LinkHsps(program_number, hsp_list, query_info, subject_length,
                     sbp, hit_params->link_hsp_params, kGapped);
   } else {
      /* Only use the scaling factor from parameters structure for RPS BLAST, 
       * because for other programs either there is no scaling at all, or, in
       * case of composition based statistics, Lambda is scaled as well as 
       * scores, and hence scaling factor should not be used for e-value 
       * computations. 
       */
      double scale_factor = 
         (Blast_ProgramIsRpsBlast(program_number) ?
         score_params->scale_factor : 1.0);

      /* For nucleotide search, if match score is = 2, the odd scores
         are rounded down to the nearest even number. */
      Blast_HSPListAdjustOddBlastnScores(hsp_list, kGapped, sbp);

      Blast_HSPListGetEvalues(query_info, hsp_list, kGapped, sbp, 0,
                              scale_factor);
   }

   Blast_HSPListReapByEvalue(hsp_list, hit_params->options);
   
   /* Rescale the scores by scaling factor, if necessary. This rescaling
    * should be done for all programs where scaling factor is not 1.
    */
   s_HSPListRescaleScores(hsp_list, score_params->scale_factor);
   
   /** Calculate and fill the bit scores. @todo: This is not the only time 
    * when they are calculated, s_HSPListRescaleScores also does this in 
    * blast_kappa.c.
    */
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
   const BlastHitSavingParameters* hit_params, const Uint1* gen_code_string,
   Boolean * fence_hit)
{
   Int4 index;
   BlastHSP* hsp;
   Uint1* query;
   Uint1* query_nomask = NULL;
   const Uint1* subject = NULL;
   Int4 query_length;
   BlastHSP** hsp_array;
   Int4 subject_length=0;
   Int4 q_start, s_start;
   BlastHitSavingOptions* hit_options = hit_params->options;
   BlastScoringOptions* score_options = score_params->options;
   Uint1* translation_buffer = NULL;
   Int4 * frame_offsets   = NULL;
   Int4 * frame_offsets_a = NULL;
   const Boolean is_rpsblast = Blast_ProgramIsRpsBlast(program_number);
   const Boolean kIsOutOfFrame = score_options->is_ooframe;
   const Boolean kGreedyTraceback = (ext_options->eTbackExt == eGreedyTbck);
   const Boolean kTranslateSubject = 
        (Blast_SubjectIsTranslated(program_number) || program_number == eBlastTypeRpsTblastn);
   const Boolean kFullTranslation = (fence_hit && *fence_hit);
   const Boolean kSmithWaterman = (ext_options->eTbackExt == 
                                   eSmithWatermanTbckFull);
   BlastQueryInfo* query_info = query_info_in;
   Int4 offsets[2];
   Int4 num_initial_hsps = hsp_list->hspcnt;
   BlastIntervalTree* tree = NULL;
   BlastHSPList * orig_hsplist = NULL;
   Boolean fence_error = FALSE;
   SBlastTargetTranslation* target_t = NULL;
   Int2 status = 0;
   
   if (num_initial_hsps == 0) {
      return 0;
   }
   
   if (fence_hit) {
       orig_hsplist = BlastHSPListDup(hsp_list);
       *fence_hit = FALSE;  /* reset fence_hit */
   }
   
   hsp_array = hsp_list->hsp_array;
   
   /* for Smith-Waterman, the results do not overwrite 
      the input list of HSPs one by one; instead, hsp_list
      is rebuilt from scratch */
   if (kSmithWaterman) {
       hsp_list->hspcnt = 0;
       hsp_list->hsp_array = (BlastHSP **)malloc(hsp_list->allocated *
                                                 sizeof(BlastHSP *));
   }

   if (kTranslateSubject) {
      if (! gen_code_string && program_number != eBlastTypeRpsTblastn) {
          if (orig_hsplist) {
              Blast_HSPListFree(orig_hsplist);
          }
          return -1;
      }
      
      if (program_number == eBlastTypeRpsTblastn) {
          translation_buffer = subject_blk->sequence - 1;
          frame_offsets_a = frame_offsets =
              ContextOffsetsToOffsetArray(query_info_in);
      } 
      else
      	BlastTargetTranslationNew(subject_blk, gen_code_string, program_number, kIsOutOfFrame, &target_t);
      if (kIsOutOfFrame)
        subject_length = subject_blk->length;
   } else {
      /* Subject is not translated */
      subject = subject_blk->sequence;
      subject_length = subject_blk->length;
   }

   if (Blast_ProgramIsRpsBlast(program_number)) {
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

   /* set up the tree for HSP containment tests. subject_length 
      is zero only for translated subject sequences, whose maximum
      length is bounded by the length of the first frame */

   tree = Blast_IntervalTreeInit(0, query_blk->length + 1,
                                 0, (subject_length > 0 ? subject_length :
                                 subject_blk->length / CODON_LENGTH) + 1);
   
   for (index=0; index < num_initial_hsps; index++) {
      hsp = hsp_array[index];
      if (program_number == eBlastTypeBlastx && kIsOutOfFrame) {
          Int4 context = hsp->context - hsp->context % CODON_LENGTH;
          Int4 context_offset = query_info->contexts[context].query_offset;
         
          query = query_blk->oof_sequence + CODON_LENGTH + context_offset;
          query_nomask = query;
          query_length = query_info->contexts[context+2].query_offset +
              query_info->contexts[context+2].query_length - context_offset;
      } else {
          query = query_blk->sequence + 
              query_info->contexts[hsp->context].query_offset;
          query_nomask = query_blk->sequence_nomask + 
              query_info->contexts[hsp->context].query_offset;
          query_length = query_info->contexts[hsp->context].query_length;
      }

      /* preliminary RPS blast alignments have not had
         the composition-based correction applied yet, so
         we cannot reliably check whether an HSP is contained
         within another */

      /** @todo FIXME Traceback is always performed for rpsblast
       * because the composition-based correction can change an
       * HSP. It is optional for RPStblastn since no corrections
       * are applied there. Such corrections should be added.
       */
      if (program_number == eBlastTypeRpsBlast ||
          !BlastIntervalTreeContainsHSP(tree, hsp, query_info,
                               hit_options->min_diag_separation)) {

         Int4 start_shift = 0;
         Int4 adjusted_s_length;
         const Uint1* adjusted_subject;
         Int4 cutoff;

         if (kTranslateSubject) {
            if (program_number == eBlastTypeRpsTblastn) {
                 const Int4 context = BLAST_FrameToContext(hsp->subject.frame, program_number);
                 subject = translation_buffer + frame_offsets[context] + 1;
                 subject_length = frame_offsets[context+1] - frame_offsets[context] - 1;
            } else if (kIsOutOfFrame) {
                 /* Out-of-frame with full translation; point subject to the
                       start of the right strand in the mixed-frame sequence. */
                 subject = subject_blk->oof_sequence + CODON_LENGTH;
                 if (hsp->subject.frame < 0)
                      subject += subject_length + 1;
            } else if (kFullTranslation) {
                 /* previous traceback has hit the fence, will do full translation */
                 BlastHSP* hsp_new = Blast_HSPNew();
                 hsp_new->subject.frame = hsp->subject.frame;
                 hsp_new->subject.offset = -1;
            	 subject = Blast_HSPGetTargetTranslation(target_t, hsp_new, &subject_length);
                 hsp_new = Blast_HSPFree(hsp_new);
            } else
            	 subject = Blast_HSPGetTargetTranslation(target_t, hsp, &subject_length);
         }

         if (!kIsOutOfFrame && (((hsp->query.gapped_start == 0 && 
                                  hsp->subject.gapped_start == 0) ||
                 !BLAST_CheckStartForGappedAlignment(hsp, query, 
                                                     subject, sbp)))) {
            Boolean retval = 
               BlastGetOffsetsForGappedAlignment(query, subject, sbp,
                   hsp, &q_start, &s_start);
            if (!retval)
            {  /* Unable to find start for this HSP */
               hsp_array[index] = Blast_HSPFree(hsp);
               continue;
            }
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

         if (!kTranslateSubject && !kSmithWaterman) {
             AdjustSubjectRange(&s_start, &adjusted_s_length, q_start, 
                                query_length, &start_shift);
             adjusted_subject = subject + start_shift;
             /* Shift the gapped start in HSP structure, to compensate for 
                a shift in the other direction later. */
             hsp->subject.gapped_start = s_start;
         }

         /* compute the cutoff score to use */
         if (is_rpsblast) {
            Int4 rps_context = hsp_list->query_index;
            if (program_number == eBlastTypeRpsTblastn) {
               rps_context = hsp_list->query_index * NUM_FRAMES +
                        BLAST_FrameToContext(hsp->subject.frame, program_number);
            }
            cutoff = hit_params->cutoffs[rps_context].cutoff_score;
         }
         else {
            cutoff = hit_params->cutoffs[hsp->context].cutoff_score;
         }

         /* Perform the gapped extension with traceback */
         if (kSmithWaterman) {

             /* with Smith-Waterman, 'hsp' is a placeholder that gives
                the context and frames of local alignments. The following
                will compute all of the HSPs that really belong to this
                query-subject pair, then append them to hsp_list. */
             SmithWatermanScoreWithTraceback(program_number,
                         query, query_length,
                         adjusted_subject, adjusted_s_length,
                         hsp, hsp_list, score_params, 
                         hit_params, gap_align, start_shift, cutoff);
             /* remove the original HSP unconditionally */
             gap_align->score = INT4_MIN;

         } else if (kGreedyTraceback) {
             BLAST_GreedyGappedAlignment(query, adjusted_subject, 
                 query_length, adjusted_s_length, gap_align, 
                 score_params, q_start, s_start, FALSE, TRUE,
                 fence_hit);
         } else {
             BLAST_GappedAlignmentWithTraceback(program_number, query, 
                 adjusted_subject, gap_align, score_params, q_start, s_start, 
                 query_length, adjusted_s_length,
                 fence_hit);
             ASSERT(!(kFullTranslation && *fence_hit));
         }
         
         fence_error = (fence_hit && *fence_hit);
         
         if (fence_error) {
             break;
         }
         
         if (gap_align->score >= cutoff) {
            Boolean delete_hsp = FALSE;
            Blast_HSPUpdateWithTraceback(gap_align, hsp);

            if (kGreedyTraceback) {
               /* Low level greedy algorithm ignores ambiguities, so the score
                * needs to be reevaluated. */
                delete_hsp = 
                    Blast_HSPReevaluateWithAmbiguitiesGapped(hsp, query, 
                        adjusted_subject, hit_params, score_params, sbp);
            }
            if (!delete_hsp) {
                /* Calculate number of identities and check if this HSP meets the
                   percent identity and length criteria. */
                delete_hsp = 
                    Blast_HSPTestIdentityAndLength(program_number, hsp, query_nomask, 
                                                   adjusted_subject, 
                                                   score_options, hit_options);
            }
            if (!delete_hsp) {
               Blast_HSPAdjustSubjectOffset(hsp, start_shift);
               status = BlastIntervalTreeAddHSP(hsp, tree, query_info, 
                                       eQueryAndSubject);
               if (status)
                  return status;
            } else {
               hsp_array[index] = Blast_HSPFree(hsp);
            }
         } else {
            /* Score is below threshold */
            gap_align->edit_script = GapEditScriptDelete(gap_align->edit_script);
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

   target_t = BlastTargetTranslationFree(target_t);
   
   if (kSmithWaterman) {
       /* switch over to the result of the traceback */
       sfree(hsp_array);
       hsp_array = hsp_list->hsp_array;
   }

   if (frame_offsets_a) {
       sfree(frame_offsets_a);
   }
   
   if (! fence_error) {
       /* Remove any HSPs that share a starting or ending diagonal
          with a higher-scoring HSP. */
       Blast_HSPListPurgeHSPsWithCommonEndpoints(program_number, hsp_list);

       /* Sort HSPs by score again, as the scores might have changed. */
       Blast_HSPListSortByScore(hsp_list);

       /* Remove any HSPs that are contained within other HSPs.
          Since the list is sorted by score already, any HSP
          contained by a previous HSP is guaranteed to have a
          lower score, and may be purged. */
       Blast_IntervalTreeReset(tree);
       for (index = 0; index < hsp_list->hspcnt; index++) {
           hsp = hsp_array[index];
       
           if (BlastIntervalTreeContainsHSP(tree, hsp, query_info,
                                     hit_options->min_diag_separation)) {
               hsp_array[index] = Blast_HSPFree(hsp);
           }
           else {
               status = BlastIntervalTreeAddHSP(hsp, tree, query_info, 
                                       eQueryAndSubject);
               if (status)
                  return status;
           }
       }
   }
   
   tree = Blast_IntervalTreeFree(tree);
   
   /* Free the local query_info structure, if necessary (RPS tblastn only) */
   if (query_info != query_info_in)
      sfree(query_info);
   
   if (fence_error) {
       /* Roll back this function by restoring the input HSP list. */
       
       Blast_HSPListSwap(hsp_list, orig_hsplist);
       Blast_HSPListFree(orig_hsplist);
       
       if (gap_align->edit_script) {
           gap_align->edit_script =
               GapEditScriptDelete(gap_align->edit_script);
       }
   } else {
       Blast_HSPListPurgeNullHSPs(hsp_list);
       
       if (orig_hsplist) {
           Blast_HSPListFree(orig_hsplist);
       }
       s_HSPListPostTracebackUpdate(program_number, hsp_list, query_info_in, 
                                    score_params, hit_params, sbp, 
                                    subject_blk->length);
   }
   
   return 0;
}

/** Performs traceback alignment for one HSP list in a PHI BLAST search.
 * @param program_number eBlastTypePhiBlastn or eBlastTypePhiBlastp [in]
 * @param hsp_list HSP list for a single query/subject pair, with preliminary
 *                 alignment information [in]
 * @param query_blk Query sequence [in]
 * @param subject_blk Subject sequence [in]
 * @param gap_align Gapped alignment structure [in]
 * @param sbp Scoring block [in]
 * @param score_params Scoring parameters [in]
 * @param hit_params Hit saving parameters [in]
 * @param query_info Query information, including pattern occurrences in 
 *                   query [in]
 * @param pattern_blk Pattern information and auxiliary structures [in]
 */
static Int2
s_PHITracebackFromHSPList(EBlastProgramType program_number, 
                          BlastHSPList* hsp_list, BLAST_SequenceBlk* query_blk, 
                          BLAST_SequenceBlk* subject_blk, 
                          BlastGapAlignStruct* gap_align, BlastScoreBlk* sbp, 
                          const BlastScoringParameters* score_params,
                          const BlastHitSavingParameters* hit_params,
                          const BlastQueryInfo* query_info,
                          SPHIPatternSearchBlk* pattern_blk)
{
   Int4 index;
   BlastHSP* hsp;
   Uint1* query,* subject;
   Int4 query_length;
   Int4 subject_length=0;
   BlastHSP** hsp_array;
   Int4 q_start, s_start;
   SPHIQueryInfo* pattern_info = NULL;
   
   if ( !Blast_ProgramIsPhiBlast(program_number))
       return -1;
   
   ASSERT(hsp_list && query_blk && subject_blk && gap_align && sbp &&
          score_params && hit_params && query_info && pattern_blk);

   if (hsp_list->hspcnt == 0) {
      return 0;
   }

   hsp_array = hsp_list->hsp_array;

   query = query_blk->sequence;
   query_length = query_blk->length;
   subject = subject_blk->sequence;
   subject_length = subject_blk->length;
   pattern_info = query_info->pattern_info;

   /* Make sure the HSPs in the HSP list are sorted by score, as they should 
      be. */
   ASSERT(Blast_HSPListIsSortedByScore(hsp_list));

   for (index=0; index < hsp_list->hspcnt; index++) {
       Int4 query_pattern_length;
       hsp = hsp_array[index];
       
       q_start = hsp->query.gapped_start;
       s_start = hsp->subject.gapped_start;
       query_pattern_length =
           pattern_info->occurrences[hsp->pat_info->index].length;
       
       /* Perform the gapped extension with traceback */
       PHIGappedAlignmentWithTraceback(query, subject, gap_align, 
                                       score_params, q_start, s_start, 
                                       query_length, subject_length,
                                       query_pattern_length, 
                                       hsp->pat_info->length, pattern_blk);
       
       if (gap_align->score >= hit_params->cutoff_score_min) {
           Blast_HSPUpdateWithTraceback(gap_align, hsp);

       } else {
           /* Score is below threshold */
           gap_align->edit_script = GapEditScriptDelete(gap_align->edit_script);
           hsp_array[index] = Blast_HSPFree(hsp);
       }
   } /* End loop on HSPs */

   /* Sort HSPs by score again, as the scores might have changed. */
   Blast_HSPListSortByScore(hsp_list);
   Blast_HSPListPurgeNullHSPs(hsp_list);

   /* Calculate scores and e-values. */
   Blast_HSPListPHIGetEvalues(hsp_list, sbp, query_info, pattern_blk);
   Blast_HSPListReapByEvalue(hsp_list, hit_params->options);

   Blast_HSPListPHIGetBitScores(hsp_list, sbp);
   
   return 0;
}

EBlastEncoding Blast_TracebackGetEncoding(EBlastProgramType program_number) 
{
    if (Blast_SubjectIsProtein(program_number)) {
        return eBlastEncodingProtein;
    } else if (Blast_SubjectIsTranslated(program_number)) {
        return eBlastEncodingNcbi4na;
    } else {
        return eBlastEncodingNucleotide;
    }
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

void RPSPsiMatrixAttach(BlastScoreBlk* sbp, Int4** rps_pssm,
                        Int4 alphabet_size)
{
    ASSERT(sbp);

    /* Create a dummy PSI-BLAST matrix structure, only to then free it as we'd
     * like to piggy back on the already created structure to use the gapped
     * alignment routines */
    sbp->psi_matrix = (SPsiBlastScoreMatrix*) 
        calloc(1, sizeof(SPsiBlastScoreMatrix));
    ASSERT(sbp->psi_matrix);

    sbp->psi_matrix->pssm = (SBlastScoreMatrix*)
        calloc(1, sizeof(SBlastScoreMatrix));
    ASSERT(sbp->psi_matrix->pssm);

    /* The only data fields that RPS-BLAST really needs */
    sbp->psi_matrix->pssm->data = rps_pssm;
    sbp->psi_matrix->pssm->nrows = alphabet_size;
}

void RPSPsiMatrixDetach(BlastScoreBlk* sbp)
{
    ASSERT(sbp);
    sbp->psi_matrix->pssm->data = NULL;
    sfree(sbp->psi_matrix->pssm);
    sfree(sbp->psi_matrix);
}

/** Prepares an auxiliary BlastQueryInfo structure for the concatenated 
 * database and creates a memory mapped PSSM for RPS BLAST traceback.
 * @param concat_db_info BlastQueryInfo structure to fill. [out]
 * @param gap_align Gapped alignment structure to modify [in] [out]
 * @param rps_info RPS BLAST information structure [in]
 */
static Int2 
s_RPSGapAlignDataPrepare(BlastQueryInfo* concat_db_info, 
                         BlastGapAlignStruct* gap_align, 
                         const BlastRPSInfo* rps_info)
{
   Int4** rps_pssm = NULL;
   Int4 num_profiles;
   Int4 num_pssm_rows;
   Int4* pssm_start;
   BlastRPSProfileHeader *profile_header;
   Int4 index;
   Int4 alphabet_size;

   if (!rps_info)
      return -1;

   ASSERT(concat_db_info);

   profile_header = rps_info->profile_header;
   num_profiles = profile_header->num_profiles;

   /* force the alphabet size to match up to the on-disk format */
   if (profile_header->magic_number == RPS_MAGIC_NUM)
      alphabet_size = 26;
   else
      alphabet_size = 28;

   /* Construct an auxiliary BlastQueryInfo structure for the concatenated
      database. */
   OffsetArrayToContextOffsets(concat_db_info,
                               rps_info->profile_header->start_offsets,
                               eBlastTypeRpsBlast);

   num_pssm_rows = profile_header->start_offsets[num_profiles];
   rps_pssm = (Int4 **)malloc((num_pssm_rows+1) * sizeof(Int4 *));
   pssm_start = profile_header->start_offsets + num_profiles + 1;

   for (index = 0; index < num_pssm_rows + 1; index++) {
      rps_pssm[index] = pssm_start;
      pssm_start += alphabet_size;
   }

   gap_align->positionBased = TRUE;
   RPSPsiMatrixAttach(gap_align->sbp, rps_pssm, alphabet_size);

   return 0;
}

/** Factor to multiply the Karlin-Altschul K parameter by for RPS BLAST, to make
 * e-values more conservative.
 */
#define RPS_K_MULT 1.2

/** Compute traceback information for alignments found by an
 *  RPS blast search. This function performs two major tasks:
 *  - Computes a composition-specific PSSM to be used during the
 *    traceback computation (non-translated searches only)
 *  - After traceback is computed, switch query offsets with 
 *    subject offsets and switch the edit blocks that describe
 *    the alignment. This is required because the entire RPS search
 *    was performed with these quatities reversed.
 * This call is also the first time that enough information 
 * exists to compute E-values for alignments that are found.
 *
 * @param program_number Type of the BLAST program [in]
 * @param hsp_stream A stream for reading HSP lists [in]
 * @param seq_src Source of RPS database consensus sequences; needed only
 *                to calculate number of identities in alignments [in]
 * @param query The original query sequence [in]
 * @param query_info Information associated with the original query. 
 *                   Only used for the search space [in]
 * @param gap_align The auxiliary structure for gapped alignment [in]
 * @param score_params Scoring parameters (esp. scale factor) [in]
 * @param ext_params Gapped extension parameters [in]
 * @param hit_params Parameters for saving hits. Can change if not a 
                     database search [in]
 * @param rps_info Extra information about RPS database. [in]
 * @param results Results structure containing all HSPs, with added
 *                traceback information. [out]
 * @param interrupt_search function callback to allow interruption of BLAST
 *                   search [in, optional]
 * @param progress_info contains information about the progress of the current
 *                   BLAST search [in|out]
 * @return nonzero indicates failure, otherwise zero
 */
static 
Int2 s_RPSComputeTraceback(EBlastProgramType program_number, 
                           BlastHSPStream* hsp_stream, 
                           const BlastSeqSrc* seq_src, 
                           BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
                           BlastGapAlignStruct* gap_align,
                           const BlastScoringParameters* score_params,
                           const BlastExtensionParameters* ext_params,
                           BlastHitSavingParameters* hit_params,
                           const BlastRPSInfo* rps_info,                
                           BlastHSPResults* results,
                           TInterruptFnPtr interrupt_search, 
                           SBlastProgress* progress_info)
{
   Int2 status = 0;
   BlastHSPList* hsp_list;
   BlastScoreBlk* sbp;
   Int4 **rpsblast_pssms = NULL;
   Int4 db_seq_start;
   EBlastEncoding encoding;
   BlastSeqSrcGetSeqArg seq_arg;
   BlastQueryInfo* one_query_info = NULL;
   BLAST_SequenceBlk* one_query = NULL;
   BlastQueryInfo* concat_db_info = NULL;
   Boolean make_up_kbp = FALSE;

   if (!hsp_stream || !seq_src || !results) {
      return -1;
   }
   
   concat_db_info = BlastQueryInfoNew(program_number,
                                      rps_info->profile_header->num_profiles);
   if ((status = 
        s_RPSGapAlignDataPrepare(concat_db_info, gap_align, rps_info)) != 0)
      return status;
      
   sbp = gap_align->sbp;
   rpsblast_pssms = gap_align->sbp->psi_matrix->pssm->data;

   encoding = Blast_TracebackGetEncoding(program_number);
   memset((void*) &seq_arg, 0, sizeof(seq_arg));

   /* the first Karlin block must be valid for traceback to
      be performed; if it is not valid (i.e. if the first query
      was completely masked), make up a valid Karlin block.
      This is possible because all gapped Karlin blocks currently
      look the same */

   if (sbp->kbp_gap[0] == NULL) {
       Int4 index;
       for (index = query_info->first_context; 
                        index <= query_info->last_context; index++) {
           if (sbp->kbp_gap[index] != NULL) {
               sbp->kbp_gap[0] = sbp->kbp_gap[index];
               break;
           }
       }
       make_up_kbp = TRUE;
   }

   while (BlastHSPStreamRead(hsp_stream, &hsp_list) 
          != kBlastHSPStream_Eof) {

      /* check for interrupt */
      if (interrupt_search && (*interrupt_search)(progress_info) == TRUE) {
          hsp_list = Blast_HSPListFree(hsp_list);
          status = BLASTERR_INTERRUPTED;
          break;
      }

      if (!hsp_list)
         continue;

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
      if (BlastSeqSrcGetSequence(seq_src, &seq_arg) < 0)
          continue;

      db_seq_start = concat_db_info->contexts[hsp_list->oid].query_offset;
      
      /* Update the statistics for this database sequence
         (if not a translated search) */
      
      if (program_number == eBlastTypeRpsTblastn) {
         sbp->psi_matrix->pssm->data = rpsblast_pssms + db_seq_start;
      } else {
         const double* karlin_k = rps_info->aux_info.karlin_k;
         /* replace the PSSM and the Karlin values for this DB sequence
            and this query sequence. */
         sbp->psi_matrix->pssm->data = 
            RPSRescalePssm(score_params->scale_factor,
                           one_query->length, one_query->sequence, 
                           seq_arg.seq->length,
                           rpsblast_pssms + db_seq_start, sbp);
         /* The composition of the query could have caused this one
            subject sequence to produce a bad PSSM. This should
            not be a fatal error, so just go on to the next subject
            sequence */
         if (sbp->psi_matrix->pssm->data == NULL) {
            /** @todo FIXME Results should not be silently skipped,
             *        need a warning here
             */
            hsp_list = Blast_HSPListFree(hsp_list);
            BlastSeqSrcReleaseSequence(seq_src, &seq_arg);
            continue;
         }
         
         sbp->kbp_gap[0]->K = RPS_K_MULT * karlin_k[hsp_list->oid];
         sbp->kbp_gap[0]->logK = log(RPS_K_MULT * karlin_k[hsp_list->oid]);
      }


      /* compute the traceback information and calculate E values
         for all HSPs in the list */
      
      Blast_TracebackFromHSPList(program_number, hsp_list, seq_arg.seq, 
         one_query, one_query_info, gap_align, sbp, score_params, 
         ext_params->options, hit_params, NULL, NULL);

      BlastSeqSrcReleaseSequence(seq_src, &seq_arg);

      if (program_number != eBlastTypeRpsTblastn) {
         _PSIDeallocateMatrix((void**)sbp->psi_matrix->pssm->data, 
                              seq_arg.seq->length);
      }

      if (hsp_list->hspcnt == 0) {
         hsp_list = Blast_HSPListFree(hsp_list);
         continue;
      }

      /* Save this HSP list in the results structure. */
      Blast_HSPResultsInsertHSPList(results, hsp_list, 
                                    hit_params->options->hitlist_size);
   }

   /* post-traceback pipes */
   BlastHSPStreamTBackClose(hsp_stream, results);

   if (make_up_kbp)
       sbp->kbp_gap[0] = NULL;

   BlastQueryInfoFree(concat_db_info);

   /* Free the sequence block allocated inside the loop */
   BlastSequenceBlkFree(seq_arg.seq);

   /* Free the single-query structures allocated inside the loop. */
   BlastQueryInfoFree(one_query_info);
   BlastSequenceBlkFree(one_query);

   /* The traceback calculated the E values, so it's safe
      to sort the results now */
   Blast_HSPResultsSortByEvalue(results);

   /* Free the allocated array of memory mapped matrix columns and restore
      the original settings in the gapped alignment structure. */
   sfree(rpsblast_pssms);
   gap_align->positionBased = FALSE;
   RPSPsiMatrixDetach(sbp);

   return status;
}

Int2 
BLAST_ComputeTraceback(EBlastProgramType program_number, 
                       BlastHSPStream* hsp_stream, BLAST_SequenceBlk* query, 
                       BlastQueryInfo* query_info, const BlastSeqSrc* seq_src, 
                       BlastGapAlignStruct* gap_align, 
                       BlastScoringParameters* score_params,
                       const BlastExtensionParameters* ext_params,
                       BlastHitSavingParameters* hit_params,
                       BlastEffectiveLengthsParameters* eff_len_params,
                       const BlastDatabaseOptions* db_options,
                       const PSIBlastOptions* psi_options, 
                       const BlastRPSInfo* rps_info, 
                       SPHIPatternSearchBlk* pattern_blk,
                       BlastHSPResults** results_out, 
                       TInterruptFnPtr interrupt_search, 
                       SBlastProgress* progress_info)
{
   Int2 status = 0;
   BlastHSPResults* results = NULL;
   BlastHSPList* hsp_list = NULL;
   BlastScoreBlk* sbp;
   Int4 default_db_genetic_code = db_options->genetic_code;
 
   if (!query_info || !seq_src || !hsp_stream || !results_out) {
      return -1;
   }
   
   /* Set the raw X-dropoff value for the final gapped extension with 
      traceback */
   gap_align->gap_x_dropoff = ext_params->gap_x_dropoff_final;

   sbp = gap_align->sbp;
  
   /* signal the traceback stage has started */
   if (progress_info)
       progress_info->stage = eTracebackSearch;

   results = Blast_HSPResultsNew(query_info->num_queries);

   if (Blast_ProgramIsRpsBlast(program_number)) {
       status =
           s_RPSComputeTraceback(program_number, hsp_stream, seq_src, query,
                                 query_info, gap_align, score_params,
                                 ext_params, hit_params, rps_info, results,
                                 interrupt_search, progress_info);
   } else if (ext_params->options->compositionBasedStats > 0 ||
              ext_params->options->eTbackExt == eSmithWatermanTbck) {
      /* FIXME partial sequence fetching/translation could lead to fence hit
         and seg fault */
      status =
          Blast_RedoAlignmentCore(program_number, query, query_info, sbp,
                                  hsp_stream, seq_src, default_db_genetic_code,
                                  score_params, ext_params, hit_params, 
                                  psi_options, results);
   } else {
      Int4 i;
      BlastSeqSrcGetSeqArg seq_arg;
      EBlastEncoding encoding = Blast_TracebackGetEncoding(program_number);
      Boolean perform_traceback = score_params->options->gapped_calculation;
      const Boolean kPhiBlast = Blast_ProgramIsPhiBlast(program_number);
      BlastHSPStreamResultBatch *batch = 
                      Blast_HSPStreamResultBatchInit(query_info->num_queries);

      memset((void*) &seq_arg, 0, sizeof(seq_arg));

      /* Retrieve all HSP lists from the HSPStream that contain
         hits to the next subject OID. */
      while (BlastHSPStreamBatchRead(hsp_stream, batch) 
             != kBlastHSPStream_Eof) {

         /* check for interrupt */
         if (interrupt_search && (*interrupt_search)(progress_info) == TRUE) {
             Blast_HSPStreamResultBatchReset(batch);
             status = BLASTERR_INTERRUPTED;
             break;
         }

         /* traceback will require fetching the subject sequence */

         if (perform_traceback) {
            seq_arg.oid = batch->hsplist_array[0]->oid;
            seq_arg.encoding = encoding;
            seq_arg.check_oid_exclusion = TRUE;
            seq_arg.reset_ranges = FALSE;
            
            BlastSequenceBlkClean(seq_arg.seq);
            if (BlastSeqSrcGetSequence(seq_src, &seq_arg) < 0) {
               Blast_HSPStreamResultBatchReset(batch);
               continue;
            }

            /* If the subject is translated and the BlastSeqSrc implementation
             * doesn't provide a genetic code string, use the default genetic
             * code for all subjects (as in the C toolkit) */
            if (Blast_SubjectIsTranslated(program_number) && 
                seq_arg.seq->gen_code_string == NULL) {
                seq_arg.seq->gen_code_string = 
                    GenCodeSingletonFind(default_db_genetic_code);
                ASSERT(seq_arg.seq->gen_code_string);
            }
            
            if (BlastSeqSrcGetTotLen(seq_src) == 0) {
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
                                NULL, eff_len_params)) != 0) {
                  Blast_HSPStreamResultBatchReset(batch);
                  break;
               }
            }
         }

         /* process all the hits to this subject sequence, one
            list at a time */

         for (i = 0; i < batch->num_hsplists; i++) {

            hsp_list = batch->hsplist_array[i];

            if (perform_traceback) {
               if (kPhiBlast) {
                  s_PHITracebackFromHSPList(program_number, hsp_list, query, 
                                            seq_arg.seq, gap_align, sbp, 
                                            score_params, hit_params, 
                                            query_info, pattern_blk);
               } else {
                  Boolean fence_hit = FALSE;
                  Blast_TracebackFromHSPList(program_number, hsp_list, query,
                                             seq_arg.seq, query_info, 
                                             gap_align, sbp, score_params,
                                             ext_params->options, hit_params,
                                             seq_arg.seq->gen_code_string,
                                             &fence_hit);
                    
                  if (fence_hit) {
                     /* Disable range support and refetch the 
                        (whole) subject sequence */
                        
                     seq_arg.reset_ranges = TRUE;
                     BlastSeqSrcReleaseSequence(seq_src, &seq_arg);
                     BlastSeqSrcGetSequence(seq_src, &seq_arg);
                        
                     /* The C toolkit will erase genetic_code, so do it again */
                     if (Blast_SubjectIsTranslated(program_number) && 
                         seq_arg.seq->gen_code_string == NULL) {
                         seq_arg.seq->gen_code_string = 
                             GenCodeSingletonFind(default_db_genetic_code);
                         ASSERT(seq_arg.seq->gen_code_string);
                     }
            
                     /* Retry the alignment with fence_hit set*/
                     Blast_TracebackFromHSPList(program_number, hsp_list, 
                                                query, seq_arg.seq, 
                                                query_info, gap_align,
                                                sbp, score_params, 
                                                ext_params->options, 
                                                hit_params, 
                                                seq_arg.seq->gen_code_string,
                                                &fence_hit);
                     ASSERT(fence_hit == FALSE);
                  } /* fence_hit */
               }    /* !phi_blast */

            } else {
               /* traceback skipped; compute bit scores for searches 
                  where the traceback phase is seperated from the 
                  preliminary search. */
             
               Blast_HSPListGetBitScores(hsp_list, FALSE, sbp);
            }
         
            /* Free HSP list if all HSPs have been deleted. */

            batch->hsplist_array[i] = NULL;
            if (hsp_list->hspcnt == 0) {
               hsp_list = Blast_HSPListFree(hsp_list);
            }
            else {
               Blast_HSPResultsInsertHSPList(results, hsp_list, 
                                          hit_params->options->hitlist_size);
            }
         }      /* loop over one HSPList batch */

         if (perform_traceback) {
            BlastSeqSrcReleaseSequence(seq_src, &seq_arg);
         }

      }         /* loop over all batches */

      batch = Blast_HSPStreamResultBatchFree(batch);
      /* post-traceback pipes */
      BlastHSPStreamTBackClose(hsp_stream, results);

      BlastSequenceBlkFree(seq_arg.seq);
   }

   /* Re-sort the hit lists according to their best e-values, because
      they could have changed. Only do this for a database search. */
   if (BlastSeqSrcGetTotLen(seq_src) > 0)
      Blast_HSPResultsSortByEvalue(results);

   /* Eliminate extra hits from results, if preliminary hit list size is larger
      than the final hit list size */
    s_BlastPruneExtraHits(results, hit_params->options->hitlist_size);

    if (status == BLASTERR_INTERRUPTED) {
        results = Blast_HSPResultsFree(results);
    }

    *results_out = results;

    return status;
}

Int2 
Blast_RunTracebackSearch(EBlastProgramType program, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
   const BlastSeqSrc* seq_src, const BlastScoringOptions* score_options,
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingOptions* hit_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   const BlastDatabaseOptions* db_options, 
   const PSIBlastOptions* psi_options, BlastScoreBlk* sbp,
   BlastHSPStream* hsp_stream, const BlastRPSInfo* rps_info,
   SPHIPatternSearchBlk* pattern_blk, BlastHSPResults** results)
{
   Int2 status = 0;
   BlastScoringParameters* score_params = NULL; /**< Scoring parameters */
   BlastExtensionParameters* ext_params = NULL; /**< Gapped extension 
                                                   parameters */
   BlastHitSavingParameters* hit_params = NULL; /**< Hit saving parameters*/
   BlastEffectiveLengthsParameters* eff_len_params = NULL; /**< Parameters
                                        for effective lengths calculations */
   BlastGapAlignStruct* gap_align = NULL; /**< Gapped alignment structure */

   status = 
      BLAST_GapAlignSetUp(program, seq_src, score_options, eff_len_options, 
         ext_options, hit_options, query_info, sbp, &score_params, 
         &ext_params, &hit_params, &eff_len_params, &gap_align);
   if (status)
      return status;

   /* Prohibit any subsequent writing to the HSP stream. */
   BlastHSPStreamClose(hsp_stream);

   status = 
      BLAST_ComputeTraceback(program, hsp_stream, query, query_info,
                             seq_src, gap_align, score_params, ext_params, 
                             hit_params, eff_len_params, db_options, psi_options,
                             rps_info, pattern_blk, results, 0, 0);

   /* Do not destruct score block here */
   gap_align->sbp = NULL;
   BLAST_GapAlignStructFree(gap_align);

   score_params = BlastScoringParametersFree(score_params);
   hit_params = BlastHitSavingParametersFree(hit_params);
   ext_params = BlastExtensionParametersFree(ext_params);
   eff_len_params = BlastEffectiveLengthsParametersFree(eff_len_params);
   return status;
}
