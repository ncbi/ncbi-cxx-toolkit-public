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

#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/link_hsps.h>

static char const rcsid[] = "$Id$";

/** Comparison function for sorting HSPs by score. 
 * Ties are broken based on subject sequence offsets.
 */
static int
score_compare_hsps(const void* v1, const void* v2)
{
   BlastHSP* h1,* h2;
   BlastHSP** hp1,** hp2;
   
   hp1 = (BlastHSP**) v1;
   hp2 = (BlastHSP**) v2;
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

static void BLASTCheckHSPInclusion(BlastHSP* *hsp_array, Int4 hspcnt, 
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
BLAST_CheckStartForGappedAlignment (BlastHSP* hsp, Uint1* query, 
                                    Uint1* subject, BlastScoreBlk* sbp)
{
   Int4 index1, score, start, end, width;
   Uint1* query_var,* subject_var;
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
BLAST_GetStartForGappedAlignment (BlastHSP* hsp, Uint1* query, 
                                  Uint1* subject, BlastScoreBlk* sbp)
{
   Int4 index1, max_offset, score, max_score, hsp_end;
   Uint1* query_var,* subject_var;
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
BlastHSPGetNumIdentical(Uint1* query, Uint1* subject, BlastHSP* hsp,
   Boolean gapped_calculation, Int4* num_ident_ptr, Int4* align_length_ptr)
{
   Int4 i, num_ident, align_length, q_off, s_off;
   Uint1* q,* s;
   GapEditBlock* gap_info;
   GapEditScript* esp;

   gap_info = hsp->gap_info;

   if (!gap_info && gapped_calculation)
      return -1;

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
BlastOOFGetNumIdentical(Uint1* query_seq, Uint1* subject_seq, 
   BlastHSP* hsp, Uint1 program, 
   Int4* num_ident_ptr, Int4* align_length_ptr)
{
   Int4 i, num_ident, align_length;
   Uint1* q,* s;
   GapEditScript* esp;

   if (!hsp->gap_info || !subject_seq || !query_seq)
      return -1;


   if (program == blast_type_tblastn) {
       q = &query_seq[hsp->query.offset];
       s = &subject_seq[hsp->subject.offset];
   } else {
       s = &query_seq[hsp->query.offset];
       q = &subject_seq[hsp->subject.offset];
   }
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

static Int4 GetPatternLengthFromBlastHSP(BlastHSP* hsp)
{
   return hsp->query.end_trim;
}

static void SavePatternLengthInGapAlignStruct(Int4 length,
               BlastGapAlignStruct* gap_align)
{
   /* Kludge: save length in an output structure member, to avoid introducing 
      a new structure member. Probably should be changed??? */
   gap_align->query_stop = length;
}


#define MAX_FULL_TRANSLATION 2100

/** Compute gapped alignment with traceback for all HSPs from a single
 * query/subject sequence pair. 
 * Final e-values are calculated here, except when sum statistics is used,
 * in which case this is done in file link_hsps.c:
 * @sa { BLAST_LinkHsps }
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
BlastHSPListGetTraceback(Uint1 program_number, BlastHSPList* hsp_list, 
   BLAST_SequenceBlk* query_blk, BLAST_SequenceBlk* subject_blk, 
   BlastQueryInfo* query_info,
   BlastGapAlignStruct* gap_align, BlastScoreBlk* sbp, 
   const BlastScoringOptions* score_options,
   const BlastExtensionOptions* ext_options,
   BlastHitSavingParameters* hit_params,
   const BlastDatabaseOptions* db_options,
   const PSIBlastOptions* psi_options)
{
   Int4 index, index1, index2;
   Boolean hsp_start_is_contained, hsp_end_is_contained;
   BlastHSP* hsp,* hsp1=NULL,* hsp2;
   Uint1* query,* subject;
   Int4 query_length, query_length_orig;
   Int4 subject_length=0;
   BlastHSP** hsp_array;
   Int4 q_start, s_start, max_offset;
   Boolean keep;
   BlastHitSavingOptions* hit_options = hit_params->options;
   Int4 min_score_to_keep = hit_params->cutoff_score;
   Int4 align_length;
   double scalingFactor = ((psi_options) ? psi_options->scalingFactor : 1.0);
   Int4 new_hspcnt = 0;
   Boolean is_ooframe = score_options->is_ooframe;
   Int4 context_offset;
   Boolean translate_subject = 
      (program_number == blast_type_tblastn ||
       program_number == blast_type_psitblastn);   
   Uint1* translation_buffer = NULL;
   Int4* frame_offsets = NULL;
   Uint1* nucl_sequence = NULL;
   BLAST_KarlinBlk** kbp;
   Boolean phi_align = (hit_options->phi_align);
   Boolean greedy_traceback;
   Boolean partial_translation = FALSE;
   Int4 start_shift = 0, translation_length;
   Uint1* oof_sequence = NULL; /* Needed when partial translations are done */

   if (hsp_list->hspcnt == 0) {
      return 0;
   }

   hsp_array = hsp_list->hsp_array;
   
   if (translate_subject) {
      if (!db_options)
         return -1;
      partial_translation = (subject_blk->length > MAX_FULL_TRANSLATION);
      
      if (is_ooframe && subject_blk->oof_sequence) {
         /* If mixed-frame sequence is already available, then no need 
            to translate again */
         partial_translation = FALSE;
         subject = subject_blk->oof_sequence + CODON_LENGTH;
         /* Mixed-frame sequence spans all 6 frames, i.e. both strands
            of the nucleotide sequence. However its start will also be 
            shifted by 3.*/
         subject_length = 2*subject_blk->length - 1;
      } else {
         nucl_sequence = subject_blk->sequence_start;
         if (!partial_translation) {
            if (is_ooframe) {
               BLAST_GetAllTranslations(nucl_sequence, NCBI4NA_ENCODING,
                  subject_blk->length, db_options->gen_code_string,
                  &translation_buffer, &frame_offsets, 
                  &subject_blk->oof_sequence);
               subject_length = 2*subject_blk->length - 1;
            } else {
               BLAST_GetAllTranslations(nucl_sequence, NCBI4NA_ENCODING,
                  subject_blk->length, db_options->gen_code_string, 
                  &translation_buffer, &frame_offsets, NULL);
            }
         }
      }
   } else {
      /* Subject is not translated */
      subject = subject_blk->sequence;
      subject_length = subject_blk->length;
   }

   for (index=0; index < hsp_list->hspcnt; index++) {
      hsp_start_is_contained = FALSE;
      hsp_end_is_contained = FALSE;
      hsp = hsp_array[index];
      if (program_number == blast_type_blastx || 
          program_number == blast_type_tblastx) {
         Int4 context = hsp->context - hsp->context % 3;
         context_offset = query_info->context_offsets[context];
         query_length_orig = 
            query_info->context_offsets[context+3] - context_offset - 1;
         if (is_ooframe) {
            query = query_blk->oof_sequence + CODON_LENGTH + context_offset;
            query_length = query_length_orig;
         } else {
            query = query_blk->sequence + 
               query_info->context_offsets[hsp->context];
            query_length = BLAST_GetQueryLength(query_info, hsp->context);
         }
      } else {
         query = query_blk->sequence + 
            query_info->context_offsets[hsp->context];
         query_length = query_length_orig = 
            BLAST_GetQueryLength(query_info, hsp->context);
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
      
      if ((hsp_start_is_contained == FALSE || 
           hsp_end_is_contained == FALSE || hsp->score > hsp1->score)) {

         if (translate_subject && partial_translation) {
            Int4 nucl_shift;
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
                  nucl_shift = subject_blk->length - start_shift 
                     - translation_length;
               }
               GetPartialTranslation(nucl_sequence+nucl_shift, 
                  translation_length, hsp->subject.frame,
                  db_options->gen_code_string, &translation_buffer, 
                  &subject_length, NULL);
               /* Below, the start_shift will be used for the protein
                  coordinates, so need to divide it by 3 */
               start_shift /= CODON_LENGTH;
            } else {
               Int4 oof_start, oof_end;
               sfree(oof_sequence);
               if (hsp->subject.frame > 0) {
                  oof_start = 0;
                  oof_end = subject_blk->length;
               } else {
                  oof_start = subject_blk->length + 1;
                  oof_end = 2*subject_blk->length + 1;
               }

               start_shift = 
                  MAX(oof_start, hsp->subject.offset - MAX_FULL_TRANSLATION);
               translation_length =
                  MIN(hsp->subject.end + MAX_FULL_TRANSLATION, 
                      oof_end) - start_shift;
               if (hsp->subject.frame > 0) {
                  nucl_shift = start_shift - oof_start;
               } else {
                  nucl_shift = oof_end - start_shift - translation_length;
               }
               GetPartialTranslation(nucl_sequence+nucl_shift, 
                  translation_length, hsp->subject.frame, 
                  db_options->gen_code_string, NULL, 
                  &subject_length, &oof_sequence);
            }

            hsp->subject.offset -= start_shift;
            hsp->subject.gapped_start -= start_shift;
         }

         if (translate_subject) {
            if (!is_ooframe) {
               if (!partial_translation) {
                  Int2 context = FrameToContext(hsp->subject.frame);
                  subject = translation_buffer + frame_offsets[context] + 1;
                  subject_length = 
                     frame_offsets[context+1] - frame_offsets[context] - 1;
               } else {
                  subject = translation_buffer + 1;
                  /* Subject length is already assigned in call to 
                     GetPartialTranslation. */
               }
            } else if (partial_translation) {
               subject = oof_sequence + CODON_LENGTH;
            }
         }

         if (!phi_align && !is_ooframe && 
             (((hsp->query.gapped_start == 0 && 
                hsp->subject.gapped_start == 0) ||
               !BLAST_CheckStartForGappedAlignment(hsp, query, 
                   subject, sbp)))) {
            max_offset = BLAST_GetStartForGappedAlignment(hsp, query, 
                            subject, sbp);
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
         
         greedy_traceback = (ext_options->algorithm_type ==
                             EXTEND_GREEDY_NO_TRACEBACK);

         /* Perform the gapped extension with traceback */
         if (phi_align) {
            Int4 pat_length = GetPatternLengthFromBlastHSP(hsp);
            SavePatternLengthInGapAlignStruct(pat_length, gap_align);
            PHIGappedAlignmentWithTraceback(program_number, query, subject,
               gap_align, score_options, q_start, s_start, query_length, 
               subject_length);
         } else if (greedy_traceback) {
            BLAST_GreedyGappedAlignment(query, subject, 
                 query_length, subject_length, gap_align, 
                 score_options, q_start, s_start, FALSE, TRUE);
         } else {
            BLAST_GappedAlignmentWithTraceback(program_number, query, subject, 
               gap_align, score_options, q_start, s_start, 
               query_length, subject_length);
         }

         if (gap_align->score >= min_score_to_keep) {
            hsp->subject.offset = gap_align->subject_start;
            hsp->query.offset = gap_align->query_start;
            hsp->subject.end = gap_align->subject_stop;
            hsp->query.end = gap_align->query_stop;
            
            hsp->query.length = hsp->query.end - hsp->query.offset;
            hsp->subject.length = hsp->subject.end - hsp->subject.offset;
            hsp->score = gap_align->score;
            
            hsp->gap_info = gap_align->edit_block;
            gap_align->edit_block = NULL;

            if (greedy_traceback) {
               ReevaluateHSPWithAmbiguities(hsp, query, subject, hit_options,
                                            score_options, query_info, sbp);
            }
            
            if (hsp->gap_info) {
               hsp->gap_info->frame1 = hsp->query.frame;
               hsp->gap_info->frame2 = hsp->subject.frame;
               hsp->gap_info->original_length1 = query_length_orig;
               hsp->gap_info->original_length2 = subject_blk->length;
               if (program_number == blast_type_blastx)
                  hsp->gap_info->translate1 = TRUE;
               if (program_number == blast_type_tblastn)
                  hsp->gap_info->translate2 = TRUE;
            }

            keep = TRUE;
            if (is_ooframe) {
               BlastOOFGetNumIdentical(query, subject, hsp, program_number,
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

            if (start_shift > 0) {
               hsp->subject.offset += start_shift;
               hsp->subject.end += start_shift;
               hsp->subject.gapped_start += start_shift;
               if (hsp->gap_info)
                  hsp->gap_info->start2 += start_shift;
            }

            if (hsp->num_ident * 100 < 
                align_length * hit_options->percent_identity) {
               keep = FALSE;
            }
            
            if (program_number == blast_type_blastp ||
                program_number == blast_type_blastn) {
               if (score_options->gapped_calculation && 
                   program_number != blast_type_blastn)
                  kbp = sbp->kbp_gap;
               else
                  kbp = sbp->kbp;

               if (hit_options->phi_align) {
                  hsp->evalue = PHIScoreToEvalue(hsp->score, sbp);
               } else {
                  hsp->evalue = 
                     BLAST_KarlinStoE_simple(hsp->score, kbp[hsp->context],
                     (double)query_info->eff_searchsp_array[hsp->context]);
               }
               if (hsp->evalue > hit_options->expect_value) 
                  /* put in for comp. based stats. */
                  keep = FALSE;
            }

            if (scalingFactor != 0.0 && scalingFactor != 1.0) {
               /* Scale down score for blastp and tblastn. */
               hsp->score = (Int4) ((hsp->score+(0.5*scalingFactor))/scalingFactor);
            }
            /* only one alignment considered for blast[np]. */
            /* This may be changed by LinkHsps for blastx or tblastn. */
            hsp->num = 1;
            if ((program_number == blast_type_tblastn ||
                 program_number == blast_type_psitblastn) && 
                hit_options->longest_intron > 0) {
               hsp->evalue = 
                  BLAST_KarlinStoE_simple(hsp->score, 
                     sbp->kbp_gap[hsp->context],
                     (double) query_info->eff_searchsp_array[hsp->context]);
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

    if (translation_buffer) {
        sfree(translation_buffer);
    }
    if (frame_offsets) {
        sfree(frame_offsets);
    }
    /* Now try to detect simular alignments */

    BLASTCheckHSPInclusion(hsp_array, hsp_list->hspcnt, is_ooframe);
    
    /* Make up fake hitlist, relink and rereap. */

    if (program_number == blast_type_blastx ||
        program_number == blast_type_tblastn ||
        program_number == blast_type_psitblastn) {
        hsp_list->hspcnt = BlastHSPArrayPurge(hsp_array, hsp_list->hspcnt);
        
        if (hit_options->do_sum_stats == TRUE) {
           BLAST_LinkHsps(program_number, hsp_list, query_info, subject_blk,
                         sbp, hit_params, score_options->gapped_calculation);
        } else if (hit_options->phi_align) {
           PHIGetEvalue(hsp_list, sbp);
        } else {
           BLAST_GetNonSumStatsEvalue(program_number, query_info, hsp_list, 
                                      score_options, sbp);
        }
        
        BLAST_ReapHitlistByEvalue(hsp_list, hit_options);
    }
    
    new_hspcnt = BlastHSPArrayPurge(hsp_array, hsp_list->hspcnt);
    
    qsort(hsp_array,new_hspcnt,sizeof(BlastHSP*), score_compare_hsps);
    
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

Int2 BLAST_ComputeTraceback(Uint1 program_number, BlastHSPResults* results, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        const BlastSeqSrc* bssp, BlastGapAlignStruct* gap_align,
        const BlastScoringOptions* score_options,
        const BlastExtensionParameters* ext_params,
        BlastHitSavingParameters* hit_params,
        const BlastDatabaseOptions* db_options,
        const PSIBlastOptions* psi_options)
{
   Int2 status = 0;
   Int4 query_index, subject_index;
   BlastHitList* hit_list;
   BlastHSPList* hsp_list;
   BlastScoreBlk* sbp;
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
   sfree(gap_align->dyn_prog);

   sbp = gap_align->sbp;
   
   encoding = GetTracebackEncoding(program_number);
   memset((void*) &seq_arg, 0, sizeof(seq_arg));

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
               ext_params->options, hit_params, db_options, psi_options);
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
        BlastHSPResults* results, 
        BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
        BLAST_SequenceBlk* subject, 
        BlastGapAlignStruct* gap_align,
        const BlastScoringOptions* score_options,
        BlastExtensionParameters* ext_params,
        BlastHitSavingParameters* hit_params,
        const BlastDatabaseOptions* db_options)
{
   Int2 status = 0;
   Int4 query_index;
   BlastHitList* hit_list;
   BlastHSPList* hsp_list;
   BlastScoreBlk* sbp;
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
   sfree(gap_align->dyn_prog);

   sbp = gap_align->sbp;

   if (db_is_na) {
      /* Two sequences case: free the compressed sequence */
      sfree(subject->sequence);
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
            query_info, gap_align, sbp, score_options, ext_params->options, 
            hit_params, db_options, NULL);
      }
   }

   return status;
}
