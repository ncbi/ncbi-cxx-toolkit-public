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

/** @file blast_hits.c
 * BLAST functions for saving hits after the (preliminary) gapped alignment
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_util.h>
#include "blast_hits_priv.h"
#include "blast_itree.h"

NCBI_XBLAST_EXPORT
Int2 SBlastHitsParametersNew(const BlastHitSavingOptions* hit_options,
                             const BlastExtensionOptions* ext_options,
                             const BlastScoringOptions* scoring_options,
                             SBlastHitsParameters* *retval)
{
       ASSERT(retval);
       *retval = NULL;

       if (hit_options == NULL ||
           ext_options == NULL || 
           scoring_options == NULL)
           return 1;

       *retval = (SBlastHitsParameters*) malloc(sizeof(SBlastHitsParameters));
       if (*retval == NULL)
           return 2;

       if (ext_options->compositionBasedStats)
            (*retval)->prelim_hitlist_size = 2*hit_options->hitlist_size;
       else if (scoring_options->gapped_calculation)
            (*retval)->prelim_hitlist_size = 
                MIN(2*hit_options->hitlist_size, hit_options->hitlist_size+50);
       else
            (*retval)->prelim_hitlist_size = hit_options->hitlist_size;

       (*retval)->options = hit_options;

       return 0;
}

NCBI_XBLAST_EXPORT
SBlastHitsParameters* SBlastHitsParametersFree(SBlastHitsParameters* param)
{
       if (param)
       {
               param->options = NULL;
               sfree(param);
       }
       return NULL;
}

/********************************************************************************
          Functions manipulating BlastHSP's
********************************************************************************/

BlastHSP* Blast_HSPFree(BlastHSP* hsp)
{
   if (!hsp)
      return NULL;
   hsp->gap_info = GapEditScriptDelete(hsp->gap_info);
   sfree(hsp->pat_info);
   sfree(hsp);
   return NULL;
}

BlastHSP* Blast_HSPNew(void)
{
     BlastHSP* new_hsp = (BlastHSP*) calloc(1, sizeof(BlastHSP));
     return new_hsp;
}

/*
   Comments in blast_hits.h
*/
Int2
Blast_HSPInit(Int4 query_start, Int4 query_end, Int4 subject_start, 
              Int4 subject_end, Int4 query_gapped_start, 
              Int4 subject_gapped_start, Int4 query_context, 
              Int2 query_frame, Int2 subject_frame, Int4 score, 
              GapEditScript* *gap_edit, BlastHSP* *ret_hsp)
{
   BlastHSP* new_hsp = NULL;

   if (!ret_hsp)
      return -1;

   new_hsp = Blast_HSPNew();

   *ret_hsp = NULL;

   if (new_hsp == NULL)
	return -1;


   new_hsp->query.offset = query_start;
   new_hsp->subject.offset = subject_start;
   new_hsp->query.end = query_end;
   new_hsp->subject.end = subject_end;
   new_hsp->query.gapped_start = query_gapped_start;
   new_hsp->subject.gapped_start = subject_gapped_start;
   new_hsp->context = query_context;
   new_hsp->query.frame = query_frame;
   new_hsp->subject.frame = subject_frame;
   new_hsp->score = score;
   if (gap_edit && *gap_edit)
   { /* If this is non-NULL transfer ownership. */
        new_hsp->gap_info = *gap_edit;
        *gap_edit = NULL;
   }

   *ret_hsp = new_hsp;

   return 0;
}

/** Calculate e-value for an HSP found by PHI BLAST.
 * @param hsp An HSP found by PHI BLAST [in]
 * @param sbp Scoring block with statistical parameters [in]
 * @param query_info Structure containing information about pattern counts [in]
 */
static void 
s_HSPPHIGetEvalue(BlastHSP* hsp, BlastScoreBlk* sbp, 
                  const BlastQueryInfo* query_info)
{
   double paramC;
   double Lambda;
   Int8 pattern_space;
  
   if (!hsp || !sbp)
      return;

   paramC = sbp->kbp[0]->paramC;
   Lambda = sbp->kbp[0]->Lambda;

   pattern_space = query_info->contexts[0].eff_searchsp;

   hsp->evalue = pattern_space*paramC*(1+Lambda*hsp->score)*
                    exp(-Lambda*hsp->score);
}

/** Update HSP data after reevaluation with ambiguities. In particular this 
 * function calculates number of identities and checks if the percent identity
 * criterion is satisfied.
 * @param hsp HSP to update [in] [out]
 * @param gapped Is this a gapped search? [in]
 * @param cutoff_score Cutoff score for saving the HSP [in]
 * @param score New score [in]
 * @param query_start Start of query sequence [in]
 * @param subject_start Start of subject sequence [in]
 * @param best_q_start Pointer to start of the new alignment in query [in]
 * @param best_q_end Pointer to end of the new alignment in query [in]
 * @param best_s_start Pointer to start of the new alignment in subject [in]
 * @param best_s_end Pointer to end of the new alignment in subject [in]
 * @param best_start_esp Link in the edit script chain where the new alignment
 *                       starts. [in]
 * @param best_prev_esp Link in the edit script chain immediately preceding
 *                      best_start_esp. [in]
 * @param best_end_esp Link in the edit script chain where the new alignment 
 *                     ends. [in]
 * @param best_end_esp_num Number of edit operations in the last edit script,
 *                         that are included in the alignment. [in]
 * @return TRUE if HSP is scheduled to be deleted.
 */
static Boolean
s_UpdateReevaluatedHSP(BlastHSP* hsp, Boolean gapped,
                       Int4 cutoff_score,
                       Int4 score, Uint1* query_start, Uint1* subject_start, 
                       Uint1* best_q_start, Uint1* best_q_end, 
                       Uint1* best_s_start, Uint1* best_s_end, 
                       GapEditScript* best_start_esp, 
                       GapEditScript* best_prev_esp, GapEditScript* best_end_esp,
                       Int4 best_end_esp_num)
{
    Boolean delete_hsp = TRUE;

    hsp->score = score;

    /* Make corrections in edit block and free any parts that are no longer
       needed */
    if (gapped && best_start_esp && best_start_esp != hsp->gap_info) {
        /* best_prev_esp is the link in the chain exactly preceding the starting
           edit script of the best part of the alignment. If best alignment was
           found, but it does not start from the original start, best_prev_esp 
           cannot be NULL. */
        ASSERT (best_prev_esp);
        /* Unlink the good part of the alignment from the previous 
           (negative-scoring) part that is being deleted. */
        best_prev_esp->next = NULL;
        GapEditScriptDelete(hsp->gap_info);
        hsp->gap_info = best_start_esp;
    }
    
    if (hsp->score >= cutoff_score) {
        /* Update all HSP offsets. */
        hsp->query.offset = best_q_start - query_start;
        hsp->query.end = hsp->query.offset + best_q_end - best_q_start;
        hsp->subject.offset = best_s_start - subject_start;
        hsp->subject.end = hsp->subject.offset + best_s_end - best_s_start;

        if (gapped) {
            if (best_end_esp->next != NULL) {
                GapEditScriptDelete(best_end_esp->next);
                best_end_esp->next = NULL;
            }
            best_end_esp->num = best_end_esp_num;
        }
        delete_hsp = FALSE;
    }

    return delete_hsp;
}
                               
Boolean Blast_HSPReevaluateWithAmbiguitiesGapped(BlastHSP* hsp, 
           Uint1* query_start, Uint1* subject_start, 
           const BlastHitSavingParameters* hit_params, 
           const BlastScoringParameters* score_params, 
           BlastScoreBlk* sbp)
{
   Int4 sum, score, gap_open, gap_extend;

   GapEditScript* best_start_esp; /* Starting edit script for the best scoring
                                     piece of the alignment. */
   GapEditScript* best_end_esp; /* Ending edit script for the best scoring piece
                                   of the alignment. */
   GapEditScript* best_prev_esp; /* Previous link in the edit script chain 
                                    before best_end_esp. */
   Int4 best_end_esp_num; /* Number of operations inside the ending edit script
                             for the best scoring piece. */

   Uint1* best_q_start; /* Start of the best scoring part in query. */
   Uint1* best_s_start; /* Start of the best scoring part in subject. */
   Uint1* best_q_end;   /* End of the best scoring part in query. */
   Uint1* best_s_end;   /* End of the best scoring part in subject. */
   
   GapEditScript* current_start_esp; /* Starting edit script for the current 
                                        part of the alignment. */
   GapEditScript* current_esp; /* Ending edit script for the current part
                          of the alignment. */
   GapEditScript* current_prev_esp; /* Previous link in the edit script chain 
                                       before current_esp. */
   GapEditScript* current_start_prev_esp; /* Previous link in the edit script 
                                             chain before current_start_esp. */

   Uint1* current_q_start; /* Start of the current part of the alignment in 
                           query. */
   Uint1* current_s_start; /* Start of the current part of the alignment in 
                           subject. */
   Int4 op_index; /* Index of an operation within a single edit script. */

   Uint1* query,* subject;
   Int4** matrix;
   Int2 factor = 1;
   const Uint1 kResidueMask = 0x0f;

   matrix = sbp->matrix->data;

   /* For a non-affine greedy case, calculate the real value of the gap 
      extension penalty. Multiply all scores by 2 if it is not integer. */
   if (score_params->gap_open == 0 && score_params->gap_extend == 0) {
      if (score_params->reward % 2 == 1) 
         factor = 2;
      gap_open = 0;
      gap_extend = 
         (score_params->reward - 2*score_params->penalty) * factor / 2;
   } else {
      gap_open = score_params->gap_open;
      gap_extend = score_params->gap_extend;
   }

   query = query_start + hsp->query.offset; 
   subject = subject_start + hsp->subject.offset;
   score = 0;
   sum = 0;

   /* Point all pointers to the beginning of the alignment. */
   best_q_start = best_q_end = current_q_start = query;
   best_s_start = best_s_end = current_s_start = subject;
   best_start_esp = best_end_esp = current_start_esp = current_esp =
       hsp->gap_info;
   /* There are no previous edit scripts at the beginning. */
   best_prev_esp = current_prev_esp = current_start_prev_esp = NULL;
   best_end_esp_num = 0;
   op_index = 0;
   
   while (current_esp) {
       /* Process substitutions one operation at a time, full gaps in one 
          step. */
       if (current_esp->op_type == eGapAlignSub) {
           sum += factor*matrix[*query & kResidueMask][*subject];
           query++;
           subject++;
           op_index++;
       } else if (current_esp->op_type == eGapAlignDel) {
           sum -= gap_open + gap_extend * current_esp->num;
           subject += current_esp->num;
           op_index += current_esp->num;
       } else if (current_esp->op_type == eGapAlignIns) {
           sum -= gap_open + gap_extend * current_esp->num;
           query += current_esp->num;
           op_index += current_esp->num;
       }
      
       if (sum < 0) {
           /* Point current edit script chain start to the new place.
              If we are in the middle of an edit script, reduce its length and
              point operation index to the beginning of a modified edit script;
              if we are at the end, move to the next edit script. */
           if (op_index < current_esp->num) {
               current_esp->num -= op_index;
               current_start_esp = current_esp;
               current_start_prev_esp = current_prev_esp;
               op_index = 0;
           } else {
               current_start_esp = current_esp->next;
               current_start_prev_esp = current_esp;
           }
           /* Set sum to 0, to start a fresh count. */
           sum = 0;
           /* Set current starting positions in sequences to the new start. */
           current_q_start = query;
           current_s_start = subject;

           /* If score has passed the cutoff at some point, leave the best score
              and edit scripts positions information untouched, otherwise reset
              the best score to 0 and point the best edit script positions to
              the new start. */
           if (score < hit_params->cutoff_score) {
               /* Start from new offset; discard all previous information. */
               best_q_start = query;
               best_s_start = subject;
               score = 0; 
               
               /* Set best start and end edit script pointers to new start. */
               best_start_esp = current_start_esp;
               best_end_esp = current_start_esp;
               best_prev_esp = current_prev_esp;
           }
       } else if (sum > score) {
           /* Remember this point as the best scoring end point, and the current
              start of the alignment as the start of the best alignment. */
           score = sum;
           
           best_q_start = current_q_start;
           best_s_start = current_s_start;
           best_q_end = query;
           best_s_end = subject;
           
           best_start_esp = current_start_esp;
           best_end_esp = current_esp;
           best_prev_esp = current_start_prev_esp;
           best_end_esp_num = op_index;
       }
       /* If operation index has reached the end of the current edit script, 
          move on to the next link in the edit script chain. */
       if (op_index >= current_esp->num) {
           op_index = 0;
           current_prev_esp = current_esp;
           current_esp = current_esp->next;
       }
   } /* loop on edit scripts */
   
   score /= factor;
   
   /* Update HSP data. */
   return
       s_UpdateReevaluatedHSP(hsp, TRUE, hit_params->cutoff_score, score, 
                              query_start, subject_start, best_q_start, 
                              best_q_end, best_s_start, best_s_end, 
                              best_start_esp, best_prev_esp, best_end_esp, 
                              best_end_esp_num);
}

