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

File name: blast_engine.c

Author: Ilya Dondoshansky

Contents: High level BLAST functions

******************************************************************************/

static char const rcsid[] = "$Id$";

#include <algo/blast/core/blast_engine.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/aa_ungapped.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/phi_extend.h>
#include <algo/blast/core/link_hsps.h>

/** Deallocates all memory in BlastCoreAuxStruct */
static BlastCoreAuxStruct* 
BlastCoreAuxStructFree(BlastCoreAuxStruct* aux_struct)
{
   BlastExtendWordFree(aux_struct->ewp);
   BLAST_InitHitListFree(aux_struct->init_hitlist);
   Blast_HSPListFree(aux_struct->hsp_list);
   sfree(aux_struct->query_offsets);
   sfree(aux_struct->subject_offsets);
   
   sfree(aux_struct);
   return NULL;
}

/** Adjust HSP coordinates for out-of-frame gapped extension.
 * @param program One of blastx or tblastn [in]
 * @param init_hitlist List of hits after ungapped extension [in]
 * @param query_info Query information containing context offsets;
 *                   needed for blastx only [in]
 * @param subject_frame Frame of the subject sequence; tblastn only [in]
 * @param subject_length Length of the original nucleotide subject sequence;
 *                       tblastn only [in]
 * @param offset Shift in the subject sequence protein coordinates [in]
 */
static void TranslateHSPsToDNAPCoord(Uint1 program, 
        BlastInitHitList* init_hitlist, BlastQueryInfo* query_info,
        Int2 subject_frame, Int4 subject_length, Int4 offset)
{
   BlastInitHSP* init_hsp;
   Int4 index, context, frame;
   Int4* context_offsets = query_info->context_offsets;
   
   for (index = 0; index < init_hitlist->total; ++index) {
      init_hsp = &init_hitlist->init_hsp_array[index];

      if (program == blast_type_blastx) {
         context = 
            BSearchInt4(init_hsp->q_off, context_offsets,
                             query_info->last_context+1);
         frame = context % 3;
      
         init_hsp->q_off = 
            (init_hsp->q_off - context_offsets[context]) * CODON_LENGTH + 
            context_offsets[context-frame] + frame;
         init_hsp->ungapped_data->q_start = 
            (init_hsp->ungapped_data->q_start - context_offsets[context]) 
            * CODON_LENGTH + context_offsets[context-frame] + frame;
      } else {
         init_hsp->s_off += offset;
         init_hsp->ungapped_data->s_start += offset;
         if (subject_frame > 0) {
            init_hsp->s_off = 
               (init_hsp->s_off * CODON_LENGTH) + subject_frame - 1;
            init_hsp->ungapped_data->s_start = 
               (init_hsp->ungapped_data->s_start * CODON_LENGTH) + 
               subject_frame - 1;
         } else {
            init_hsp->s_off = (init_hsp->s_off * CODON_LENGTH) + 
               subject_length - subject_frame;
            init_hsp->ungapped_data->s_start = 
               (init_hsp->ungapped_data->s_start * CODON_LENGTH) + 
               subject_length - subject_frame;
         }
      }
   }
   return;
}

/** The core of the BLAST search: comparison between the (concatenated)
 * query against one subject sequence. Translation of the subject sequence
 * into 6 frames is done inside, if necessary. If subject sequence is 
 * too long, it can be split into several chunks. 
 */
