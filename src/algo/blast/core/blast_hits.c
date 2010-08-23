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

/** @file blast_hits.c
 * BLAST functions for saving hits after the (preliminary) gapped alignment
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/ncbi_math.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_hspstream.h>
#include "blast_hits_priv.h"
#include "blast_itree.h"

NCBI_XBLAST_EXPORT
Int2 SBlastHitsParametersNew(const BlastHitSavingOptions* hit_options,
                             const BlastExtensionOptions* ext_options,
                             const BlastScoringOptions* scoring_options,
                             SBlastHitsParameters* *retval)
{
       Int4 prelim_hitlist_size;
       ASSERT(retval);
       *retval = NULL;

       if (hit_options == NULL ||
           ext_options == NULL || 
           scoring_options == NULL)
           return 1;

       *retval = (SBlastHitsParameters*) malloc(sizeof(SBlastHitsParameters));
       if (*retval == NULL)
           return 2;

       prelim_hitlist_size = hit_options->hitlist_size;
       if (ext_options->compositionBasedStats)
            prelim_hitlist_size = prelim_hitlist_size * 2 + 50;  
       else if (scoring_options->gapped_calculation)
            prelim_hitlist_size = MIN(2 * prelim_hitlist_size, 
                                      prelim_hitlist_size + 50);

       (*retval)->prelim_hitlist_size = MAX(prelim_hitlist_size, 10);

       (*retval)->hsp_num_max = BlastHspNumMax(scoring_options->gapped_calculation, hit_options);

       return 0;
}

SBlastHitsParameters* 
SBlastHitsParametersDup(const SBlastHitsParameters* hit_params)
{
    SBlastHitsParameters* retval = (SBlastHitsParameters*)
        malloc(sizeof(SBlastHitsParameters));

    if ( !retval ) {
        return NULL;
    }

    memcpy((void*)retval, (void*) hit_params, sizeof(SBlastHitsParameters));
    return retval;
}

NCBI_XBLAST_EXPORT
SBlastHitsParameters* SBlastHitsParametersFree(SBlastHitsParameters* param)
{
       if (param)
       {
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
	return BLASTERR_MEMORY;


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

Int4 BlastHspNumMax(Boolean gapped_calculation, const BlastHitSavingOptions* options)
{
   Int4 retval=0;

   if (options->hsp_num_max <= 0)
   {
      if (!gapped_calculation || options->program_number == eBlastTypeTblastx)
         retval = kUngappedHSPNumMax;
      else
         retval = INT4_MAX;
   }
   else
       retval = options->hsp_num_max;

   return retval;
}

/** Copies all contents of a BlastHSP structure. Used in PHI BLAST for splitting
 * results corresponding to different pattern occurrences in query.
 * @param hsp Original HSP [in]
 * @return New HSP, copied from the original.
 */
static BlastHSP* 
s_BlastHSPCopy(const BlastHSP* hsp)
{
    BlastHSP* new_hsp = NULL;
    /* Do not pass the edit script, because we don't want to tranfer 
       ownership. */
    Blast_HSPInit(hsp->query.offset, hsp->query.end, hsp->subject.offset, 
                  hsp->subject.end, hsp->query.gapped_start, 
                  hsp->subject.gapped_start, hsp->context, 
                  hsp->query.frame, hsp->subject.frame, hsp->score, 
                  NULL, &new_hsp);
    new_hsp->evalue = hsp->evalue;
    new_hsp->num = hsp->num;
    new_hsp->num_ident = hsp->num_ident;
    new_hsp->bit_score = hsp->bit_score;
    new_hsp->comp_adjustment_method = hsp->comp_adjustment_method;
    if (hsp->gap_info) {
        new_hsp->gap_info = GapEditScriptDup(hsp->gap_info);
    }

    if (hsp->pat_info) {
        /* Copy this HSP's pattern data. */
        new_hsp->pat_info = 
            (SPHIHspInfo*) BlastMemDup(hsp->pat_info, sizeof(SPHIHspInfo));
    }
    return new_hsp;
}

/** Count the number of occurrences of pattern in sequence, which
 * do not overlap by more than half the pattern match length. 
 * @param query_info Query information structure, containing pattern info. [in]
 */
Int4
PhiBlastGetEffectiveNumberOfPatterns(const BlastQueryInfo *query_info)
{
    Int4 index; /*loop index*/
    Int4 lastEffectiveOccurrence; /*last nonoverlapping occurrence*/
    Int4 count; /* Count of effective (nonoverlapping) occurrences */
    Int4 min_pattern_length;
    SPHIQueryInfo* pat_info;

    ASSERT(query_info && query_info->pattern_info && query_info->contexts);

    pat_info = query_info->pattern_info;

    if (pat_info->num_patterns <= 1)
        return pat_info->num_patterns;

    /* Minimal length of a pattern is saved in the length adjustment field. */
    min_pattern_length = query_info->contexts[0].length_adjustment;

    count = 1;
    lastEffectiveOccurrence = pat_info->occurrences[0].offset;
    for(index = 1; index < pat_info->num_patterns; ++index) {
        if (((pat_info->occurrences[index].offset - lastEffectiveOccurrence) * 2)
            > min_pattern_length) {
            lastEffectiveOccurrence = pat_info->occurrences[index].offset;
            ++count;
        }
    }

    return count;
}


/** Calculate e-value for an HSP found by PHI BLAST.
 * @param hsp An HSP found by PHI BLAST [in]
 * @param sbp Scoring block with statistical parameters [in]
 * @param query_info Structure containing information about pattern counts [in]
 * @param pattern_blk Structure containing counts of PHI pattern hits [in]
 */
