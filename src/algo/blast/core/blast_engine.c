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

#ifdef GAPPED_LOG
/** Print out the initial hits information. Used for debugging only, will be 
 * removed later.
 * @param hsp_list A structure holding an array of HSPs [in]
 * @param query_info Query information structure [in]
 * @param filename Name of the output file [in]
 * @param first_time Is this the first time the output file will be opened? [in]
 */
static Boolean WriteLogInfo(BlastHSPListPtr hsp_list, BlastQueryInfoPtr query_info,
                            CharPtr filename, Int4 oid, Boolean first_time)
{
   Int4 query_length, i;
   Int4 q_start, q_end, s_start, s_end;
   FILE *logfp2;

   if (!hsp_list || hsp_list->hspcnt == 0)
      return first_time;

   query_length = SeqLocLen(query_info->query_slp);

   if (first_time) {
      logfp2 = FileOpen(filename, "w");
   } else {
      logfp2 = FileOpen(filename, "a");
   }

   fprintf(logfp2, "# Sequence %ld\n", (long)oid);
   for (i=0; i<hsp_list->hspcnt; ++i) {
      if (hsp_list->hsp_array[i]->query.offset > query_length) {
         q_end = 2*query_length -
            hsp_list->hsp_array[i]->query.offset + 1;
         q_start = 2*query_length - hsp_list->hsp_array[i]->query.end + 2;
         s_start = hsp_list->hsp_array[i]->subject.end;
         s_end = hsp_list->hsp_array[i]->subject.offset + 1;
      } else {
         q_start = hsp_list->hsp_array[i]->query.offset + 1;
         q_end = hsp_list->hsp_array[i]->query.end;
         s_start = hsp_list->hsp_array[i]->subject.offset + 1;
         s_end = hsp_list->hsp_array[i]->subject.end;
      }
      fprintf(logfp2, "%ld [%ld %ld] [%ld %ld] %ld\n", 
              (long)(oid + 1), (long)q_start, (long)q_end, (long)s_start, 
              (long)s_end, (long)hsp_list->hsp_array[i]->score);
   }
   FileClose(logfp2);

   return FALSE;
}

Int4 
BLAST_SearchEngineCore(BLAST_SequenceBlkPtr query, 
        LookupTableWrapPtr lookup, BlastQueryInfoPtr query_info,
        ReadDBFILEPtr db, Int4 numseqs, 
        BLAST_ExtendWordPtr ewp, BlastGapAlignStructPtr gap_align, 
        BlastScoringOptionsPtr score_options, 
        BlastInitialWordParametersPtr word_params, 
        BlastExtensionParametersPtr ext_params, 
        BlastHitSavingParametersPtr hit_params, 
        BlastResultsPtr PNTR results_ptr, CharPtr logname)
#else
Int4 
BLAST_SearchEngineCore(BLAST_SequenceBlkPtr query, 
        LookupTableWrapPtr lookup, BlastQueryInfoPtr query_info,
        ReadDBFILEPtr db, Int4 numseqs, 
        BLAST_ExtendWordPtr ewp, BlastGapAlignStructPtr gap_align, 
        BlastScoringOptionsPtr score_options, 
        BlastInitialWordParametersPtr word_params, 
        BlastExtensionParametersPtr ext_params, 
        BlastHitSavingParametersPtr hit_params, 
        BlastResultsPtr PNTR results_ptr)