static Int2
BLAST_SearchEngineCore(Uint1 program_number, BLAST_SequenceBlk* query, 
   BlastQueryInfo* query_info, BLAST_SequenceBlk* subject, 
   LookupTableWrap* lookup, BlastGapAlignStruct* gap_align, 
   BlastScoringParameters* score_params, 
   BlastInitialWordParameters* word_params, 
   BlastExtensionParameters* ext_params, 
   BlastHitSavingParameters* hit_params, 
   const BlastDatabaseOptions* db_options,
   BlastReturnStat* return_stats,
   BlastCoreAuxStruct* aux_struct,
   BlastHSPList** hsp_list_out)
{
   BlastInitHitList* init_hitlist = aux_struct->init_hitlist;
   BlastHSPList* hsp_list = aux_struct->hsp_list;
   Uint1* translation_buffer = NULL;
   Int4* frame_offsets = NULL;
   BlastHitSavingOptions* hit_options = hit_params->options;
   BlastScoringOptions* score_options = score_params->options;
   BlastHSPList* combined_hsp_list = NULL;
   Int2 status = 0;
   Int4 context, first_context, last_context;
   Int4 orig_length = subject->length;
   Uint1* orig_sequence = subject->sequence;
   Int4 **matrix;
   Int4 hsp_num_max;
   const Boolean k_translated_subject = (program_number == blast_type_tblastn
                         || program_number == blast_type_tblastx
                         || program_number == blast_type_rpstblastn);

   if (k_translated_subject) {
      first_context = 0;
      last_context = 5;
      if (score_options->is_ooframe) {
         BLAST_GetAllTranslations(orig_sequence, NCBI2NA_ENCODING,
            orig_length, db_options->gen_code_string, &translation_buffer,
            &frame_offsets, &subject->oof_sequence);
         subject->oof_sequence_allocated = TRUE;
      } else {
         BLAST_GetAllTranslations(orig_sequence, NCBI2NA_ENCODING,
            orig_length, db_options->gen_code_string, &translation_buffer,
            &frame_offsets, NULL);
      }
   } else if (program_number == blast_type_blastn) {
      first_context = 1;
      last_context = 1;
   } else {
      first_context = 0;
      last_context = 0;
   }

   *hsp_list_out = NULL;

   if (gap_align->positionBased)
      matrix = gap_align->sbp->posMatrix;
   else
      matrix = gap_align->sbp->matrix;

   hsp_num_max = (hit_options->hsp_num_max ? hit_options->hsp_num_max : INT4_MAX);

   /* Loop over frames of the subject sequence */
   for (context=first_context; context<=last_context; context++) {
      Int4 chunk; /* loop variable below. */
      Int4 num_chunks; /* loop variable below. */
      Int4 offset = 0; /* Used as offset into subject sequence (if chunked) */
      Int4 total_subject_length; /* Length of subject sequence used when split. */

      if (k_translated_subject) {
         subject->frame = BLAST_ContextToFrame(blast_type_blastx, context);
         subject->sequence = 
            translation_buffer + frame_offsets[context] + 1;
         subject->length = 
           frame_offsets[context+1] - frame_offsets[context] - 1;
      } else {
         subject->frame = context;
      }
     
      /* Split subject sequence into chunks if it is too long */
      num_chunks = (subject->length - DBSEQ_CHUNK_OVERLAP) / 
         (MAX_DBSEQ_LEN - DBSEQ_CHUNK_OVERLAP) + 1;
      total_subject_length = subject->length;
      
      for (chunk = 0; chunk < num_chunks; ++chunk) {
         if (chunk > 0) {
            offset += subject->length - DBSEQ_CHUNK_OVERLAP;
            if (program_number == blast_type_blastn) {
               subject->sequence += 
                  (subject->length - DBSEQ_CHUNK_OVERLAP)/COMPRESSION_RATIO;
            } else {
               subject->sequence += (subject->length - DBSEQ_CHUNK_OVERLAP);
            }
         }
         subject->length = MIN(total_subject_length - offset, 
                               MAX_DBSEQ_LEN);
         
         BlastInitHitListReset(init_hitlist);
         
         return_stats->db_hits +=
            aux_struct->WordFinder(subject, query, lookup, 
               matrix, word_params, aux_struct->ewp, aux_struct->query_offsets, 
               aux_struct->subject_offsets, GetOffsetArraySize(lookup), init_hitlist);
            
         if (init_hitlist->total == 0)
            continue;
         
         return_stats->init_extends += init_hitlist->total;
         
         if (score_options->gapped_calculation) {
            Int4 prot_length = 0;
            if (score_options->is_ooframe) {
               /* Convert query offsets in all HSPs into the mixed-frame  
                  coordinates */
               TranslateHSPsToDNAPCoord(program_number, init_hitlist, 
                  query_info, subject->frame, orig_length, offset);
               if (k_translated_subject) {
                  prot_length = subject->length;
                  subject->length = 2*orig_length + 1;
               }
            }
            /** NB: If queries are concatenated, HSP offsets must be adjusted
             * inside the following function call, so coordinates are
             * relative to the individual contexts (i.e. queries, strands or
             * frames). Contexts should also be filled in HSPs when they 
             * are saved.
            */
            aux_struct->GetGappedScore(program_number, query, query_info, 
               subject, gap_align, score_params, ext_params, hit_params, 
               init_hitlist, &hsp_list);
            if (score_options->is_ooframe && k_translated_subject)
               subject->length = prot_length;
         } else {
            BLAST_GetUngappedHSPList(init_hitlist, query_info, subject,
                                     hit_options, &hsp_list);
         }

         if (hsp_list->hspcnt == 0)
            continue;
         
         return_stats->good_init_extends += hsp_list->hspcnt;
         
         /* The subject ordinal id is not yet filled in this HSP list */
         hsp_list->oid = subject->oid;
         
         /* Assign frames in all HSPs. */
         Blast_HSPListSetFrames(program_number, hsp_list, 
                                score_options->is_ooframe);
         
         Blast_HSPListAdjustOffsets(hsp_list, offset);
         /* Allow merging of HSPs either if traceback is already 
            available, or if it is an ungapped search */
         Blast_HSPListsMerge(hsp_list, &combined_hsp_list, hsp_num_max, offset,
            (hsp_list->traceback_done || !score_options->gapped_calculation));
      } /* End loop on chunks of subject sequence */
      
      Blast_HSPListAppend(combined_hsp_list, hsp_list_out, hsp_num_max);
      combined_hsp_list = Blast_HSPListFree(combined_hsp_list);
   } /* End loop on frames */

   /* Restore the original contents of the subject block */
   subject->length = orig_length;
   subject->sequence = orig_sequence;

   hsp_list = *hsp_list_out;

   if (hit_params->do_sum_stats == TRUE) {
      status = BLAST_LinkHsps(program_number, hsp_list, query_info,
                              subject, gap_align->sbp, hit_params, 
                              score_options->gapped_calculation);
   } else if (hit_options->phi_align) {
      /* These e-values will not be accurate yet, since we don't know
         the number of pattern occurrencies in the database. That
         is arbitrarily set to 1 at this time. */
      Blast_HSPListPHIGetEvalues(hsp_list, gap_align->sbp);
   } else {
      /* Calculate e-values for all HSPs. Skip this step
         for RPS blast, since calculating the E values 
         requires precomputation that has not been done yet */
      if (program_number != blast_type_rpsblast &&
          program_number != blast_type_rpstblastn)
         status = Blast_HSPListGetEvalues(program_number, query_info, 
                     hsp_list, score_options->gapped_calculation, 
                     gap_align->sbp);
   }
   
   /* Discard HSPs that don't pass the e-value test */
   status = Blast_HSPListReapByEvalue(hsp_list, hit_options);
   
   if (translation_buffer) {
       sfree(translation_buffer);
   }
   if (frame_offsets) {
       sfree(frame_offsets);
   }

   return status;
}

