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

******************************************************************************
 * $Revision$
 * */

static char const rcsid[] = "$Id$";

#include <blast_engine.h>
#include <readdb.h>
#include <aa_ungapped.h>
#include <blast_util.h>
#include <blast_gapalign.h>
#include <blast_traceback.h>

extern Uint1Ptr
GetPrivatTranslationTable PROTO((CharPtr genetic_code,
                                 Boolean reverse_complement));
extern OIDListPtr LIBCALL 
BlastGetVirtualOIDList PROTO((ReadDBFILEPtr rdfp_chain));

/** Deallocates all memory in BlastCoreAuxStruct */
static BlastCoreAuxStructPtr 
BlastCoreAuxStructFree(BlastCoreAuxStructPtr aux_struct)
{
   BlastExtendWordFree(aux_struct->ewp);
   MemFree(aux_struct->translation_buffer);
   MemFree(aux_struct->translation_table);
   MemFree(aux_struct->translation_table_rc);
   BLAST_InitHitListDestruct(aux_struct->init_hitlist);
   BlastHSPListFree(aux_struct->hsp_list);
   MemFree(aux_struct->query_offsets);
   MemFree(aux_struct->subject_offsets);
   
   return (BlastCoreAuxStructPtr) MemFree(aux_struct);
}

/** The core of the BLAST search: comparison between the (concatenated)
 * query against one subject sequence. Translation of the subject sequence
 * into 6 frames is done inside, if necessary. If subject sequence is 
 * too long, it can be split into several chunks. 
 */
