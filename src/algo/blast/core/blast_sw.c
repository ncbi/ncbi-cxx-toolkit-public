/* $Id$
 * ===========================================================================
 *
 *                     PUBLIC DOMAIN NOTICE
 *            National Center for Biotechnology Information
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
 * Author:  Jason Papadopoulos
 *
 */

/** @file blast_sw.c
 * Smith-Waterman gapped alignment, for use with infrastructure of BLAST
 * @sa blast_sw.h
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
   "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_sw.h>
#include <algo/blast/core/blast_util.h> /* for NCBI2NA_UNPACK_BASE */

/** swap (pointers to) a pair of sequences */
#define SWAP_SEQS(A, B) {Uint1 *tmp = (A); (A) = (B); (B) = tmp; }

/** swap two integers */
#define SWAP_INT(A, B) {Int4 tmp = (A); (A) = (B); (B) = tmp; }

/** Compute the score of the best local alignment between
 *  two protein sequences. When using Smith-Waterman, the vast
 *  majority of the runtime is tied up in this routine.
 * @param A The first sequence [in]
 * @param a_size Length of the first sequence [in]
 * @param B The second sequence [in]
 * @param b_size Length of the second sequence [in]
 * @param gap_open Gap open penalty [in]
 * @param gap_extend Gap extension penalty [in]
 * @param gap_align Auxiliary data for gapped alignment 
 *             (used for score matrix info) [in]
 * @return The score of the best local alignment between A and B
 */
static Int4 s_SmithWatermanScoreOnly(Uint1 *A, Int4 a_size,
                            Uint1 *B, Int4 b_size,
                            Int4 gap_open, Int4 gap_extend,
                            BlastGapAlignStruct *gap_align)
{
   Int4 i, j;
   Int4 **matrix;
   Int4 *matrix_row;

   Int4 final_best_score;
   Int4 best_score;
   Int4 insert_score;
   Int4 row_score;
   BlastGapDP *scores;

   Boolean is_pssm = gap_align->positionBased;
   Int4 gap_open_extend = gap_open + gap_extend;

   /* choose the score matrix */
   if (is_pssm) {
      matrix = gap_align->sbp->psi_matrix->pssm->data;
   }
   else {
      /* for square score matrices, assume the matrix
         is symmetric. This means that A and B can be
         switched without changing the score, and this
         saves memory if one sequence is large but the
         other is not */
      if (a_size < b_size) {
         SWAP_SEQS(A, B);
         SWAP_INT(a_size, b_size);
      }
      matrix = gap_align->sbp->matrix->data;
   }

   /* allocate space for scratch structures */
   if (b_size + 1 > gap_align->dp_mem_alloc) {
      gap_align->dp_mem_alloc = MAX(b_size + 100,
                             2 * gap_align->dp_mem_alloc); 
      sfree(gap_align->dp_mem);
      gap_align->dp_mem = (BlastGapDP *)malloc(gap_align->dp_mem_alloc *
                                               sizeof(BlastGapDP));
   }
   scores = gap_align->dp_mem;
   memset(scores, 0, (b_size + 1) * sizeof(BlastGapDP));
   final_best_score = 0;


   for (i = 1; i <= a_size; i++) {

      if (is_pssm)
         matrix_row = matrix[i-1];
      else
         matrix_row = matrix[A[i-1]];

      insert_score = 0;
      row_score = 0;

      for (j = 1; j <= b_size; j++) {

         /* score of best alignment ending at (i,j) with gap in B */
         best_score = scores[j].best_gap - gap_extend;
         if (scores[j].best - gap_open_extend > best_score)
            best_score = scores[j].best - gap_open_extend;
         scores[j].best_gap = best_score;

         /* score of best alignment ending at (i,j) with gap in A */
         best_score = insert_score - gap_extend;
         if (row_score - gap_open_extend > best_score)
            best_score = row_score - gap_open_extend;
         insert_score = best_score;

         /* score of best alignment ending at (i,j) */
         best_score = MAX(scores[j-1].best + matrix_row[B[j-1]], 0);
         if (insert_score > best_score)
            best_score = insert_score;
         if (scores[j].best_gap > best_score)
            best_score = scores[j].best_gap;

         if (best_score > final_best_score)
            final_best_score = best_score;

         scores[j-1].best = row_score;
         row_score = best_score;
      }

      scores[j-1].best = row_score;
   }

   return final_best_score;
}