static Int2 
FillReturnXDropoffsInfo(BlastReturnStat* return_stats, 
   BlastScoringParameters* score_params, 
   BlastInitialWordParameters* word_params, 
   BlastExtensionParameters* ext_params)
{
   /* since the cutoff score here will be used for display
      putposes, strip out any internal scaling of the scores */

   Int4 scale_factor = (Int4)score_params->scale_factor;

   if (!return_stats)
      return -1;

   return_stats->x_drop_ungapped = word_params->x_dropoff / scale_factor;
   return_stats->x_drop_gap = ext_params->gap_x_dropoff / scale_factor;
   return_stats->x_drop_gap_final = ext_params->gap_x_dropoff_final / 
                                                        scale_factor;
   return_stats->gap_trigger = ext_params->gap_trigger / scale_factor;

   return 0;
}

/** Setup of the auxiliary BLAST structures; 
 * also calculates internally used parameters from options. 
 * @param program_number blastn, blastp, blastx, etc. [in]
 * @param seq_src Sequence source information, with callbacks to get 
 *             sequences, their lengths, etc. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_options  used to calculate effective lengths. [in]
 * @param lookup_wrap Lookup table, already constructed. [in]
 * @param word_options options for initial word finding. [in]
 * @param ext_options options for gapped extension. [in]
 * @param hit_options options for saving hits. [in]
 * @param query The query sequence block [in]
 * @param query_info The query information block [in]
 * @param sbp Contains scoring information. [in]
 * @param gap_align Gapped alignment information and allocated memory [out]
 * @param score_params Parameters for scoring [out]
 * @param word_params Parameters for initial word processing [out]
 * @param ext_params Parameters for gapped extension [out]
 * @param hit_params Parameters for saving hits [out]
 * @param eff_len_params Parameters for calculating effective lengths [out]
 * @param aux_struct_ptr Placeholder joining various auxiliary memory 
 *                       structures [out]
 */
