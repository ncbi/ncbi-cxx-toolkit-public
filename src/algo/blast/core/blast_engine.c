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

/** @file blast_engine.c
 * Function calls to actually perform a BLAST search (high level).
 * The hierarchy of function calls, starting from
 * the top level in the BLAST core, is described below.
 * <pre>
 * Preliminary stage of the BLAST search:
 *
 *    Blast_RunPreliminarySearch 
 *        BLAST_GapAlignSetUp
 *        BLAST_PreliminarySearchEngine
 *            if (RPS BLAST) {
 *                s_RPSPreliminarySearchEngine
                      s_BlastSearchEngineCore
 *            } else {
 *                for (all sequences in the database) 
 *                    s_BlastSearchEngineCore
 *            }
 * 
 * Full BLAST search, including preliminary search and traceback:
 *
 *    Blast_RunFullSearch 
 *        BLAST_GapAlignSetUp
 *        BLAST_PreliminarySearchEngine
 *        BLAST_ComputeTraceback
 *
 * </pre>
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_engine.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/aa_ungapped.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_traceback.h>
#include <algo/blast/core/phi_extend.h>
#include <algo/blast/core/link_hsps.h>
#include "blast_gapalign_priv.h"

/** Structure to be passed to s_BlastSearchEngineCore, containing pointers 
    to various preallocated structures and arrays. */
typedef struct BlastCoreAuxStruct {

   Blast_ExtendWord* ewp; /**< Structure for keeping track of diagonal
                               information for initial word matches */
   BlastWordFinderType WordFinder; /**< Word finder function pointer */
   BlastGetGappedScoreType GetGappedScore; /**< Gapped extension function
                                              pointer */
   BlastInitHitList* init_hitlist; /**< Placeholder for HSPs after 
                                        ungapped extension */
   BlastHSPList* hsp_list; /**< Placeholder for HSPs after gapped 
                                extension */
   Uint4* query_offsets; /**< Placeholder for initial word match query 
                              offsets */
   Uint4* subject_offsets; /**< Placeholder for initial word match  
                              subject offsets */
   Uint1* translation_buffer; /**< Placeholder for translated subject
                                   sequences */
   Uint1* translation_table; /**< Translation table for forward strand */
   Uint1* translation_table_rc; /**< Translation table for reverse 
                                     strand */
} BlastCoreAuxStruct;

