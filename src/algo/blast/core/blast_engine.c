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

Int4 
BLAST_SearchEngineCore(BLAST_SequenceBlkPtr query, 
        LookupTableWrapPtr lookup, BlastQueryInfoPtr query_info,
        ReadDBFILEPtr rdfp, BLAST_SequenceBlkPtr subject_blk, 
        BLAST_ExtendWordPtr ewp, BlastGapAlignStructPtr gap_align, 
        BlastScoringOptionsPtr score_options, 
        BlastInitialWordParametersPtr word_params, 
        BlastExtensionParametersPtr ext_params, 
        BlastHitSavingParametersPtr hit_params, 
        BlastResultsPtr PNTR results_ptr,
        BlastReturnStatPtr return_stats)
{
   Int4 oid, index;
   BLAST_SequenceBlkPtr subject = NULL;
   Int4 num_chunks, chunk, total_subject_length, offset;
   BlastInitHitListPtr init_hitlist;
   Boolean blastp = (lookup->lut_type == AA_LOOKUP_TABLE);
   Boolean mb_lookup = (lookup->lut_type == MB_LOOKUP_TABLE);
   BlastInitialWordOptionsPtr word_options = word_params->options;
   Boolean ag_blast = (Boolean)
      (word_options->extend_word_method & EXTEND_WORD_AG);
   Int4 max_hits = 0, total_hits = 0, num_hits;
   BlastHSPListPtr hsp_list, combined_hsp_list, full_hsp_list;
   BlastResultsPtr results;
   Int2 status;
   BlastHitSavingOptionsPtr hit_options = hit_params->options;
   Int2 (*GetGappedScore) (BLAST_SequenceBlkPtr, /* query */ 
			   BLAST_SequenceBlkPtr, /* subject */ 
			   BlastGapAlignStructPtr, /* gap_align */
			   BlastScoringOptionsPtr, /* score_options */
			   BlastExtensionParametersPtr, /* ext_params */
			   BlastHitSavingParametersPtr, /* hit_params */
			   BlastInitHitListPtr, /* init_hitlist */
			   BlastHSPListPtr PNTR); /* hsp_list_ptr */

   Int4 (*wordfinder) (BLAST_SequenceBlkPtr, /*subject*/
		       BLAST_SequenceBlkPtr, /*query*/
		       LookupTableWrapPtr, /* lookup wrapper */
                       Int4Ptr PNTR, /* matrix */
		       BlastInitialWordParametersPtr, /* word options */
		       BLAST_ExtendWordPtr, /* extend word */
		       Uint4Ptr, /* query offsets */
		       Uint4Ptr, /* subject offsets */
		       Int4, /* offset_array_size */
		       BlastInitHitListPtr); /* initial hitlist */
   Uint4Ptr query_offsets, subject_offsets;
   Int4 offset_array_size = 0;
   Uint1 program_number = gap_align->program;
   Boolean translated_subject;
   Int2 frame, frame_min, frame_max;
   Int4 orig_length = 0;
   Uint1Ptr orig_sequence = NULL, translation_buffer = NULL;
   Uint1Ptr translation_table = NULL, translation_table_rc = NULL;
   Int4 num_init_hsps = 0;
   Int4 num_hsps = 0;
   Int4 num_good_hsps = 0;
   BlastThrInfoPtr thr_info = NULL;
   Int4Ptr oid_list = NULL;
   Int4 start = 0, stop = 0, oid_list_length = 0;
   Boolean done = FALSE, use_oid_list = FALSE;
   
   /* search prologue */

   /* pick which gapped alignment algorithm to use */
   if (ext_params->options->algorithm_type == EXTEND_DYN_PROG)
     GetGappedScore = BLAST_GetGappedScore;
   else
     GetGappedScore = BLAST_MbGetGappedScore;

   if (mb_lookup) {
      wordfinder = MB_WordFinder;
      offset_array_size = OFFSET_ARRAY_SIZE + 
         ((MBLookupTablePtr)lookup->lut)->longest_chain;
   } else {
      if (blastp) {
         wordfinder = BlastAaWordFinder;
      } else if (ag_blast) {
         wordfinder = BlastNaWordFinder_AG;
      } else {
         wordfinder = BlastNaWordFinder;
      }
      offset_array_size = OFFSET_ARRAY_SIZE + 
         ((LookupTablePtr)lookup->lut)->longest_chain;
   }


   query_offsets = Malloc(offset_array_size * sizeof(Uint4));
   subject_offsets = Malloc(offset_array_size * sizeof(Uint4));

   /* end prologue */

   BLAST_ResultsInit(query_info->num_queries, results_ptr);
   results = *results_ptr;

   init_hitlist = BLAST_InitHitListNew();
   hsp_list = BlastHSPListNew();

   translated_subject = (program_number == blast_type_tblastn
                         || program_number == blast_type_tblastx
                         || program_number == blast_type_psitblastn);
   if (translated_subject) {
      /** DATABASE GENETIC CODE OPTION IS NOT PASSED HERE!!!
          THIS MUST BE CORRECTED IN THE FUTURE */
      CharPtr genetic_code=NULL;
      ValNodePtr vnp;
      GeneticCodePtr gcp;
      Int4 length;
      /* Preallocate buffer for the translated sequences */
      if (rdfp)
         length = readdb_get_maxlen(rdfp);
      else
         length = subject_blk->length;

      translation_buffer = (Uint1Ptr) Malloc(3 + length/CODON_LENGTH);

      gcp = GeneticCodeFind(1, NULL);
      for (vnp = (ValNodePtr)gcp->data.ptrvalue; vnp != NULL; 
           vnp = vnp->next) {
         if (vnp->choice == 3) {  /* ncbieaa */
            genetic_code = (CharPtr)vnp->data.ptrvalue;
            break;
         }
      }

      translation_table = GetPrivatTranslationTable(genetic_code, FALSE);
      translation_table_rc = GetPrivatTranslationTable(genetic_code, TRUE);
   }

   /* Allocate subject sequence block once for the entire run */
   subject = (BLAST_SequenceBlkPtr) MemNew(sizeof(BLAST_SequenceBlk));

   if (rdfp) {
      thr_info = BLAST_ThrInfoNew(rdfp);
      if (BlastGetVirtualOIDList(rdfp))
         oid_list = MemNew((thr_info->db_chunk_size+33)*sizeof(Int4));
   }

   while (!done) {
      /* Get the next chunk of the database */
      if (rdfp) {
         done = BLAST_GetDbChunk(rdfp, &start, &stop, oid_list, 
                                &oid_list_length, thr_info);
         use_oid_list = (oid_list && (oid_list_length > 0));
         if (use_oid_list) {
            start = 0;
            stop = oid_list_length;
         }
      } else {
         done = TRUE;
         start = 0; 
         stop = 1;
      }

      /* iterate over all subject sequences */
      for (index = start; index < stop; index++) {
         oid = (use_oid_list ? oid_list[index] : index);

      if (rdfp) {
         /* Retrieve subject sequence in ncbistdaa for proteins or ncbi2na 
            for nucleotides */
         MakeBlastSequenceBlk(rdfp, &subject, oid, BLASTP_ENCODING);
      } else {
         MemCpy(subject, subject_blk, sizeof(BLAST_SequenceBlk));
      }

      if (translated_subject) {
         frame_min = -3;
         frame_max = 3;
      } else if (program_number == blast_type_blastn) {
         frame_min = 1;
         frame_max = 1;
      } else {
         frame_min = 0;
         frame_max = 0;
      }

      orig_length = subject->length;
      orig_sequence = subject->sequence;
      if (translated_subject) {
         subject->sequence = translation_buffer;
      }

      full_hsp_list = NULL;

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
            
            num_hits = wordfinder(subject,
                                  query,
                                  lookup,
                                  gap_align->sbp->matrix,
                                  word_params,
                                  ewp,
                                  query_offsets,
                                  subject_offsets,
                                  OFFSET_ARRAY_SIZE,
                                  init_hitlist);
            
            if (init_hitlist->total == 0)
               continue;
            
            max_hits = MAX(max_hits, num_hits);
            total_hits += num_hits;
            num_init_hsps += init_hitlist->total;
            
            if (score_options->gapped_calculation) {
               GetGappedScore(query,
                              subject,
                              gap_align,
                              score_options,
                              ext_params,
                              hit_params,
                              init_hitlist,
                              &hsp_list);
            } else {
               BLAST_GetUngappedHSPList(init_hitlist, subject,
                                        hit_params->options, &hsp_list);
            }

            if (hsp_list->hspcnt == 0)
               continue;
            
            num_hsps += hsp_list->hspcnt;
            
            /* The subject ordinal id is not yet filled in this HSP list */
            hsp_list->oid = oid;
            
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
                   (hsp_list->traceback_done || !hit_options->is_gapped),
                   FALSE)) {
               /* HSPs from this list are moved elsewhere, reset count back 
                  to 0 */
               hsp_list->hspcnt = 0;
            }
            offset += subject->length - DBSEQ_CHUNK_OVERLAP;
            subject->sequence += 
               (subject->length - DBSEQ_CHUNK_OVERLAP)/COMPRESSION_RATIO;
         } /* End loop on chunks of subject sequence */
         
         MergeHSPLists(combined_hsp_list, &full_hsp_list, 0, FALSE, TRUE);
         
      } /* End loop on frames */
      
      if (!full_hsp_list || full_hsp_list->hspcnt == 0)
         continue;

      num_good_hsps += full_hsp_list->hspcnt;

      if (rdfp) {
         /* Restore the original contents of the subject block */
         subject->length = orig_length;
         subject->sequence = orig_sequence;
      }

      /* Save the HSPs into a hit list */
      BLAST_SaveHitlist(program_number, query, subject, results, 
         full_hsp_list, hit_params, query_info, gap_align->sbp, 
         score_options, rdfp, NULL);

      if (rdfp) {
         BlastSequenceBlkClean(subject);
      }
   }
   }

   if (rdfp) {
      BLAST_ThrInfoFree(thr_info);
      oid_list = MemFree(oid_list);
   }
   subject = MemFree(subject);
   BLAST_InitHitListDestruct(init_hitlist);
   BlastHSPListFree(hsp_list);

   if (translated_subject) {
      MemFree(translation_table);
      MemFree(translation_table_rc);
      MemFree(translation_buffer);
   }

   /* Now sort the hit lists for all queries */
   BLAST_SortResults(results);

   /* epilogue */

   MemFree(query_offsets); query_offsets = NULL;
   MemFree(subject_offsets); subject_offsets = NULL;

   /* end epilogue */
   return_stats->db_hits = total_hits;
   return_stats->init_extends = num_init_hsps;
   return_stats->good_init_extends = num_hsps;
   return_stats->prelim_gap_passed = num_good_hsps;
   
   return total_hits;
}