/** Update HSP data after reevaluation with ambiguities for an ungapped search.
 * In particular this function calculates number of identities and checks if the
 * percent identity criterion is satisfied.
 * @param hsp HSP to update [in] [out]
 * @param cutoff_score Cutoff score for saving the HSP [in]
 * @param score New score [in]
 * @param query_start Start of query sequence [in]
 * @param subject_start Start of subject sequence [in]
 * @param best_q_start Pointer to start of the new alignment in query [in]
 * @param best_q_end Pointer to end of the new alignment in query [in]
 * @param best_s_start Pointer to start of the new alignment in subject [in]
 * @param best_s_end Pointer to end of the new alignment in subject [in]
 * @return TRUE if HSP is scheduled to be deleted.
 */
static Boolean
s_UpdateReevaluatedHSPUngapped(BlastHSP* hsp, Int4 cutoff_score, Int4 score, 
                               Uint1* query_start, Uint1* subject_start, 
                               Uint1* best_q_start, Uint1* best_q_end, 
                               Uint1* best_s_start, Uint1* best_s_end)
{
    return
        s_UpdateReevaluatedHSP(hsp, FALSE, cutoff_score, score, query_start, 
                               subject_start, best_q_start, best_q_end, 
                               best_s_start, best_s_end, NULL, NULL, NULL, 0);
}

Boolean 
Blast_HSPReevaluateWithAmbiguitiesUngapped(BlastHSP* hsp, Uint1* query_start, 
   Uint1* subject_start, const BlastInitialWordParameters* word_params,
   BlastScoreBlk* sbp, Boolean translated)
{
   Int4 sum, score;
   Int4** matrix;
   Uint1* query,* subject;
   Uint1* best_q_start,* best_s_start,* best_q_end,* best_s_end;
   Uint1* current_q_start, * current_s_start;
   Int4 index;
   const Uint1 kResidueMask = (translated ? 0xff : 0x0f);
   Int4 hsp_length = hsp->query.end - hsp->query.offset;

   matrix = sbp->matrix->data;

   query = query_start + hsp->query.offset; 
   subject = subject_start + hsp->subject.offset;
   score = 0;
   sum = 0;
   best_q_start = best_q_end = current_q_start = query;
   best_s_start = best_s_end = current_s_start = subject;
   index = 0;
   
   for (index = 0; index < hsp_length; ++index) {
      sum += matrix[*query & kResidueMask][*subject];
      query++;
      subject++;
      if (sum < 0) {
          /* Start from new offset */
          sum = 0;
          current_q_start = query;
          current_s_start = subject;
          /* If previous top score never reached the cutoff, discard the front
             part of the alignment completely. Otherwise keep pointer to the 
             top-scoring front part. */
         if (score < word_params->cutoff_score) {
            best_q_start = best_q_end = query;
            best_s_start = best_s_end = subject;
            score = 0;
         }
      } else if (sum > score) {
         /* Remember this point as the best scoring end point */
         score = sum;
         best_q_end = query;
         best_s_end = subject;
         /* Set start of alignment to the current start, dismissing the 
            previous top-scoring piece. */
         best_q_start = current_q_start;
         best_s_start = current_s_start;
      }
   }

   /* Update HSP data. */
   return
       s_UpdateReevaluatedHSPUngapped(hsp, word_params->cutoff_score, score, 
                                      query_start, subject_start, best_q_start,
                                      best_q_end, best_s_start, best_s_end);
} 