static Int2 
BLAST_SetUpAuxStructures(Uint1 program_number,
   const BlastSeqSrc* seq_src,
   const BlastScoringOptions* scoring_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   LookupTableWrap* lookup_wrap,	
   const BlastInitialWordOptions* word_options,
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingOptions* hit_options,
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
   BlastScoreBlk* sbp, 
   BlastGapAlignStruct** gap_align, 
   BlastScoringParameters** score_params,
   BlastInitialWordParameters** word_params,
   BlastExtensionParameters** ext_params,
   BlastHitSavingParameters** hit_params,
   BlastEffectiveLengthsParameters** eff_len_params,                         
   BlastCoreAuxStruct** aux_struct_ptr)
{
   Int2 status = 0;
   BlastCoreAuxStruct* aux_struct;
   Boolean blastp = (lookup_wrap->lut_type == AA_LOOKUP_TABLE ||
                         lookup_wrap->lut_type == RPS_LOOKUP_TABLE);
   Boolean mb_lookup = (lookup_wrap->lut_type == MB_LOOKUP_TABLE);
   Boolean phi_lookup = (lookup_wrap->lut_type == PHI_AA_LOOKUP ||
                         lookup_wrap->lut_type == PHI_NA_LOOKUP);
   Boolean ag_blast = (word_options->extension_method == eRightAndLeft);
   Int4 offset_array_size = GetOffsetArraySize(lookup_wrap);
   Uint4 avg_subj_length;

   ASSERT(seq_src);

   *aux_struct_ptr = aux_struct = (BlastCoreAuxStruct*)
      calloc(1, sizeof(BlastCoreAuxStruct));

   avg_subj_length = BLASTSeqSrcGetAvgSeqLen(seq_src);
     
   if ((status = BlastExtendWordNew(query->length, word_options, 
                    avg_subj_length, &aux_struct->ewp)) != 0)
      return status;

   if ((status = BLAST_GapAlignSetUp(program_number, seq_src, 
                    scoring_options, eff_len_options, ext_options, 
                    hit_options, query_info, sbp, score_params,
                    ext_params, hit_params, eff_len_params, gap_align)) != 0)
      return status;
      
   BlastInitialWordParametersNew(program_number, word_options, 
      *hit_params, *ext_params, sbp, query_info, 
      avg_subj_length, word_params);

   if (mb_lookup) {
      aux_struct->WordFinder = MB_WordFinder;
   } else if (phi_lookup) {
      aux_struct->WordFinder = PHIBlastWordFinder;
   } else if (blastp) {
      aux_struct->WordFinder = BlastAaWordFinder;
   } else if (ag_blast) {
      aux_struct->WordFinder = BlastNaWordFinder_AG;
   } else {
      aux_struct->WordFinder = BlastNaWordFinder;
   }
   
   aux_struct->query_offsets = 
      (Uint4*) malloc(offset_array_size * sizeof(Uint4));
   aux_struct->subject_offsets = 
      (Uint4*) malloc(offset_array_size * sizeof(Uint4));
   
   aux_struct->init_hitlist = BLAST_InitHitListNew();
   /* Pick which gapped alignment algorithm to use. */
   if (phi_lookup)
      aux_struct->GetGappedScore = PHIGetGappedScore;
   else if (ext_options->algorithm_type == EXTEND_DYN_PROG)
      aux_struct->GetGappedScore = BLAST_GetGappedScore;
   else
      aux_struct->GetGappedScore = BLAST_MbGetGappedScore;

   aux_struct->hsp_list = Blast_HSPListNew(hit_options->hsp_num_max);
   return status;
}