/** Deallocates all memory in BlastCoreAuxStruct */
static BlastCoreAuxStruct* 
s_BlastCoreAuxStructFree(BlastCoreAuxStruct* aux_struct)
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
static void 
s_TranslateHSPsToDNAPCoord(EBlastProgramType program, 
                           BlastInitHitList* init_hitlist, 
                           BlastQueryInfo* query_info,
                           Int2 subject_frame, Int4 subject_length, 
                           Int4 offset)
{
    BlastInitHSP * init_hsp = 0;
    Int4 index = 0;

    for(index = 0; index < init_hitlist->total; ++index) {
        BlastContextInfo * contexts = query_info->contexts;
        init_hsp = &init_hitlist->init_hsp_array[index];
        
        if (program == eBlastTypeBlastx) {
            Int4 context_idx    = 0; /* Index of this HSP's context */
            Int4 frame_idx      = 0; /* Index of this frame within set of
                                        frames with same query and sign */
            Int4 init_frame_idx = 0; /* First frame of this query */
            Int4 frame_pos      = 0; /* Start of this frame in DNA */
            
            /* Find context containing this HSP */
            context_idx = BSearchContextInfo(init_hsp->q_off, query_info);
            
            frame_idx = context_idx % 3;
            init_frame_idx = context_idx - frame_idx;
            
            frame_pos = contexts[init_frame_idx].query_offset + frame_idx;
            
            init_hsp->q_off =
                (init_hsp->q_off -
                 contexts[context_idx].query_offset) * CODON_LENGTH + frame_pos;
            
            init_hsp->ungapped_data->q_start =
                (init_hsp->ungapped_data->q_start -
                 contexts[context_idx].query_offset) * CODON_LENGTH + frame_pos;
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
    Blast_InitHitListSortByScore(init_hitlist);
}

/** Set up context offsets for the auxiliary BlastQueryInfo structure that is
 * created for the concatenated database in RPS BLAST search. Calls the public 
 * function OffsetArrayToContextOffsets with a blastp program, because subjects
 * are protein sequences. This guarantees that all frames are set to 0.
 * @param info The preallocated structure [in] [out]
 * @param new_offsets The array context offsets to fill [in]
 */
static void
s_RPSOffsetArrayToContextOffsets(BlastQueryInfo    * info,
                                 Int4              * new_offsets)
{
   const EBlastProgramType kProgram = eBlastTypeBlastp;
   OffsetArrayToContextOffsets(info, new_offsets, kProgram);
}

/** The core of the BLAST search: comparison between the (concatenated)
 * query against one subject sequence. Translation of the subject sequence
 * into 6 frames is done inside, if necessary. If subject sequence is 
 * too long, it can be split into several chunks. 
 * @param program_number BLAST program type [in]
 * @param query Query sequence structure [in]
 * @param query_info_in Query information [in]
 * @param subject Subject sequence structure [in]
 * @param lookup Lookup table [in]
 * @param gap_align Structure for gapped alignment information [in]
 * @param score_params Scoring parameters [in]
 * @param word_params Initial word finding and ungapped extension 
 *                    parameters [in]
 * @param ext_params Gapped extension parameters [in]
 * @param hit_params Hit saving parameters [in]
 * @param db_options Database options [in]
 * @param diagnostics Hit counts and other diagnostics [in] [out]
 * @param aux_struct Structure containing different auxiliary data and memory
 *                   for the preliminary stage of the BLAST search [in]
 * @param hsp_list_out List of HSPs found for a given subject sequence [in]
 */
static Int2
s_BlastSearchEngineCore(EBlastProgramType program_number, BLAST_SequenceBlk* query, 
   BlastQueryInfo* query_info_in, BLAST_SequenceBlk* subject, 
   LookupTableWrap* lookup, BlastGapAlignStruct* gap_align, 
   const BlastScoringParameters* score_params, 
   const BlastInitialWordParameters* word_params, 
   const BlastExtensionParameters* ext_params, 
   const BlastHitSavingParameters* hit_params, 
   const BlastDatabaseOptions* db_options,
   BlastDiagnostics* diagnostics,
   BlastCoreAuxStruct* aux_struct,
   BlastHSPList** hsp_list_out)
{
   BlastInitHitList* init_hitlist = aux_struct->init_hitlist;
   BlastHSPList* hsp_list = aux_struct->hsp_list;
   Uint1* translation_buffer = NULL;
   Int4* frame_offsets   = NULL;
   Int4* frame_offsets_a = NULL; /* Will be freed if non-null */
   BlastHitSavingOptions* hit_options = hit_params->options;
   BlastScoringOptions* score_options = score_params->options;
   BlastHSPList* combined_hsp_list = NULL;
   Int2 status = 0;
   Uint4 context, first_context, last_context;
   Int4 orig_length = subject->length;
   Uint1* orig_sequence = subject->sequence;
   Int4 **matrix;
   Int4 hsp_num_max;
   BlastUngappedStats* ungapped_stats = NULL;
   BlastGappedStats* gapped_stats = NULL;
   BlastQueryInfo* query_info = query_info_in;
   Boolean prelim_traceback = 
      (ext_params->options->ePrelimGapExt == eGreedyWithTracebackExt);

   const Boolean kTranslatedSubject = (program_number == eBlastTypeTblastn
                         || program_number == eBlastTypeTblastx
                         || program_number == eBlastTypeRpsTblastn);

   if (kTranslatedSubject) {
      first_context = 0;
      last_context = 5;
      if (score_options->is_ooframe) {
         BLAST_GetAllTranslations(orig_sequence, NCBI2NA_ENCODING,
            orig_length, db_options->gen_code_string, &translation_buffer,
            &frame_offsets, &subject->oof_sequence);
         subject->oof_sequence_allocated = TRUE;
         frame_offsets_a = frame_offsets;
      } else if (program_number == eBlastTypeRpsTblastn ) {
          /* For RPS tblastn, subject is actually query, which has already 
             been translated during the setup stage. */
          translation_buffer = orig_sequence - 1;
          frame_offsets_a = frame_offsets =
              ContextOffsetsToOffsetArray(query_info_in);
      } else {
         BLAST_GetAllTranslations(orig_sequence, NCBI2NA_ENCODING,
            orig_length, db_options->gen_code_string, &translation_buffer,
            &frame_offsets, NULL);
         frame_offsets_a = frame_offsets;
      }
   } else if (program_number == eBlastTypeBlastn) {
      first_context = 1;
      last_context = 1;
   } else {
      first_context = 0;
      last_context = 0;
   }

   *hsp_list_out = NULL;

   if (gap_align->positionBased)
      matrix = gap_align->sbp->psi_matrix->pssm->data;
   else
      matrix = gap_align->sbp->matrix->data;

   hsp_num_max = (hit_options->hsp_num_max ? hit_options->hsp_num_max : INT4_MAX);

   if (diagnostics) {
      ungapped_stats = diagnostics->ungapped_stat;
      gapped_stats = diagnostics->gapped_stat;
   }

   /* Substitute query info by concatenated database info for RPS BLAST search */
   if (program_number == eBlastTypeRpsBlast || 
       program_number == eBlastTypeRpsTblastn) {
      BlastRPSLookupTable* lut = (BlastRPSLookupTable*) lookup->lut;
      query_info = (BlastQueryInfo*) calloc(1, sizeof(BlastQueryInfo));
      query_info->num_queries = lut->num_profiles;
      query_info->last_context = lut->num_profiles - 1;
      /* Since this will really be "subject info", not "query info",
         pass program argument such that all frames will be set to 0. */
      s_RPSOffsetArrayToContextOffsets(query_info,
                                       lut->rps_seq_offsets);
   }

   /* Loop over frames of the subject sequence */
   for (context=first_context; context<=last_context; context++) {
      Int4 chunk; /* loop variable below. */
      Int4 num_chunks; /* loop variable below. */
      Int4 offset = 0; /* Used as offset into subject sequence (if chunked) */
      Int4 total_subject_length; /* Length of subject sequence used when split. */

      if (kTranslatedSubject) {
         subject->frame =
             BLAST_ContextToFrame(eBlastTypeBlastx, context);
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
            if (program_number == eBlastTypeBlastn) {
               subject->sequence += 
                  (subject->length - DBSEQ_CHUNK_OVERLAP)/COMPRESSION_RATIO;
            } else {
               subject->sequence += (subject->length - DBSEQ_CHUNK_OVERLAP);
            }
         }
         subject->length = MIN(total_subject_length - offset, 
                               MAX_DBSEQ_LEN);
         
         BlastInitHitListReset(init_hitlist);
         
         aux_struct->WordFinder(subject, query, lookup, 
            matrix, word_params, aux_struct->ewp, aux_struct->query_offsets, 
            aux_struct->subject_offsets, GetOffsetArraySize(lookup), 
            init_hitlist, ungapped_stats);
            
         if (init_hitlist->total == 0)
            continue;
         
         if (score_options->gapped_calculation) {
            Int4 prot_length = 0;
            if (score_options->is_ooframe) {
               /* Convert query offsets in all HSPs into the mixed-frame  
                  coordinates */
               s_TranslateHSPsToDNAPCoord(program_number, init_hitlist, 
                  query_info, subject->frame, orig_length, offset);
               if (kTranslatedSubject) {
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
               init_hitlist, &hsp_list, gapped_stats);
            if (score_options->is_ooframe && kTranslatedSubject)
               subject->length = prot_length;
         } else {
            BLAST_GetUngappedHSPList(init_hitlist, query_info, subject, 
                                     hit_options, &hsp_list);
         }

         if (hsp_list->hspcnt == 0)
            continue;
         
         /* The subject ordinal id is not yet filled in this HSP list */
         hsp_list->oid = subject->oid;

         Blast_HSPListAdjustOffsets(hsp_list, offset);
         /* Allow merging of HSPs either if traceback is already 
            available, or if it is an ungapped search */
         Blast_HSPListsMerge(hsp_list, &combined_hsp_list, hsp_num_max, offset,
            (Boolean)(prelim_traceback || !score_options->gapped_calculation));
      } /* End loop on chunks of subject sequence */

      if (Blast_HSPListAppend(combined_hsp_list, hsp_list_out, hsp_num_max)) {
         status = 1;
         break;
      }
      combined_hsp_list = Blast_HSPListFree(combined_hsp_list);
   } /* End loop on frames */

   /* Restore the original contents of the subject block */
   subject->length = orig_length;
   subject->sequence = orig_sequence;

   if (status) 
      return status;

   hsp_list = *hsp_list_out;

   if (hit_params->link_hsp_params) {
      status = BLAST_LinkHsps(program_number, hsp_list, query_info,
                  subject->length, gap_align->sbp, hit_params->link_hsp_params, 
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
      if (program_number != eBlastTypeRpsBlast &&
          program_number != eBlastTypeRpsTblastn)
         status = Blast_HSPListGetEvalues(query_info, hsp_list, 
                                          score_options->gapped_calculation, 
                                          gap_align->sbp, 0, 1.0);
   }
   
   /* Free the local query info structure when needed (in RPS BLAST). */
   if (query_info != query_info_in)
      BlastQueryInfoFree(query_info);

   /* Discard HSPs that don't pass the e-value test. */
   status = Blast_HSPListReapByEvalue(hsp_list, hit_options);

   /* If there are no HSPs left, destroy the HSP list too. */
   if (hsp_list && hsp_list->hspcnt == 0)
      *hsp_list_out = hsp_list = Blast_HSPListFree(hsp_list);

   if (gapped_stats && hsp_list && hsp_list->hspcnt > 0) {
      ++gapped_stats->num_seqs_passed;
      gapped_stats->good_extensions += hsp_list->hspcnt;
   }

   /* Free translation buffer and frame offsets, except for RPS tblastn,
    * where they are taken from different structures, and hence shouldn't 
    * be freed here. 
    */
   if (program_number != eBlastTypeRpsTblastn) {
      if (translation_buffer) {
         sfree(translation_buffer);
      }
   }
   
   if (frame_offsets_a) {
       sfree(frame_offsets_a);
   }
   
   return status;
}

/** Fills the output information about the cutoffs uses in a BLAST search. 
 * @param return_cutoffs Structure for saving cutoffs information [in] [out]
 * @param score_params Scoring parameters, containing the scaling factor [in]
 * @param word_params Initial word parameters [in]
 * @param ext_params Gapped extension parameters [in]
 * @param hit_params Hit saving parameters [in]
 */
static Int2 
s_FillReturnCutoffsInfo(BlastRawCutoffs* return_cutoffs, 
                        const BlastScoringParameters* score_params, 
                        const BlastInitialWordParameters* word_params, 
                        const BlastExtensionParameters* ext_params,
                        const BlastHitSavingParameters* hit_params)
{
   /* since the cutoff score here will be used for display
      putposes, strip out any internal scaling of the scores */

   Int4 scale_factor = (Int4)score_params->scale_factor;

   if (!return_cutoffs)
      return -1;

   return_cutoffs->x_drop_ungapped = 
      word_params->x_dropoff / scale_factor;
   return_cutoffs->x_drop_gap = ext_params->gap_x_dropoff / scale_factor;
   return_cutoffs->x_drop_gap_final = ext_params->gap_x_dropoff_final / 
                                                        scale_factor;
   return_cutoffs->ungapped_cutoff = word_params->cutoff_score / scale_factor;
   return_cutoffs->cutoff_score = hit_params->cutoff_score;

   return 0;
}

/** Setup of the auxiliary BLAST structures; 
 * also calculates internally used parameters from options. 
 * @param seq_src Sequence source information, with callbacks to get 
 *             sequences, their lengths, etc. [in]
 * @param lookup_wrap Lookup table, already constructed. [in]
 * @param word_params Parameters for initial word finding and ungapped 
 *                    extension. [in]
 * @param ext_options options for gapped extension. [in]
 * @param hit_options options for saving hits. [in]
 * @param query The query sequence block [in]
 * @param aux_struct_ptr Placeholder joining various auxiliary memory 
 *                       structures [out]
 */
static Int2 
s_BlastSetUpAuxStructures(const BlastSeqSrc* seq_src,
   LookupTableWrap* lookup_wrap,	
   const BlastInitialWordParameters* word_params,
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingOptions* hit_options,
   BLAST_SequenceBlk* query, BlastCoreAuxStruct** aux_struct_ptr)
{
   Int2 status = 0;
   BlastCoreAuxStruct* aux_struct;
   Boolean blastp = (lookup_wrap->lut_type == AA_LOOKUP_TABLE ||
                     lookup_wrap->lut_type == RPS_LOOKUP_TABLE);
   Boolean mb_lookup = (lookup_wrap->lut_type == MB_LOOKUP_TABLE);
   Boolean phi_lookup = (lookup_wrap->lut_type == PHI_AA_LOOKUP ||
                         lookup_wrap->lut_type == PHI_NA_LOOKUP);
   Int4 offset_array_size = GetOffsetArraySize(lookup_wrap);
   Uint4 avg_subj_length;
   Boolean is_na = mb_lookup || (lookup_wrap->lut_type == NA_LOOKUP_TABLE);

   ASSERT(seq_src);

   *aux_struct_ptr = aux_struct = (BlastCoreAuxStruct*)
      calloc(1, sizeof(BlastCoreAuxStruct));

   avg_subj_length = BLASTSeqSrcGetAvgSeqLen(seq_src);
     
   if ((status = BlastExtendWordNew(is_na, query->length, word_params, 
                    avg_subj_length, &aux_struct->ewp)) != 0)
      return status;

   if (mb_lookup) {
      aux_struct->WordFinder = MB_WordFinder;
   } else if (phi_lookup) {
      aux_struct->WordFinder = PHIBlastWordFinder;
   } else if (blastp) {
      aux_struct->WordFinder = BlastAaWordFinder;
   } else if (word_params->extension_method == eRightAndLeft) { /* Used AG word finding. */
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
   else if (ext_options->ePrelimGapExt == eDynProgExt)
      aux_struct->GetGappedScore = BLAST_GetGappedScore;
   else
      aux_struct->GetGappedScore = BLAST_MbGetGappedScore;

   aux_struct->hsp_list = Blast_HSPListNew(hit_options->hsp_num_max);
   return status;
}

/** Performs the preliminary stage of an RPS BLAST search, after all set up has
 * already been done.
 * @param program_number Type of BLAST program [in]
 * @param query The query sequence [in]
 * @param query_info Additional query information [in]
 * @param seq_src Structure containing BLAST database [in]
 * @param score_params Hit scoring parameters [in]
 * @param lookup_wrap The lookup table, constructed earlier [in] 
 * @param aux_struct Wrapper for auxiliary structures used in preliminary
 *                   search [in]
 * @param word_params Parameters for processing initial word hits [in]
 * @param ext_params Parameters for the gapped extension [in]
 * @param gap_align Structure containing scoring block and memory allocated
 *                  for gapped alignment. [in]
 * @param hit_params Parameters for saving the HSPs [in]
 * @param hsp_stream Placeholder for saving HSP lists [in]
 * @param diagnostics Return statistics containing numbers of hits on 
 *                    different stages of the search. Statistics saved only 
 *                    for the allocated parts of the structure. [in] [out]
 */
static Int2 
s_RPSPreliminarySearchEngine(EBlastProgramType program_number, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info,
   const BlastSeqSrc* seq_src,
   const BlastScoringParameters* score_params, 
   LookupTableWrap* lookup_wrap, BlastCoreAuxStruct* aux_struct,
   const BlastInitialWordParameters* word_params, 
   const BlastExtensionParameters* ext_params, 
   BlastGapAlignStruct* gap_align,
   const BlastHitSavingParameters* hit_params,
   BlastHSPStream* hsp_stream, BlastDiagnostics* diagnostics)
{
   BlastHSPList* hsp_list = NULL;
   Int2 status = 0;
   Int8 dbsize;
   Int4 num_db_seqs;
   Uint4 avg_subj_length = 0;
   BlastRPSLookupTable *lookup = (BlastRPSLookupTable *)lookup_wrap->lut;
   BLAST_SequenceBlk concat_db;
   BlastQueryInfo* one_query_info = NULL;
   BLAST_SequenceBlk* one_query = NULL;
   Int4 index;

   if (program_number != eBlastTypeRpsBlast &&
       program_number != eBlastTypeRpsTblastn)
      return -1;

   /* modify scoring and gap alignment structures for
      use with RPS blast. */

   gap_align->positionBased = TRUE;
   RPSPsiMatrixAttach(gap_align->sbp, lookup->rps_pssm);

   /* determine the total number of residues in the db.
      This figure must also include one trailing NULL for
      each DB sequence */

   num_db_seqs = BLASTSeqSrcGetNumSeqs(seq_src);
   dbsize = BLASTSeqSrcGetTotLen(seq_src) + num_db_seqs;
   if (dbsize > INT4_MAX)
      return -3;

   /* Concatenate all of the DB sequences together, and pretend
      this is a large multiplexed sequence. Note that because the
      scoring is position-specific, the actual sequence data is
      not needed */

   memset(&concat_db, 0, sizeof(concat_db)); /* fill in SequenceBlk */
   concat_db.length = (Int4) dbsize;

   /* Change the table of diagonals that will be used for the
      search; we need a diag table that can fit the entire
      concatenated DB */
   avg_subj_length = (Uint4) (dbsize / num_db_seqs);
   BlastExtendWordFree(aux_struct->ewp);
   /* First argument in BlastExtendWordNew is false because RPS search is 
      never a blastn search. */
   BlastExtendWordNew(FALSE, concat_db.length, word_params, 
                      avg_subj_length, &aux_struct->ewp);

   /* Run the search; the input query is what gets scanned
      and the concatenated DB is the sequence associated with
      the score matrix. This essentially means that 'query'
      and 'subject' have opposite conventions for the search. 
    
      Note that while scores can be calculated for any alignment
      found, we have not set up any Karlin parameters or effective
      search space sizes for the concatenated DB. This means that
      E-values cannot be calculated after hits are found. */

   for (index = 0; index < query_info->num_queries; ++index) {
       /* Separate one query from the set: create an auxiliary query_info 
          structure which refers to this single query. */
       if (Blast_GetOneQueryStructs(&one_query_info, &one_query, 
                                    query_info, query, index) != 0)
           return -1;

       /* It is OK to pass NULL for the BlastDatabaseOptions argument, because it
          will not be checked for RPS BLAST program types. */
       status = (Int4)
          s_BlastSearchEngineCore(program_number, &concat_db, one_query_info, 
             one_query, lookup_wrap, gap_align, score_params, 
             word_params, ext_params, hit_params, NULL, 
             diagnostics, aux_struct, &hsp_list);

       /* Save the resulting list of HSPs. 'query' and 'subject' are
          still reversed */
       if (hsp_list && hsp_list->hspcnt > 0) {
           hsp_list->query_index = index;
           /* Save the HSP list */
           BlastHSPStreamWrite(hsp_stream, &hsp_list);
       }
   }

   BlastQueryInfoFree(one_query_info);
   BlastSequenceBlkFree(one_query);

   /* Restore original settings in the gapped alignment structure. */
   RPSPsiMatrixDetach(gap_align->sbp);
   gap_align->positionBased = FALSE;

   /* Fill the cutoff values in the diagnostics structure */
   if (diagnostics && diagnostics->cutoffs) {
      s_FillReturnCutoffsInfo(diagnostics->cutoffs, score_params, word_params,
                              ext_params, hit_params);
   }

   return status;
}

Int4 
BLAST_PreliminarySearchEngine(EBlastProgramType program_number, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info,
   const BlastSeqSrc* seq_src, BlastGapAlignStruct* gap_align,
   BlastScoringParameters* score_params, 
   LookupTableWrap* lookup_wrap,
   const BlastInitialWordOptions* word_options, 
   BlastExtensionParameters* ext_params, 
   BlastHitSavingParameters* hit_params,
   BlastEffectiveLengthsParameters* eff_len_params,
   const PSIBlastOptions* psi_options, 
   const BlastDatabaseOptions* db_options,
   BlastHSPStream* hsp_stream, BlastDiagnostics* diagnostics)
{
   BlastCoreAuxStruct* aux_struct = NULL;
   BlastHSPList* hsp_list = NULL; 
   GetSeqArg seq_arg;
   Int2 status = 0;
   Int8 db_length = 0;
   Boolean prelim_traceback;
   const BlastScoringOptions* score_options = score_params->options;
   const BlastHitSavingOptions* hit_options = hit_params->options;
   const BlastExtensionOptions* ext_options = ext_params->options;
   BlastInitialWordParameters* word_params = NULL;
   Boolean gapped_calculation = score_options->gapped_calculation;
   BlastScoreBlk* sbp = gap_align->sbp;
   BlastSeqSrcIterator* itr;

   BlastInitialWordParametersNew(program_number, word_options, 
      hit_params, lookup_wrap, sbp, query_info, 
      BLASTSeqSrcGetAvgSeqLen(seq_src), &word_params);

   if ((status = 
       s_BlastSetUpAuxStructures(seq_src, lookup_wrap, word_params, 
          ext_options, hit_options, query, &aux_struct)) != 0)
      return status;

   /* For RPS BLAST, there is no loop over subject sequences, so the preliminary
      search engine is done in a separate function. */
   if (program_number == eBlastTypeRpsBlast || 
       program_number == eBlastTypeRpsTblastn) {
      status =         
         s_RPSPreliminarySearchEngine(program_number, query, query_info, 
            seq_src, score_params, lookup_wrap, aux_struct, word_params, 
            ext_params, gap_align, hit_params, hsp_stream, diagnostics);
      word_params = BlastInitialWordParametersFree(word_params);
      s_BlastCoreAuxStructFree(aux_struct);
      return status;
   }

   /* Update the parameters for linking HSPs, if necessary. */
   BlastLinkHSPParametersUpdate(word_params, hit_params, gapped_calculation);
   
   prelim_traceback = (ext_options->ePrelimGapExt == eGreedyWithTracebackExt);

   memset((void*) &seq_arg, 0, sizeof(seq_arg));

   /* Encoding is set so there are no sentinel bytes, and protein/nucleotide
      sequences are retieved in ncbistdaa/ncbi2na encodings respectively. */
   seq_arg.encoding = BLASTP_ENCODING; 

   db_length = BLASTSeqSrcGetTotLen(seq_src);

   itr = BlastSeqSrcIteratorNew(0);

   /* iterate over all subject sequences */
   while ( (seq_arg.oid = BlastSeqSrcIteratorNext(seq_src, itr)) 
           != BLAST_SEQSRC_EOF) {
      if (seq_arg.oid == BLAST_SEQSRC_ERROR)
         break;
      if (BLASTSeqSrcGetSequence(seq_src, (void*) &seq_arg) < 0)
          continue;
      if (db_length == 0) {
         /* This is not a database search, hence need to recalculate and save
            the effective search spaces and length adjustments for all 
            queries based on the length of the current single subject 
            sequence. */
         if ((status = BLAST_OneSubjectUpdateParameters(program_number, 
                          seq_arg.seq->length, score_options, query_info, 
                          sbp, hit_params, word_params, 
                          eff_len_params)) != 0)
            return status;
      }

      /* Calculate cutoff scores for linking HSPs. Do this only for ungapped
         protein searches. */
      if (hit_params->link_hsp_params && program_number != eBlastTypeBlastn &&
          !gapped_calculation) {
         CalculateLinkHSPCutoffs(program_number, query_info, sbp, 
            hit_params->link_hsp_params, word_params, db_length, 
            seq_arg.seq->length); 
      }

      status = 
         s_BlastSearchEngineCore(program_number, query, query_info,
            seq_arg.seq, lookup_wrap, gap_align, score_params, word_params, 
            ext_params, hit_params, db_options, diagnostics, aux_struct, 
            &hsp_list);
 
      if (status)
         break;

      if (hsp_list && hsp_list->hspcnt > 0) {
         if (!gapped_calculation || prelim_traceback) {
            /* The following must be performed for any ungapped search with a 
               nucleotide database. */
            if (program_number == eBlastTypeBlastn ||
                program_number == eBlastTypeTblastn ||
                program_number == eBlastTypeTblastx) {
               status = 
                  Blast_HSPListReevaluateWithAmbiguities(program_number, 
                     hsp_list, query, seq_arg.seq, word_params, hit_params, 
                     query_info, sbp, score_params, seq_src, 
                     (db_options ? db_options->gen_code_string : NULL));
               /* Relink HSPs if sum statistics is used, because scores might
                * have changed after reevaluation with ambiguities, and there
                * will be no traceback stage where relinking is done normally.
                * If sum statistics is not used, just recalculate the e-values. 
                */
               if (hit_params->link_hsp_params) {
                   status = 
                       BLAST_LinkHsps(program_number, hsp_list, query_info,
                                      seq_arg.seq->length, sbp, 
                                      hit_params->link_hsp_params, 
                                      gapped_calculation);
               } else {
                  Blast_HSPListGetEvalues(query_info, hsp_list, 
                                          score_options->gapped_calculation, 
                                          sbp, 0, 1.0);
               }
               status = Blast_HSPListReapByEvalue(hsp_list, hit_params->options);
            }
             
            /* Calculate and fill the bit scores, since there will be no
               traceback stage where this can be done. */
            Blast_HSPListGetBitScores(hsp_list, gapped_calculation, sbp);
         } 
         
         /* Save the results. */
         BlastHSPStreamWrite(hsp_stream, &hsp_list);
      }
      
      BLASTSeqSrcRetSequence(seq_src, (void*) &seq_arg);
   }
   
   BlastSequenceBlkFree(seq_arg.seq);
   itr = BlastSeqSrcIteratorFree(itr);

   /* Fill the cutoff values in the diagnostics structure */
   if (diagnostics && diagnostics->cutoffs) {
      s_FillReturnCutoffsInfo(diagnostics->cutoffs, score_params, word_params, 
                              ext_params, hit_params);
   }

   if (hit_options->phi_align) {
      /* Save the product of effective occurrencies of pattern in query and
         occurrencies of pattern in database */
      Int8 db_hits = 1;
      if (diagnostics && diagnostics->ungapped_stat)
         db_hits = diagnostics->ungapped_stat->lookup_hits;
      gap_align->sbp->effective_search_sp *= db_hits;
   }

   word_params = BlastInitialWordParametersFree(word_params);
   s_BlastCoreAuxStructFree(aux_struct);
   return status;
}

Int2 
Blast_RunPreliminarySearch(EBlastProgramType program, 
   BLAST_SequenceBlk* query, BlastQueryInfo* query_info, 
   const BlastSeqSrc* seq_src, const BlastScoringOptions* score_options,
   BlastScoreBlk* sbp, LookupTableWrap* lookup_wrap,
   const BlastInitialWordOptions* word_options, 
   const BlastExtensionOptions* ext_options,
   const BlastHitSavingOptions* hit_options,
   const BlastEffectiveLengthsOptions* eff_len_options,
   const PSIBlastOptions* psi_options, const BlastDatabaseOptions* db_options, 
   BlastHSPStream* hsp_stream, BlastDiagnostics* diagnostics)
{
   Int2 status = 0;
   BlastScoringParameters* score_params = NULL;/**< Scoring parameters */
   BlastExtensionParameters* ext_params = NULL;/**< Gapped extension 
                                                    parameters */
   BlastHitSavingParameters* hit_params = NULL;/**< Hit saving parameters */
   BlastEffectiveLengthsParameters* eff_len_params = NULL; /**< Parameters 
                                          for effective lengths calculations */
   BlastGapAlignStruct* gap_align = NULL; /**< Gapped alignment structure */
   
   /* Use a local diagnostics structure, because the one passed in an input 
      argument can be shared between multiple threads, so we don't want to pass
      it to the engine and have a lot of mutex contention. */
   BlastDiagnostics* local_diagnostics = Blast_DiagnosticsInit();

   if ((status = 
        BLAST_GapAlignSetUp(program, seq_src, score_options, 
                            eff_len_options, ext_options, hit_options, 
                            query_info, sbp, &score_params, &ext_params, 
                            &hit_params, &eff_len_params, &gap_align)) != 0)
      return status;
   
   if ((status=
        BLAST_PreliminarySearchEngine(program, query, query_info, 
                                      seq_src, gap_align, score_params, 
                                      lookup_wrap, word_options, 
                                      ext_params, hit_params, eff_len_params,
                                      psi_options, db_options, hsp_stream, 
                                      local_diagnostics)) != 0)
      return status;

   /* Do not destruct score block here */
   gap_align->sbp = NULL;
   gap_align = BLAST_GapAlignStructFree(gap_align);
   
   score_params = BlastScoringParametersFree(score_params);
   hit_params = BlastHitSavingParametersFree(hit_params);
   ext_params = BlastExtensionParametersFree(ext_params);
   eff_len_params = BlastEffectiveLengthsParametersFree(eff_len_params);
    
   /* Now update the input diagonistics structure. */
   Blast_DiagnosticsUpdate(diagnostics, local_diagnostics);
   Blast_DiagnosticsFree(local_diagnostics);

   return status;
}

Int4 
Blast_RunFullSearch(EBlastProgramType program_number, 
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
   BlastHSPStream* hsp_stream, const BlastRPSInfo* rps_info, 
   BlastDiagnostics* diagnostics, BlastHSPResults** results)
{
   Int4 status = 0;
   BlastScoringParameters* score_params = NULL;
   BlastExtensionParameters* ext_params = NULL;
   BlastHitSavingParameters* hit_params = NULL;
   BlastEffectiveLengthsParameters* eff_len_params = NULL;
   BlastGapAlignStruct* gap_align;

   if ((status = 
        BLAST_GapAlignSetUp(program_number, seq_src, score_options, 
           eff_len_options, ext_options, hit_options, query_info, sbp, 
           &score_params, &ext_params, &hit_params, &eff_len_params, 
           &gap_align)) != 0)
      return status;
      
   if ((status=
        BLAST_PreliminarySearchEngine(program_number, query, query_info, 
           seq_src, gap_align, score_params, lookup_wrap, word_options, 
           ext_params, hit_params, eff_len_params, psi_options, 
           db_options, hsp_stream, diagnostics)) != 0)
      return status;
   
   /* Prohibit any subsequent writing to the HSP stream. */
   BlastHSPStreamClose(hsp_stream);

   if((status = 
      BLAST_ComputeTraceback(program_number, hsp_stream, query, query_info,
         seq_src, gap_align, score_params, ext_params, hit_params,
         eff_len_params, db_options, psi_options, rps_info, results)) != 0)
      return status;

   /* Do not destruct score block here */
   gap_align->sbp = NULL;
   BLAST_GapAlignStructFree(gap_align);

   score_params = BlastScoringParametersFree(score_params);
   hit_params = BlastHitSavingParametersFree(hit_params);
   ext_params = BlastExtensionParametersFree(ext_params);
   eff_len_params = BlastEffectiveLengthsParametersFree(eff_len_params);
   return status;
}
