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
#include <blast_engine.h>
#include <readdb.h>
#include <util.h>
#include <aa_ungapped.h>
#include <blast_util.h>
#include <blast_gapalign.h>

extern Uint1Ptr
GetPrivatTranslationTable PROTO((CharPtr genetic_code,
                                 Boolean reverse_complement));

Int4 
BLAST_SearchEngineCore(BLAST_SequenceBlkPtr query, 
        LookupTableWrapPtr lookup, BlastQueryInfoPtr query_info,
        ReadDBFILEPtr db, BLAST_SequenceBlkPtr subject_blk, Int4 numseqs, 
        BLAST_ExtendWordPtr ewp, BlastGapAlignStructPtr gap_align, 
        BlastScoringOptionsPtr score_options, 
        BlastInitialWordParametersPtr word_params, 
        BlastExtensionParametersPtr ext_params, 
        BlastHitSavingParametersPtr hit_params, 
        BlastResultsPtr PNTR results_ptr,
        BlastReturnStatPtr return_stats)
{
   Int4 oid;
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
   Int4 nucl_length = 0;
   Uint1Ptr nucl_sequence = NULL, translation_buffer = NULL;
   Uint1Ptr translation_table = NULL, translation_table_rc = NULL;
   Int4 num_init_hsps = 0;
   Int4 num_hsps = 0;
   Int4 num_good_hsps = 0;
   
   /* search prologue */

   /* pick which gapped alignment algorithm to use */
   if (ext_params->options->algorithm_type == EXTEND_DYN_PROG)
     GetGappedScore = BLAST_GetGappedScore;
   else
     GetGappedScore = BLAST_MbGetGappedScore;

   if (mb_lookup) {
      wordfinder = MB_WordFinder;
      offset_array_size = 
         MAX(OFFSET_ARRAY_SIZE, ((MBLookupTablePtr)lookup->lut)->longest_chain);
   } else {
      if (blastp) {
         wordfinder = BlastAaWordFinder;
      } else if (ag_blast) {
         wordfinder = BlastNaWordFinder_AG;
      } else {
         wordfinder = BlastNaWordFinder;
      }
      offset_array_size = 
         MAX(OFFSET_ARRAY_SIZE, ((LookupTablePtr)lookup->lut)->longest_chain);
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
      /* Preallocate buffer for the translated sequences */
      translation_buffer = (Uint1Ptr) 
         Malloc(3 + readdb_get_maxlen(db)/CODON_LENGTH);

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
   subject = (BLAST_SequenceBlkPtr) Malloc(sizeof(BLAST_SequenceBlk));

   /* iterate over all subject sequences */
   for (oid = 0; oid < numseqs; oid++) {

#ifdef _DEBUG
      if ( (oid % 10000) == 0 ) printf("oid=%d\n",oid);
#endif

      if (db) {
         /* Retrieve subject sequence in ncbistdaa for proteins or ncbi2na 
            for nucleotides */
         MakeBlastSequenceBlk(db, &subject, oid, BLASTP_ENCODING);
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

      if (translated_subject) {
         nucl_length = subject->length;
         nucl_sequence = subject->sequence;
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
                     nucl_length, nucl_sequence, frame, translation_buffer);
            } else {
               subject->length = 
                  BLAST_TranslateCompressedSequence(translation_table_rc,
                     nucl_length, nucl_sequence, frame, translation_buffer);
               
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
			       offset_array_size,
			       init_hitlist);
	 
         if (init_hitlist->total == 0)
            continue;

         max_hits = MAX(max_hits, num_hits);
         total_hits += num_hits;
         num_init_hsps += init_hitlist->total;

	 if (score_options->gapped_calculation)
	   GetGappedScore(query,
			  subject,
			  gap_align,
			  score_options,
			  ext_params,
			  hit_params,
			  init_hitlist,
			  &hsp_list);

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
            status = BLAST_GetNonSumStatsEvalue(program_number, 
                        query_info, hsp_list, hit_options, gap_align->sbp);

         /* Discard HSPs that don't pass the e-value test */
         status = BLAST_ReapHitlistByEvalue(hsp_list, hit_options);
         
         AdjustOffsetsInHSPList(hsp_list, offset);
         /* Allow merging of HSPs either if traceback is already available,
            or if it is an ungapped search */
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

      /* Save the HSPs into a hit list */
      BLAST_SaveHitlist(program_number, query, subject, results, 
         full_hsp_list, hit_params, query_info, gap_align->sbp, 
         score_options, db, NULL);

      if (db) {
         if (translated_subject) {
            /* Restore the original contents of the subject block */
            subject->length = nucl_length;
            subject->sequence = nucl_sequence;
         }
         BlastSequenceBlkClean(subject);
      }
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

   if (blastp)
      DiagFree(ewp->diag_table);

   MemFree(query_offsets); query_offsets = NULL;
   MemFree(subject_offsets); subject_offsets = NULL;

   /* end epilogue */
   return_stats->db_hits = total_hits;
   return_stats->init_extends = num_init_hsps;
   return_stats->good_init_extends = num_hsps;
   return_stats->prelim_gap_passed = num_good_hsps;
   
   return total_hits;
}