/** Compute the score of the best local alignment between
 *  two nucleotide sequences. One of the sequences must be in
 *  packed format. For nucleotide Smith-Waterman, the vast
 *  majority of the runtime is tied up in this routine.
 * @param B The first sequence (must be in ncbi2na format) [in]
 * @param b_size Length of the first sequence [in]
 * @param A The second sequence [in]
 * @param a_size Length of the second sequence [in]
 * @param gap_open Gap open penalty [in]
 * @param gap_extend Gap extension penalty [in]
 * @param gap_align Auxiliary data for gapped alignment 
 *             (used for score matrix info) [in]
 * @return The score of the best local alignment between A and B
 */
static Int4 s_NuclSmithWaterman(Uint1 *B, Int4 b_size,
                                Uint1 *A, Int4 a_size,
                                Int4 gap_open, Int4 gap_extend,
                                BlastGapAlignStruct *gap_align)
{
   Int4 i, j;
   Int4 **matrix;
   Int4 *matrix_row;

   Int4 final_best_score;
   Int4 best_score;
   Int4 insert_score;
   Int4 row_score;
   BlastGapDP *scores;

   Int4 gap_open_extend = gap_open + gap_extend;

   /* position-specific scoring is not allowed, because
      the loops below assume the score matrix is symmetric */
   matrix = gap_align->sbp->matrix->data;

   if (a_size + 1 > gap_align->dp_mem_alloc) {
      gap_align->dp_mem_alloc = MAX(a_size + 100,
                             2 * gap_align->dp_mem_alloc); 
      sfree(gap_align->dp_mem);
      gap_align->dp_mem = (BlastGapDP *)malloc(gap_align->dp_mem_alloc *
                                               sizeof(BlastGapDP));
   }
   scores = gap_align->dp_mem;
   memset(scores, 0, (a_size + 1) * sizeof(BlastGapDP));
   final_best_score = 0;


   for (i = 1; i <= b_size; i++) {

      /* The only real difference between this routine 
         and its unpacked counterpart is the choice of
         score matrix row in the outer loop */
      Int4 base_pair = NCBI2NA_UNPACK_BASE(B[(i-1)/4], (3-((i-1)%4)));
      matrix_row = matrix[base_pair];
      insert_score = 0;
      row_score = 0;

      for (j = 1; j <= a_size; j++) {

         /* score of best alignment ending at (i,j) with gap in A */
         best_score = scores[j].best_gap - gap_extend;
         if (scores[j].best - gap_open_extend > best_score)
            best_score = scores[j].best - gap_open_extend;
         scores[j].best_gap = best_score;

         /* score of best alignment ending at (i,j) with gap in B */
         best_score = insert_score - gap_extend;
         if (row_score - gap_open_extend > best_score)
            best_score = row_score - gap_open_extend;
         insert_score = best_score;

         /* score of best alignment ending at (i,j) */
         best_score = MAX(scores[j-1].best + matrix_row[A[j-1]], 0);
         if (insert_score > best_score)
            best_score = insert_score;
         if (scores[j].best_gap > best_score)
            best_score = scores[j].best_gap;

         if (best_score > final_best_score)
            final_best_score = best_score;

         scores[j-1].best = row_score;
         row_score = best_score;
      }

      scores[j-1].best = row_score;
   }

   return final_best_score;
}


/** Values for the editing script operations in traceback */
enum {
   EDIT_SUB         = eGapAlignSub,    /**< Substitution */
   EDIT_GAP_IN_A     = eGapAlignDel,    /**< Deletion */
   EDIT_GAP_IN_B     = eGapAlignIns,    /**< Insertion */
   EDIT_OP_MASK      = 0x07, /**< Mask for edit script operations */

   EDIT_START_GAP_A  = 0x10, /**< open a gap in A */
   EDIT_START_GAP_B  = 0x20  /**< open a gap in B */
};