#define BLAST_DB_CHUNK_SIZE 1024

Int4 
BLAST_RPSSearchEngine(Uint1 program_number, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info,
   const BlastSeqSrc* seq_src,  BlastScoreBlk* sbp,
   const BlastScoringOptions* score_options, 
   LookupTableWrap* lookup_wrap,
   const BlastInitialWordOptions* word_options, 
   const BlastExtensionOptions* ext_options, 
   const BlastHitSavingOptions* hit_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   const PSIBlastOptions* psi_options, 
   const BlastDatabaseOptions* db_options,
   BlastHSPResults* results, BlastReturnStat* return_stats)
{
   BlastCoreAuxStruct* aux_struct = NULL;
   BlastHSPList* hsp_list;
   BlastScoringParameters* score_params = NULL;
   BlastInitialWordParameters* word_params = NULL;
   BlastExtensionParameters* ext_params = NULL;
   BlastHitSavingParameters* hit_params = NULL;
   BlastEffectiveLengthsParameters* eff_len_params = NULL;
   BlastGapAlignStruct* gap_align;
   Int2 status = 0;
   Int8 dbsize;
   Int4 num_db_seqs;
   Uint4 avg_subj_length = 0;
   RPSLookupTable *lookup = (RPSLookupTable *)lookup_wrap->lut;
   BlastQueryInfo concat_db_info;
   BLAST_SequenceBlk concat_db;
   RPSAuxInfo *rps_info;
   BlastHSPResults prelim_results;
   Uint1 *orig_query_seq = NULL;

   if (program_number != blast_type_rpsblast &&
       program_number != blast_type_rpstblastn)
      return -1;
   if (results->num_queries != 1)
      return -2;

   if ((status =
       BLAST_SetUpAuxStructures(program_number, seq_src,
          score_options, eff_len_options, lookup_wrap, word_options,
          ext_options, hit_options, query, query_info, sbp,
          &gap_align, &score_params, &word_params, &ext_params,
          &hit_params, &eff_len_params, &aux_struct)) != 0)
      return status;

   FillReturnXDropoffsInfo(return_stats, score_params, 
                           word_params, ext_params);

   /* modify scoring and gap alignment structures for
      use with RPS blast. */

   rps_info = lookup->rps_aux_info;
   gap_align->positionBased = TRUE;
   gap_align->sbp->posMatrix = lookup->rps_pssm;

   /* determine the total number of residues in the db.
      This figure must also include one trailing NULL for
      each DB sequence */

   num_db_seqs = BLASTSeqSrcGetNumSeqs(seq_src);
   dbsize = BLASTSeqSrcGetTotLen(seq_src) + num_db_seqs;
   if (dbsize > INT4_MAX)
      return -3;

   /* If this is a translated search, pack the query into
      ncbi2na format (exclude the starting sentinel); otherwise 
      just use the input query */

   if (program_number == blast_type_rpstblastn) {
       orig_query_seq = query->sequence_start;
       query->sequence_start++;
       if (BLAST_PackDNA(query->sequence_start, query->length,
                         NCBI4NA_ENCODING, &query->sequence) != 0)
           return -4;
   }

   /* Concatenate all of the DB sequences together, and pretend
      this is a large multiplexed sequence. Note that because the
      scoring is position-specific, the actual sequence data is
      not needed */

   memset(&concat_db, 0, sizeof(concat_db)); /* fill in SequenceBlk */
   concat_db.length = (Int4) dbsize;

   memset(&concat_db_info, 0, sizeof(concat_db_info)); /* fill in QueryInfo */
   concat_db_info.num_queries = num_db_seqs;
   concat_db_info.first_context = 0;
   concat_db_info.last_context = num_db_seqs - 1;
   concat_db_info.context_offsets = lookup->rps_seq_offsets;

   prelim_results.num_queries = num_db_seqs;
   prelim_results.hitlist_array = (BlastHitList **)calloc(num_db_seqs,
                                             sizeof(BlastHitList *));

   /* Change the table of diagonals that will be used for the
      search; we need a diag table that can fit the entire
      concatenated DB */
   avg_subj_length = (Uint4) (dbsize / num_db_seqs);
   BlastExtendWordFree(aux_struct->ewp);
   BlastExtendWordNew(concat_db.length, word_options, 
			avg_subj_length, &aux_struct->ewp);

   /* Run the search; the input query is what gets scanned
      and the concatenated DB is the sequence associated with
      the score matrix. This essentially means that 'query'
      and 'subject' have opposite conventions for the search. 
    
      Note that while scores can be calculated for any alignment
      found, we have not set up any Karlin parameters or effective
      search space sizes for the concatenated DB. This means that
      E-values cannot be calculated after hits are found. */

   BLAST_SearchEngineCore(program_number, &concat_db, &concat_db_info, 
         query, lookup_wrap, gap_align, score_params, 
         word_params, ext_params, hit_params, db_options, 
         return_stats, aux_struct, &hsp_list);

   /* save the resulting list of HSPs. 'query' and 'subject' are
      still reversed */

   if (hsp_list && hsp_list->hspcnt > 0) {
      return_stats->prelim_gap_passed += hsp_list->hspcnt;
      /* Save the HSPs into a hit list */
      Blast_HSPResultsSaveHitList(program_number, &prelim_results, hsp_list, hit_params);
   }

   /* Change the results from a single hsplist with many 
      contexts to many hsplists each with a single context.
      'query' and 'subject' offsets are still reversed. */

   Blast_HSPResultsRPSUpdate(results, &prelim_results);

   /* for a translated search, throw away the packed version
      of the query and replace with the original (excluding the
      starting sentinel) */

   if (program_number == blast_type_rpstblastn) {
       sfree(query->sequence);
       query->sequence = query->sequence_start;
   }

   /* Do the traceback. After this call, query and 
      subject have reverted to their traditional meanings. */

   BLAST_RPSTraceback(program_number, results, &concat_db, 
            &concat_db_info, query, query_info, gap_align, 
            score_params, ext_params, hit_params, db_options, 
            rps_info->karlin_k);

   /* The traceback calculated the E values, so it's safe
      to sort the results now */

   Blast_HSPResultsSortByEvalue(results);

   /* free the internal structures used */
   /* Do not destruct score block here */

   if (program_number == blast_type_rpstblastn) {
      query->sequence_start = orig_query_seq;
      query->sequence = orig_query_seq + 1;
   }
   sfree(prelim_results.hitlist_array);
   gap_align->sbp->posMatrix = NULL;
   gap_align->positionBased = FALSE;
   gap_align->sbp = NULL;
   BLAST_GapAlignStructFree(gap_align);
   BlastCoreAuxStructFree(aux_struct);
   sfree(score_params);
   sfree(hit_params);
   sfree(ext_params);
   sfree(word_params);
   return status;
}