static Int2
BLAST_SearchEngineCore(Uint1 program_number, BLAST_SequenceBlkPtr query, 
   BlastQueryInfoPtr query_info, BLAST_SequenceBlkPtr subject, 
   LookupTableWrapPtr lookup, BlastGapAlignStructPtr gap_align, 
   const BlastScoringOptionsPtr score_options, 
   BlastInitialWordParametersPtr word_params, 
   BlastExtensionParametersPtr ext_params, 
   BlastHitSavingParametersPtr hit_params, 
   const PSIBlastOptionsPtr psi_options, 
   const BlastDatabaseOptionsPtr db_options,
   BlastReturnStatPtr return_stats,
   BlastCoreAuxStructPtr aux_struct,
   BlastHSPListPtr PNTR hsp_list_out)
{
   BlastInitHitListPtr init_hitlist = aux_struct->init_hitlist;
   BlastHSPListPtr hsp_list = aux_struct->hsp_list;
   BLAST_ExtendWordPtr ewp = aux_struct->ewp;
   Uint4Ptr query_offsets = aux_struct->query_offsets;
   Uint4Ptr subject_offsets = aux_struct->subject_offsets;
   Uint1Ptr translation_buffer = aux_struct->translation_buffer;
   Uint1Ptr translation_table = aux_struct->translation_table;
   Uint1Ptr translation_table_rc = aux_struct->translation_table_rc;
   Int4 num_chunks, chunk, total_subject_length, offset;
   BlastHitSavingOptionsPtr hit_options = hit_params->options;
   BlastHSPListPtr combined_hsp_list;
   Int2 status = 0;
   Boolean translated_subject;
   Int2 frame, frame_min, frame_max;
   Int4 orig_length = subject->length;
   Uint1Ptr orig_sequence = subject->sequence;

   translated_subject = (program_number == blast_type_tblastn
                         || program_number == blast_type_tblastx
                         || program_number == blast_type_psitblastn);

   if (translated_subject) {
      frame_min = -3;
      frame_max = 3;
      subject->sequence = translation_buffer;
   } else if (program_number == blast_type_blastn) {
      frame_min = 1;
      frame_max = 1;
   } else {
      frame_min = 0;
      frame_max = 0;
   }

   *hsp_list_out = NULL;

   /* Loop over frames of the subject sequence */
   for (frame=frame_min; frame<=frame_max; frame++) {
      subject->frame = frame;
      if (translated_subject) {
         if (frame == 0) {
            continue;
         } else if (frame > 0) {
            subject->length = 
               BLAST_TranslateCompressedSequence(translation_table,
                  orig_length, orig_sequence, frame, translation_buffer);
         } else {
            subject->length = 
               BLAST_TranslateCompressedSequence(translation_table_rc,
                  orig_length, orig_sequence, frame, translation_buffer);
         }
         subject->sequence = translation_buffer + 1;
      }
     
      /* Split subject sequence into chunks if it is too long */
      num_chunks = (subject->length - DBSEQ_CHUNK_OVERLAP) / 
         (MAX_DBSEQ_LEN - DBSEQ_CHUNK_OVERLAP) + 1;
      offset = 0;
      total_subject_length = subject->length;
      combined_hsp_list = NULL;
      
      for (chunk = 0; chunk < num_chunks; ++chunk) {
         subject->length = MIN(total_subject_length - offset, 
                               MAX_DBSEQ_LEN);
         
         init_hitlist->total = 0;
         
         return_stats->db_hits +=
            aux_struct->WordFinder(subject, query, lookup, 
               gap_align->sbp->matrix, word_params, ewp, query_offsets, 
               subject_offsets, OFFSET_ARRAY_SIZE, init_hitlist);
            
         if (init_hitlist->total == 0)
            continue;
         
         return_stats->init_extends += init_hitlist->total;
         
         if (score_options->gapped_calculation) {
            aux_struct->GetGappedScore(program_number, query, subject, 
               gap_align, score_options, ext_params, hit_params, 
               init_hitlist, &hsp_list);
         } else {
            BLAST_GetUngappedHSPList(init_hitlist, subject,
                                     hit_params->options, &hsp_list);
         }

         if (hsp_list->hspcnt == 0)
            continue;
         
         return_stats->good_init_extends += hsp_list->hspcnt;
         
         /* The subject ordinal id is not yet filled in this HSP list */
         hsp_list->oid = subject->oid;
         
         /* Multiple contexts - adjust all HSP offsets to the individual 
            query coordinates; also assign frames */
         BLAST_AdjustQueryOffsets(program_number, hsp_list, query_info);
         
#ifdef DO_LINK_HSPS
         if (hit_options->do_sum_stats == TRUE)
            status = BLAST_LinkHsps(hsp_list);
         else
#endif
            /* Calculate e-values for all HSPs */
            status = 
               BLAST_GetNonSumStatsEvalue(program_number, query_info, 
                  hsp_list, hit_options, gap_align->sbp);
         
         /* Discard HSPs that don't pass the e-value test */
         status = BLAST_ReapHitlistByEvalue(hsp_list, hit_options);
         
         AdjustOffsetsInHSPList(hsp_list, offset);
         /* Allow merging of HSPs either if traceback is already 
            available, or if it is an ungapped search */
         if (MergeHSPLists(hsp_list, &combined_hsp_list, offset,
             (hsp_list->traceback_done || !hit_options->is_gapped), FALSE)) {
            /* HSPs from this list are moved elsewhere, reset count to 0 */
            hsp_list->hspcnt = 0;
         }
         offset += subject->length - DBSEQ_CHUNK_OVERLAP;
         subject->sequence += 
            (subject->length - DBSEQ_CHUNK_OVERLAP)/COMPRESSION_RATIO;
      } /* End loop on chunks of subject sequence */
      
      MergeHSPLists(combined_hsp_list, hsp_list_out, 0, 
                    FALSE, TRUE);
   } /* End loop on frames */
      
   /* Restore the original contents of the subject block */
   subject->length = orig_length;
   subject->sequence = orig_sequence;

   return status;
}