static void 
s_HSPPHIGetEvalue(BlastHSP* hsp, BlastScoreBlk* sbp, 
                  const BlastQueryInfo* query_info,
                  const SPHIPatternSearchBlk* pattern_blk)
{
   double paramC;
   double Lambda;
  
   ASSERT(query_info && hsp && sbp && pattern_blk);

   paramC = sbp->kbp[0]->paramC;
   Lambda = sbp->kbp[0]->Lambda;

   /* We have the actual number of occurrences of pattern in db. */
   hsp->evalue = paramC*(1+Lambda*hsp->score)*
                PhiBlastGetEffectiveNumberOfPatterns(query_info)*
                pattern_blk->num_patterns_db*
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
 * @param best_start_esp_index index of the edit script array where the new alignment
 *                       starts. [in]
 * @param best_end_esp_index index in the edit script array where the new alignment 
 *                     ends. [in]
 * @param best_end_esp_num Number of edit operations in the last edit script,
 *                         that are included in the alignment. [in]
 * @return TRUE if HSP is scheduled to be deleted.
 */
static Boolean
s_UpdateReevaluatedHSP(BlastHSP* hsp, Boolean gapped,
                       Int4 cutoff_score,
                       Int4 score, const Uint1* query_start, const Uint1* subject_start, 
                       const Uint1* best_q_start, const Uint1* best_q_end, 
                       const Uint1* best_s_start, const Uint1* best_s_end, 
                       int best_start_esp_index,
                       int best_end_esp_index,
                       int best_end_esp_num)
{
    Boolean delete_hsp = TRUE;

    hsp->score = score;

    if (hsp->score >= cutoff_score) {
        /* Update all HSP offsets. */
        hsp->query.offset = best_q_start - query_start;
        hsp->query.end = hsp->query.offset + best_q_end - best_q_start;
        hsp->subject.offset = best_s_start - subject_start;
        hsp->subject.end = hsp->subject.offset + best_s_end - best_s_start;

        if (gapped) {
            int last_num=hsp->gap_info->size - 1;
            if (best_end_esp_index != last_num|| best_start_esp_index > 0)
            {
                GapEditScript* esp_temp = GapEditScriptNew(best_end_esp_index-best_start_esp_index+1);
                GapEditScriptPartialCopy(esp_temp, 0, hsp->gap_info, best_start_esp_index, best_end_esp_index);
                hsp->gap_info = GapEditScriptDelete(hsp->gap_info);
                hsp->gap_info = esp_temp;
            }
            last_num = hsp->gap_info->size - 1;
            hsp->gap_info->num[last_num] = best_end_esp_num;
            ASSERT(best_end_esp_num >= 0);
        }
        delete_hsp = FALSE;
    }

    return delete_hsp;
}
                               
Boolean Blast_HSPReevaluateWithAmbiguitiesGapped(BlastHSP* hsp, 
           const Uint1* query_start, const Uint1* subject_start, 
           const BlastHitSavingParameters* hit_params, 
           const BlastScoringParameters* score_params, 
           BlastScoreBlk* sbp)
{
   Int4 sum, score, gap_open, gap_extend;
   Int4 index; /* loop index */

   int best_start_esp_index = 0;
   int best_end_esp_index = 0;
   int current_start_esp_index = 0;
   int best_end_esp_num = 0;
   GapEditScript* esp;  /* Used to hold GapEditScript of hsp->gap_info */

   const Uint1* best_q_start; /* Start of the best scoring part in query. */
   const Uint1* best_s_start; /* Start of the best scoring part in subject. */
   const Uint1* best_q_end;   /* End of the best scoring part in query. */
   const Uint1* best_s_end;   /* End of the best scoring part in subject. */
   

   const Uint1* current_q_start; /* Start of the current part of the alignment in 
                           query. */
   const Uint1* current_s_start; /* Start of the current part of the alignment in 
                           subject. */

   const Uint1* query,* subject;
   Int4** matrix;
   Int2 factor = 1;
   const Uint1 kResidueMask = 0x0f;
   Int4 cutoff_score = hit_params->cutoffs[hsp->context].cutoff_score;

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
   /* There are no previous edit scripts at the beginning. */
   
   best_end_esp_num = -1;
   esp = hsp->gap_info;
   for (index=0; index<esp->size; index++)
   {
       int op_index = 0;  /* Index of an operation within a single edit script. */
       for (op_index=0; op_index<esp->num[index]; )
       {
          /* Process substitutions one operation at a time, full gaps in one step. */
          if (esp->op_type[index] == eGapAlignSub) {
              sum += factor*matrix[*query & kResidueMask][*subject];
              query++;
              subject++;
              op_index++;
          } else if (esp->op_type[index] == eGapAlignDel) {
              sum -= gap_open + gap_extend * esp->num[index];
              subject += esp->num[index];
              op_index += esp->num[index];
          } else if (esp->op_type[index] == eGapAlignIns) {
              sum -= gap_open + gap_extend * esp->num[index];
              query += esp->num[index];
              op_index += esp->num[index];
          }
      
          if (sum < 0) {
           /* Point current edit script chain start to the new place.
              If we are in the middle of an edit script, reduce its length and
              point operation index to the beginning of a modified edit script;
              if we are at the end, move to the next edit script. */
              if (op_index < esp->num[index]) {
                  esp->num[index] -= op_index;
                  current_start_esp_index = index;
                  op_index = 0;
              } else {
                  current_start_esp_index = index + 1;
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
              if (score < cutoff_score) {
                  /* Start from new offset; discard all previous information. */
                  best_q_start = query;
                  best_s_start = subject;
                  score = 0; 
               
                  /* Set best start and end edit script pointers to new start. */
                  best_start_esp_index = current_start_esp_index;
                  best_end_esp_index = current_start_esp_index;
              }
             /* break; */ /* start on next GapEditScript. */
          } else if (sum > score) {
              /* Remember this point as the best scoring end point, and the current
              start of the alignment as the start of the best alignment. */
              score = sum;
           
              best_q_start = current_q_start;
              best_s_start = current_s_start;
              best_q_end = query;
              best_s_end = subject;
           
              best_start_esp_index = current_start_esp_index;
              best_end_esp_index = index;
              best_end_esp_num = op_index;
          }
       }
   } /* loop on edit scripts */
   
   score /= factor;
   
   /* Update HSP data. */
   return
       s_UpdateReevaluatedHSP(hsp, TRUE, cutoff_score,
                              score, query_start, subject_start, best_q_start, 
                              best_q_end, best_s_start, best_s_end, 
                              best_start_esp_index, best_end_esp_index,
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
                               const Uint1* query_start, const Uint1* subject_start, 
                               const Uint1* best_q_start, const Uint1* best_q_end, 
                               const Uint1* best_s_start, const Uint1* best_s_end)
{
    return
        s_UpdateReevaluatedHSP(hsp, FALSE, cutoff_score, score, query_start, 
                               subject_start, best_q_start, best_q_end, 
                               best_s_start, best_s_end, 0, 0, 0);
}

Boolean 
Blast_HSPReevaluateWithAmbiguitiesUngapped(BlastHSP* hsp, const Uint1* query_start, 
   const Uint1* subject_start, const BlastInitialWordParameters* word_params,
   BlastScoreBlk* sbp, Boolean translated)
{
   Int4 sum, score;
   Int4** matrix;
   const Uint1* query,* subject;
   const Uint1* best_q_start,* best_s_start,* best_q_end,* best_s_end;
   const Uint1* current_q_start, * current_s_start;
   Int4 index;
   const Uint1 kResidueMask = (translated ? 0xff : 0x0f);
   Int4 hsp_length = hsp->query.end - hsp->query.offset;
   Int4 cutoff_score = word_params->cutoffs[hsp->context].cutoff_score;

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
         if (score < cutoff_score) {
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
       s_UpdateReevaluatedHSPUngapped(hsp, cutoff_score, score,
                                      query_start, subject_start, best_q_start,
                                      best_q_end, best_s_start, best_s_end);
} 

/** Calculate number of identities in a regular HSP.
 * @param query The query sequence [in]
 * @param subject The uncompressed subject sequence [in]
 * @param hsp All information about the HSP [in]
 * @param num_ident_ptr Number of identities [out]
 * @param align_length_ptr The alignment length, including gaps [out]
 * @return 0 on success, -1 on invalid parameters or error
 */
static Int2
s_Blast_HSPGetNumIdentities(const Uint1* query, const Uint1* subject, 
                            const BlastHSP* hsp, Int4* num_ident_ptr, 
                            Int4* align_length_ptr)
{
   Int4 i, num_ident, align_length, q_off, s_off;
   Uint1* q,* s;
   Int4 q_length = hsp->query.end - hsp->query.offset;
   Int4 s_length = hsp->subject.end - hsp->subject.offset;

   q_off = hsp->query.offset;
   s_off = hsp->subject.offset;

   if ( !subject || !query || !hsp )
      return -1;

   q = (Uint1*) &query[q_off];
   s = (Uint1*) &subject[s_off];

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
      Int4 index;
      GapEditScript* esp = hsp->gap_info;
      for (index=0; index<esp->size; index++)
      {
         align_length += esp->num[index];
         switch (esp->op_type[index]) {
         case eGapAlignSub:
            for (i=0; i<esp->num[index]; i++) {
               if (*q++ == *s++)
                  num_ident++;
            }
            break;
         case eGapAlignDel:
            s += esp->num[index];
            break;
         case eGapAlignIns:
            q += esp->num[index];
            break;
         default: 
            s += esp->num[index];
            q += esp->num[index];
            break;
         }
      }
   }

   if (align_length_ptr) {
       *align_length_ptr = align_length;
   }
   *num_ident_ptr = num_ident;
   return 0;
}

/** Calculate number of identities in an HSP for an out-of-frame alignment.
 * @param query The query sequence [in]
 * @param subject The uncompressed subject sequence [in]
 * @param hsp All information about the HSP [in]
 * @param program BLAST program (blastx or tblastn) [in]
 * @param num_ident_ptr Number of identities [out]
 * @param align_length_ptr The alignment length, including gaps [out]
 * @return 0 on success, -1 on invalid parameters or error
 */
static Int2
s_Blast_HSPGetOOFNumIdentities(const Uint1* query, const Uint1* subject, 
   const BlastHSP* hsp, EBlastProgramType program, 
   Int4* num_ident_ptr, Int4* align_length_ptr)
{
   Int4 num_ident, align_length;
   Int4 index;
   Uint1* q,* s;
   GapEditScript* esp;

   if (!hsp->gap_info || !subject || !query)
      return -1;


   if (program == eBlastTypeTblastn ||
       program == eBlastTypeRpsTblastn) {
       q = (Uint1*) &query[hsp->query.offset];
       s = (Uint1*) &subject[hsp->subject.offset];
   } else {
       s = (Uint1*) &query[hsp->query.offset];
       q = (Uint1*) &subject[hsp->subject.offset];
   }
   num_ident = 0;
   align_length = 0;

   esp = hsp->gap_info;
   for (index=0; index<esp->size; index++)
   {
      int i;
      switch (esp->op_type[index]) {
      case eGapAlignSub: /* Substitution */
         align_length += esp->num[index];
         for (i=0; i<esp->num[index]; i++) {
            if (*q == *s)
               num_ident++;
            ++q;
            s += CODON_LENGTH;
         }
         break;
      case eGapAlignIns: /* Insertion */
         align_length += esp->num[index];
         s += esp->num[index] * CODON_LENGTH;
         break;
      case eGapAlignDel: /* Deletion */
         align_length += esp->num[index];
         q += esp->num[index];
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
         s += esp->num[index] * CODON_LENGTH;
         q += esp->num[index];
         break;
      }
   }

   if (align_length_ptr) {
       *align_length_ptr = align_length;
   }
   *num_ident_ptr = num_ident;

   return 0;
}

Int2
Blast_HSPGetNumIdentities(const Uint1* query, 
                          const Uint1* subject, 
                          BlastHSP* hsp, 
                          const BlastScoringOptions* score_options,
                          Int4* align_length_ptr)
{
    Int2 retval = 0;

    /* Calculate alignment length and number of identical letters. 
       Do not get the number of identities if the query is not available */
    if (score_options->is_ooframe) {
        retval = s_Blast_HSPGetOOFNumIdentities(query, subject, hsp, 
                                               score_options->program_number, 
                                               &hsp->num_ident, 
                                               align_length_ptr);
    } else {
        retval = s_Blast_HSPGetNumIdentities(query, subject, hsp, 
                                            &hsp->num_ident, 
                                            align_length_ptr);
    }

    return retval;
}

Boolean
Blast_HSPTestIdentityAndLength(EBlastProgramType program_number, 
                               BlastHSP* hsp, const Uint1* query, const Uint1* subject, 
                               const BlastScoringOptions* score_options,
                               const BlastHitSavingOptions* hit_options)
{
   Int4 align_length = 0;
   Boolean delete_hsp = FALSE;
   Int2 status = 0;

   ASSERT(hsp && query && subject && score_options && hit_options);

   status = Blast_HSPGetNumIdentities(query, subject, hsp, score_options,
                                      &align_length);
   ASSERT(status == 0);

   /* Check whether this HSP passes the percent identity and minimal hit 
      length criteria, and delete it if it does not. */
   if ((hsp->num_ident * 100.0 < 
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
      Int4 index;
      for (index=0; index<esp->size; index++) {
         if (esp->op_type[index] == eGapAlignDel) {
            length += esp->num[index];
            gaps += esp->num[index];
            ++gap_opens;
         } else if (esp->op_type[index] == eGapAlignIns) {
            ++gap_opens;
            gaps += esp->num[index];
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

   if (!Blast_QueryIsTranslated(program) &&
       !Blast_SubjectIsTranslated(program)) {
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


const Uint1* 
Blast_HSPGetTargetTranslation(SBlastTargetTranslation* target_t,
                              const BlastHSP* hsp, Int4* translated_length)
{
    Int4 context = -1;
    Int4 start, stop;

    ASSERT(target_t != NULL);

    if (hsp == NULL)
       return NULL;

    context = BLAST_FrameToContext(hsp->subject.frame, target_t->program_number);
    start = target_t->range[2*context];
    stop = target_t->range[2*context+1];

    /* skip translation if full translation has already been done */
    if (target_t->partial && (start || 
        (stop < target_t->subject_blk->length / CODON_LENGTH -3))) 
    {  
    	 const int kMaxTranslation = 2100; /* Needs to be divisible by three (?) */
         Int4 nucl_length = 0;
         Int4 translation_length = 0;
         Int4 nucl_start = 0;
         Int4 nucl_end = 0;
    	 Int4 nucl_shift = 0;
    	 Int4 start_shift = 0;

         /* HSP coordinates are in terms of protein sequences. */
         if (hsp->subject.offset < 0) {
             nucl_start = 0;
             nucl_end = target_t->subject_blk->length;
         } else {
             nucl_start = MAX(0, 3*hsp->subject.offset - kMaxTranslation);
             nucl_end = MIN(target_t->subject_blk->length, 3*hsp->subject.end + kMaxTranslation);
             /* extend to the end of the sequence if close */
             if (target_t->subject_blk->length - nucl_end <= 21) {
                 nucl_end = target_t->subject_blk->length;
             }
         }

         nucl_length = nucl_end - nucl_start;

         translation_length = 1+nucl_length/CODON_LENGTH;
         start_shift = nucl_start/CODON_LENGTH;

         if (hsp->subject.frame < 0)
             nucl_shift = target_t->subject_blk->length - nucl_start - nucl_length;
         else
             nucl_shift = nucl_start;

         if (start_shift < start || start_shift+translation_length > stop) {
               /* needs re-translation */
               Int4 length = 0; /* actual translation length */
               Uint1* nucl_seq = target_t->subject_blk->sequence + nucl_shift;
               Uint1* nucl_seq_rev = NULL;

               target_t->range[2*context] = start_shift;

               if (translation_length > stop-start) {
                   /* needs re-allocation */
                   sfree(target_t->translations[context]);
                   target_t->translations[context] = (Uint1*) malloc(translation_length+2);
               }

               if (hsp->subject.frame < 0) {
                   /* needs reverse sequence */
                   GetReverseNuclSequence(nucl_seq, nucl_length, &nucl_seq_rev);
               }

               length = BLAST_GetTranslation(nucl_seq, nucl_seq_rev,
                       nucl_length, hsp->subject.frame, target_t->translations[context], 
                       target_t->gen_code_string);

               target_t->range[2*context+1] = start_shift + length;

               sfree(nucl_seq_rev);

               /* partial translation needs to be fenced */
               if(hsp->subject.offset >= 0) {
                   target_t->translations[context][0] = FENCE_SENTRY;
                   target_t->translations[context][length+1] = FENCE_SENTRY;
               }
         }
    }
    if (translated_length)
        *translated_length = target_t->range[2*context+1];

    /* +1 as the first byte is a sentinel. */
    return target_t->translations[context] - target_t->range[2*context] + 1;
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
   hsp->subject.end -= start_shift;
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


/** Retrieve the starting diagonal of an HSP
 * @param hsp The target HSP
 * @return The starting diagonal
 */
static Int4
s_HSPStartDiag(const BlastHSP *hsp)
{
    return hsp->query.offset - hsp->subject.offset;
}

/** Retrieve the ending diagonal of an HSP
 * @param hsp The target HSP
 * @return The ending diagonal
 */
static Int4
s_HSPEndDiag(const BlastHSP *hsp)
{
    return hsp->query.end - hsp->subject.end;
}

/** Given two hits, check if the hits can be merged and do 
 * the merge if so. Hits must not contain traceback
 * @param hsp1 The first hit. If merging happens, this hit is
 *             overwritten with the merged version [in][out]
 * @param hsp2 The second hit [in]
 * @return TRUE if a merge was performed, FALSE if not
 */
static Boolean 
s_BlastMergeTwoHSPs(BlastHSP* hsp1, BlastHSP* hsp2, Boolean allow_gap)
{
   ASSERT(!hsp1->gap_info || !hsp2->gap_info);

   /* do not merge off-diagonal hsps for ungapped search */
   if (!allow_gap && 
       hsp1->subject.offset - hsp2->subject.offset -hsp1->query.offset + hsp2->query.offset) 
   {  
       return FALSE;
   }

   /* combine the boundaries of the two HSPs, 
      assuming they intersect at all */
   if (CONTAINED_IN_HSP(hsp1->query.offset, hsp1->query.end,
                        hsp2->query.offset,
                        hsp1->subject.offset, hsp1->subject.end,
                        hsp2->subject.offset) ||
       CONTAINED_IN_HSP(hsp1->query.offset, hsp1->query.end,
                        hsp2->query.end,
                        hsp1->subject.offset, hsp1->subject.end,
                        hsp2->subject.end)) {

      hsp1->query.offset = MIN(hsp1->query.offset, hsp2->query.offset);
      hsp1->subject.offset = MIN(hsp1->subject.offset, hsp2->subject.offset);
      hsp1->query.end = MAX(hsp1->query.end, hsp2->query.end);
      hsp1->subject.end = MAX(hsp1->subject.end, hsp2->subject.end);
      if (hsp2->score > hsp1->score) {
          hsp1->query.gapped_start = hsp2->query.gapped_start;
          hsp1->subject.gapped_start = hsp2->subject.gapped_start;
          hsp1->score = hsp2->score;
      }
      return TRUE;
   }

   return FALSE;
}

/** Maximal diagonal distance between HSP starting offsets, within which HSPs 
 * from search of different chunks of subject sequence are considered for 
 * merging.
 */
#define OVERLAP_DIAG_CLOSE 10
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

Boolean
Blast_HSPList_IsEmpty(const BlastHSPList* hsp_list)
{
    return (hsp_list && hsp_list->hspcnt == 0) ? TRUE : FALSE;
}

BlastHSPList* BlastHSPListDup(const BlastHSPList* hsp_list) 
{
    BlastHSPList * rv = 0;
    
    if (hsp_list) {
        int index = 0;
        int num = hsp_list->hspcnt;
        
        rv = malloc(sizeof(BlastHSPList));
        *rv = *hsp_list;
        
        if (num) {
            rv->hsp_array = malloc(sizeof(BlastHSP*) * num);
            
            for(index = 0; index < hsp_list->hspcnt; ++index) {
                BlastHSP * h = hsp_list->hsp_array[index];
                BlastHSP ** h2 = & rv->hsp_array[index];
                
                if (h) {
                    *h2 = malloc(sizeof(BlastHSP));
                    **h2 = *h;
                } else {
                    *h2 = 0;
                }
            }
        }
    }
    
    return rv;
}

void Blast_HSPListSwap(BlastHSPList* list1, BlastHSPList* list2)
{
    BlastHSPList tmp;
    
    tmp = *list1;
    *list1 = *list2;
    *list2 = tmp;
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

#ifndef NDEBUG
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
#endif /* _DEBUG */

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
                             const BlastScoreBlk* sbp, double gap_decay_rate,
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
      ASSERT(sbp->round_down == FALSE || (hsp->score & 1) == 0);

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
                               Boolean gapped_calculation, 
                               const BlastScoreBlk* sbp)
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
      ASSERT(sbp->round_down == FALSE || (hsp->score & 1) == 0);
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
                           const BlastQueryInfo* query_info,
                           const SPHIPatternSearchBlk* pattern_blk)
{
   Int4 index;
   BlastHSP* hsp;

   if (!hsp_list || hsp_list->hspcnt == 0)
       return;

   for (index = 0; index < hsp_list->hspcnt; ++index) {
      hsp = hsp_list->hsp_array[index];
      s_HSPPHIGetEvalue(hsp, sbp, query_info, pattern_blk);
   }
   /* The best e-value is the one for the highest scoring HSP, which 
      must be the first in the list. Check that HSPs are sorted by score
      to make sure this assumption is correct. */
   ASSERT(Blast_HSPListIsSortedByScore(hsp_list));
   hsp_list->best_evalue = s_BlastGetBestEvalue(hsp_list);
}

Int2 Blast_HSPListReapByEvalue(BlastHSPList* hsp_list, 
        const BlastHitSavingOptions* hit_options)
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

/** callback used to sort HSP lists in order of increasing OID
 * @param x First HSP list [in]
 * @param y Second HSP list [in]
 * @return compare result
 */
static int s_SortHSPListByOid(const void *x, const void *y)
{
    BlastHSPList **xx = (BlastHSPList **)x;
    BlastHSPList **yy = (BlastHSPList **)y;
    return (*xx)->oid - (*yy)->oid;
}

Int2 Blast_HitListMerge(BlastHitList** old_hit_list_ptr,
                        BlastHitList** combined_hit_list_ptr,
                        Int4 contexts_per_query, Int4 *split_offsets,
                        Int4 chunk_overlap_size, Boolean allow_gap)
{
    Int4 i, j;
    Boolean query_is_split;
    BlastHitList* hitlist1 = *old_hit_list_ptr;
    BlastHitList* hitlist2 = *combined_hit_list_ptr;
    BlastHitList* new_hitlist;
    Int4 num_hsplists1;
    Int4 num_hsplists2;

    if (hitlist1 == NULL)
        return 0;
    if (hitlist2 == NULL) {
        *combined_hit_list_ptr = hitlist1;
        *old_hit_list_ptr = NULL;
        return 0;
    }
    num_hsplists1 = hitlist1->hsplist_count;
    num_hsplists2 = hitlist2->hsplist_count;
    new_hitlist = Blast_HitListNew(hitlist1->hsplist_max);

    /* sort the lists of HSPs by oid */

    if (num_hsplists1 > 1) {
        qsort(hitlist1->hsplist_array, num_hsplists1, 
              sizeof(BlastHSPList*), s_SortHSPListByOid);
    }
    if (num_hsplists2 > 1) {
        qsort(hitlist2->hsplist_array, num_hsplists2, 
              sizeof(BlastHSPList*), s_SortHSPListByOid);
    }

    /* find out if the two hitlists contain hits for a single
       (split) query sequence */

    query_is_split = FALSE;
    for (i = 0; i < contexts_per_query; i++) {
        if (split_offsets[i] > 0) {
            query_is_split = TRUE;
            break;
        }
    }
    ASSERT(chunk_overlap_size != 0);

    /* merge the HSPlists of the two HitLists */

    i = j = 0;
    while (i < num_hsplists1 && j < num_hsplists2) {
        BlastHSPList* hsplist1 = hitlist1->hsplist_array[i];
        BlastHSPList* hsplist2 = hitlist2->hsplist_array[j];

        if (hsplist1->oid < hsplist2->oid) {
            Blast_HitListUpdate(new_hitlist, hsplist1);
            i++;
        }
        else if (hsplist1->oid > hsplist2->oid) {
            Blast_HitListUpdate(new_hitlist, hsplist2);
            j++;
        }
        else {
            /* the old and new Hitlists contain hits to the same
               DB sequence, and these must be merged. */

            if (query_is_split) {
                Blast_HSPListsMerge(hitlist1->hsplist_array + i,
                                    hitlist2->hsplist_array + j,
                                    hsplist2->hsp_max, split_offsets,
                                    contexts_per_query,
                                    chunk_overlap_size,
                                    allow_gap);
            }
            else {
                Blast_HSPListAppend(hitlist1->hsplist_array + i,
                                    hitlist2->hsplist_array + j,
                                    hsplist2->hsp_max);
            }
            Blast_HitListUpdate(new_hitlist, hitlist2->hsplist_array[j]);
            i++;
            j++;
        }
    }
    for (; i < num_hsplists1; i++) {
        BlastHSPList* hsplist1 = hitlist1->hsplist_array[i];
        Blast_HitListUpdate(new_hitlist, hsplist1);
    }
    for (; j < num_hsplists2; j++) {
        BlastHSPList* hsplist2 = hitlist2->hsplist_array[j];
        Blast_HitListUpdate(new_hitlist, hsplist2);
    }
    hitlist1->hsplist_count = 0;
    Blast_HitListFree(hitlist1);
    hitlist2->hsplist_count = 0;
    Blast_HitListFree(hitlist2);

    *old_hit_list_ptr = NULL;
    *combined_hit_list_ptr = new_hitlist;
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

/** Callback for sorting HSPs by starting offset in query. Sorting is by
 * increasing context, then increasing query start offset, then increasing
 * subject start offset, then decreasing score, then increasing query end 
 * offset, then increasing subject end offset. Null HSPs are moved to the 
 * end of the array.
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

   /* tie breakers: sort by decreasing score, then 
      by increasing size of query range, then by
      increasing subject range. */

   if (h1->score < h2->score)
      return 1;
   if (h1->score > h2->score)
      return -1;

   if (h1->query.end < h2->query.end)
      return 1;
   if (h1->query.end > h2->query.end)
      return -1;

   if (h1->subject.end < h2->subject.end)
      return 1;
   if (h1->subject.end > h2->subject.end)
      return -1;

   return 0;
}

/** Callback for sorting HSPs by ending offset in query. Sorting is by
 * increasing context, then increasing query end offset, then increasing
 * subject end offset, then decreasing score, then decreasing query start
 * offset, then decreasing subject start offset. Null HSPs are moved to the 
 * end of the array.
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

   /* tie breakers: sort by decreasing score, then 
      by increasing size of query range, then by
      increasing size of subject range. The shortest range 
      means the *largest* sequence offset must come 
      first */
   if (h1->score < h2->score)
      return 1;
   if (h1->score > h2->score)
      return -1;

   if (h1->query.offset < h2->query.offset)
      return 1;
   if (h1->query.offset > h2->query.offset)
      return -1;

   if (h1->subject.offset < h2->subject.offset)
      return 1;
   if (h1->subject.offset > h2->subject.offset)
      return -1;

   return 0;
}

Int4
Blast_HSPListPurgeHSPsWithCommonEndpoints(EBlastProgramType program, 
                                          BlastHSPList* hsp_list)

{
   BlastHSP** hsp_array;  /* hsp_array to purge. */
   Int4 i, j;
   Int2 retval;
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
   i = 0;
   while (i < hsp_count) {
      j = 1;
      while (i+j < hsp_count &&
             hsp_array[i] && hsp_array[i+j] &&
             hsp_array[i]->context == hsp_array[i+j]->context &&
             hsp_array[i]->query.offset == hsp_array[i+j]->query.offset &&
             hsp_array[i]->subject.offset == hsp_array[i+j]->subject.offset) {
         hsp_array[i+j] = Blast_HSPFree(hsp_array[i+j]);
         j++;
      }
      i += j;
   }
   
   qsort(hsp_array, hsp_count, sizeof(BlastHSP*), s_QueryEndCompareHSPs);
   i = 0;
   while (i < hsp_count) {
      j = 1;
      while (i+j < hsp_count &&
             hsp_array[i] && hsp_array[i+j] &&
             hsp_array[i]->context == hsp_array[i+j]->context &&
             hsp_array[i]->query.end == hsp_array[i+j]->query.end &&
             hsp_array[i]->subject.end == hsp_array[i+j]->subject.end) {
         hsp_array[i+j] = Blast_HSPFree(hsp_array[i+j]);
         j++;
      }
      i += j;
   }

   retval = Blast_HSPListPurgeNullHSPs(hsp_list);
   if (retval < 0)
      return retval;

   return hsp_list->hspcnt;
}

Int2 
Blast_HSPListReevaluateUngapped(EBlastProgramType program, 
   BlastHSPList* hsp_list, BLAST_SequenceBlk* query_blk, 
   BLAST_SequenceBlk* subject_blk, 
   const BlastInitialWordParameters* word_params,
   const BlastHitSavingParameters* hit_params, const BlastQueryInfo* query_info, 
   BlastScoreBlk* sbp, const BlastScoringParameters* score_params, 
   const BlastSeqSrc* seq_src, const Uint1* gen_code_string)
{
   BlastHSP** hsp_array,* hsp;
   const Uint1* subject_start = NULL;
   Uint1* query_start;
   Int4 index, context, hspcnt;
   Boolean purge;
   Int2 status = 0;
   const Boolean kTranslateSubject = Blast_SubjectIsTranslated(program);
   const Boolean kNucleotideSubject = Blast_SubjectIsNucleotide(program);
   SBlastTargetTranslation* target_t = NULL;

   ASSERT(!score_params->options->gapped_calculation);

   if (!hsp_list)
      return status;

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
      /* If this was a protein subject, leave as is. */
      if (kNucleotideSubject)
      {
      	BlastSeqSrcGetSeqArg seq_arg;
      	memset((void*) &seq_arg, 0, sizeof(seq_arg));
      	seq_arg.oid = subject_blk->oid;
      	seq_arg.encoding =
       	  (kTranslateSubject ? eBlastEncodingNcbi4na : eBlastEncodingNucleotide);
        seq_arg.check_oid_exclusion = TRUE;
      	seq_arg.seq = subject_blk;
      	/* Return the packed sequence to the database */
      	BlastSeqSrcReleaseSequence(seq_src, &seq_arg);
      	/* Get the unpacked sequence */
      	if ((status=BlastSeqSrcGetSequence(seq_src, &seq_arg)))
          return status;
       }
   }

   if (kTranslateSubject) {
      if (!gen_code_string)
         return -1;
      /* Get the translation buffer */
      BlastTargetTranslationNew(subject_blk, gen_code_string, program, 
                   score_params->options->is_ooframe, &target_t);
   } else {
      /* Store sequence in blastna encoding in sequence_start */
      if (subject_blk->sequence_start)
          subject_start = subject_blk->sequence_start + 1;
      else
          subject_start = subject_blk->sequence;
   }

   purge = FALSE;
   for (index = 0; index < hspcnt; ++index) {
      Boolean delete_hsp = FALSE;
      if (hsp_array[index] == NULL)
         continue;
      else
         hsp = hsp_array[index];

      context = hsp->context;

      query_start = query_blk->sequence +
          query_info->contexts[context].query_offset;

      if (kTranslateSubject)
         subject_start = Blast_HSPGetTargetTranslation(target_t, hsp, NULL);

      if (kNucleotideSubject) {
         delete_hsp = 
             Blast_HSPReevaluateWithAmbiguitiesUngapped(hsp, query_start, 
                subject_start, word_params, sbp, kTranslateSubject);
      }
   
      if (!delete_hsp) {
          const Uint1* query_nomask = query_blk->sequence_nomask +
              query_info->contexts[context].query_offset;
          delete_hsp = 
              Blast_HSPTestIdentityAndLength(program, hsp, query_nomask, 
                                             subject_start, 
                                             score_params->options, 
                                             hit_params->options);
      }
      if (delete_hsp) { /* This HSP is now below the cutoff */
         hsp_array[index] = Blast_HSPFree(hsp_array[index]);
         purge = TRUE;
      }
   }

   if (target_t)
      target_t = BlastTargetTranslationFree(target_t);

   if (purge)
      Blast_HSPListPurgeNullHSPs(hsp_list);

   /* Sort the HSP array by score (scores may have changed!) */
   Blast_HSPListSortByScore(hsp_list);
   Blast_HSPListAdjustOddBlastnScores(hsp_list, FALSE, sbp);
   return 0;
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

Int2 Blast_HSPListAppend(BlastHSPList** old_hsp_list_ptr,
        BlastHSPList** combined_hsp_list_ptr, Int4 hsp_num_max)
{
   BlastHSPList* combined_hsp_list = *combined_hsp_list_ptr;
   BlastHSPList* hsp_list = *old_hsp_list_ptr;
   Int4 new_hspcnt;

   if (!hsp_list || hsp_list->hspcnt == 0)
      return 0;

   /* If no previous HSP list, return a pointer to the old one */
   if (!combined_hsp_list) {
      *combined_hsp_list_ptr = hsp_list;
      *old_hsp_list_ptr = NULL;
      return 0;
   }

   /* Just append new list to the end of the old list, in case of 
      multiple frames of the subject sequence */
   new_hspcnt = MIN(combined_hsp_list->hspcnt + hsp_list->hspcnt, 
                    hsp_num_max);
   if (new_hspcnt > combined_hsp_list->allocated && 
       !combined_hsp_list->do_not_reallocate) {
      Int4 new_allocated = MIN(2*new_hspcnt, hsp_num_max);
      BlastHSP** new_hsp_array;
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

   hsp_list = Blast_HSPListFree(hsp_list); 
   *old_hsp_list_ptr = NULL;

   return 0;
}

Int2 Blast_HSPListsMerge(BlastHSPList** hsp_list_ptr, 
                   BlastHSPList** combined_hsp_list_ptr,
                   Int4 hsp_num_max, Int4 *split_offsets, 
                   Int4 contexts_per_query, Int4 chunk_overlap_size,
                   Boolean allow_gap)
{
   BlastHSPList* combined_hsp_list = *combined_hsp_list_ptr;
   BlastHSPList* hsp_list = *hsp_list_ptr;
   BlastHSP* hsp1, *hsp2, *hsp_var;
   BlastHSP** hspp1,** hspp2;
   Int4 index1, index2;
   Int4 hspcnt1, hspcnt2, new_hspcnt = 0;
   Int4 start_diag, end_diag;
   Int4 offset_idx;
   BlastHSP** new_hsp_array;
  
   if (!hsp_list || hsp_list->hspcnt == 0)
      return 0;

   /* If no previous HSP list, just return a copy of the new one. */
   if (!combined_hsp_list) {
      *combined_hsp_list_ptr = hsp_list;
      *hsp_list_ptr = NULL;
      return 0;
   }

   /* Merge the two HSP lists for successive chunks of the subject sequence.
      First put all HSPs that intersect the overlap region at the front of 
      the respective HSP arrays. Note that if the query sequence is
      assumed split, each context of the query sequence can have a
      different split point */
   hspcnt1 = hspcnt2 = 0;

   if (contexts_per_query < 0) {      /* subject seq is split */
      for (index1 = 0; index1 < combined_hsp_list->hspcnt; index1++) {
         hsp1 = combined_hsp_list->hsp_array[index1];
         if (hsp1->subject.end > split_offsets[0]) {
            /* At least part of this HSP lies in the overlap strip. */
            hsp_var = combined_hsp_list->hsp_array[hspcnt1];
            combined_hsp_list->hsp_array[hspcnt1] = hsp1;
            combined_hsp_list->hsp_array[index1] = hsp_var;
            ++hspcnt1;
         }
      }
      for (index2 = 0; index2 < hsp_list->hspcnt; index2++) {
         hsp2 = hsp_list->hsp_array[index2];
         if (hsp2->subject.offset < split_offsets[0] + chunk_overlap_size) {
            /* At least part of this HSP lies in the overlap strip. */
            hsp_var = hsp_list->hsp_array[hspcnt2];
            hsp_list->hsp_array[hspcnt2] = hsp2;
            hsp_list->hsp_array[index2] = hsp_var;
            ++hspcnt2;
         }
      }
   }
   else {            /* query seq is split */

      /* An HSP can be a candidate for merging if it lies in the
         overlap region. Whether this is true depends on whether the
         HSP starts to the left of the split point, or ends to the 
         right of the overlap region. A complication is that 'left' 
         and 'right' have opposite meaning when the HSP is on the 
         minus strand of the query sequence */

      for (index1 = 0; index1 < combined_hsp_list->hspcnt; index1++) {
         hsp1 = combined_hsp_list->hsp_array[index1];
         offset_idx = hsp1->context % contexts_per_query;
         if (split_offsets[offset_idx] < 0) continue;
         if ((hsp1->query.frame >= 0 && hsp1->query.end > 
                         split_offsets[offset_idx]) ||
             (hsp1->query.frame < 0 && hsp1->query.offset < 
                         split_offsets[offset_idx] + chunk_overlap_size)) {
            /* At least part of this HSP lies in the overlap strip. */
            hsp_var = combined_hsp_list->hsp_array[hspcnt1];
            combined_hsp_list->hsp_array[hspcnt1] = hsp1;
            combined_hsp_list->hsp_array[index1] = hsp_var;
            ++hspcnt1;
         }
      }
      for (index2 = 0; index2 < hsp_list->hspcnt; index2++) {
         hsp2 = hsp_list->hsp_array[index2];
         offset_idx = hsp2->context % contexts_per_query;
         if (split_offsets[offset_idx] < 0) continue;
         if ((hsp2->query.frame < 0 && hsp2->query.end > 
                         split_offsets[offset_idx]) ||
             (hsp2->query.frame >= 0 && hsp2->query.offset < 
                         split_offsets[offset_idx] + chunk_overlap_size)) {
            /* At least part of this HSP lies in the overlap strip. */
            hsp_var = hsp_list->hsp_array[hspcnt2];
            hsp_list->hsp_array[hspcnt2] = hsp2;
            hsp_list->hsp_array[index2] = hsp_var;
            ++hspcnt2;
         }
      }
   }

   /* the merge process is independent of whether merging happens
      between query chunks or subject chunks */

   if (hspcnt1 > 0 && hspcnt2 > 0) {
      hspp1 = combined_hsp_list->hsp_array;
      hspp2 = hsp_list->hsp_array;
   
      for (index1 = 0; index1 < hspcnt1; index1++) {
   
         hsp1 = hspp1[index1];
   
         for (index2 = 0; index2 < hspcnt2; index2++) {

            hsp2 = hspp2[index2];
   
            /* Skip already deleted HSPs, or HSPs from different contexts */
            if (!hsp2 || hsp1->context != hsp2->context)
               continue;

            /* we have to determine the starting diagonal of one HSP
               and the ending diagonal of the other */

            if (contexts_per_query < 0 || hsp1->query.frame >= 0) {
               end_diag = s_HSPEndDiag(hsp1);
               start_diag = s_HSPStartDiag(hsp2);
            }
            else {
               end_diag = s_HSPEndDiag(hsp2);
               start_diag = s_HSPStartDiag(hsp1);
            }
   
            if (ABS(end_diag - start_diag) < OVERLAP_DIAG_CLOSE) {
               if (s_BlastMergeTwoHSPs(hsp1, hsp2, allow_gap)) {
                  /* Free the second HSP. */
                  hspp2[index2] = Blast_HSPFree(hsp2);
               }
            }
         }
      }
   
      /* Purge the nulled out HSPs from the new HSP list */
      Blast_HSPListPurgeNullHSPs(hsp_list);
   }

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

   hsp_list = Blast_HSPListFree(hsp_list); 
   *hsp_list_ptr = NULL;

   return 0;
}

void Blast_HSPListAdjustOffsets(BlastHSPList* hsp_list, Int4 offset)
{
   Int4 index;

   if (offset == 0) {
       return;
   }

   for (index=0; index<hsp_list->hspcnt; index++) {
      BlastHSP* hsp = hsp_list->hsp_array[index];
      hsp->subject.offset += offset;
      hsp->subject.end += offset;
      hsp->subject.gapped_start += offset;
   }
}

void Blast_HSPListAdjustOddBlastnScores(BlastHSPList* hsp_list, 
                                        Boolean gapped_calculation, 
                                        const BlastScoreBlk* sbp)
{
    int index;
    
    if (!hsp_list || hsp_list->hspcnt == 0 ||
        gapped_calculation == FALSE ||
        sbp->round_down == FALSE)
       return;
    
    for (index = 0; index < hsp_list->hspcnt; ++index)
        hsp_list->hsp_array[index]->score &= ~1;

    /* Sort the HSPs again, since the order may have to be different now. */
    Blast_HSPListSortByScore(hsp_list);
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
   return BLAST_CMP(h2->oid, h1->oid);
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
   new_hitlist->hsplist_count = 0;
   new_hitlist->hsplist_current = 0;
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

/** Given a BlastHitList pointer this function makes the 
 * hsplist_array larger, up to a maximum size.
 * These incremental increases are mostly an issue for users who
 * put in a very large number for number of hits to save, but only save a few.
 * @param hit_list object containing the hsplist_array to grow [in]
 * @return zero on success, 1 if full already.
 */
static Int2 s_Blast_HitListGrowHSPListArray(BlastHitList* hit_list)

{
    const int kStartValue = 100; /* default number of hsplist_array to start with. */

    ASSERT(hit_list);

    if (hit_list->hsplist_current >= hit_list->hsplist_max)
       return 1;

    if (hit_list->hsplist_current <= 0)
       hit_list->hsplist_current = kStartValue;
    else
       hit_list->hsplist_current = MIN(2*hit_list->hsplist_current, hit_list->hsplist_max);

    hit_list->hsplist_array = 
       (BlastHSPList**) realloc(hit_list->hsplist_array, hit_list->hsplist_current*sizeof(BlastHSPList*));

    if (hit_list->hsplist_array == NULL)
       return -1;

    return 0;
}

Int2 Blast_HitListUpdate(BlastHitList* hit_list, 
                         BlastHSPList* hsp_list)
{
   hsp_list->best_evalue = s_BlastGetBestEvalue(hsp_list);

#ifndef NDEBUG
   ASSERT(s_BlastCheckBestEvalue(hsp_list) == TRUE); /* NCBI_FAKE_WARNING */
#endif /* _DEBUG */
 
   if (hit_list->hsplist_count < hit_list->hsplist_max) {
      /* If the array of HSP lists for this query is not yet allocated, 
         do it here */
      if (hit_list->hsplist_current == hit_list->hsplist_count)
      {
         Int2 status = s_Blast_HitListGrowHSPListArray(hit_list);
         if (status)
           return status;
      }
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

Int2 
Blast_HitListPurgeNullHSPLists(BlastHitList* hit_list)
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

BlastHSPResults** 
PHIBlast_HSPResultsSplit(const BlastHSPResults* results, 
                         const SPHIQueryInfo* pattern_info)
{
    BlastHSPResults* *phi_results = NULL;
    int pattern_index, hit_index;
    int num_patterns;
    BlastHitList* hit_list = NULL;
    BlastHSPList** hsplist_array; /* Temporary per-pattern HSP lists. */  

    if (!pattern_info || pattern_info->num_patterns == 0)
        return NULL;

    num_patterns = pattern_info->num_patterns;
    
    phi_results = 
        (BlastHSPResults**) calloc(num_patterns, sizeof(BlastHSPResults*));

    if (!results || !results->hitlist_array[0])
        return phi_results;   /* An empty results set is expected if no hits. */

    hsplist_array = (BlastHSPList**) calloc(num_patterns, sizeof(BlastHSPList*));
    hit_list = results->hitlist_array[0];

    for (hit_index = 0; hit_index < hit_list->hsplist_count; ++hit_index) {
        BlastHSPList* hsp_list = hit_list->hsplist_array[hit_index];
        int hsp_index;
        /* Copy HSPs corresponding to different pattern occurrences into
           separate HSP lists. */
        for (hsp_index = 0; hsp_index < hsp_list->hspcnt; ++hsp_index) {
            BlastHSP* hsp = s_BlastHSPCopy(hsp_list->hsp_array[hsp_index]);
            pattern_index = hsp->pat_info->index;
            if (!hsplist_array[pattern_index])
                hsplist_array[pattern_index] = Blast_HSPListNew(0);
            hsplist_array[pattern_index]->oid = hsp_list->oid;
            Blast_HSPListSaveHSP(hsplist_array[pattern_index], hsp);
        }
        
        /* Save HSP lists corresponding to different pattern occurrences 
           in separate results structures. */
        for (pattern_index = 0; pattern_index < num_patterns; 
             ++pattern_index) {
            if (hsplist_array[pattern_index]) {
                if (!phi_results[pattern_index])
                    phi_results[pattern_index] = Blast_HSPResultsNew(1);
                Blast_HSPResultsInsertHSPList(phi_results[pattern_index],
                                              hsplist_array[pattern_index],
                                              hit_list->hsplist_max);
                hsplist_array[pattern_index] = NULL;
            }
        }
    }
    
    sfree(hsplist_array);

    /* Sort HSPLists in each of the results structures by e-value. */
    for (pattern_index = 0; pattern_index < num_patterns; ++pattern_index) {
        Blast_HSPResultsSortByEvalue(phi_results[pattern_index]);
    }

    return phi_results;
}

BlastHSPResults*
Blast_HSPResultsFromHSPStream(BlastHSPStream* hsp_stream, 
                              size_t num_queries, 
                              SBlastHitsParameters* bhp)
{
    BlastHSPResults* retval = NULL;
    BlastHSPList* hsp_list = NULL;

    retval = Blast_HSPResultsNew((Int4) num_queries);

    while (BlastHSPStreamRead(hsp_stream, &hsp_list) != kBlastHSPStream_Eof) {
        Blast_HSPResultsInsertHSPList(retval, hsp_list, 
                                      bhp->prelim_hitlist_size);
    }
    SBlastHitsParametersFree(bhp);
    return retval;
}

/** Comparison function for sorting HSP lists in increasing order of the 
 * number of HSPs in a hit. Needed for s_TrimResultsByTotalHSPLimit below. 
 * @param v1 Pointer to the first HSP list [in]
 * @param v2 Pointer to the second HSP list [in]
 */
static int
s_CompareHsplistHspcnt(const void* v1, const void* v2)
{
   BlastHSPList* r1 = *((BlastHSPList**) v1);
   BlastHSPList* r2 = *((BlastHSPList**) v2);

   if (r1->hspcnt < r2->hspcnt)
      return -1;
   else if (r1->hspcnt > r2->hspcnt)
      return 1;
   else
      return 0;
}

/** Removes extra results if a limit is imposed on the total number of HSPs
 * returned. If the search involves multiple query sequences, the total HSP 
 * limit is applied separately to each query.
 * The trimming algorithm makes sure that at least 1 HSP is returned for each
 * database sequence hit. Suppose results for a given query consist of HSP 
 * lists for N database sequences, and the limit is T. HSP lists are sorted in
 * order of increasing number of HSPs in each list. Then the algorithm proceeds
 * by leaving at most i*T/N HSPs for the first i HSP lists, for every 
 * i = 1, 2, ..., N.
 * @param results Results after preliminary stage of a BLAST search [in|out]
 * @param total_hsp_limit Limit on total number of HSPs [in]
 * @return TRUE if HSP limit has been exceeded, FALSE otherwise.
 */
static Boolean
s_TrimResultsByTotalHSPLimit(BlastHSPResults* results, Uint4 total_hsp_limit)
{
    int query_index;
    Boolean hsp_limit_exceeded = FALSE;
    
    if (total_hsp_limit == 0) {
        return hsp_limit_exceeded;
    }

    for (query_index = 0; query_index < results->num_queries; ++query_index) {
        BlastHitList* hit_list = NULL;
        BlastHSPList** hsplist_array = NULL;
        Int4 hsplist_count = 0;
        int subj_index;

        if ( !(hit_list = results->hitlist_array[query_index]) )
            continue;
        /* The count of HSPs is separate for each query. */
        hsplist_count = hit_list->hsplist_count;

        hsplist_array = (BlastHSPList**) 
            malloc(hsplist_count*sizeof(BlastHSPList*));
        
        for (subj_index = 0; subj_index < hsplist_count; ++subj_index) {
            hsplist_array[subj_index] = hit_list->hsplist_array[subj_index];
        }
 
        qsort((void*)hsplist_array, hsplist_count,
              sizeof(BlastHSPList*), s_CompareHsplistHspcnt);
        
        {
            Int4 tot_hsps = 0;  /* total number of HSPs */
            Uint4 hsp_per_seq = MAX(1, total_hsp_limit/hsplist_count);
            for (subj_index = 0; subj_index < hsplist_count; ++subj_index) {
                Int4 allowed_hsp_num = ((subj_index+1)*hsp_per_seq) - tot_hsps;
                BlastHSPList* hsp_list = hsplist_array[subj_index];
                if (hsp_list->hspcnt > allowed_hsp_num) {
                    /* Free the extra HSPs */
                    int hsp_index;
                    for (hsp_index = allowed_hsp_num; 
                         hsp_index < hsp_list->hspcnt; ++hsp_index) {
                        Blast_HSPFree(hsp_list->hsp_array[hsp_index]);
                    }
                    hsp_list->hspcnt = allowed_hsp_num;
                    hsp_limit_exceeded = TRUE;
                }
                tot_hsps += hsp_list->hspcnt;
            }
        }
        sfree(hsplist_array);
    }

    return hsp_limit_exceeded;
}

BlastHSPResults*
Blast_HSPResultsFromHSPStreamWithLimit(BlastHSPStream* hsp_stream, 
                                   Uint4 num_queries, 
                                   SBlastHitsParameters* hit_param,
                                   Uint4 max_num_hsps,
                                   Boolean* removed_hsps)
{
    Boolean rm_hsps = FALSE;
    BlastHSPResults* retval = Blast_HSPResultsFromHSPStream(hsp_stream,
                                                            num_queries,
                                                            hit_param);

    rm_hsps = s_TrimResultsByTotalHSPLimit(retval, max_num_hsps);
    if (removed_hsps) {
        *removed_hsps = rm_hsps;
    }
    return retval;
}