Int4 
BLAST_SearchEngine(Uint1 program_number, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info,
   const BlastSeqSrc* seq_src,  BlastScoreBlk* sbp,
   const BlastScoringOptions* score_options, 
   LookupTableWrap* lookup_wrap,
   const BlastInitialWordOptions* word_options, 
   const BlastExtensionOptions* ext_options, 
   const BlastHitSavingOptions* hit_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   const PSIBlastOptions* psi_options, 
   const BlastDatabaseOptions* db_options,
   BlastHSPResults* results, BlastReturnStat* return_stats)
{
   BlastCoreAuxStruct* aux_struct = NULL;
   BlastHSPList* hsp_list; 
   BlastScoringParameters* score_params = NULL;
   BlastInitialWordParameters* word_params = NULL;
   BlastExtensionParameters* ext_params = NULL;
   BlastHitSavingParameters* hit_params = NULL;
   BlastEffectiveLengthsParameters* eff_len_params = NULL;
   BlastGapAlignStruct* gap_align;
   GetSeqArg seq_arg;
   Int2 status = 0;
   BlastSeqSrcIterator* itr = NULL;
   Int8 db_length = 0;
   
   if ((status = 
       BLAST_SetUpAuxStructures(program_number, seq_src,
          score_options, eff_len_options, lookup_wrap, word_options, 
          ext_options, hit_options, query, query_info, sbp, 
          &gap_align, &score_params, &word_params, &ext_params, 
          &hit_params, &eff_len_params, &aux_struct)) != 0)
      return status;

   FillReturnXDropoffsInfo(return_stats, score_params,
                           word_params, ext_params);
   memset((void*) &seq_arg, 0, sizeof(seq_arg));

   /* Encoding is set so there are no sentinel bytes, and protein/nucleotide
      sequences are retieved in ncbistdaa/ncbi2na encodings respectively. */
   seq_arg.encoding = BLASTP_ENCODING; 

   /* Initialize the iterator */
   if ( !(itr = BlastSeqSrcIteratorNew(BLAST_DB_CHUNK_SIZE))) {
      /** NEED AN ERROR MESSAGE HERE? */
      return -1;
   }
   
   db_length = BLASTSeqSrcGetTotLen(seq_src);

   /* iterate over all subject sequences */
   while ( (seq_arg.oid = BlastSeqSrcIteratorNext(seq_src, itr)) 
           != BLAST_SEQSRC_EOF) {
      BlastSequenceBlkClean(seq_arg.seq);
      if (BLASTSeqSrcGetSequence(seq_src, (void*) &seq_arg) < 0)
          continue;

      if (db_length == 0) {
         /* This is not a database search, hence need to recalculate and save
            the effective search spaces and length adjustments for all 
            queries based on the length of the current single subject 
            sequence. */
         if ((status = BLAST_OneSubjectUpdateParameters(program_number, 
                          seq_arg.seq->length, score_options, query_info, 
                          sbp, ext_params, hit_params, word_params, 
                          eff_len_params)) != 0)
            return status;
      }

      /* Calculate cutoff scores for linking HSPs */
      if (hit_params->do_sum_stats) {
         CalculateLinkHSPCutoffs(program_number, query_info, sbp, 
                                 hit_params, ext_params, db_length, seq_arg.seq->length); 
      }

      BLAST_SearchEngineCore(program_number, query, query_info,
         seq_arg.seq, lookup_wrap, gap_align, score_params, word_params, 
         ext_params, hit_params, db_options, return_stats, aux_struct, 
         &hsp_list);
 
      if (hsp_list && hsp_list->hspcnt > 0) {
         return_stats->prelim_gap_passed += hsp_list->hspcnt;
         if (program_number == blast_type_blastn) {
            status = 
               Blast_HSPListReevaluateWithAmbiguities(hsp_list, query, 
                  seq_arg.seq, hit_options, query_info, sbp, score_params, 
                  seq_src);
         }
         /* Save the HSPs into a hit list */
         Blast_HSPResultsSaveHitList(program_number, results, hsp_list, hit_params);
      }
         /*BlastSequenceBlkClean(subject);*/
   }
   
   itr = BlastSeqSrcIteratorFree(itr);
   BlastSequenceBlkFree(seq_arg.seq);

   if (hit_options->phi_align) {
      /* Save the product of effective occurrencies of pattern in query and
         occurrencies of pattern in database */
      gap_align->sbp->effective_search_sp *= return_stats->db_hits;
   }

   /* Now sort the hit lists for all queries, but only if this is a database
      search. */
   if (db_length > 0)
      Blast_HSPResultsSortByEvalue(results);

   if (!ext_options->skip_traceback && score_options->gapped_calculation) {
      status = 
         BLAST_ComputeTraceback(program_number, results, query, query_info,
            seq_src, gap_align, score_params, ext_params, hit_params,
            eff_len_params, db_options, psi_options);
   }

   /* Do not destruct score block here */
   gap_align->sbp = NULL;
   BLAST_GapAlignStructFree(gap_align);
   BlastCoreAuxStructFree(aux_struct);

   sfree(score_params);
   sfree(hit_params);
   sfree(ext_params);
   sfree(word_params);
   sfree(eff_len_params);

   return status;
}