static Int2 
FillReturnXDropoffsInfo(BlastReturnStatPtr return_stats, 
   BlastInitialWordParametersPtr word_params, 
   BlastExtensionParametersPtr ext_params)
{
   if (!return_stats)
      return -1;

   return_stats->x_drop_ungapped = word_params->x_dropoff;
   return_stats->x_drop_gap = ext_params->gap_x_dropoff;
   return_stats->x_drop_gap_final = ext_params->gap_x_dropoff_final;
   return_stats->gap_trigger = ext_params->gap_trigger;

   return 0;
}

Int2 BLAST_CalcEffLengths (Uint1 program_number, 
   const BlastScoringOptionsPtr scoring_options,
   const BlastEffectiveLengthsOptionsPtr eff_len_options, 
   const BLAST_ScoreBlkPtr sbp, BlastQueryInfoPtr query_info)
{
   Nlm_FloatHi alpha, beta; /*alpha and beta for new scoring system */
   Int4 min_query_length;   /* lower bound on query length. */
   Int4 length_adjustment;  /* length adjustment for current iteration. */
   Int4 last_length_adjustment;/* length adjustment in previous iteration.*/
   Int4 index;		/* loop index. */
   Int4	db_num_seqs;	/* number of sequences in database. */
   Int8	db_length;	/* total length of database. */
   BLAST_KarlinBlkPtr *kbp_ptr; /* Array of Karlin block pointers */
   BLAST_KarlinBlkPtr kbp; /* Karlin-Blk pointer from ScoreBlk. */
   Int4 query_length;   /* length of an individual query sequence */
   Int8 effective_length, effective_db_length; /* effective lengths of 
                                                  query and database */
   Int8 effective_search_space; /* Effective search space for a given 
                                   sequence/strand/frame */
   Int2 i; /* Iteration index for calculating length adjustment */
   Uint1 num_strands;

   if (sbp == NULL || eff_len_options == NULL)
      return 1;

   /* use values in BlastEffectiveLengthsOptionsPtr */
   db_length = eff_len_options->db_length;
   if (program_number == blast_type_tblastn || 
       program_number == blast_type_tblastx)
      db_length = db_length/3;	
   
   db_num_seqs = eff_len_options->dbseq_num;
   
   if (program_number != blast_type_blastn) {
      if (scoring_options->gapped_calculation) {
         getAlphaBeta(sbp->name,&alpha,&beta,TRUE, 
            scoring_options->gap_open, scoring_options->gap_extend);
      }
   }
   
   if (scoring_options->gapped_calculation && 
       program_number != blast_type_blastn) 
      kbp_ptr = sbp->kbp_gap_std;
   else
      kbp_ptr = sbp->kbp_std; 
   
   if (program_number == blast_type_blastn)
      num_strands = 2;
   else
      num_strands = 1;

   for (index = query_info->first_context; 
        index <= query_info->last_context; ) {
      if (eff_len_options->searchsp_eff) {
         effective_search_space = eff_len_options->searchsp_eff;
      } else {
         query_length = BLAST_GetQueryLength(query_info, index);
         /* Use the correct Karlin block. For blastn, two identical Karlin
            blocks are allocated for each sequence (one per strand), but we
            only need one of them.
         */
         kbp = kbp_ptr[index];
         length_adjustment = 0;
         last_length_adjustment = 0;
         
         min_query_length = (Int4) (1/(kbp->K));

         for (i=0; i<5; i++) {
            if (program_number != blast_type_blastn && 
                scoring_options->gapped_calculation) {
               length_adjustment = Nlm_Nint((((kbp->logK)+log((Nlm_FloatHi)(query_length-last_length_adjustment)*(Nlm_FloatHi)MAX(db_num_seqs, db_length-db_num_seqs*last_length_adjustment)))*alpha/kbp->Lambda) + beta);
            } else {
               length_adjustment = (Int4) ((kbp->logK+log((Nlm_FloatHi)(query_length-last_length_adjustment)*(Nlm_FloatHi)MAX(1, db_length-db_num_seqs*last_length_adjustment)))/(kbp->H));
            }

            if (length_adjustment >= query_length-min_query_length) {
               length_adjustment = query_length-min_query_length;
               break;
            }
            
            if (ABS(last_length_adjustment-length_adjustment) <= 1)
               break;
            last_length_adjustment = length_adjustment;
         }
         
         effective_length = 
            MAX(query_length - length_adjustment, min_query_length);
         effective_db_length = MAX(1, db_length - db_num_seqs*length_adjustment);
         
         effective_search_space = effective_length * effective_db_length;
      }
      for (i = 0; i < num_strands; ++i) {
         query_info->eff_searchsp_array[index] = effective_search_space;
         ++index;
      }
   }

   return 0;
}