/** Function to calculate effective query length and db length as well as
 * effective search space. 
 * @param program_number blastn, blastp, blastx, etc. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_options used to calc. effective lengths [in]
 * @param sbp Karlin-Altschul parameters [out]
 * @param query_info The query information block, which stores the effective
 *                   search spaces for all queries [in] [out]
*/
static Int2 BLAST_CalcEffLengths (const Uint1 program_number, 
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
         
         min_query_length = (Int4) 1/(kbp->K);

         for (i=0; i<5; i++) {
            if (program_number != blast_type_blastn && 
                scoring_options->gapped_calculation) {
               length_adjustment = Nlm_Nint((((kbp->logK)+log((Nlm_FloatHi)(query_length-last_length_adjustment)*(Nlm_FloatHi)MAX(db_num_seqs, db_length-db_num_seqs*last_length_adjustment)))*alpha/kbp->Lambda) + beta);
            } else {
               length_adjustment = (Int4) (kbp->logK+log((Nlm_FloatHi)(query_length-last_length_adjustment)*(Nlm_FloatHi)MAX(1, db_length-db_num_seqs*last_length_adjustment)))/(kbp->H);
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

/** Setup of the auxiliary BLAST structures: lookup table, diagonal table for 
 * word extension, structure with memory for gapped alignment; also calculates
 * internally used parameters from options. 
 * @param program blastn, blastp, blastx, etc. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_options  used to calc. eff len. [in]
 * @param lookup_options options for lookup table. [in]
 * @param word_options options for initial word finding. [in]
 * @param ext_options options for gapped extension. [in]
 * @param hit_options options for saving hits. [in]
 * @param query The query sequence block [in]
 * @param lookup_segments Start/stop locations for non-masked query 
 *                        segments [in]
 * @param query_info The query information block [in]
 * @param sbp Contains scoring information. [in]
 * @param rdfp Pointer to database structure [in]
 * @param subject Subject sequence block (in 2 sequences case) [in]
 * @param lookup_wrap Lookup table [out]
 * @param ewp Word extension information and allocated memory [out]
 * @param gap_align Gapped alignment information and allocated memory [out]
 * @param word_params Parameters for initial word processing [out]
 * @param ext_params Parameters for gapped extension [out]
 * @param hit_params Parameters for saving hits [out]
 */
static Int2 
BLAST_SetUpAuxStructures(const Uint1 program,
   const BlastScoringOptionsPtr scoring_options,
   const BlastEffectiveLengthsOptionsPtr eff_len_options,
   const LookupTableOptionsPtr lookup_options,	
   const BlastInitialWordOptionsPtr word_options,
   const BlastExtensionOptionsPtr ext_options,
   const BlastHitSavingOptionsPtr hit_options,
   BLAST_SequenceBlkPtr query, ValNodePtr lookup_segments,
   BlastQueryInfoPtr query_info, BLAST_ScoreBlkPtr sbp, 
   ReadDBFILEPtr rdfp, BLAST_SequenceBlkPtr subject,
   LookupTableWrapPtr PNTR lookup_wrap, BLAST_ExtendWordPtr PNTR ewp,
   BlastGapAlignStructPtr PNTR gap_align, 
   BlastInitialWordParametersPtr PNTR word_params,
   BlastExtensionParametersPtr PNTR ext_params,
   BlastHitSavingParametersPtr PNTR hit_params)
{
   Int2 status = 0;

   /* Construct the lookup table. */
   *lookup_wrap = MemNew(sizeof(LookupTableWrap));

   switch ( lookup_options->lut_type ) {
   case AA_LOOKUP_TABLE:
      (*lookup_wrap)->lut_type = AA_LOOKUP_TABLE;
      BlastAaLookupNew(lookup_options, (LookupTablePtr *)
                       &((*lookup_wrap)->lut));
      BlastAaLookupIndexQueries( (LookupTablePtr) (*lookup_wrap)->lut,
	 sbp->matrix, query, lookup_segments, 1);
      _BlastAaLookupFinalize((LookupTablePtr) (*lookup_wrap)->lut);
      break;
   case MB_LOOKUP_TABLE:
      (*lookup_wrap)->lut_type = MB_LOOKUP_TABLE;
	    
      MB_LookupTableNew(query, lookup_segments, 
         (MBLookupTablePtr *) &((*lookup_wrap)->lut), lookup_options);
      break;
   case NA_LOOKUP_TABLE:
      (*lookup_wrap)->lut_type = NA_LOOKUP_TABLE;
	    
      LookupTableNew(lookup_options, 
         (LookupTablePtr *) &((*lookup_wrap)->lut), FALSE);
	    
      BlastNaLookupIndexQuery((LookupTablePtr) (*lookup_wrap)->lut, query,
                              lookup_segments);
      _BlastAaLookupFinalize((LookupTablePtr) (*lookup_wrap)->lut);
      break;
   default:
      {
         /* FIXME - emit error condition here */
      }
   } /* end switch */

   if (ewp && (status = BLAST_ExtendWordInit(query, word_options, 
                           eff_len_options->db_length, 
                           eff_len_options->dbseq_num, ewp)) != 0)
      return status;

   if ((status = BLAST_CalcEffLengths(program, scoring_options,
                    eff_len_options, sbp, query_info)) != 0)
      return status;

   BlastExtensionParametersNew(program, ext_options, sbp, query_info, 
                               ext_params);

   BlastHitSavingParametersNew(hit_options, NULL, sbp, query_info,
                               hit_params);

   if ((status = BLAST_CalcEffLengths(program, scoring_options, 
                    eff_len_options, sbp, query_info)) != 0)
      return status;

   BlastInitialWordParametersNew(word_options, *hit_params, *ext_params,
      sbp, query_info, eff_len_options, word_params);

   if ((status = BLAST_GapAlignStructNew(scoring_options, *ext_params, 1, 
                    rdfp, subject, query->length, program, sbp,
                    gap_align))) {
      ErrPostEx(SEV_ERROR, 0, 0, 
                "Cannot allocate memory for gapped extension");
      return status;
   }
   return status;
}

Int2 
BLAST_SearchEngine(const Uint1 blast_program, BLAST_SequenceBlkPtr query, 
   BlastQueryInfoPtr query_info, ReadDBFILEPtr rdfp, 
   BLAST_SequenceBlkPtr subject, BLAST_ScoreBlkPtr sbp, 
   BlastScoringOptionsPtr score_options, LookupTableOptionsPtr lookup_options,
   ValNodePtr lookup_segments, BlastInitialWordOptionsPtr word_options, 
   BlastExtensionOptionsPtr ext_options, 
   BlastEffectiveLengthsOptionsPtr eff_len_options,
   BlastHitSavingOptionsPtr hit_options, 
   BlastInitialWordParametersPtr PNTR word_params_ptr, 
   BlastExtensionParametersPtr PNTR ext_params_ptr, 
   BlastResultsPtr PNTR results, BlastReturnStatPtr return_stats)
{
   BlastExtensionParametersPtr ext_params = NULL;
   BlastInitialWordParametersPtr word_params = NULL;
   BlastHitSavingParametersPtr hit_params = NULL; 
   Int4 total_hits;
   LookupTableWrapPtr lookup = NULL;
   BLAST_ExtendWordPtr ewp = NULL;
   BlastGapAlignStructPtr gap_align = NULL;
   Int2 status = 0;

   BLAST_SetUpAuxStructures(blast_program, score_options, eff_len_options,
      lookup_options, word_options, ext_options, hit_options, query,
      lookup_segments, query_info, sbp, rdfp, subject, &lookup, &ewp,
      &gap_align, &word_params, &ext_params, &hit_params);

   /* The main part of the search, producing intermediate results, with 
      or without traceback */
   total_hits = 
      BLAST_SearchEngineCore(query, lookup, query_info, rdfp, subject,
         ewp, gap_align, score_options, word_params, ext_params, 
         hit_params, results, return_stats);

   BlastExtendWordFree(ewp);

   if (hit_options->is_gapped) {
      status = 
         BLAST_ComputeTraceback(*results, query, query_info, rdfp, subject,
            gap_align, score_options, ext_params, hit_params);
   }

   /* Do not destruct score block here */
   gap_align->sbp = NULL;
   BLAST_GapAlignStructFree(gap_align);

   BlastLookupTableDestruct(lookup);
   query = BlastSequenceBlkFree(query);
   hit_params = MemFree(hit_params);
   *ext_params_ptr = ext_params;
   *word_params_ptr = word_params;
   
   return 0;
}