Int2
Blast_HSPGetNumIdentities(Uint1* query, Uint1* subject, 
                          BlastHSP* hsp, Int4* num_ident_ptr, 
                          Int4* align_length_ptr)
{
   Int4 i, num_ident, align_length, q_off, s_off;
   Uint1* q,* s;
   GapEditScript* esp;
   Int4 q_length = hsp->query.end - hsp->query.offset;
   Int4 s_length = hsp->subject.end - hsp->subject.offset;

   q_off = hsp->query.offset;
   s_off = hsp->subject.offset;

   if (!subject || !query)
      return -1;

   q = &query[q_off];
   s = &subject[s_off];

   num_ident = 0;
   align_length = 0;


   if (!hsp->gap_info) {
      /* Ungapped case. Check that lengths are the same in query and subject,
         then count number of matches. */
      if (q_length != s_length)
         return -1;
      align_length = q_length; 
      for (i=0; i<align_length; i++) {
         if (*q++ == *s++)
            num_ident++;
      }
   } else {
      for (esp = hsp->gap_info; esp; esp = esp->next) {
         align_length += esp->num;
         switch (esp->op_type) {
         case eGapAlignSub:
            for (i=0; i<esp->num; i++) {
               if (*q++ == *s++)
                  num_ident++;
            }
            break;
         case eGapAlignDel:
            s += esp->num;
            break;
         case eGapAlignIns:
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

Int2
Blast_HSPGetOOFNumIdentities(Uint1* query, Uint1* subject, 
   BlastHSP* hsp, EBlastProgramType program, 
   Int4* num_ident_ptr, Int4* align_length_ptr)
{
   Int4 i, num_ident, align_length;
   Uint1* q,* s;
   GapEditScript* esp;

   if (!hsp->gap_info || !subject || !query)
      return -1;


   if (program == eBlastTypeTblastn ||
       program == eBlastTypeRpsTblastn) {
       q = &query[hsp->query.offset];
       s = &subject[hsp->subject.offset];
   } else {
       s = &query[hsp->query.offset];
       q = &subject[hsp->subject.offset];
   }
   num_ident = 0;
   align_length = 0;

   for (esp = hsp->gap_info; esp; esp = esp->next) {
      switch (esp->op_type) {
      case eGapAlignSub: /* Substitution */
         align_length += esp->num;
         for (i=0; i<esp->num; i++) {
            if (*q == *s)
               num_ident++;
            ++q;
            s += CODON_LENGTH;
         }
         break;
      case eGapAlignIns: /* Insertion */
         align_length += esp->num;
         s += esp->num * CODON_LENGTH;
         break;
      case eGapAlignDel: /* Deletion */
         align_length += esp->num;
         q += esp->num;
         break;
      case eGapAlignDel2: /* Gap of two nucleotides. */
         s -= 2;
         break;
      case eGapAlignDel1: /* Gap of one nucleotide. */
         s -= 1;
         break;
      case eGapAlignIns1: /* Insertion of one nucleotide. */
         s += 1;
         break;
      case eGapAlignIns2: /* Insertion of two nucleotides. */
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

/** Calculates number of identities and alignment lengths of an HSP and 
 * determines whether this HSP should be kept or deleted. The num_ident
 * field of the BlastHSP structure is filled here.
 * @param program_number Type of BLAST program [in]
 * @param hsp An HSP structure [in] [out]
 * @param query Query sequence [in]
 * @param subject Subject sequence [in]
 * @param score_options Scoring options, needed to distinguish the 
 *                      out-of-frame case. [in]
 * @param hit_options Hit saving options containing percent identity and
 *                    HSP length thresholds.
 * @return FALSE if HSP passes the test, TRUE if it should be deleted.
 */ 
Boolean
Blast_HSPTestIdentityAndLength(EBlastProgramType program_number, 
                               BlastHSP* hsp, Uint1* query, Uint1* subject, 
                               const BlastScoringOptions* score_options,
                               const BlastHitSavingOptions* hit_options)
{
   Int4 align_length = 0;
   Boolean delete_hsp = FALSE;

   ASSERT(hsp && query && subject && score_options && hit_options);

   /* Calculate alignment length and number of identical letters. 
      Do not get the number of identities if the query is not available */
   if (score_options->is_ooframe) {
       Blast_HSPGetOOFNumIdentities(query, subject, hsp, program_number, 
                                    &hsp->num_ident, &align_length);
   } else {
       Blast_HSPGetNumIdentities(query, subject, hsp, 
                                 &hsp->num_ident, &align_length);
   }
      
   /* Check whether this HSP passes the percent identity and minimal hit 
      length criteria, and delete it if it does not. */
   if ((hsp->num_ident * 100 < 
        align_length * hit_options->percent_identity) ||
       align_length < hit_options->min_hit_length) {
      delete_hsp = TRUE;
   }

   return delete_hsp;
}

void 
Blast_HSPCalcLengthAndGaps(const BlastHSP* hsp, Int4* length_out,
                           Int4* gaps_out, Int4* gap_opens_out)
{
   Int4 length = hsp->query.end - hsp->query.offset;
   Int4 s_length = hsp->subject.end - hsp->subject.offset;
   Int4 gap_opens = 0, gaps = 0;

   if (hsp->gap_info) {
      GapEditScript* esp = hsp->gap_info;
      for ( ; esp; esp = esp->next) {
         if (esp->op_type == eGapAlignDel) {
            length += esp->num;
            gaps += esp->num;
            ++gap_opens;
         } else if (esp->op_type == eGapAlignIns) {
            ++gap_opens;
            gaps += esp->num;
         }
      }
   } else if (s_length > length) {
      length = s_length;
   }

   *length_out = length;
   *gap_opens_out = gap_opens;
   *gaps_out = gaps;
}

/** Adjust start and end of an HSP in a translated sequence segment.
 * @param segment BlastSeg structure (part of BlastHSP) [in]
 * @param seq_length Length of the full sequence [in]
 * @param start Start of the alignment in this segment in nucleotide 
 *              coordinates, 1-offset [out]
 * @param end End of the alignment in this segment in nucleotide 
 *            coordinates, 1-offset [out]
 */
static void 
s_BlastSegGetTranslatedOffsets(const BlastSeg* segment, Int4 seq_length, 
                              Int4* start, Int4* end)
{
   if (segment->frame < 0)	{
      *start = seq_length - CODON_LENGTH*segment->offset + segment->frame;
      *end = seq_length - CODON_LENGTH*segment->end + segment->frame + 1;
   } else if (segment->frame > 0)	{
      *start = CODON_LENGTH*(segment->offset) + segment->frame - 1;
      *end = CODON_LENGTH*segment->end + segment->frame - 2;
   } else {
      *start = segment->offset + 1;
      *end = segment->end;
   }
}

void 
Blast_HSPGetAdjustedOffsets(EBlastProgramType program, BlastHSP* hsp, 
                            Int4 query_length, Int4 subject_length,
                            Int4* q_start, Int4* q_end,
                            Int4* s_start, Int4* s_end)
{
   if (!hsp->gap_info) {
      *q_start = hsp->query.offset + 1;
      *q_end = hsp->query.end;
      *s_start = hsp->subject.offset + 1;
      *s_end = hsp->subject.end;
      return;
   }

   if (program != eBlastTypeTblastn && program != eBlastTypeBlastx &&
       program != eBlastTypeTblastx && program != eBlastTypeRpsTblastn) {
      if (hsp->query.frame != hsp->subject.frame) {
         /* Blastn: if different strands, flip offsets in query; leave 
            offsets in subject as they are, but change order for correct
            correspondence. */
         *q_end = query_length - hsp->query.offset;
         *q_start = *q_end - hsp->query.end + hsp->query.offset + 1;
         *s_end = hsp->subject.offset + 1;
         *s_start = hsp->subject.end;
      } else {
         *q_start = hsp->query.offset + 1;
         *q_end = hsp->query.end;
         *s_start = hsp->subject.offset + 1;
         *s_end = hsp->subject.end;
      }
   } else {
      s_BlastSegGetTranslatedOffsets(&hsp->query, query_length, q_start, q_end);
      s_BlastSegGetTranslatedOffsets(&hsp->subject, subject_length, 
                                    q_start, s_end);
   }
}

/** Checks if the first HSP is contained in the second. 
 * @todo FIXME: can this function be reused in other contexts?
 */
static Boolean 
s_BlastHSPContained(BlastHSP* hsp1, BlastHSP* hsp2)
{
   Boolean hsp_start_is_contained=FALSE, hsp_end_is_contained=FALSE;

   if (hsp1->score > hsp2->score)
      return FALSE;

   if (CONTAINED_IN_HSP(hsp2->query.offset, hsp2->query.end, hsp1->query.offset, hsp2->subject.offset, hsp2->subject.end, hsp1->subject.offset) == TRUE) {
      hsp_start_is_contained = TRUE;
   }
   if (CONTAINED_IN_HSP(hsp2->query.offset, hsp2->query.end, hsp1->query.end, hsp2->subject.offset, hsp2->subject.end, hsp1->subject.end) == TRUE) {
      hsp_end_is_contained = TRUE;
   }
   
   return (hsp_start_is_contained && hsp_end_is_contained);
}


Int2
Blast_HSPGetPartialSubjectTranslation(BLAST_SequenceBlk* subject_blk, 
                                      BlastHSP* hsp,
                                      Boolean is_ooframe, 
                                      const Uint1* gen_code_string, 
                                      Uint1** translation_buffer_ptr,
                                      Uint1** subject_ptr, 
                                      Int4* subject_length_ptr,
                                      Int4* start_shift_ptr)
{
   Int4 translation_length;
   Uint1* translation_buffer;
   Uint1* subject;
   Int4 start_shift;
   Int4 nucl_shift;
   Int2 status = 0;

   ASSERT(subject_blk && hsp && gen_code_string && translation_buffer_ptr &&
          subject_ptr && subject_length_ptr && start_shift_ptr);

   translation_buffer = *translation_buffer_ptr;
   sfree(translation_buffer);

   if (!is_ooframe) {
      start_shift = 
         MAX(0, 3*hsp->subject.offset - MAX_FULL_TRANSLATION);
      translation_length =
         MIN(3*hsp->subject.end + MAX_FULL_TRANSLATION, 
             subject_blk->length) - start_shift;
      if (hsp->subject.frame > 0) {
         nucl_shift = start_shift;
      } else {
         nucl_shift = subject_blk->length - start_shift - translation_length;
      }
      status = (Int2)
         Blast_GetPartialTranslation(subject_blk->sequence_start+nucl_shift, 
                                     translation_length, hsp->subject.frame,
                                     gen_code_string, &translation_buffer, 
                                     subject_length_ptr, NULL);
      /* Below, the start_shift will be used for the protein
         coordinates, so need to divide it by 3 */
      start_shift /= CODON_LENGTH;
   } else {
      Int4 oof_end;
      oof_end = subject_blk->length;
      
      start_shift = 
         MAX(0, hsp->subject.offset - MAX_FULL_TRANSLATION);
      translation_length =
         MIN(hsp->subject.end + MAX_FULL_TRANSLATION, oof_end) - start_shift;
      if (hsp->subject.frame > 0) {
         nucl_shift = start_shift;
      } else {
         nucl_shift = oof_end - start_shift - translation_length;
      }
      status = (Int2)
         Blast_GetPartialTranslation(subject_blk->sequence_start+nucl_shift, 
                                     translation_length, hsp->subject.frame, 
                                     gen_code_string, NULL, 
                                     subject_length_ptr, &translation_buffer);
   }
   hsp->subject.offset -= start_shift;
   hsp->subject.gapped_start -= start_shift;
   *translation_buffer_ptr = translation_buffer;
   *start_shift_ptr = start_shift;

   if (!is_ooframe) {
      subject = translation_buffer + 1;
   } else {
      subject = translation_buffer + CODON_LENGTH;
   }
   *subject_ptr = subject;

   return status;
}

void
Blast_HSPAdjustSubjectOffset(BlastHSP* hsp, Int4 start_shift)
{
    /* Adjust subject offsets if shifted (partial) sequence was used 
       for extension */
    if (start_shift > 0) {
        hsp->subject.offset += start_shift;
        hsp->subject.end += start_shift;
        hsp->subject.gapped_start += start_shift;
    }
    
    return;
}

int
ScoreCompareHSPs(const void* h1, const void* h2)
{
   BlastHSP* hsp1,* hsp2;   /* the HSPs to be compared */
   int result = 0;      /* the result of the comparison */
   
   hsp1 = *((BlastHSP**) h1);
   hsp2 = *((BlastHSP**) h2);

   /* Null HSPs are "greater" than any non-null ones, so they go to the end
      of a sorted list. */
   if (!hsp1 && !hsp2)
       return 0;
   else if (!hsp1)
       return 1;
   else if (!hsp2)
       return -1;

   if (0 == (result = BLAST_CMP(hsp2->score,          hsp1->score)) &&
       0 == (result = BLAST_CMP(hsp1->subject.offset, hsp2->subject.offset)) &&
       0 == (result = BLAST_CMP(hsp2->subject.end,    hsp1->subject.end)) &&
       0 == (result = BLAST_CMP(hsp1->query  .offset, hsp2->query  .offset))) {
       /* if all other test can't distinguish the HSPs, then the final
          test is the result */
       result = BLAST_CMP(hsp2->query.end, hsp1->query.end);
   }
   return result;
}

Boolean Blast_HSPListIsSortedByScore(const BlastHSPList* hsp_list) 
{
    Int4 index;
    
    if (!hsp_list || hsp_list->hspcnt <= 1)
        return TRUE;

    for (index = 0; index < hsp_list->hspcnt - 1; ++index) {
        if (ScoreCompareHSPs(&hsp_list->hsp_array[index], 
                             &hsp_list->hsp_array[index+1]) > 0) {
            return FALSE;
        }
    }
    return TRUE;
}

void Blast_HSPListSortByScore(BlastHSPList* hsp_list)
{
    if (!hsp_list || hsp_list->hspcnt <= 1)
        return;

    if (!Blast_HSPListIsSortedByScore(hsp_list)) {
        qsort(hsp_list->hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*),
              ScoreCompareHSPs);
    }
}

/** Precision to which e-values are compared. */
#define FUZZY_EVALUE_COMPARE_FACTOR 1e-6

/** Compares 2 real numbers up to a fixed precision.
 * @param evalue1 First evalue [in]
 * @param evalue2 Second evalue [in]
 */
static int 
s_FuzzyEvalueComp(double evalue1, double evalue2)
{
   if (evalue1 < (1-FUZZY_EVALUE_COMPARE_FACTOR)*evalue2)
      return -1;
   else if (evalue1 > (1+FUZZY_EVALUE_COMPARE_FACTOR)*evalue2)
      return 1;
   else 
      return 0;
}

/** Comparison callback function for sorting HSPs by e-value and score, before
 * saving BlastHSPList in a BlastHitList. E-value has priority over score, 
 * because lower scoring HSPs might have lower e-values, if they are linked
 * with sum statistics.
 * E-values are compared only up to a certain precision. 
 * @param v1 Pointer to first HSP [in]
 * @param v2 Pointer to second HSP [in]
 */
static int
s_EvalueCompareHSPs(const void* v1, const void* v2)
{
   BlastHSP* h1,* h2;
   int retval = 0;

   h1 = *((BlastHSP**) v1);
   h2 = *((BlastHSP**) v2);
   
   /* Check if one or both of these are null. Those HSPs should go to the end */
   if (!h1 && !h2)
      return 0;
   else if (!h1)
      return 1;
   else if (!h2)
      return -1;

   if ((retval = s_FuzzyEvalueComp(h1->evalue, h2->evalue)) != 0)
      return retval;

   return ScoreCompareHSPs(v1, v2);
}

void Blast_HSPListSortByEvalue(BlastHSPList* hsp_list)
{
    if (!hsp_list)
        return;

    if (hsp_list->hspcnt > 1) {
        Int4 index;
        BlastHSP** hsp_array = hsp_list->hsp_array;
        /* First check if HSP array is already sorted. */
        for (index = 0; index < hsp_list->hspcnt - 1; ++index) {
            if (s_EvalueCompareHSPs(&hsp_array[index], &hsp_array[index+1]) > 0) {
                break;
            }
        }
        /* Sort the HSP array if it is not sorted yet. */
        if (index < hsp_list->hspcnt - 1) {
            qsort(hsp_list->hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*),
                  s_EvalueCompareHSPs);
        }
    }
}


/** Comparison callback for sorting HSPs by diagonal. Do not compare
 * diagonals for HSPs from different contexts. The absolute value of the
 * return is the diagonal difference between the HSPs.
 */
static int
s_DiagCompareHSPs(const void* v1, const void* v2)
{
   BlastHSP* h1,* h2;

   h1 = *((BlastHSP**) v1);
   h2 = *((BlastHSP**) v2);
   
   if (h1->context < h2->context)
      return INT4_MIN;
   else if (h1->context > h2->context)
      return INT4_MAX;
   return (h1->query.offset - h1->subject.offset) - 
      (h2->query.offset - h2->subject.offset);
}

/** An auxiliary structure used for merging HSPs */
typedef struct BlastHSPSegment {
    Int4 q_start, /**< Start of segment in query. */
        q_end;    /**< End of segment in query. */
    Int4 s_start, /**< Start of segment in subject. */
        s_end;    /**< End of segment in subject. */
   struct BlastHSPSegment* next; /**< Next link in the chain. */
} BlastHSPSegment;

/** Maximal diagonal distance between HSP starting offsets, within which HSPs 
 * from search of different chunks of subject sequence are considered for 
 * merging.
 */
#define OVERLAP_DIAG_CLOSE 10

/** Merge the two HSPs if they intersect.
 * @param hsp1 The first HSP; also contains the result of merge. [in] [out]
 * @param hsp2 The second HSP [in]
 * @param start The starting offset of the subject coordinates where the 
 *               intersection is possible [in]
*/
static Boolean
s_BlastHSPsMerge(BlastHSP* hsp1, BlastHSP* hsp2, Int4 start)
{
   BlastHSPSegment* segments1,* segments2,* new_segment1,* new_segment2;
   GapEditScript* esp1,* esp2,* esp;
   Int4 end = start + DBSEQ_CHUNK_OVERLAP - 1;
   Int4 min_diag, max_diag, num1, num2, dist = 0, next_dist = 0;
   Int4 diag1_start, diag1_end, diag2_start, diag2_end;
   Int4 index;
   Uint1 intersection_found;
   EGapAlignOpType op_type = eGapAlignSub;

   if (!hsp1->gap_info || !hsp2->gap_info) {
      /* Assume that this is an ungapped alignment, hence simply compare 
         diagonals. Do not merge if they are on different diagonals */
      if (s_DiagCompareHSPs(&hsp1, &hsp2) == 0 &&
          hsp1->query.end >= hsp2->query.offset) {
         hsp1->query.end = hsp2->query.end;
         hsp1->subject.end = hsp2->subject.end;
         return TRUE;
      } else
         return FALSE;
   }
   /* Find whether these HSPs have an intersection point */
   segments1 = (BlastHSPSegment*) calloc(1, sizeof(BlastHSPSegment));
   
   esp1 = hsp1->gap_info;
   esp2 = hsp2->gap_info;
   
   segments1->q_start = hsp1->query.offset;
   segments1->s_start = hsp1->subject.offset;
   while (segments1->s_start < start) {
      if (esp1->op_type == eGapAlignIns)
         segments1->q_start += esp1->num;
      else if (segments1->s_start + esp1->num < start) {
         if (esp1->op_type == eGapAlignSub) { 
            segments1->s_start += esp1->num;
            segments1->q_start += esp1->num;
         } else if (esp1->op_type == eGapAlignDel)
            segments1->s_start += esp1->num;
      } else 
         break;
      esp1 = esp1->next;
   }
   /* Current esp is the first segment within the overlap region */
   segments1->s_end = segments1->s_start + esp1->num - 1;
   if (esp1->op_type == eGapAlignSub)
      segments1->q_end = segments1->q_start + esp1->num - 1;
   else
      segments1->q_end = segments1->q_start;
   
   new_segment1 = segments1;
   
   for (esp = esp1->next; esp; esp = esp->next) {
      new_segment1->next = (BlastHSPSegment*)
         calloc(1, sizeof(BlastHSPSegment));
      new_segment1->next->q_start = new_segment1->q_end + 1;
      new_segment1->next->s_start = new_segment1->s_end + 1;
      new_segment1 = new_segment1->next;
      if (esp->op_type == eGapAlignSub) {
         new_segment1->q_end += esp->num - 1;
         new_segment1->s_end += esp->num - 1;
      } else if (esp->op_type == eGapAlignIns) {
         new_segment1->q_end += esp->num - 1;
         new_segment1->s_end = new_segment1->s_start;
      } else {
         new_segment1->s_end += esp->num - 1;
         new_segment1->q_end = new_segment1->q_start;
      }
   }
   
   /* Now create the second segments list */
   
   segments2 = (BlastHSPSegment*) calloc(1, sizeof(BlastHSPSegment));
   segments2->q_start = hsp2->query.offset;
   segments2->s_start = hsp2->subject.offset;
   segments2->q_end = segments2->q_start + esp2->num - 1;
   segments2->s_end = segments2->s_start + esp2->num - 1;
   
   new_segment2 = segments2;
   
   for (esp = esp2->next; esp && new_segment2->s_end < end; 
        esp = esp->next) {
      new_segment2->next = (BlastHSPSegment*)
         calloc(1, sizeof(BlastHSPSegment));
      new_segment2->next->q_start = new_segment2->q_end + 1;
      new_segment2->next->s_start = new_segment2->s_end + 1;
      new_segment2 = new_segment2->next;
      if (esp->op_type == eGapAlignIns) {
         new_segment2->s_end = new_segment2->s_start;
         new_segment2->q_end = new_segment2->q_start + esp->num - 1;
      } else if (esp->op_type == eGapAlignDel) {
         new_segment2->s_end = new_segment2->s_start + esp->num - 1;
         new_segment2->q_end = new_segment2->q_start;
      } else if (esp->op_type == eGapAlignSub) {
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

         op_type = eGapAlignIns;
         dist = new_segment2->s_end - new_segment1->s_start + 1;
         next_dist = new_segment2->q_end - new_segment1->q_start - dist + 1;
         if (new_segment2->q_end - new_segment1->q_start < dist) {
            dist = new_segment2->q_end - new_segment1->q_start + 1;
            op_type = eGapAlignDel;
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
      } else {
         new_segment2 = new_segment2->next;
         num2++;
      }
   }

   /* Free the segments linked lists that are no longer needed. */
   for ( ; segments1; segments1 = new_segment1) {
       new_segment1 = segments1->next;
       sfree(segments1);
   }

   for ( ; segments2; segments2 = new_segment2) {
       new_segment2 = segments2->next;
       sfree(segments2);
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
   }

   return (Boolean) intersection_found;
}

/********************************************************************************
          Functions manipulating BlastHSPList's
********************************************************************************/

BlastHSPList* Blast_HSPListFree(BlastHSPList* hsp_list)
{
   Int4 index;

   if (!hsp_list)
      return hsp_list;

   for (index = 0; index < hsp_list->hspcnt; ++index) {
      Blast_HSPFree(hsp_list->hsp_array[index]);
   }
   sfree(hsp_list->hsp_array);

   sfree(hsp_list);
   return NULL;
}

BlastHSPList* Blast_HSPListNew(Int4 hsp_max)
{
   BlastHSPList* hsp_list = (BlastHSPList*) calloc(1, sizeof(BlastHSPList));
   const Int4 kDefaultAllocated=100;

   /* hsp_max is max number of HSP's allowed in an HSP list; 
      INT4_MAX taken as infinity. */
   hsp_list->hsp_max = INT4_MAX; 
   if (hsp_max > 0)
      hsp_list->hsp_max = hsp_max;

   hsp_list->allocated = MIN(kDefaultAllocated, hsp_list->hsp_max);

   hsp_list->hsp_array = (BlastHSP**) 
      calloc(hsp_list->allocated, sizeof(BlastHSP*));

   return hsp_list;
}

/** Duplicate HSPList's contents, copying only pointers to individual HSPs,
 * effectively transferring the ownership of the pointers to the return value.
 * The caller is still responsible for calling Blast_HSPListFree on the first
 * argument to this function.
 * @param hsp_list source Blast_HSPList to copy HSP pointers from [in|out]
 * @return duplicate of hsp_list or NULL if out of memory
 */
static BlastHSPList* 
s_BlastHSPListReleaseHSPs(BlastHSPList* hsp_list)
{
   BlastHSPList* new_hsp_list = (BlastHSPList*) 
      BlastMemDup(hsp_list, sizeof(BlastHSPList));
   if ( !new_hsp_list ) {
       return NULL;
   }

   new_hsp_list->hsp_array = (BlastHSP**) 
      BlastMemDup(hsp_list->hsp_array, hsp_list->allocated*sizeof(BlastHSP*));
   if (!new_hsp_list->hsp_array) {
      sfree(new_hsp_list);
      return NULL;
   }
   new_hsp_list->allocated = hsp_list->allocated;
   ASSERT(hsp_list->hspcnt <= hsp_list->allocated);

   /* hsp_list->hsp_array elements past hsp_list->hspcnt should be NULL, so it
    * doesn't hurt to zero hsp_list->allocated */
   memset(hsp_list->hsp_array, 0, hsp_list->allocated*sizeof(BlastHSP*));
   hsp_list->hspcnt = 0;

   return new_hsp_list;
}

/** This is a copy of a static function from ncbimisc.c.
 * Turns array into a heap with respect to a given comparison function.
 */
static void
s_Heapify (char* base0, char* base, char* lim, char* last, size_t width, int (*compar )(const void*, const void* ))
{
   size_t i;
   char   ch;
   char* left_son,* large_son;
   
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

/** Creates a heap of elements based on a comparison function.
 * @param b An array [in] [out]
 * @param nel Number of elements in b [in]
 * @param width The size of each element [in]
 * @param compar Callback to compare two heap elements [in]
 */
static void 
s_CreateHeap (void* b, size_t nel, size_t width, 
   int (*compar )(const void*, const void* ))   
{
   char*    base = (char*)b;
   size_t i;
   char*    base0 = (char*)base,* lim,* basef;
   
   if (nel < 2)
      return;
   
   lim = &base[((nel-2)/2)*width];
   basef = &base[(nel-1)*width];
   i = nel/2;
   for (base = &base0[(i - 1)*width]; i > 0; base = base - width) {
      s_Heapify(base0, base, lim, basef, width, compar);
      i--;
   }
}

/** Given a BlastHSPList* with a heapified HSP array, check whether the 
 * new HSP is better than the worst scoring.  If it is, then remove the 
 * worst scoring and insert, otherwise free the new one.
 * HSP and insert the new HSP in the heap. 
 * @param hsp_list Contains all HSPs for a given subject. [in] [out]
 * @param hsp A pointer to new HSP to be inserted into the HSP list [in] [out]
 */
static void 
s_BlastHSPListInsertHSPInHeap(BlastHSPList* hsp_list, 
                             BlastHSP** hsp)
{
    BlastHSP** hsp_array = hsp_list->hsp_array;
    if (ScoreCompareHSPs(hsp, &hsp_array[0]) > 0)
    {
         Blast_HSPFree(*hsp);
         return;
    }
    else
         Blast_HSPFree(hsp_array[0]);

    hsp_array[0] = *hsp;
    if (hsp_list->hspcnt >= 2) {
        s_Heapify((char*)hsp_array, (char*)hsp_array, 
                (char*)&hsp_array[hsp_list->hspcnt/2 - 1],
                 (char*)&hsp_array[hsp_list->hspcnt-1],
                 sizeof(BlastHSP*), ScoreCompareHSPs);
    }
}

/** Verifies that the best_evalue field on the BlastHSPList is correct.
 * @param hsp_list object to check [in]
 * @return TRUE if OK, FALSE otherwise.
 */
static Boolean
s_BlastCheckBestEvalue(const BlastHSPList* hsp_list)
{
    int index = 0;
    double best_evalue = (double) INT4_MAX;
    const double kDelta = 1.0e-200;

    /* There are no HSP's here. */
    if (hsp_list->hspcnt == 0)
       return TRUE;

    for (index=0; index<hsp_list->hspcnt; index++)
       best_evalue = MIN(hsp_list->hsp_array[index]->evalue, best_evalue);

    /* check that it's within 1%. */
    if (ABS(best_evalue-hsp_list->best_evalue)/(best_evalue+kDelta) > 0.01)
       return FALSE;

    return TRUE;
}

/** Gets the best (lowest) evalue from the BlastHSPList.
 * @param hsp_list object containing the evalues [in]
 * @return TRUE if OK, FALSE otherwise.
 */
static double
s_BlastGetBestEvalue(const BlastHSPList* hsp_list)
{
    int index = 0;
    double best_evalue = (double) INT4_MAX;

    for (index=0; index<hsp_list->hspcnt; index++)
       best_evalue = MIN(hsp_list->hsp_array[index]->evalue, best_evalue);

    return best_evalue;
}

/* Comments in blast_hits.h
 */
Int2 
Blast_HSPListSaveHSP(BlastHSPList* hsp_list, BlastHSP* new_hsp)
{
   BlastHSP** hsp_array;
   Int4 hspcnt;
   Int4 hsp_allocated; /* how many hsps are in the array. */
   Int2 status = 0;

   hspcnt = hsp_list->hspcnt;
   hsp_allocated = hsp_list->allocated;
   hsp_array = hsp_list->hsp_array;
   

   /* Check if list is already full, then reallocate. */
   if (hspcnt >= hsp_allocated && hsp_list->do_not_reallocate == FALSE)
   {
      Int4 new_allocated = MIN(2*hsp_list->allocated, hsp_list->hsp_max);
      if (new_allocated > hsp_list->allocated) {
         hsp_array = (BlastHSP**)
            realloc(hsp_array, new_allocated*sizeof(BlastHSP*));
         if (hsp_array == NULL)
         {
            hsp_list->do_not_reallocate = TRUE; 
            hsp_array = hsp_list->hsp_array;
            /** Return a non-zero status, because restriction on number
                of HSPs here is a result of memory allocation failure. */
            status = -1;
         } else {
            hsp_list->hsp_array = hsp_array;
            hsp_list->allocated = new_allocated;
            hsp_allocated = new_allocated;
         }
      } else {
         hsp_list->do_not_reallocate = TRUE; 
      }
      /* If it is the first time when the HSP array is filled to capacity,
         create a heap now. */
      if (hsp_list->do_not_reallocate) {
          s_CreateHeap(hsp_array, hspcnt, sizeof(BlastHSP*), ScoreCompareHSPs);
      }
   }

   /* If there is space in the allocated HSP array, simply save the new HSP. 
      Othewise, if the new HSP has lower score than the worst HSP in the heap,
      then delete it, else insert it in the heap. */
   if (hspcnt < hsp_allocated)
   {
      hsp_array[hsp_list->hspcnt] = new_hsp;
      (hsp_list->hspcnt)++;
      return status;
   } else {
       /* Insert the new HSP in heap. */
       s_BlastHSPListInsertHSPInHeap(hsp_list, &new_hsp);
   }
   
   return status;
}

Int2 Blast_HSPListGetEvalues(const BlastQueryInfo* query_info,
                             BlastHSPList* hsp_list, 
                             Boolean gapped_calculation, 
                             BlastScoreBlk* sbp, double gap_decay_rate,
                             double scaling_factor)
{
   BlastHSP* hsp;
   BlastHSP** hsp_array;
   Blast_KarlinBlk** kbp;
   Int4 hsp_cnt;
   Int4 index;
   double gap_decay_divisor = 1.;
   
   if (hsp_list == NULL || hsp_list->hspcnt == 0)
      return 0;
   
   kbp = (gapped_calculation ? sbp->kbp_gap : sbp->kbp);
   hsp_cnt = hsp_list->hspcnt;
   hsp_array = hsp_list->hsp_array;

   if (gap_decay_rate != 0.)
      gap_decay_divisor = BLAST_GapDecayDivisor(gap_decay_rate, 1);

   for (index=0; index<hsp_cnt; index++) {
      hsp = hsp_array[index];

      ASSERT(hsp != NULL);
      ASSERT(scaling_factor != 0.0);

      /* Divide Lambda by the scaling factor, so e-value is 
         calculated correctly from a scaled score. This is needed only
         for RPS BLAST, where scores are scaled, but Lambda is not. */
      kbp[hsp->context]->Lambda /= scaling_factor;

      /* Get effective search space from the query information block */
      hsp->evalue =
          BLAST_KarlinStoE_simple(hsp->score, kbp[hsp->context], 
                               query_info->contexts[hsp->context].eff_searchsp);
      hsp->evalue /= gap_decay_divisor;
      /* Put back the unscaled value of Lambda. */
      kbp[hsp->context]->Lambda *= scaling_factor;
   }
   
   /* Assign the best e-value field. Here the best e-value will always be
      attained for the first HSP in the list. Check that the incoming
      HSP list is properly sorted by score. */
   ASSERT(Blast_HSPListIsSortedByScore(hsp_list));
   hsp_list->best_evalue = s_BlastGetBestEvalue(hsp_list);

   return 0;
}

Int2 Blast_HSPListGetBitScores(BlastHSPList* hsp_list, 
                               Boolean gapped_calculation, BlastScoreBlk* sbp)
{
   BlastHSP* hsp;
   Blast_KarlinBlk** kbp;
   Int4 index;
   
   if (hsp_list == NULL)
      return 1;
   
   kbp = (gapped_calculation ? sbp->kbp_gap : sbp->kbp);
   
   for (index=0; index<hsp_list->hspcnt; index++) {
      hsp = hsp_list->hsp_array[index];
      ASSERT(hsp != NULL);
      hsp->bit_score = 
         (hsp->score*kbp[hsp->context]->Lambda - kbp[hsp->context]->logK) / 
         NCBIMATH_LN2;
   }
   
   return 0;
}

void Blast_HSPListPHIGetBitScores(BlastHSPList* hsp_list, BlastScoreBlk* sbp)
{
    Int4 index;
   
    double lambda, logC;
    
    ASSERT(sbp && sbp->kbp_gap && sbp->kbp_gap[0]);

    lambda = sbp->kbp_gap[0]->Lambda;
    logC = log(sbp->kbp_gap[0]->paramC);

    for (index=0; index<hsp_list->hspcnt; index++) {
        BlastHSP* hsp = hsp_list->hsp_array[index];
        ASSERT(hsp != NULL);
        hsp->bit_score = 
            (hsp->score*lambda - logC - log(1.0 + hsp->score*lambda))
            / NCBIMATH_LN2;
    }
}

void 
Blast_HSPListPHIGetEvalues(BlastHSPList* hsp_list, BlastScoreBlk* sbp, 
                           const BlastQueryInfo* query_info)
{
   Int4 index;
   BlastHSP* hsp;

   if (!hsp_list || hsp_list->hspcnt == 0)
       return;

   for (index = 0; index < hsp_list->hspcnt; ++index) {
      hsp = hsp_list->hsp_array[index];
      s_HSPPHIGetEvalue(hsp, sbp, query_info);
   }
   /* The best e-value is the one for the highest scoring HSP, which 
      must be the first in the list. Check that HSPs are sorted by score
      to make sure this assumption is correct. */
   ASSERT(Blast_HSPListIsSortedByScore(hsp_list));
   hsp_list->best_evalue = s_BlastGetBestEvalue(hsp_list);
}

Int2 Blast_HSPListReapByEvalue(BlastHSPList* hsp_list, 
        BlastHitSavingOptions* hit_options)
{
   BlastHSP* hsp;
   BlastHSP** hsp_array;
   Int4 hsp_cnt = 0;
   Int4 index;
   double cutoff;
   
   if (hsp_list == NULL)
      return 0;

   cutoff = hit_options->expect_value;

   hsp_array = hsp_list->hsp_array;
   for (index = 0; index < hsp_list->hspcnt; index++) {
      hsp = hsp_array[index];

      ASSERT(hsp != NULL);
      
      if (hsp->evalue > cutoff) {
         hsp_array[index] = Blast_HSPFree(hsp_array[index]);
      } else {
         if (index > hsp_cnt)
            hsp_array[hsp_cnt] = hsp_array[index];
         hsp_cnt++;
      }
   }
      
   hsp_list->hspcnt = hsp_cnt;

   return 0;
}

/* See description in blast_hits.h */

Int2
Blast_HSPListPurgeNullHSPs(BlastHSPList* hsp_list)

{
        Int4 index, index1; /* loop indices. */
        Int4 hspcnt; /* total number of HSP's to iterate over. */
        BlastHSP** hsp_array;  /* hsp_array to purge. */

	if (hsp_list == NULL || hsp_list->hspcnt == 0)
		return 0;

        hsp_array = hsp_list->hsp_array;
        hspcnt = hsp_list->hspcnt;

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

	hsp_list->hspcnt = index1;

        return 0;
}

/** Callback for sorting HSPs by starting offset in query. The sorting criteria
 * in order of priority: context, starting offset in query, starting offset in 
 * subject. Null HSPs are moved to the end of the array.
 * @param v1 pointer to first HSP [in]
 * @param v2 pointer to second HSP [in]
 * @return Result of comparison.
 */
static int
s_QueryOffsetCompareHSPs(const void* v1, const void* v2)
{
	BlastHSP* h1,* h2;
	BlastHSP** hp1,** hp2;

	hp1 = (BlastHSP**) v1;
	hp2 = (BlastHSP**) v2;
	h1 = *hp1;
	h2 = *hp2;

   if (!h1 && !h2)
      return 0;
   else if (!h1) 
      return 1;
   else if (!h2)
      return -1;

   /* If these are from different contexts, don't compare offsets */
   if (h1->context < h2->context) 
      return -1;
   if (h1->context > h2->context)
      return 1;

	if (h1->query.offset < h2->query.offset)
		return -1;
	if (h1->query.offset > h2->query.offset)
		return 1;

	if (h1->subject.offset < h2->subject.offset)
		return -1;
	if (h1->subject.offset > h2->subject.offset)
		return 1;

	return 0;
}

/** Callback for sorting HSPs by ending offset in query. The sorting criteria
 * in order of priority: context, ending offset in query, ending offset in 
 * subject. Null HSPs are moved to the end of the array.
 * @param v1 pointer to first HSP [in]
 * @param v2 pointer to second HSP [in]
 * @return Result of comparison.
 */
static int
s_QueryEndCompareHSPs(const void* v1, const void* v2)
{
	BlastHSP* h1,* h2;
	BlastHSP** hp1,** hp2;

	hp1 = (BlastHSP**) v1;
	hp2 = (BlastHSP**) v2;
	h1 = *hp1;
	h2 = *hp2;

   if (!h1 && !h2)
      return 0;
   else if (!h1) 
      return 1;
   else if (!h2)
      return -1;

   /* If these are from different contexts, don't compare offsets */
   if (h1->context < h2->context) 
      return -1;
   if (h1->context > h2->context)
      return 1;

	if (h1->query.end < h2->query.end)
		return -1;
	if (h1->query.end > h2->query.end)
		return 1;

	if (h1->subject.end < h2->subject.end)
		return -1;
	if (h1->subject.end > h2->subject.end)
		return 1;

	return 0;
}

Int4
Blast_HSPListPurgeHSPsWithCommonEndpoints(EBlastProgramType program, 
                                          BlastHSPList* hsp_list)

{
   BlastHSP** hsp_array;  /* hsp_array to purge. */
   Int4 index = 0;        /* loop index. */
   Int4 increment = 1;    
   Int2 retval = 0;
   Int4 hsp_count;
   
   /* If HSP list is empty, return immediately. */
   if (hsp_list == NULL || hsp_list->hspcnt == 0)
       return 0;

   /* Do nothing for PHI BLAST, because HSPs corresponding to different pattern
      occurrences may have common end points, but should all be kept. */
   if (Blast_ProgramIsPhiBlast(program))
       return hsp_list->hspcnt;

   hsp_array = hsp_list->hsp_array;
   hsp_count = hsp_list->hspcnt;

   qsort(hsp_array, hsp_count, sizeof(BlastHSP*), s_QueryOffsetCompareHSPs);
   while (index < hsp_count-increment) 
   {
      if (hsp_array[index+increment] == NULL) 
      {
         increment++;
         continue;
      }
      
      if (hsp_array[index] && hsp_array[index]->query.offset == hsp_array[index+increment]->query.offset &&
          hsp_array[index]->subject.offset == hsp_array[index+increment]->subject.offset &&
          hsp_array[index]->context == hsp_array[index+increment]->context)
      {
         if (hsp_array[index]->score > hsp_array[index+increment]->score) {
            hsp_array[index+increment] = 
                                Blast_HSPFree(hsp_array[index+increment]);
            increment++;
         } else {
            hsp_array[index] = Blast_HSPFree(hsp_array[index]);
            index++;
            increment = 1;
         }
      } else {
         index++;
         increment = 1;
      }
   }
   
   qsort(hsp_array, hsp_count, sizeof(BlastHSP*), s_QueryEndCompareHSPs);
   index=0;
   increment=1;
   while (index < hsp_count-increment)
   { 
      if (hsp_array[index+increment] == NULL)
      {
         increment++;
         continue;
      }
      
      if (hsp_array[index] &&
          hsp_array[index]->query.end == hsp_array[index+increment]->query.end &&
          hsp_array[index]->subject.end == hsp_array[index+increment]->subject.end &&
          hsp_array[index]->context == hsp_array[index+increment]->context)
      {
         if (hsp_array[index]->score > hsp_array[index+increment]->score) {
            hsp_array[index+increment] = 
                                Blast_HSPFree(hsp_array[index+increment]);
            increment++;
         } else	{
            hsp_array[index] = Blast_HSPFree(hsp_array[index]);
            index++;
            increment = 1;
         }
      } else {
         index++;
         increment = 1;
      }
   }

   retval = Blast_HSPListPurgeNullHSPs(hsp_list);
   if (retval < 0)
      return retval;

   return hsp_list->hspcnt;
}

/** Status values returned by an HSP inclusion test function. */
typedef enum EHSPInclusionStatus {
   eEqual = 0,      /**< Identical */
   eFirstInSecond,  /**< First included in rectangle formed by second */
   eSecondInFirst,  /**< Second included in rectangle formed by first */
   eDiagNear,       /**< Diagonals are near, but neither HSP is included in
                       the other. */
   eDiagDistant     /**< Diagonals are far apart, or different contexts */
} EHSPInclusionStatus;

/** Diagonal distance between HSPs, outside of which one HSP cannot be 
 * considered included in the other.
 */ 
#define MIN_DIAG_DIST 60

/** HSP inclusion criterion for megablast: one HSP must be included in a
 * diagonal strip of a certain width around the other, and also in a rectangle
 * formed by the other HSP's endpoints.
 */
static EHSPInclusionStatus 
s_BlastHSPInclusionTest(BlastHSP* hsp1, BlastHSP* hsp2)
{
   if (hsp1->context != hsp2->context || 
       !MB_HSP_CLOSE(hsp1->query.offset, hsp2->query.offset,
                     hsp1->subject.offset, hsp2->subject.offset, 
                     MIN_DIAG_DIST))
      return eDiagDistant;

   if (hsp1->query.offset == hsp2->query.offset && 
       hsp1->query.end == hsp2->query.end &&  
       hsp1->subject.offset == hsp2->subject.offset && 
       hsp1->subject.end == hsp2->subject.end && 
       hsp1->score == hsp2->score) {
      return eEqual;
   } else if (hsp1->query.offset >= hsp2->query.offset && 
       hsp1->query.end <= hsp2->query.end &&  
       hsp1->subject.offset >= hsp2->subject.offset && 
       hsp1->subject.end <= hsp2->subject.end && 
       hsp1->score <= hsp2->score) { 
      return eFirstInSecond;
   } else if (hsp1->query.offset <= hsp2->query.offset &&  
              hsp1->query.end >= hsp2->query.end &&  
              hsp1->subject.offset <= hsp2->subject.offset && 
              hsp1->subject.end >= hsp2->subject.end && 
              hsp1->score >= hsp2->score) { 
      return eSecondInFirst;
   }
   return eDiagNear;
}

/** How many HSPs to check for inclusion for each new HSP? */
#define MAX_NUM_CHECK_INCLUSION 20

/** Remove redundant HSPs in an HSP list based on a diagonal inclusion test: if 
 * an HSP is within a certain diagonal distance of another HSP, and its endpoints 
 * are contained in a rectangle formed by another HSP, then it is removed.
 * Performed only after a single-phase greedy gapped extension, when there is no 
 * extra traceback stage that could fix the inclusions.
 * @param hsp_list HSP list to check [in] [out]
 */
static Int2
s_BlastHSPListCheckDiagonalInclusion(BlastHSPList* hsp_list)
{
   Int4 index, new_hspcnt, index1, index2;
   BlastHSP** hsp_array = hsp_list->hsp_array;
   Boolean shift_needed = FALSE;
   EHSPInclusionStatus inclusion_status = eDiagNear;

   if (hsp_list->hspcnt <= 1)
      return 0;

   qsort(hsp_array, hsp_list->hspcnt, sizeof(BlastHSP*), 
         s_DiagCompareHSPs);

   for (index=1, new_hspcnt=0; index<hsp_list->hspcnt; index++) {
      if (!hsp_array[index])
         break;
      inclusion_status = eDiagNear;
      for (index1 = new_hspcnt; inclusion_status != eDiagDistant &&
           index1 >= 0 && new_hspcnt-index1 < MAX_NUM_CHECK_INCLUSION;
           index1--) {
         inclusion_status = 
            s_BlastHSPInclusionTest(hsp_array[index], hsp_array[index1]);
         if (inclusion_status == eFirstInSecond || 
             inclusion_status == eEqual) {
            /* Free the new HSP and break out of the inclusion test loop */
            hsp_array[index] = Blast_HSPFree(hsp_array[index]);
            break;
         } else if (inclusion_status == eSecondInFirst) {
            hsp_array[index1] = Blast_HSPFree(hsp_array[index1]);
            shift_needed = TRUE;
         }
      }
      
      /* If some lower indexed HSPs have been removed, shift the subsequent 
         HSPs */
      if (shift_needed) {
         /* Find the first non-NULL HSP, going backwards */
         while (index1 >= 0 && !hsp_array[index1])
            index1--;
         /* Go forward, and shift any non-NULL HSPs */
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
	  (hsp_list->hspcnt - new_hspcnt - 1)*sizeof(BlastHSP*));
   hsp_list->hspcnt = new_hspcnt + 1;

   /* Sort the HSP array by score again. */
   Blast_HSPListSortByScore(hsp_list);

   return 0;
}

Int2 
Blast_HSPListReevaluateWithAmbiguities(EBlastProgramType program, 
   BlastHSPList* hsp_list, BLAST_SequenceBlk* query_blk, 
   BLAST_SequenceBlk* subject_blk, 
   const BlastInitialWordParameters* word_params,
   const BlastHitSavingParameters* hit_params, const BlastQueryInfo* query_info, 
   BlastScoreBlk* sbp, const BlastScoringParameters* score_params, 
   const BlastSeqSrc* seq_src, const Uint1* gen_code_string)
{
   BlastHSP** hsp_array,* hsp;
   Uint1* query_start,* subject_start = NULL;
   Int4 index, context, hspcnt;
   Boolean gapped;
   Boolean purge, delete_hsp;
   Int2 status = 0;
   const Boolean kTranslateSubject = (program == eBlastTypeTblastn ||
                                      program == eBlastTypeTblastx); 
   Boolean partial_translation;
   Uint1* translation_buffer = NULL;
   Int4* frame_offsets = NULL;

   if (!hsp_list)
      return status;

   gapped = score_params->options->gapped_calculation;
   hspcnt = hsp_list->hspcnt;
   hsp_array = hsp_list->hsp_array;

   if (hsp_list->hspcnt == 0)
      /* All HSPs have been deleted */
      return status;

   /* Retrieve the unpacked subject sequence and save it in the 
      sequence_start element of the subject structure.
      NB: for the BLAST 2 Sequences case, the uncompressed sequence must 
      already be there */
   if (seq_src) {
      /* Wrap subject sequence block into a BlastSeqSrcGetSeqArg structure, which is 
         needed by the BlastSeqSrc API. */
      BlastSeqSrcGetSeqArg seq_arg;
      seq_arg.oid = subject_blk->oid;
      seq_arg.encoding =
         (kTranslateSubject ? eBlastEncodingNcbi4na : eBlastEncodingNucleotide);
      seq_arg.seq = subject_blk;
      /* Return the packed sequence to the database */
      BlastSeqSrcReleaseSequence(seq_src, (void*) &seq_arg);
      /* Get the unpacked sequence */
      BlastSeqSrcGetSequence(seq_src, (void*) &seq_arg);
   }

   if (kTranslateSubject) {
      if (!gen_code_string)
         return -1;
      /* Get the translation buffer, unless sequence is too long, in which case
         the partial translations will be performed for each HSP as needed. */
      Blast_SetUpSubjectTranslation(subject_blk, gen_code_string,
                                    &translation_buffer, &frame_offsets, 
                                    &partial_translation);
   } else {
      /* Store sequence in blastna encoding in sequence_start */
      subject_start = subject_blk->sequence_start + 1;
   }

   purge = FALSE;
   for (index = 0; index < hspcnt; ++index) {
      Int4 start_shift = 0; /* Subject shift in case of partial translation. */
      if (hsp_array[index] == NULL)
         continue;
      else
         hsp = hsp_array[index];

      context = hsp->context;

      query_start = query_blk->sequence +
          query_info->contexts[context].query_offset;

      if (gapped) {
         /* NB: This can only happen for blastn after greedy extension with 
            traceback. */
         delete_hsp = 
            Blast_HSPReevaluateWithAmbiguitiesGapped(hsp, query_start, 
               subject_start, hit_params, score_params, sbp);
      } else {
         if (kTranslateSubject) {
            if (partial_translation) {
               Int4 subject_length = 0; /* Dummy variable */
               Blast_HSPGetPartialSubjectTranslation(subject_blk, hsp, FALSE,
                  gen_code_string, &translation_buffer, &subject_start,
                  &subject_length, &start_shift);
            } else {
               Int4 subject_context = FrameToContext(hsp->subject.frame);
               subject_start = 
                  translation_buffer + frame_offsets[subject_context] + 1;
            }
         }

         delete_hsp = 
            Blast_HSPReevaluateWithAmbiguitiesUngapped(hsp, query_start, 
               subject_start, word_params, sbp, kTranslateSubject);
      }
   
      if (!delete_hsp) {
          delete_hsp = 
              Blast_HSPTestIdentityAndLength(program, hsp, query_start, 
                                             subject_start, 
                                             score_params->options, 
                                             hit_params->options);
      }
      /* If partial translation was done and subject sequence was shifted,
         shift back offsets in the HSP structure. */
      Blast_HSPAdjustSubjectOffset(hsp, start_shift);

      if (delete_hsp) { /* This HSP is now below the cutoff */
         hsp_array[index] = Blast_HSPFree(hsp_array[index]);
         purge = TRUE;
      }

   }

   sfree(translation_buffer);
   sfree(frame_offsets);
    
   if (purge) {
      Blast_HSPListPurgeNullHSPs(hsp_list);
   }

   /* If greedy extension with traceback was used, check for 
      HSP inclusion in a diagonal strip around another HSP. */
   if (gapped) 
      status = s_BlastHSPListCheckDiagonalInclusion(hsp_list);

   /* Sort the HSP array by score (scores may have changed!) */
   Blast_HSPListSortByScore(hsp_list);

   return status;
}

/** Combine two HSP lists, without altering the individual HSPs, and without 
 * reallocating the HSP array. 
 * @param hsp_list New HSP list [in]
 * @param combined_hsp_list Old HSP list, to which new HSPs are added [in] [out]
 * @param new_hspcnt How many HSPs to save in the combined list? The extra ones
 *                   are freed. The best scoring HSPs are saved. This argument
 *                   cannot be greater than the allocated size of the 
 *                   combined list's HSP array. [in]
 */
static void 
s_BlastHSPListsCombineByScore(BlastHSPList* hsp_list, 
                             BlastHSPList* combined_hsp_list,
                             Int4 new_hspcnt)
{
   Int4 index, index1, index2;
   BlastHSP** new_hsp_array;

   ASSERT(new_hspcnt <= combined_hsp_list->allocated);

   if (new_hspcnt >= hsp_list->hspcnt + combined_hsp_list->hspcnt) {
      /* All HSPs from both arrays are saved */
      for (index=combined_hsp_list->hspcnt, index1=0; 
           index1<hsp_list->hspcnt; index1++) {
         if (hsp_list->hsp_array[index1] != NULL)
            combined_hsp_list->hsp_array[index++] = hsp_list->hsp_array[index1];
      }
      combined_hsp_list->hspcnt = new_hspcnt;
      Blast_HSPListSortByScore(combined_hsp_list);
   } else {
      /* Not all HSPs are be saved; sort both arrays by score and save only
         the new_hspcnt best ones. 
         For the merged set of HSPs, allocate array the same size as in the 
         old HSP list. */
      new_hsp_array = (BlastHSP**) 
         malloc(combined_hsp_list->allocated*sizeof(BlastHSP*));

      Blast_HSPListSortByScore(combined_hsp_list);
      Blast_HSPListSortByScore(hsp_list);
      index1 = index2 = 0;
      for (index = 0; index < new_hspcnt; ++index) {
         if (index1 < combined_hsp_list->hspcnt &&
             (index2 >= hsp_list->hspcnt ||
              ScoreCompareHSPs(&combined_hsp_list->hsp_array[index1],
                               &hsp_list->hsp_array[index2]) <= 0)) {
            new_hsp_array[index] = combined_hsp_list->hsp_array[index1];
            ++index1;
         } else {
            new_hsp_array[index] = hsp_list->hsp_array[index2];
            ++index2;
         }
      }
      /* Free the extra HSPs that could not be saved */
      for ( ; index1 < combined_hsp_list->hspcnt; ++index1) {
         combined_hsp_list->hsp_array[index1] = 
            Blast_HSPFree(combined_hsp_list->hsp_array[index1]);
      }
      for ( ; index2 < hsp_list->hspcnt; ++index2) {
         hsp_list->hsp_array[index2] = 
            Blast_HSPFree(hsp_list->hsp_array[index2]);
      }
      /* Point combined_hsp_list's HSP array to the new one */
      sfree(combined_hsp_list->hsp_array);
      combined_hsp_list->hsp_array = new_hsp_array;
      combined_hsp_list->hspcnt = new_hspcnt;
   }
   
   /* Second HSP list now does not own any HSPs */
   hsp_list->hspcnt = 0;
}

Int2 Blast_HSPListAppend(BlastHSPList* hsp_list,
        BlastHSPList** combined_hsp_list_ptr, Int4 hsp_num_max)
{
   BlastHSPList* combined_hsp_list = *combined_hsp_list_ptr;
   BlastHSP** new_hsp_array;
   Int4 new_hspcnt;

   if (!hsp_list || hsp_list->hspcnt == 0)
      return 0;

   /* If no previous HSP list, just return a copy of the new one. */
   if (!combined_hsp_list) {
      if (!(combined_hsp_list = s_BlastHSPListReleaseHSPs(hsp_list)))
         return 1;
      *combined_hsp_list_ptr = combined_hsp_list;

      return 0;
   }
   /* Just append new list to the end of the old list, in case of 
      multiple frames of the subject sequence */
   new_hspcnt = MIN(combined_hsp_list->hspcnt + hsp_list->hspcnt, 
                    hsp_num_max);
   if (new_hspcnt > combined_hsp_list->allocated && 
       !combined_hsp_list->do_not_reallocate) {
      Int4 new_allocated = MIN(2*new_hspcnt, hsp_num_max);
      new_hsp_array = (BlastHSP**) 
         realloc(combined_hsp_list->hsp_array, 
                 new_allocated*sizeof(BlastHSP*));
      
      if (new_hsp_array) {
         combined_hsp_list->allocated = new_allocated;
         combined_hsp_list->hsp_array = new_hsp_array;
      } else {
         combined_hsp_list->do_not_reallocate = TRUE;
         new_hspcnt = combined_hsp_list->allocated;
      }
   }
   if (combined_hsp_list->allocated == hsp_num_max)
      combined_hsp_list->do_not_reallocate = TRUE;

   s_BlastHSPListsCombineByScore(hsp_list, combined_hsp_list, new_hspcnt);

   return 0;
}

Int2 Blast_HSPListsMerge(BlastHSPList* hsp_list, 
                   BlastHSPList** combined_hsp_list_ptr,
                   Int4 hsp_num_max, Int4 start, Boolean merge_hsps)
{
   BlastHSPList* combined_hsp_list = *combined_hsp_list_ptr;
   BlastHSP* hsp, *hsp_var;
   BlastHSP** hspp1,** hspp2;
   Int4 index, index1;
   Int4 hspcnt1, hspcnt2, new_hspcnt = 0;
   BlastHSP** new_hsp_array;
  
   if (!hsp_list || hsp_list->hspcnt == 0)
      return 0;

   /* If no previous HSP list, just return a copy of the new one. */
   if (!combined_hsp_list) {
      *combined_hsp_list_ptr = s_BlastHSPListReleaseHSPs(hsp_list);
      return 0;
   }

   /* Merge the two HSP lists for successive chunks of the subject sequence */
   hspcnt1 = hspcnt2 = 0;

   /* Put all HSPs that intersect the overlap region at the front of the
      respective HSP arrays. */
   for (index = 0; index < combined_hsp_list->hspcnt; index++) {
      hsp = combined_hsp_list->hsp_array[index];
      if (hsp->subject.end > start) {
         /* At least part of this HSP lies in the overlap strip. */
         hsp_var = combined_hsp_list->hsp_array[hspcnt1];
         combined_hsp_list->hsp_array[hspcnt1] = hsp;
         combined_hsp_list->hsp_array[index] = hsp_var;
         ++hspcnt1;
      }
   }
   for (index = 0; index < hsp_list->hspcnt; index++) {
      hsp = hsp_list->hsp_array[index];
      if (hsp->subject.offset < start + DBSEQ_CHUNK_OVERLAP) {
         /* At least part of this HSP lies in the overlap strip. */
         hsp_var = hsp_list->hsp_array[hspcnt2];
         hsp_list->hsp_array[hspcnt2] = hsp;
         hsp_list->hsp_array[index] = hsp_var;
         ++hspcnt2;
      }
   }

   hspp1 = combined_hsp_list->hsp_array;
   hspp2 = hsp_list->hsp_array;
   /* Sort HSPs from in the overlap region by diagonal */
   qsort(hspp1, hspcnt1, sizeof(BlastHSP*), s_DiagCompareHSPs);
   qsort(hspp2, hspcnt2, sizeof(BlastHSP*), s_DiagCompareHSPs);

   for (index=0; index<hspcnt1; index++) {
      for (index1 = 0; index1<hspcnt2; index1++) {
         /* Skip already deleted HSPs */
         if (!hspp2[index1])
            continue;
         if (hspp1[index]->context == hspp2[index1]->context && 
             ABS(s_DiagCompareHSPs(&hspp1[index], &hspp2[index1])) < 
             OVERLAP_DIAG_CLOSE) {
            if (merge_hsps) {
               if (s_BlastHSPsMerge(hspp1[index], hspp2[index1], start)) {
                  /* Free the second HSP. */
                  hspp2[index1] = Blast_HSPFree(hspp2[index1]);
               }
            } else { /* No gap information available */
               if (s_BlastHSPContained(hspp1[index], hspp2[index1])) {
                  sfree(hspp1[index]);
                  /* Point the first HSP to the new HSP; free the old HSP. */
                  hspp1[index] = hspp2[index1];
                  hspp2[index1] = NULL;
                  /* This HSP has been removed, so break out of the inner 
                     loop */
                  break;
               } else if (s_BlastHSPContained(hspp2[index1], hspp1[index])) {
                  /* Just free the second HSP */
                  hspp2[index1] = Blast_HSPFree(hspp2[index1]);
               }
            }
         } else {
            /* This and remaining HSPs are too far from the one being 
               checked */
            break;
         }
      }
   }

   /* Purge the nulled out HSPs from the new HSP list */
   Blast_HSPListPurgeNullHSPs(hsp_list);

   /* The new number of HSPs is now the sum of the remaining counts in the 
      two lists, but if there is a restriction on the number of HSPs to keep,
      it might have to be reduced. */
   new_hspcnt = 
      MIN(hsp_list->hspcnt + combined_hsp_list->hspcnt, hsp_num_max);
   
   if (new_hspcnt >= combined_hsp_list->allocated-1 && 
       combined_hsp_list->do_not_reallocate == FALSE) {
      Int4 new_allocated = MIN(2*new_hspcnt, hsp_num_max);
      if (new_allocated > combined_hsp_list->allocated) {
         new_hsp_array = (BlastHSP**) 
            realloc(combined_hsp_list->hsp_array, 
                    new_allocated*sizeof(BlastHSP*));
         if (new_hsp_array == NULL) {
            combined_hsp_list->do_not_reallocate = TRUE; 
         } else {
            combined_hsp_list->hsp_array = new_hsp_array;
            combined_hsp_list->allocated = new_allocated;
         }
      } else {
         combined_hsp_list->do_not_reallocate = TRUE;
      }
      new_hspcnt = MIN(new_hspcnt, combined_hsp_list->allocated);
   }

   s_BlastHSPListsCombineByScore(hsp_list, combined_hsp_list, new_hspcnt);

   return 0;
}

void Blast_HSPListAdjustOffsets(BlastHSPList* hsp_list, Int4 offset)
{
   Int4 index;
   BlastHSP* hsp;

   for (index=0; index<hsp_list->hspcnt; index++) {
      hsp = hsp_list->hsp_array[index];
      hsp->subject.offset += offset;
      hsp->subject.end += offset;
      hsp->subject.gapped_start += offset;
   }
}

/** Callback for sorting hsp lists by their best evalue/score;
 * Evalues are compared only up to a relative error of 
 * FUZZY_EVALUE_COMPARE_FACTOR. 
 * It is assumed that the HSP arrays in each hit list are already sorted by 
 * e-value/score.
*/
static int
s_EvalueCompareHSPLists(const void* v1, const void* v2)
{
   BlastHSPList* h1,* h2;
   int retval = 0;
   
   h1 = *(BlastHSPList**) v1;
   h2 = *(BlastHSPList**) v2;
   
   /* If any of the HSP lists is empty, it is considered "worse" than the 
      other, unless the other is also empty. */
   if (h1->hspcnt == 0 && h2->hspcnt == 0)
      return 0;
   else if (h1->hspcnt == 0)
      return 1;
   else if (h2->hspcnt == 0)
      return -1;

   if ((retval = s_FuzzyEvalueComp(h1->best_evalue, 
                                   h2->best_evalue)) != 0)
      return retval;

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

/** Callback for sorting hsp lists by their best e-value/score, in 
 * reverse order - from higher e-value to lower (lower score to higher).
*/
static int
s_EvalueCompareHSPListsRev(const void* v1, const void* v2)
{
   return s_EvalueCompareHSPLists(v2, v1);
}

/********************************************************************************
          Functions manipulating BlastHitList's
********************************************************************************/

/*
   description in blast_hits.h
 */
BlastHitList* Blast_HitListNew(Int4 hitlist_size)
{
   BlastHitList* new_hitlist = (BlastHitList*) 
      calloc(1, sizeof(BlastHitList));
   new_hitlist->hsplist_max = hitlist_size;
   new_hitlist->low_score = INT4_MAX;
   return new_hitlist;
}

/*
   description in blast_hits.h
*/
BlastHitList* Blast_HitListFree(BlastHitList* hitlist)
{
   if (!hitlist)
      return NULL;

   Blast_HitListHSPListsFree(hitlist);
   sfree(hitlist);
   return NULL;
}

/* description in blast_hits.h */
Int2 Blast_HitListHSPListsFree(BlastHitList* hitlist)
{
   Int4 index;
   
   if (!hitlist)
	return 0;

   for (index = 0; index < hitlist->hsplist_count; ++index)
      hitlist->hsplist_array[index] =
         Blast_HSPListFree(hitlist->hsplist_array[index]);

   sfree(hitlist->hsplist_array);

   hitlist->hsplist_count = 0;

   return 0;
}

/** Purge a BlastHitList of empty HSP lists. 
 * @param hit_list BLAST hit list structure. [in] [out]
 */
static void 
s_BlastHitListPurge(BlastHitList* hit_list)
{
   Int4 index, hsplist_count;
   
   if (!hit_list)
      return;
   
   hsplist_count = hit_list->hsplist_count;
   for (index = 0; index < hsplist_count && 
           hit_list->hsplist_array[index]->hspcnt > 0; ++index);
   
   hit_list->hsplist_count = index;
   /* Free all empty HSP lists */
   for ( ; index < hsplist_count; ++index) {
      Blast_HSPListFree(hit_list->hsplist_array[index]);
   }
}

/** Given a BlastHitList* with a heapified HSP list array, remove
 *  the worst scoring HSP list and insert the new HSP list in the heap
 * @param hit_list Contains all HSP lists for a given query [in] [out]
 * @param hsp_list A new HSP list to be inserted into the hit list [in]
 */
static void 
s_BlastHitListInsertHSPListInHeap(BlastHitList* hit_list, 
                                 BlastHSPList* hsp_list)
{
      Blast_HSPListFree(hit_list->hsplist_array[0]);
      hit_list->hsplist_array[0] = hsp_list;
      if (hit_list->hsplist_count >= 2) {
         s_Heapify((char*)hit_list->hsplist_array, (char*)hit_list->hsplist_array, 
                 (char*)&hit_list->hsplist_array[hit_list->hsplist_count/2 - 1],
                 (char*)&hit_list->hsplist_array[hit_list->hsplist_count-1],
                 sizeof(BlastHSPList*), s_EvalueCompareHSPLists);
      }
      hit_list->worst_evalue = 
         hit_list->hsplist_array[0]->best_evalue;
      hit_list->low_score = hit_list->hsplist_array[0]->hsp_array[0]->score;
}


Int2 Blast_HitListUpdate(BlastHitList* hit_list, 
                         BlastHSPList* hsp_list)
{
   hsp_list->best_evalue = s_BlastGetBestEvalue(hsp_list);

   ASSERT(s_BlastCheckBestEvalue(hsp_list) == TRUE);
 
   if (hit_list->hsplist_count < hit_list->hsplist_max) {
      /* If the array of HSP lists for this query is not yet allocated, 
         do it here */
      if (!hit_list->hsplist_array)
         hit_list->hsplist_array = (BlastHSPList**)
            malloc(hit_list->hsplist_max*sizeof(BlastHSPList*));
      /* Just add to the end; sort later */
      hit_list->hsplist_array[hit_list->hsplist_count++] = hsp_list;
      hit_list->worst_evalue = 
         MAX(hsp_list->best_evalue, hit_list->worst_evalue);
      hit_list->low_score = 
         MIN(hsp_list->hsp_array[0]->score, hit_list->low_score);
   } else {
      /* Compare e-values only with a certain precision */
      int evalue_order = s_FuzzyEvalueComp(hsp_list->best_evalue,
                                           hit_list->worst_evalue);
      if (evalue_order > 0 || 
          (evalue_order == 0 &&
           (hsp_list->hsp_array[0]->score < hit_list->low_score))) {
         /* This hit list is less significant than any of those already saved;
            discard it. Note that newer hits with score and e-value both equal
            to the current worst will be saved, at the expense of some older 
            hit.
         */
         Blast_HSPListFree(hsp_list);
      } else {
         if (!hit_list->heapified) {
            s_CreateHeap(hit_list->hsplist_array, hit_list->hsplist_count, 
                       sizeof(BlastHSPList*), s_EvalueCompareHSPLists);
            hit_list->heapified = TRUE;
         }
         s_BlastHitListInsertHSPListInHeap(hit_list, hsp_list);
      }
   }
   return 0;
}

/** Purges a BlastHitList of NULL HSP lists.
 * @param hit_list BLAST hit list to purge. [in] [out]
 */
static Int2 
s_BlastHitListPurgeNullHSPLists(BlastHitList* hit_list)
{
   Int4 index, index1; /* loop indices. */
   Int4 hsplist_count; /* total number of HSPList's to iterate over. */
   BlastHSPList** hsplist_array;  /* hsplist_array to purge. */

   if (hit_list == NULL || hit_list->hsplist_count == 0)
      return 0;

   hsplist_array = hit_list->hsplist_array;
   hsplist_count = hit_list->hsplist_count;

   index1 = 0;
   for (index=0; index<hsplist_count; index++) {
      if (hsplist_array[index]) {
         hsplist_array[index1] = hsplist_array[index];
         index1++;
      }
   }

   for (index=index1; index<hsplist_count; index++) {
      hsplist_array[index] = NULL;
   }

   hit_list->hsplist_count = index1;

   return 0;
}

/********************************************************************************
          Functions manipulating BlastHSPResults
********************************************************************************/

BlastHSPResults* Blast_HSPResultsNew(Int4 num_queries)
{
   BlastHSPResults* retval = NULL;

   retval = (BlastHSPResults*) malloc(sizeof(BlastHSPResults));
   if ( !retval ) {
       return NULL;
   }

   retval->num_queries = num_queries;
   retval->hitlist_array = (BlastHitList**) 
      calloc(num_queries, sizeof(BlastHitList*));

   if ( !retval->hitlist_array ) {
       return Blast_HSPResultsFree(retval);
   }
   
   return retval;
}

BlastHSPResults* Blast_HSPResultsFree(BlastHSPResults* results)
{
   Int4 index;

   if (!results)
       return NULL;

   for (index = 0; index < results->num_queries; ++index)
      Blast_HitListFree(results->hitlist_array[index]);
   sfree(results->hitlist_array);
   sfree(results);
   return NULL;
}

Int2 Blast_HSPResultsSortByEvalue(BlastHSPResults* results)
{
   Int4 index;
   BlastHitList* hit_list;

   if (!results)
       return 0;

   for (index = 0; index < results->num_queries; ++index) {
      hit_list = results->hitlist_array[index];
      if (hit_list && hit_list->hsplist_count > 1) {
         qsort(hit_list->hsplist_array, hit_list->hsplist_count, 
                  sizeof(BlastHSPList*), s_EvalueCompareHSPLists);
      }
      s_BlastHitListPurge(hit_list);
   }
   return 0;
}

Int2 Blast_HSPResultsReverseSort(BlastHSPResults* results)
{
   Int4 index;
   BlastHitList* hit_list;

   for (index = 0; index < results->num_queries; ++index) {
      hit_list = results->hitlist_array[index];
      if (hit_list && hit_list->hsplist_count > 1) {
         qsort(hit_list->hsplist_array, hit_list->hsplist_count, 
               sizeof(BlastHSPList*), s_EvalueCompareHSPListsRev);
      }
      s_BlastHitListPurge(hit_list);
   }
   return 0;
}

Int2 Blast_HSPResultsReverseOrder(BlastHSPResults* results)
{
   Int4 index;
   BlastHitList* hit_list;

   for (index = 0; index < results->num_queries; ++index) {
      hit_list = results->hitlist_array[index];
      if (hit_list && hit_list->hsplist_count > 1) {
	 BlastHSPList* hsplist;
	 Int4 index1;
	 /* Swap HSP lists: first with last; second with second from the end,
	    etc. */
	 for (index1 = 0; index1 < hit_list->hsplist_count/2; ++index1) {
	    hsplist = hit_list->hsplist_array[index1];
	    hit_list->hsplist_array[index1] = 
	       hit_list->hsplist_array[hit_list->hsplist_count-index1-1];
	    hit_list->hsplist_array[hit_list->hsplist_count-index1-1] =
	       hsplist;
	 }
      }
   }
   return 0;
}

/** Auxiliary structure for sorting HSPs */
typedef struct SHspWrap {
    BlastHSPList *hsplist;  /**< The HSPList to which this HSP belongs */
    BlastHSP *hsp;          /**< HSP described by this structure */
} SHspWrap;

/** callback used to sort a list of encapsulated HSP
 *  structures in order of increasing e-value, using
 *  the raw score as a tiebreaker
 */
static int s_SortHspWrapEvalue(const void *x, const void *y)
{
    SHspWrap *wrap1 = (SHspWrap *)x;
    SHspWrap *wrap2 = (SHspWrap *)y;
    if (wrap1->hsp->evalue < wrap2->hsp->evalue)
        return -1;
    if (wrap1->hsp->evalue > wrap2->hsp->evalue)
        return 1;

    return (wrap2->hsp->score - wrap1->hsp->score);
}


Int2 Blast_HSPResultsPerformCulling(BlastHSPResults *results,
                                    const BlastQueryInfo *query_info,
                                    Int4 culling_limit, Int4 query_length)
{
   Int4 i, j, k, m;
   Int4 hsp_count;
   SHspWrap *hsp_array;
   BlastIntervalTree *tree;

   /* set up the interval tree; subject offsets are not needed */

   tree = Blast_IntervalTreeInit(0, query_length + 1, 0, 0);

   for (i = 0; i < results->num_queries; i++) {
      BlastHitList *hitlist = results->hitlist_array[i];
      if (hitlist == NULL)
         continue;

      /* count the number of HSPs in this hitlist. If this is
         less than the culling limit, no HSPs will be pruned */

      for (j = hsp_count = 0; j < hitlist->hsplist_count; j++) {
         BlastHSPList *hsplist = hitlist->hsplist_array[j];
         hsp_count += hsplist->hspcnt;
      }
      if (hsp_count < culling_limit)
          continue;

      /* empty each HSP into a combined HSP array, then
         sort the array by e-value */

      hsp_array = (SHspWrap *)malloc(hsp_count * sizeof(SHspWrap));

      for (j = k = 0; j < hitlist->hsplist_count; j++) {
         BlastHSPList *hsplist = hitlist->hsplist_array[j];
         for (m = 0; m < hsplist->hspcnt; k++, m++) {
            BlastHSP *hsp = hsplist->hsp_array[m];
            hsp_array[k].hsplist = hsplist;
            hsp_array[k].hsp = hsp;
         }
         hsplist->hspcnt = 0;
      }

      qsort(hsp_array, hsp_count, sizeof(SHspWrap), s_SortHspWrapEvalue);

      /* Starting with the best HSP, use the interval tree to
         check that the query range of each HSP in the list has
         not already been enveloped by too many higher-scoring
         HSPs. If this is not the case, add the HSP back into results */

      Blast_IntervalTreeReset(tree);

      for (j = 0; j < hsp_count; j++) {
         BlastHSPList *hsplist = hsp_array[j].hsplist;
         BlastHSP *hsp = hsp_array[j].hsp;

         if (BlastIntervalTreeNumRedundant(tree, 
                         hsp, query_info) >= culling_limit) {
             Blast_HSPFree(hsp);
         }
         else {
             BlastIntervalTreeAddHSP(hsp, tree, query_info, eQueryOnly);
             Blast_HSPListSaveHSP(hsplist, hsp);

             /* the first HSP added back into an HSPList
                automatically has the best e-value */
             if (hsplist->hspcnt == 1)
                 hsplist->best_evalue = hsp->evalue;
         }
      }
      sfree(hsp_array);

      /* remove any HSPLists that are still empty after the 
         culling process. Sort any remaining lists by score */

      for (j = 0; j < hitlist->hsplist_count; j++) {
         BlastHSPList *hsplist = hitlist->hsplist_array[j];
         if (hsplist->hspcnt == 0) {
             hitlist->hsplist_array[j] = 
                   Blast_HSPListFree(hitlist->hsplist_array[j]);
         }
         else {
             Blast_HSPListSortByScore(hitlist->hsplist_array[j]);
         }
      }
      s_BlastHitListPurgeNullHSPLists(hitlist);
   }

   tree = Blast_IntervalTreeFree(tree);
   return 0;
}

static int
s_ScoreCompareHSPWithContext(const void* h1, const void* h2)
{
   BlastHSP* hsp1,* hsp2;   /* the HSPs to be compared */
   int result = 0;      /* the result of the comparison */
   
   hsp1 = *((BlastHSP**) h1);
   hsp2 = *((BlastHSP**) h2);

   /* Null HSPs are "greater" than any non-null ones, so they go to the end
      of a sorted list. */
   if (!hsp1 && !hsp2)
       return 0;
   else if (!hsp1)
       return 1;
   else if (!hsp2)
       return -1;

   if ((result = BLAST_CMP(hsp1->context, hsp2->context)) != 0)
       return result;
   return ScoreCompareHSPs(h1, h2);
}

Int2 
Blast_HSPResultsSaveRPSHSPList(EBlastProgramType program, 
                               BlastHSPResults* results, 
                               BlastHSPList* hsplist_in,
                               const SBlastHitsParameters* blasthit_params)
{
   Int4 index, next_index;
   BlastHitList* hit_list;

   if (!hsplist_in || hsplist_in->hspcnt == 0)
      return 0;

   /* Check that the query index is in the correct range. */
   ASSERT(hsplist_in->query_index < results->num_queries);

   /* If hit list for this query has not yet been allocated, do it here. */
   hit_list = results->hitlist_array[hsplist_in->query_index];
   if (!hit_list) {
       results->hitlist_array[hsplist_in->query_index] =
           hit_list = Blast_HitListNew(blasthit_params->prelim_hitlist_size);
   }

   /* Sort the input HSPList with context (i.e. oid) as the first priority, 
      and then score, etc. */
   qsort(hsplist_in->hsp_array, hsplist_in->hspcnt, sizeof(BlastHSP*), 
         s_ScoreCompareHSPWithContext);

   /* Sequentially extract HSPs corresponding to one subject into a new 
      HSPList, and save these new HSPLists in a normal way, as in all other
      BLAST programs. */
   next_index = 0;
   
   for (index = 0; index < hsplist_in->hspcnt; index = next_index) {
       BlastHSPList* hsp_list;
       Int4 oid = hsplist_in->hsp_array[index]->context;
       Int4 hspcnt;
       /* Find the first HSP that corresponds to a different subject. 
          At the same time, set all HSP contexts to 0, since this is what
          traceback code expects. */
       for (next_index = index; next_index < hsplist_in->hspcnt; 
            ++next_index) {
           if (hsplist_in->hsp_array[next_index]->context != oid)
               break;
           hsplist_in->hsp_array[next_index]->context = 0;
       }
       hspcnt = next_index - index;
       hsp_list = Blast_HSPListNew(hspcnt);
       /* Set the oid field for this HSPList. */
       hsp_list->oid = oid;
       hsp_list->hspcnt = hspcnt;
       hsp_list->query_index = hsplist_in->query_index;
       /* Copy all BlastHSP pointers corresponding to this subject. */
       memcpy(hsp_list->hsp_array, &hsplist_in->hsp_array[index], 
              hspcnt*sizeof(BlastHSP*));
       /* Check that HSPs are correctly sorted by score, as they should be. */
       ASSERT(Blast_HSPListIsSortedByScore(hsp_list));
       /* Insert this HSPList into this query's hit list. */
       Blast_HitListUpdate(hit_list, hsp_list);
   }

   /* All HSPs have been moved from the input HSPList to new HSPLists, so
      set the input HSPList's count to 0. */
   hsplist_in->hspcnt = 0;
   Blast_HSPListFree(hsplist_in);
   
   return 0;
}

Int2 Blast_HSPResultsSaveHSPList(EBlastProgramType program, BlastHSPResults* results, 
        BlastHSPList* hsp_list, const SBlastHitsParameters* blasthit_params)
{
   Int2 status = 0;
   BlastHSP* hsp;

   if (!hsp_list)
      return 0;

   if (!results || !blasthit_params)
      return -1;

   /* The HSP list should already be sorted by score coming into this function.
    * Still check that this assumption is true.
    */
   ASSERT(Blast_HSPListIsSortedByScore(hsp_list));
   ASSERT(s_BlastCheckBestEvalue(hsp_list) == TRUE);
   
   /* Rearrange HSPs into multiple hit lists if more than one query */
   if (results->num_queries > 1) {
      BlastHSPList** hsp_list_array;
      BlastHSPList* tmp_hsp_list;
      Int4 index;

      hsp_list_array = calloc(results->num_queries, sizeof(BlastHSPList*));

      for (index = 0; index < hsp_list->hspcnt; index++) {
         Int4 query_index;
         hsp = hsp_list->hsp_array[index];
         query_index = Blast_GetQueryIndexFromContext(hsp->context, program);
         tmp_hsp_list = hsp_list_array[query_index];

         if (!tmp_hsp_list) {
            hsp_list_array[query_index] = tmp_hsp_list = 
               Blast_HSPListNew(blasthit_params->options->hsp_num_max);
            tmp_hsp_list->oid = hsp_list->oid;
         }

         if (!tmp_hsp_list || tmp_hsp_list->do_not_reallocate) {
            tmp_hsp_list = NULL;
         } else if (tmp_hsp_list->hspcnt >= tmp_hsp_list->allocated) {
            BlastHSP** new_hsp_array;
            Int4 new_size = 
               MIN(2*tmp_hsp_list->allocated, tmp_hsp_list->hsp_max);
            if (new_size == tmp_hsp_list->hsp_max)
               tmp_hsp_list->do_not_reallocate = TRUE;
            
            new_hsp_array = realloc(tmp_hsp_list->hsp_array, 
                                    new_size*sizeof(BlastHSP*));
            if (!new_hsp_array) {
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
            hsp_list->hsp_array[index] = Blast_HSPFree(hsp);
         }
      }

      /* Insert the hit list(s) into the appropriate places in the results 
         structure */
      for (index = 0; index < results->num_queries; index++) {
         if (hsp_list_array[index]) {
            if (!results->hitlist_array[index]) {
               results->hitlist_array[index] = 
                  Blast_HitListNew(blasthit_params->prelim_hitlist_size);
            }
            Blast_HitListUpdate(results->hitlist_array[index], 
                                hsp_list_array[index]);
         }
      }
      sfree(hsp_list_array);
      /* All HSPs from the hsp_list structure are now moved to the results 
         structure, so set the HSP count back to 0 */
      hsp_list->hspcnt = 0;
      Blast_HSPListFree(hsp_list);
   } else if (hsp_list->hspcnt > 0) {
      /* Single query; save the HSP list directly into the results 
         structure */
      if (!results->hitlist_array[0]) {
         results->hitlist_array[0] = 
            Blast_HitListNew(blasthit_params->prelim_hitlist_size);
      }
      Blast_HitListUpdate(results->hitlist_array[0], hsp_list);
   } else {
       /* Empty HSPList - free it. */
       Blast_HSPListFree(hsp_list);
   }
       
   return status; 
}

Int2 Blast_HSPResultsInsertHSPList(BlastHSPResults* results, 
        BlastHSPList* hsp_list, Int4 hitlist_size)
{
   if (!hsp_list || hsp_list->hspcnt == 0)
      return 0;

   ASSERT(hsp_list->query_index < results->num_queries);

   if (!results->hitlist_array[hsp_list->query_index]) {
      results->hitlist_array[hsp_list->query_index] = 
         Blast_HitListNew(hitlist_size);
   }
   Blast_HitListUpdate(results->hitlist_array[hsp_list->query_index], 
                       hsp_list);
   return 0;
}