/** Setup of the auxiliary BLAST structures; 
 * also calculates internally used parameters from options. 
 * @param program_number blastn, blastp, blastx, etc. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_options  used to calc. eff len. [in]
 * @param lookup_wrap Lookup table, already constructed. [in]
 * @param word_options options for initial word finding. [in]
 * @param ext_options options for gapped extension. [in]
 * @param hit_options options for saving hits. [in]
 * @param db_options Database options (containing database genetic code)[in]
 * @param query The query sequence block [in]
 * @param query_info The query information block [in]
 * @param sbp Contains scoring information. [in]
 * @param max_subject_length Length of the longest subject sequence [in]
 * @param gap_align Gapped alignment information and allocated memory [out]
 * @param word_params Parameters for initial word processing [out]
 * @param ext_params Parameters for gapped extension [out]
 * @param hit_params Parameters for saving hits [out]
 * @param aux_struct_ptr Placeholder joining various auxiliary memory 
 *                       structures [out]
 */
static Int2 
BLAST_SetUpAuxStructures(Uint1 program_number,
   const BlastScoringOptionsPtr scoring_options,
   const BlastEffectiveLengthsOptionsPtr eff_len_options,
   LookupTableWrapPtr lookup_wrap,	
   const BlastInitialWordOptionsPtr word_options,
   const BlastExtensionOptionsPtr ext_options,
   const BlastHitSavingOptionsPtr hit_options,
   const BlastDatabaseOptionsPtr db_options,
   BLAST_SequenceBlkPtr query, BlastQueryInfoPtr query_info, 
   BLAST_ScoreBlkPtr sbp, Uint4 max_subject_length,
   BlastGapAlignStructPtr PNTR gap_align, 
   BlastInitialWordParametersPtr PNTR word_params,
   BlastExtensionParametersPtr PNTR ext_params,
   BlastHitSavingParametersPtr PNTR hit_params,
   BlastCoreAuxStructPtr PNTR aux_struct_ptr)
{
   Int2 status = 0;
   Boolean blastp = (lookup_wrap->lut_type == AA_LOOKUP_TABLE);
   Boolean mb_lookup = (lookup_wrap->lut_type == MB_LOOKUP_TABLE);
   Boolean ag_blast = (Boolean)
      (word_options->extend_word_method & EXTEND_WORD_AG);
   Int4 offset_array_size = 0;
   Boolean translated_subject;
   BLAST_ExtendWordPtr ewp;
   BlastCoreAuxStructPtr aux_struct;


   if ((status = 
      BLAST_ExtendWordInit(query, word_options, eff_len_options->db_length, 
                           eff_len_options->dbseq_num, &ewp)) != 0)
      return status;
      
   BlastExtensionParametersNew(program_number, ext_options, sbp, query_info, 
                               ext_params);

   BlastHitSavingParametersNew(program_number, hit_options, NULL, sbp, 
                               query_info, hit_params);

   if ((status = BLAST_CalcEffLengths(program_number, scoring_options, 
                    eff_len_options, sbp, query_info)) != 0)
      return status;

   BlastInitialWordParametersNew(program_number, word_options, *hit_params, 
      *ext_params, sbp, query_info, eff_len_options, word_params);

   if ((status = BLAST_GapAlignStructNew(scoring_options, *ext_params, 1, 
                    max_subject_length, query->length, program_number, sbp,
                    gap_align))) {
      ErrPostEx(SEV_ERROR, 0, 0, 
                "Cannot allocate memory for gapped extension");
      return status;
   }

   *aux_struct_ptr = aux_struct = (BlastCoreAuxStructPtr)
      MemNew(sizeof(BlastCoreAuxStruct));
   aux_struct->ewp = ewp;

   /* pick which gapped alignment algorithm to use */
   if (ext_options->algorithm_type == EXTEND_DYN_PROG)
     aux_struct->GetGappedScore = BLAST_GetGappedScore;
   else
     aux_struct->GetGappedScore = BLAST_MbGetGappedScore;

   if (mb_lookup) {
      aux_struct->WordFinder = MB_WordFinder;
      offset_array_size = OFFSET_ARRAY_SIZE + 
         ((MBLookupTablePtr)lookup_wrap->lut)->longest_chain;
   } else {
      if (blastp) {
         aux_struct->WordFinder = BlastAaWordFinder;
      } else if (ag_blast) {
         aux_struct->WordFinder = BlastNaWordFinder_AG;
      } else {
         aux_struct->WordFinder = BlastNaWordFinder;
      }
      offset_array_size = OFFSET_ARRAY_SIZE + 
         ((LookupTablePtr)lookup_wrap->lut)->longest_chain;
   }

   aux_struct->query_offsets = Malloc(offset_array_size * sizeof(Uint4));
   aux_struct->subject_offsets = Malloc(offset_array_size * sizeof(Uint4));

   aux_struct->init_hitlist = BLAST_InitHitListNew();
   aux_struct->hsp_list = BlastHSPListNew();

   translated_subject = (program_number == blast_type_tblastn
                         || program_number == blast_type_tblastx
                         || program_number == blast_type_psitblastn);

   if (translated_subject) {
      CharPtr genetic_code=NULL;
      ValNodePtr vnp;
      GeneticCodePtr gcp;

      aux_struct->translation_buffer = 
         (Uint1Ptr) Malloc(3 + max_subject_length/CODON_LENGTH);

      gcp = GeneticCodeFind(db_options->genetic_code, NULL);
      for (vnp = (ValNodePtr)gcp->data.ptrvalue; vnp != NULL; 
           vnp = vnp->next) {
         if (vnp->choice == 3) {  /* ncbieaa */
            genetic_code = (CharPtr)vnp->data.ptrvalue;
            break;
         }
      }

      aux_struct->translation_table = 
         GetPrivatTranslationTable(genetic_code, FALSE);
      aux_struct->translation_table_rc = 
         GetPrivatTranslationTable(genetic_code, TRUE);
   }

   return status;
}