/** Generate the traceback for a single local alignment
 *  between two (unpacked) sequences, then create an HSP
 *  for the alignment and add to a list of such HSPs
 * @param program_number Blast program requesting traceback [in]
 * @param trace 2-D array of edit actions, size (a_size+1) x (b_size+1) [in]
 * @param A The first sequence [in]
 * @param B The second sequence [in]
 * @param b_size Length of the second sequence [in]
 * @param gap_open Gap open penalty [in]
 * @param gap_extend Gap extension penalty [in]
 * @param gap_align Auxiliary data for gapped alignment 
 *             (used for score matrix info) [in]
 * @param a_end The alignment end offset on A (plus one) [in]
 * @param b_end The alignment end offset on B (plus one) [in]
 * @param best_score Score of the alignment [in]
 * @param hsp_list Collection of alignments found so far [in][out]
 * @param swapped TRUE if A and B were swapped before the alignment 
 *               was found [in]
 * @param template_hsp Placeholder alignment, used only to
 *               determine contexts and frames [in]
 * @param score_options Structure containing gap penalties [in]
 * @param hit_options Structure used for percent identity calculation [in]
 * @param start_shift Bias to be applied to subject offsets [in]
 */
static void s_GetTraceback(EBlastProgramType program_number, 
                           Uint1 *trace, Uint1 *A, Uint1 *B, Int4 b_size,
                           Int4 gap_open, Int4 gap_extend,
                           BlastGapAlignStruct *gap_align,
                           Int4 a_end, Int4 b_end, Int4 best_score,
                           BlastHSPList *hsp_list, Boolean swapped,
                           BlastHSP *template_hsp, 
                           const BlastScoringOptions *score_options,
                           const BlastHitSavingOptions *hit_options,
                           Int4 start_shift)
{
   Int4 i, j;
   Uint1 script;
   Uint1 next_action;
   Uint1 *traceback_row;
   Int4 a_start, b_start;
   Int4 **matrix;
   Int4 curr_score = -best_score;
   Boolean is_pssm = gap_align->positionBased;
   GapPrelimEditBlock *prelim_tback = gap_align->fwd_prelim_tback;
   GapEditScript *final_tback;
   BlastHSP *new_hsp;

   i = a_end;
   j = b_end;
   traceback_row = trace + i * (b_size + 1);
   script = traceback_row[j] & EDIT_OP_MASK;
   GapPrelimEditBlockReset(prelim_tback);

   if (is_pssm)
      matrix = gap_align->sbp->psi_matrix->pssm->data;
   else
      matrix = gap_align->sbp->matrix->data;

   /* Only the start point of the alignment is unknown, but
      there was no bookkeeping performed to remember where the
      start point is located. We know the list of traceback
      actions backwards from (i,j) and know the score of the
      alignment, so the start point will be the offset pair where 
      the score becomes zero after edit operations are applied */

   while (curr_score != 0) {

      next_action = traceback_row[j];
      GapPrelimEditBlockAdd(prelim_tback, (EGapAlignOpType)script, 1);

      switch(script) {
      case EDIT_SUB:
         if (is_pssm)
            curr_score += matrix[i-1][B[j-1]];
         else
            curr_score += matrix[A[i-1]][B[j-1]];

         i--; j--;
         traceback_row -= b_size + 1;
         script = traceback_row[j] & EDIT_OP_MASK;
         break;
      case EDIT_GAP_IN_A:
         j--;
         if (next_action & EDIT_START_GAP_A) {
            script = traceback_row[j] & EDIT_OP_MASK;
            curr_score -= gap_open;
         }
         curr_score -= gap_extend;
         break;
      case EDIT_GAP_IN_B:
         i--;
         traceback_row -= b_size + 1;
         if (next_action & EDIT_START_GAP_B) {
            script = traceback_row[j] & EDIT_OP_MASK;
            curr_score -= gap_open;
         }
         curr_score -= gap_extend;
         break;
      }
   }

   /* found the start point. Now create the final (reversed) 
      edit script; if A and B were swapped before calling 
      this routine, swap them back */
   a_start = i;
   b_start = j;
   final_tback = GapEditScriptNew(prelim_tback->num_ops);
   for (i = prelim_tback->num_ops - 1, j = 0; i >= 0; i--, j++) {
      GapPrelimEditScript *p = prelim_tback->edit_ops + i;
      final_tback->num[j] = p->num;
      final_tback->op_type[j] = p->op_type;
      if (swapped) {
         if (p->op_type == eGapAlignIns)
            final_tback->op_type[j] = eGapAlignDel;
         else if (p->op_type == eGapAlignDel)
            final_tback->op_type[j] = eGapAlignIns;
      }
   }

   if (swapped) {
      SWAP_SEQS(A, B);
      SWAP_INT(a_start, b_start);
      SWAP_INT(a_end, b_end);
   }

   /* construct an HSP, verify it meets length and percent
      identity criteria, and save it */
   Blast_HSPInit(a_start, a_end, b_start, b_end,
                 a_start, b_start, template_hsp->context, 
                 template_hsp->query.frame, 
                 template_hsp->subject.frame, best_score,
                 &final_tback, &new_hsp);

   if (Blast_HSPTestIdentityAndLength(program_number, new_hsp, A, B,
                                      score_options, hit_options)) {
      Blast_HSPFree(new_hsp);
   }
   else {
      Blast_HSPAdjustSubjectOffset(new_hsp, start_shift);
      Blast_HSPListSaveHSP(hsp_list, new_hsp);
   }
}