#endif
{
   Int4 oid;
   BLAST_SequenceBlkPtr subject;
   Int4 num_chunks, chunk, total_subject_length, offset;
   BlastInitHitListPtr init_hitlist;
   Boolean blastp = (lookup->lut_type == AA_LOOKUP_TABLE);
   Boolean mb_lookup = (lookup->lut_type == MB_LOOKUP_TABLE);
   BlastInitialWordOptionsPtr word_options = word_params->options;
   Boolean ag_blast = (Boolean)
      (word_options->extend_word_method & EXTEND_WORD_AG);
   Int4 max_hits = 0, total_hits = 0, num_hits;
   BlastHSPListPtr hsp_list, combined_hsp_list, full_hsp_list;
#ifdef GAPPED_LOG  
   Boolean first_time = TRUE;
#endif
   BlastResultsPtr results;
   Int2 status;
   BlastHitSavingOptionsPtr hit_options = hit_params->options;
   Int2 (*GetGappedScore) (BLAST_SequenceBlkPtr, /* query */ 
			   BLAST_SequenceBlkPtr, /* subject */ 
			   BlastGapAlignStructPtr, /* gap_align */
			   BlastScoringOptionsPtr, /* score_options */
			   BlastExtensionParametersPtr, /* ext_params */
			   BlastHitSavingOptionsPtr, /* hit_options */
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
   
   /* search prologue */

   /* pick which gapped alignment algorithm to use */
   if (ext_params->options->algorithm_type == EXTEND_DYN_PROG)
     GetGappedScore = BLAST_GetGappedScore;
   else
     GetGappedScore = BLAST_MbGetGappedScore;

   /* pick which wordfinder to use */
   offset_array_size = 10000;
   wordfinder = BlastNaWordFinder;
   
   if (mb_lookup)
     {
       offset_array_size = 20000;
       wordfinder = MB_WordFinder;
     }

   if (ag_blast)
     wordfinder = BlastNaWordFinder_AG;

   if (blastp)
     {
       LookupTablePtr lut = lookup->lut;
       offset_array_size= MAX(1024, lut->longest_chain);
     wordfinder = BlastAaWordFinder;

#ifndef DIAG_ARRAY_SIZE_CORRECTION_MADE  
     { 
        Int4 max_len = readdb_get_maxlen(db);
        if (2*max_len > ewp->diag_table->diag_array_length) {
           ewp->diag_table->diag_array = 
              Realloc(ewp->diag_table->diag_array, 2*max_len*sizeof(DiagStruct));
           ewp->diag_table->diag_array_length = 2*max_len;
           DiagClear(ewp->diag_table);
        }
     }
     
#endif     

     }

   query_offsets = Malloc(offset_array_size * sizeof(Uint4));
   subject_offsets = Malloc(offset_array_size * sizeof(Uint4));

   /* end prologue */


   BLAST_ResultsInit(query_info->num_queries, results_ptr);
   results = *results_ptr;

   init_hitlist = BLAST_InitHitListNew();

   translated_subject = (program_number == blast_type_tblastn
                         || program_number == blast_type_tblastx
                         || program_number == blast_type_psitblastn);
   if (translated_subject) {
      /** DATABASE GENETIC CODE OPTION IS NOT PASSED HERE!!!
          THIS MUST BE CORRECTED IN THE FUTURE */
      CharPtr genetic_code;
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

   /* iterate over all subject sequences */
   for (oid = 0; oid < numseqs; oid++) {

#ifdef _DEBUG
      if ( (oid % 10000) == 0 ) printf("oid=%d\n",oid);
#endif

      /* Retrieve subject sequence in ncbistdaa for proteins or ncbi2na 
         for nucleotides */
      MakeBlastSequenceBlk(db, &subject, oid, BLASTP_ENCODING);
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
	 
         max_hits = MAX(max_hits, num_hits);
         total_hits += num_hits;
         
         hsp_list = NULL;

	 if (score_options->gapped_calculation)
	   GetGappedScore(query,
			  subject,
			  gap_align,
			  score_options,
			  ext_params,
			  hit_options,
			  init_hitlist,
			  &hsp_list);

         if (!hsp_list)
            continue;
         
         /* The subject ordinal id is not yet filled in this HSP list */
         hsp_list->oid = oid;
         
#ifdef GAPPED_LOG  
         if (logname)
            first_time = WriteLogInfo(hsp_list, query_info, logname, oid, 
                                      first_time);
#endif

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
            /* Destruct the unneeded hsp_list, but make sure the HSPs 
               themselves are not destructed */
            hsp_list->hspcnt = 0;
            hsp_list = BlastHSPListDestruct(hsp_list);
         }
         offset += subject->length - DBSEQ_CHUNK_OVERLAP;
         subject->sequence += 
            (subject->length - DBSEQ_CHUNK_OVERLAP)/COMPRESSION_RATIO;
      } /* End loop on chunks of subject sequence */

      MergeHSPLists(combined_hsp_list, &full_hsp_list, 0, FALSE, TRUE);

      } /* End loop on frames */
      
      /* Save the HSPs into a hit list */
      BLAST_SaveHitlist(program_number, query, subject, results, 
         full_hsp_list, hit_params, query_info, gap_align->sbp, 
         score_options, db, NULL);
   }

   BLAST_InitHitListDestruct(init_hitlist);

   MemFree(subject);

   /* Now sort the hit lists for all queries */
   BLAST_SortResults(results);

   /* epilogue */

   if (blastp)
     {
       DiagFree(ewp->diag_table);
     }

   MemFree(query_offsets); query_offsets = NULL;
   MemFree(subject_offsets); subject_offsets = NULL;

   /* end epilogue */
   
   return total_hits;
}