Int4 
BLAST_DatabaseSearchEngine(Uint1 program_number, 
   BLAST_SequenceBlkPtr query,  BlastQueryInfoPtr query_info,
   ReadDBFILEPtr rdfp, BLAST_ScoreBlkPtr sbp,
   const BlastScoringOptionsPtr score_options, 
   LookupTableWrapPtr lookup_wrap,
   const BlastInitialWordOptionsPtr word_options, 
   const BlastExtensionOptionsPtr ext_options, 
   const BlastHitSavingOptionsPtr hit_options,
   const BlastEffectiveLengthsOptionsPtr eff_len_options,
   const PSIBlastOptionsPtr psi_options, 
   const BlastDatabaseOptionsPtr db_options,
   BlastResultsPtr results, BlastReturnStatPtr return_stats)
{
   ReadDBFILEPtr db; /* Loop variable */
   Uint4 max_subject_length = 0; /* Longest subject sequence */
   BlastCoreAuxStructPtr aux_struct = NULL;
   BlastThrInfoPtr thr_info = NULL;
   Int4 oid; /* Subject ordinal id in the database */
   Int4Ptr oid_list = NULL;
   Int4 start = 0, stop = 0, oid_list_length = 0, index;
   Boolean use_oid_list = FALSE;
   BLAST_SequenceBlkPtr subject = NULL;
   BlastHSPListPtr hsp_list; 
   BlastInitialWordParametersPtr word_params;
   BlastExtensionParametersPtr ext_params;
   BlastHitSavingParametersPtr hit_params;
   BlastGapAlignStructPtr gap_align;
   Int2 status = 0;
   
   if (!rdfp)
      return -1;
   
   for (db = rdfp; db; db = db->next) {
      max_subject_length = 
         MAX(max_subject_length, readdb_get_maxlen(db));
   }

   if ((status = 
       BLAST_SetUpAuxStructures(program_number, score_options, 
          eff_len_options, lookup_wrap, word_options, ext_options, 
          hit_options, db_options, query, query_info, sbp, 
          max_subject_length, &gap_align, &word_params, &ext_params, 
          &hit_params, &aux_struct)) != 0)
      return status;

   FillReturnXDropoffsInfo(return_stats, word_params, ext_params);

   thr_info = BLAST_ThrInfoNew(rdfp);
   if (BlastGetVirtualOIDList(rdfp))
      oid_list = MemNew((thr_info->db_chunk_size+33)*sizeof(Int4));
   
   /* Allocate subject sequence block once for the entire run */
   subject = (BLAST_SequenceBlkPtr) MemNew(sizeof(BLAST_SequenceBlk));

   while (!BLAST_GetDbChunk(rdfp, &start, &stop, oid_list, 
                            &oid_list_length, thr_info)) {
      use_oid_list = (oid_list && (oid_list_length > 0));
      if (use_oid_list) {
         start = 0;
         stop = oid_list_length;
      }
      
      /* iterate over all subject sequences */
      for (index = start; index < stop; index++) {
         oid = (use_oid_list ? oid_list[index] : index);

         MakeBlastSequenceBlk(rdfp, &subject, oid, BLASTP_ENCODING);

         BLAST_SearchEngineCore(program_number, query, query_info,
            subject, lookup_wrap, gap_align, score_options, word_params, 
            ext_params, hit_params, psi_options, db_options,
            return_stats, aux_struct, &hsp_list);

         if (hsp_list && hsp_list->hspcnt > 0) {
            return_stats->prelim_gap_passed += hsp_list->hspcnt;
            /* Save the HSPs into a hit list */
            BLAST_SaveHitlist(program_number, query, subject, results, 
               hsp_list, hit_params, query_info, gap_align->sbp, 
               score_options, rdfp, thr_info);
         }
         BlastSequenceBlkClean(subject);
      }
   }
   BLAST_ThrInfoFree(thr_info);
   oid_list = MemFree(oid_list);
   subject = MemFree(subject);
   BlastCoreAuxStructFree(aux_struct);
   /* Now sort the hit lists for all queries */
   BLAST_SortResults(results);

   if (hit_options->is_gapped) {
      status = 
         BLAST_ComputeTraceback(program_number, results, query, query_info,
            rdfp, gap_align, score_options, ext_params, hit_params);
   }

   /* Do not destruct score block here */
   gap_align->sbp = NULL;
   BLAST_GapAlignStructFree(gap_align);

   query = BlastSequenceBlkFree(query);
   hit_params = MemFree(hit_params);
   ext_params = MemFree(ext_params);
   word_params = MemFree(word_params);

   return status;
}