/** Auxiliary structures for Smith-Waterman alignment 
 *  with traceback
 */
typedef struct BlastGapSW{
   Int4 best;        /**< Score of best alignment at this position */
   Int4 best_gap;     /**< Score of best alignment at this position that
                        ends in a gap */
   Int4 path_score;   /**< The highest score that the alignment at this
                        position has previously achieved */
   Int4 path_stop_i;   /**< Offset (plus one) on the first sequence where
                        path_score occurs */
   Int4 path_stop_j;   /**< Offset (plus one) on the second sequence where
                        path_score occurs */
} BlastGapSW;

/* See blast_sw.h for details */
void SmithWatermanScoreWithTraceback(EBlastProgramType program_number,
                                     Uint1 *A, Int4 a_size,
                                     Uint1 *B, Int4 b_size,
                                     BlastHSP *template_hsp,
                                     BlastHSPList *hsp_list,
                                     const BlastScoringParameters *score_params,
                                     const BlastHitSavingParameters *hit_params,
                                     BlastGapAlignStruct *gap_align,
                                     Int4 start_shift, Int4 cutoff)
{
   Int4 i, j;
   Int4 *matrix_row;
   Int4 **matrix;
   Boolean swapped = FALSE;

   Int4 best_score;
   Int4 insert_score;
   Int4 row_score;
   Int4 row_path_score;
   Int4 row_path_stop_i;
   Int4 row_path_stop_j;
   Int4 new_path_score;
   Int4 new_path_stop_i;
   Int4 new_path_stop_j;
   BlastGapSW *scores;

   Boolean is_pssm = gap_align->positionBased;
   Int4 gap_open = score_params->gap_open;
   Int4 gap_extend = score_params->gap_extend;
   Int4 gap_open_extend = gap_open + gap_extend;

   Uint1 *traceback_array;
   Uint1 *traceback_row;
   Uint1 script;

   /* choose the score matrix */
   if (is_pssm) {
      matrix = gap_align->sbp->psi_matrix->pssm->data;
   }
   else {
      /* for square score matrices, assume the matrix
         is symmetric. This means that A and B can be
         switched without changing the score, and this
         saves memory if one sequence is large but the
         other is not */
      if (a_size < b_size) {
         swapped = TRUE;
         SWAP_SEQS(A, B);
         SWAP_INT(a_size, b_size);
      }
      matrix = gap_align->sbp->matrix->data;
   }

   /* allocate space for scratch structures */
   scores = (BlastGapSW *)calloc(b_size + 1, sizeof(BlastGapSW));
   traceback_array = (Uint1 *)malloc((a_size + 1) * (b_size + 1) *
                                     sizeof(Uint1));
   traceback_row = traceback_array;
   for (i = 0; i <= b_size; i++)
      traceback_row[i] = EDIT_GAP_IN_A;
   traceback_row += b_size + 1;

   for (i = 1; i <= a_size; i++) {

      if (is_pssm)
         matrix_row = matrix[i-1];
      else
         matrix_row = matrix[A[i-1]];

      insert_score = 0;
      row_score = 0;
      row_path_stop_i = 0;
      row_path_stop_j = 0;
      row_path_score = 0;
      traceback_row[0] = EDIT_GAP_IN_B;

      for (j = 1; j <= b_size; j++) {

         /* score of best alignment ending at (i,j) with gap in B */
         best_score = scores[j].best_gap - gap_extend;
         script = 0;
         if (scores[j].best - gap_open_extend > best_score) {
            script |= EDIT_START_GAP_B;
            best_score = scores[j].best - gap_open_extend;
         }
         scores[j].best_gap = best_score;

         /* score of best alignment ending at (i,j) with gap in A */
         best_score = insert_score - gap_extend;
         if (row_score - gap_open_extend > best_score) {
            script |= EDIT_START_GAP_A;
            best_score = row_score - gap_open_extend;
         }
         insert_score = best_score;

         /* Every cell computed by Smith-Waterman lies on exactly 
            one highest-scoring path. This means that at any given
            time there are only b_size possible paths that need
            bookkeeping information. In addition to the 3-way 
            maximum needed to choose the highest score, we also 
            need to choose the path that (i,j) extends. Begin by 
            assuming (i,j) extends the substitution path */
         best_score = MAX(scores[j-1].best + matrix_row[B[j-1]], 0);
         traceback_row[j] = script | EDIT_SUB;
         new_path_score = scores[j-1].path_score;
         new_path_stop_i = scores[j-1].path_stop_i;
         new_path_stop_j = scores[j-1].path_stop_j;

         /* switch to the insertion or deletion path, if one
            of these has the highest overall score */
         if (insert_score > best_score) {
            best_score = insert_score;
            traceback_row[j] = script | EDIT_GAP_IN_A;

            new_path_score = row_path_score;
            new_path_stop_i = row_path_stop_i;
            new_path_stop_j = row_path_stop_j;
         }
         if (scores[j].best_gap >= best_score) {
            best_score = scores[j].best_gap;
            traceback_row[j] = script | EDIT_GAP_IN_B;

            new_path_score = scores[j].path_score;
            new_path_stop_i = scores[j].path_stop_i;
            new_path_stop_j = scores[j].path_stop_j;
         }

         if (best_score == 0) {
            /* the score of the path extended by (i,j) has
               decayed to zero, meaning this path can never
               be extended again. Before proceeding to the
               next cell and forgetting about this path, check
               whether the highest score previously achieved
               by this path exceeds the cutoff for the overall
               search, and if so then recover the alignment for
               the current path right now */
            if (new_path_score >= cutoff) {
               s_GetTraceback(program_number, traceback_array, 
                              A, B, b_size,
                              gap_open, gap_extend,
                              gap_align, new_path_stop_i,
                              new_path_stop_j, new_path_score,
                              hsp_list, swapped, template_hsp,
                              score_params->options,
                              hit_params->options, start_shift);
            }
            new_path_score = 0;
         }

         /* check if (i,j) is a new local maximum score 
            for this path */
         if (best_score > new_path_score) {
            new_path_score = best_score;
            new_path_stop_i = i;
            new_path_stop_j = j;
         }

         /* save path information */
         scores[j-1].best = row_score;
         scores[j-1].path_score = row_path_score;
         scores[j-1].path_stop_i = row_path_stop_i;
         scores[j-1].path_stop_j = row_path_stop_j;

         row_score = best_score;
         row_path_score = new_path_score;
         row_path_stop_i = new_path_stop_i;
         row_path_stop_j = new_path_stop_j;
      }

      /* save path information for the last cell in row i */
      scores[j-1].best = row_score;
      scores[j-1].path_score = row_path_score;
      scores[j-1].path_stop_i = row_path_stop_i;
      scores[j-1].path_stop_j = row_path_stop_j;

      /* the last cell may not have decayed to zero, but the
         best score of the path containing it can still exceed
         the cutoff. Recover the alignment if this is the case;
         it is the last chance to do so */
      if (scores[j-1].path_score >= cutoff) {
         s_GetTraceback(program_number, traceback_array, 
                        A, B, b_size,
                        gap_open, gap_extend,
                        gap_align, scores[j-1].path_stop_i,
                        scores[j-1].path_stop_j, 
                        scores[j-1].path_score,
                        hsp_list, swapped, template_hsp,
                        score_params->options,
                        hit_params->options, start_shift);
      }
      traceback_row += b_size + 1;
   }

   /* finally, check the last row again for paths that
      have not decayed to zero */
   for (i = 0; i < b_size; i++) {
      if (scores[i].best && scores[i].path_score >= cutoff) {
         s_GetTraceback(program_number, traceback_array, 
                        A, B, b_size,
                        gap_open, gap_extend,
                        gap_align, scores[i].path_stop_i,
                        scores[i].path_stop_j, 
                        scores[i].path_score,
                        hsp_list, swapped, template_hsp,
                        score_params->options,
                        hit_params->options, start_shift);
      }
   }

   free(scores);
   free(traceback_array);
}


/* See blast_sw.h for details */
Int2 BLAST_SmithWatermanGetGappedScore (EBlastProgramType program_number, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        BLAST_SequenceBlk* subject, 
        BlastGapAlignStruct* gap_align,
        const BlastScoringParameters* score_params,
        const BlastExtensionParameters* ext_params,
        const BlastHitSavingParameters* hit_params,
        BlastInitHitList* init_hitlist,
        BlastHSPList** hsp_list_ptr, BlastGappedStats* gapped_stats,
        Boolean * fence_hit)
{
   Boolean is_prot;
   BlastHSPList* hsp_list = NULL;
   const BlastHitSavingOptions* hit_options = hit_params->options;
   Int4 cutoff_score = 0;
   Int4 context;
   Int4 **rpsblast_pssms = NULL;   /* Pointer to concatenated PSSMs in
                                       RPS-BLAST database */
   const int kHspNumMax = BlastHspNumMax(TRUE, hit_options);

   if (!query || !subject || !gap_align || !score_params || !ext_params ||
       !hit_params || !init_hitlist || !hsp_list_ptr)
      return 1;

   is_prot = (program_number != eBlastTypeBlastn &&
              program_number != eBlastTypePhiBlastn);

   if (Blast_ProgramIsRpsBlast(program_number)) {
      Int4 rps_context = subject->oid;
      rpsblast_pssms = gap_align->sbp->psi_matrix->pssm->data;
      if (program_number == eBlastTypeRpsTblastn) {
         rps_context = rps_context * NUM_FRAMES +
                      BLAST_FrameToContext(subject->frame, program_number);
      }
      /* only one cutoff applies to an RPS search */
      cutoff_score = hit_params->cutoffs[rps_context].cutoff_score;
   }

   if (*hsp_list_ptr == NULL)
      *hsp_list_ptr = hsp_list = Blast_HSPListNew(kHspNumMax);
   else 
      hsp_list = *hsp_list_ptr;

   /* ignore any initial ungapped alignments; just search
      all contexts against the subject sequence, one context
      at a time */
   for (context = query_info->first_context;
                context <= query_info->last_context; context++) {

      BlastContextInfo *curr_ctx = query_info->contexts + context;
      Int4 score;
      BlastHSP* new_hsp;

      if (!curr_ctx->is_valid)
         continue;

      if (rpsblast_pssms) {
         gap_align->sbp->psi_matrix->pssm->data = 
                                rpsblast_pssms + curr_ctx->query_offset;
      }
      else {
         cutoff_score = hit_params->cutoffs[context].cutoff_score;
      }

      if (is_prot) {
         score = s_SmithWatermanScoreOnly(
                              query->sequence + curr_ctx->query_offset,
                              curr_ctx->query_length,
                              subject->sequence,
                              subject->length,
                              score_params->gap_open,
                              score_params->gap_extend,
                              gap_align);
      }
      else {
         score = s_NuclSmithWaterman(subject->sequence,
                                     subject->length,
                                     query->sequence + curr_ctx->query_offset,
                                     curr_ctx->query_length,
                                     score_params->gap_open,
                                     score_params->gap_extend,
                                     gap_align);
      }

      if (score >= cutoff_score) {
         /* we know the score of the highest-scoring alignment but
            not its boundaries. Use a single placeholder HSP to carry
            score, context and frame information to the traceback phase */
         Blast_HSPInit(0, curr_ctx->query_length, 0, subject->length, 0, 0,
                       context, curr_ctx->frame, subject->frame, score,
                       NULL, &new_hsp);
         Blast_HSPListSaveHSP(hsp_list, new_hsp);
      }
   }   

   if (rpsblast_pssms)
       gap_align->sbp->psi_matrix->pssm->data = rpsblast_pssms;

   *hsp_list_ptr = hsp_list;
   return 0;
}