Int4 
BLAST_TwoSequencesEngine(Uint1 program_number, BLAST_SequenceBlkPtr query, 
   BlastQueryInfoPtr query_info, BLAST_SequenceBlkPtr subject, 
   BLAST_ScoreBlkPtr sbp, const BlastScoringOptionsPtr score_options, 
   LookupTableWrapPtr lookup_wrap,
   const BlastInitialWordOptionsPtr word_options, 
   const BlastExtensionOptionsPtr ext_options, 
   const BlastHitSavingOptionsPtr hit_options, 
   const BlastEffectiveLengthsOptionsPtr eff_len_options,
   const PSIBlastOptionsPtr psi_options, 
   const BlastDatabaseOptionsPtr db_options,
   BlastResultsPtr results, BlastReturnStatPtr return_stats)
{
   BlastCoreAuxStructPtr aux_struct = NULL;
   BlastHSPListPtr hsp_list; 
   BlastHitSavingParametersPtr hit_params; 
   BlastInitialWordParametersPtr word_params; 
   BlastExtensionParametersPtr ext_params;
   BlastGapAlignStructPtr gap_align;
   Int2 status = 0;

   if (!subject)
      return -1;

   if ((status = 
        BLAST_SetUpAuxStructures(program_number, score_options, 
           eff_len_options, lookup_wrap, word_options, ext_options, 
           hit_options, db_options, query, query_info, 
           sbp, subject->length, &gap_align, &word_params, &ext_params, 
           &hit_params, &aux_struct)) != 0)
      return status;

   FillReturnXDropoffsInfo(return_stats, word_params, ext_params);

   BLAST_SearchEngineCore(program_number, query, query_info, subject, 
      lookup_wrap, gap_align, score_options, word_params, ext_params, 
      hit_params, psi_options, db_options, return_stats, aux_struct, 
      &hsp_list);

   if (hsp_list && hsp_list->hspcnt > 0) {
      return_stats->prelim_gap_passed += hsp_list->hspcnt;
      /* Save the HSPs into a hit list */
      BLAST_SaveHitlist(program_number, query, subject, results, 
         hsp_list, hit_params, query_info, gap_align->sbp, 
         score_options, NULL, NULL);
   }
   BlastCoreAuxStructFree(aux_struct);
   word_params = (BlastInitialWordParametersPtr) MemFree(word_params);

   if (hit_options->is_gapped) {
      status = 
         BLAST_TwoSequencesTraceback(program_number, results, query, 
            query_info, subject, gap_align, score_options, ext_params, 
            hit_params);
   }

   /* Do not destruct score block here */
   gap_align->sbp = NULL;
   BLAST_GapAlignStructFree(gap_align);

   query = BlastSequenceBlkFree(query);
   ext_params = (BlastExtensionParametersPtr) MemFree(ext_params);
   hit_params = (BlastHitSavingParametersPtr) MemFree(hit_params);

   return status;
}

Int2 LookupTableWrapInit(BLAST_SequenceBlkPtr query, 
        const LookupTableOptionsPtr lookup_options,	
        ValNodePtr lookup_segments, BLAST_ScoreBlkPtr sbp, 
        LookupTableWrapPtr PNTR lookup_wrap_ptr)
{
   LookupTableWrapPtr lookup_wrap;

   /* Construct the lookup table. */
   *lookup_wrap_ptr = lookup_wrap = MemNew(sizeof(LookupTableWrap));
   lookup_wrap->lut_type = lookup_options->lut_type;

   switch ( lookup_options->lut_type ) {
   case AA_LOOKUP_TABLE:
      BlastAaLookupNew(lookup_options, (LookupTablePtr *)
                       &lookup_wrap->lut);
      BlastAaLookupIndexQueries( (LookupTablePtr) lookup_wrap->lut,
                                 sbp->matrix, query, lookup_segments, 1);
      _BlastAaLookupFinalize((LookupTablePtr) lookup_wrap->lut);
      break;
   case MB_LOOKUP_TABLE:
      MB_LookupTableNew(query, lookup_segments, 
         (MBLookupTablePtr *) &(lookup_wrap->lut), lookup_options);
      break;
   case NA_LOOKUP_TABLE:
      LookupTableNew(lookup_options, 
         (LookupTablePtr *) &(lookup_wrap->lut), FALSE);
	    
      BlastNaLookupIndexQuery((LookupTablePtr) lookup_wrap->lut, query,
                              lookup_segments);
      _BlastAaLookupFinalize((LookupTablePtr) lookup_wrap->lut);
      break;
   default:
      {
         /* FIXME - emit error condition here */
      }
   } /* end switch */

   return 0;
}
