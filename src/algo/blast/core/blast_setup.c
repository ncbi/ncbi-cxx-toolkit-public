static char const rcsid[] =
    "$Id$";
/* ===========================================================================
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
* ===========================================================================*/

/*****************************************************************************

File name: blast_setup.c

Author: Tom Madden

Contents: Utilities initialize/setup BLAST.

$Revision$

******************************************************************************/

#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_filter.h>
#include <algo/blast/core/blast_encoding.h>

/* See description in blast_setup.h */
Int2
BlastScoreBlkGappedFill(BlastScoreBlk * sbp,
                        const BlastScoringOptions * scoring_options,
                        Uint1 program, BlastQueryInfo * query_info)
{
    Int2 tmp_index;

    if (sbp == NULL || scoring_options == NULL)
        return 1;

    /* At this stage query sequences are nucleotide only for blastn */

    if (program == blast_type_blastn) {

        /* for blastn, duplicate the ungapped Karlin 
           structures for use in gapped alignments */

        for (tmp_index = query_info->first_context;
             tmp_index <= query_info->last_context; tmp_index++) {
            if (sbp->kbp_std[tmp_index] != NULL) {
                Blast_KarlinBlk *kbp_gap;
                Blast_KarlinBlk *kbp;
 
                sbp->kbp_gap_std[tmp_index] = Blast_KarlinBlkCreate();
                kbp_gap = sbp->kbp_gap_std[tmp_index];
                kbp     = sbp->kbp_std[tmp_index];

                kbp_gap->Lambda = kbp->Lambda;
                kbp_gap->K      = kbp->K;
                kbp_gap->logK   = kbp->logK;
                kbp_gap->H      = kbp->H;
                kbp_gap->paramC = kbp->paramC;
            }
        }

    } else {
        for (tmp_index = query_info->first_context;
             tmp_index <= query_info->last_context; tmp_index++) {
            sbp->kbp_gap_std[tmp_index] = Blast_KarlinBlkCreate();
            Blast_KarlinBlkGappedCalc(sbp->kbp_gap_std[tmp_index],
                                      scoring_options->gap_open,
                                      scoring_options->gap_extend,
                                      scoring_options->decline_align,
                                      sbp->name, NULL);
        }
    }

    sbp->kbp_gap = sbp->kbp_gap_std;
    return 0;
}

static Int2
PHIScoreBlkFill(BlastScoreBlk* sbp, const BlastScoringOptions* options,
   Blast_Message** blast_message)
{
   Blast_KarlinBlk* kbp;
   char buffer[1024];
   Int2 status = 0;

   sbp->read_in_matrix = TRUE;
   sbp->name = strdup(options->matrix);
   if ((status = BLAST_ScoreBlkMatFill(sbp, options->matrix_path)) != 0)
      return status;
   kbp = sbp->kbp_gap_std[0] = Blast_KarlinBlkCreate();
   sbp->kbp_std = sbp->kbp_gap_std;

   if (0 == strcmp("BLOSUM62", options->matrix)) {
      kbp->paramC = 0.50;
      if ((11 == options->gap_open) && (1 == options->gap_extend)) {
         kbp->Lambda = 0.270;
         kbp->K = 0.047;
         return status;
      }
      if ((9 == options->gap_open) && (2 == options->gap_extend)) {
         kbp->Lambda = 0.285;
         kbp->K = 0.075;
         return status;
      }
      if ((8 == options->gap_open) && (2 == options->gap_extend)) {
         kbp->Lambda = 0.265;
         kbp->K = 0.046;
         return status;
      }
      if ((7 == options->gap_open) && (2 == options->gap_extend)) {
         kbp->Lambda = 0.243;
         kbp->K = 0.032;
         return status;
      }
      if ((12 == options->gap_open) && (1 == options->gap_extend)) {
         kbp->Lambda = 0.281;
         kbp->K = 0.057;
         return status;
      }
      if ((10 == options->gap_open) && (1 == options->gap_extend)) {
         kbp->Lambda = 0.250;
         kbp->K = 0.033;
         return status;
      }
      sprintf(buffer, "The combination %d for gap opening cost and %d for gap extension is not supported in PHI-BLAST with matrix %s\n", options->gap_open, options->gap_extend, options->matrix);
      Blast_MessageWrite(blast_message, BLAST_SEV_WARNING, 2, 1, buffer);
   }
   else {
      if (0 == strcmp("PAM30", options->matrix)) { 
         kbp->paramC = 0.30;
         if ((9 == options->gap_open) && (1 == options->gap_extend)) {
            kbp->Lambda = 0.295;
            kbp->K = 0.13;
            return status;
         }
         if ((7 == options->gap_open) && (2 == options->gap_extend)) {
            kbp->Lambda = 0.306;
            kbp->K = 0.15;
            return status;
         }
         if ((6 == options->gap_open) && (2 == options->gap_extend)) {
            kbp->Lambda = 0.292;
            kbp->K = 0.13;
            return status;
         }
         if ((5 == options->gap_open) && (2 == options->gap_extend)) {
            kbp->Lambda = 0.263;
            kbp->K = 0.077;
            return status;
         }
         if ((10 == options->gap_open) && (1 == options->gap_extend)) {
            kbp->Lambda = 0.309;
            kbp->K = 0.15;
            return status;
         }
         if ((8 == options->gap_open) && (1 == options->gap_extend)) {
            kbp->Lambda = 0.270;
            kbp->K = 0.070;
            return status;
         }
         sprintf(buffer, "The combination %d for gap opening cost and %d for gap extension is not supported in PHI-BLAST with matrix %s\n", options->gap_open, options->gap_extend, options->matrix);
         Blast_MessageWrite(blast_message, BLAST_SEV_WARNING, 2, 1, buffer);
      }
      else {
         if (0 == strcmp("PAM70", options->matrix)) { 
            kbp->paramC = 0.35;
            if ((10 == options->gap_open) && (1 == options->gap_extend)) {
               kbp->Lambda = 0.291;
               kbp->K = 0.089;
               return status;
            }
            if ((8 == options->gap_open) && (2 == options->gap_extend)) {
               kbp->Lambda = 0.303;
               kbp->K = 0.13;
               return status;
            }
            if ((7 == options->gap_open) && (2 == options->gap_extend)) {
               kbp->Lambda = 0.287;
               kbp->K = 0.095;
               return status;
            }
            if ((6 == options->gap_open) && (2 == options->gap_extend)) {
               kbp->Lambda = 0.269;
               kbp->K = 0.079;
               return status;
            }
            if ((11 == options->gap_open) && (1 == options->gap_extend)) {
               kbp->Lambda = 0.307;
               kbp->K = 0.13;
               return status;
            }
            if ((9 == options->gap_open) && (1 == options->gap_extend)) {
               kbp->Lambda = 0.269;
               kbp->K = 0.058;
               return status;
            }
            sprintf(buffer, "The combination %d for gap opening cost and %d for gap extension is not supported in PHI-BLAST with matrix %s\n", options->gap_open, options->gap_extend, options->matrix);
            Blast_MessageWrite(blast_message, BLAST_SEV_WARNING, 2, 1, buffer);
         }
         else {
            if (0 == strcmp("BLOSUM80", options->matrix)) { 
               kbp->paramC = 0.40;
               if ((10 == options->gap_open) && (1 == options->gap_extend)) {
                  kbp->Lambda = 0.300;
                  kbp->K = 0.072;
                  return status;
               }
               if ((8 == options->gap_open) && (2 == options->gap_extend)) {
                  kbp->Lambda = 0.308;
                  kbp->K = 0.089;
                  return status;
               }
               if ((7 == options->gap_open) && (2 == options->gap_extend)) {
                  kbp->Lambda = 0.295;
                  kbp->K = 0.077;
                  return status;
               }
               if ((6 == options->gap_open) && (2 == options->gap_extend)) {
                  kbp->Lambda = 0.271;
                  kbp->K = 0.051;
                  return status;
               }
               if ((11 == options->gap_open) && (1 == options->gap_extend)) {
                  kbp->Lambda = 0.314;
                  kbp->K = 0.096;
                  return status;
               }
               if ((9 == options->gap_open) && (1 == options->gap_extend)) {
                  kbp->Lambda = 0.277;
                  kbp->K = 0.046;
                  return status;
               }
               sprintf(buffer, "The combination %d for gap opening cost and %d for gap extension is not supported in PHI-BLAST with matrix %s\n", options->gap_open, options->gap_extend, options->matrix);
               Blast_MessageWrite(blast_message, BLAST_SEV_WARNING, 2, 1, buffer);
            }
            else {
               if (0 == strcmp("BLOSUM45", options->matrix)) { 
                  kbp->paramC = 0.60;
                  if ((14 == options->gap_open) && (2 == options->gap_extend)) {
                     kbp->Lambda = 0.199;
                     kbp->K = 0.040;
                     return status;
                  }
                  if ((13 == options->gap_open) && (3 == options->gap_extend)) {
                     kbp->Lambda = 0.209;
                     kbp->K = 0.057;
                     return status;
                  }
                  if ((12 == options->gap_open) && (3 == options->gap_extend)) {
                     kbp->Lambda = 0.203;
                     kbp->K = 0.049;
                     return status;
                  }
                  if ((11 == options->gap_open) && (3 == options->gap_extend)) {
                     kbp->Lambda = 0.193;
                     kbp->K = 0.037;
                     return status;
                  }
                  if ((10 == options->gap_open) && (3 == options->gap_extend)) {
                     kbp->Lambda = 0.182;
                     kbp->K = 0.029;
                     return status;
                  }
                  if ((15 == options->gap_open) && (2 == options->gap_extend)) {
                     kbp->Lambda = 0.206;
                     kbp->K = 0.049;
                     return status;
                  }
                  if ((13 == options->gap_open) && (2 == options->gap_extend)) {
                     kbp->Lambda = 0.190;
                     kbp->K = 0.032;
                     return status;
                  }
                  if ((12 == options->gap_open) && (2 == options->gap_extend)) {
                     kbp->Lambda = 0.177;
                     kbp->K = 0.023;
                     return status;
                  }
                  if ((19 == options->gap_open) && (1 == options->gap_extend)) {
                     kbp->Lambda = 0.209;
                     kbp->K = 0.049;
                     return status;
                  }
                  if ((18 == options->gap_open) && (1 == options->gap_extend)) {
                     kbp->Lambda = 0.202;
                     kbp->K = 0.041;
                     return status;
                  }
                  if ((17 == options->gap_open) && (1 == options->gap_extend)) {
                     kbp->Lambda = 0.195;
                     kbp->K = 0.034;
                     return status;
                  }
                  if ((16 == options->gap_open) && (1 == options->gap_extend)) {
                     kbp->Lambda = 0.183;
                     kbp->K = 0.024;
                     return status;
                  }
                  sprintf(buffer, "The combination %d for gap opening cost and %d for gap extension is not supported in PHI-BLAST with matrix %s\n", options->gap_open, options->gap_extend, options->matrix);
                  Blast_MessageWrite(blast_message, BLAST_SEV_WARNING, 2, 1, buffer);
               }
               else {
                  sprintf(buffer, "Matrix %s not allowed in PHI-BLAST\n", options->matrix);
                  Blast_MessageWrite(blast_message, BLAST_SEV_WARNING, 2, 1, buffer);
               }
            }
         }
      }
   }
return status;
}

Int2
BlastScoreBlkMatrixInit(Uint1 program_number, 
                  const BlastScoringOptions* scoring_options,
                  BlastScoreBlk* sbp)
{
   Int2 status = 0;

   if (!sbp || !scoring_options)
      return 1;

   if (program_number == blast_type_blastn) {

      BLAST_ScoreSetAmbigRes(sbp, 'N');
      sbp->penalty = scoring_options->penalty;
      sbp->reward = scoring_options->reward;
      if (scoring_options->matrix_path &&
          *scoring_options->matrix_path != NULLB &&
          scoring_options->matrix && *scoring_options->matrix != NULLB) {

          sbp->read_in_matrix = TRUE;
          sbp->name = strdup(scoring_options->matrix);

       } else {
          char buffer[50];
          sbp->read_in_matrix = FALSE;
          sprintf(buffer, "blastn matrix:%ld %ld",
                  (long) sbp->reward, (long) sbp->penalty);
          sbp->name = strdup(buffer);
       }
       status = BLAST_ScoreBlkMatFill(sbp, scoring_options->matrix_path);

    } else {
       char* p = NULL;

       sbp->read_in_matrix = TRUE;
       BLAST_ScoreSetAmbigRes(sbp, 'X');
       sbp->name = strdup(scoring_options->matrix);
       /* protein matrices are in all caps by convention */
       for (p = sbp->name; *p != NULLB; p++)
          *p = toupper(*p);
       status = BLAST_ScoreBlkMatFill(sbp, scoring_options->matrix_path);
   }

   return status;
}


Int2 
BlastSetup_GetScoreBlock(BLAST_SequenceBlk* query_blk, BlastQueryInfo* query_info, const BlastScoringOptions* scoring_options, Uint1 program_number, Boolean phi_align, BlastScoreBlk* *sbpp, Blast_Message* *blast_message)
{
    BlastScoreBlk* sbp;
    Int2 status=0;      /* return value. */
    Int4 context; /* loop variable. */

    if (sbpp == NULL)
       return 1;

    if (program_number == blast_type_blastn)
       sbp = BlastScoreBlkNew(BLASTNA_SEQ_CODE, query_info->last_context + 1);
    else
       sbp = BlastScoreBlkNew(BLASTAA_SEQ_CODE, query_info->last_context + 1);

    if (!sbp)
       return 1;

    *sbpp = sbp;

    status = BlastScoreBlkMatrixInit(program_number, scoring_options, sbp);
    if (status != 0)
       return status;

    for (context = query_info->first_context;
         context <= query_info->last_context; ++context) {
      
        Int4 context_offset;
        Int4 query_length;
        Uint1 *buffer;              /* holds sequence */

        /* For each query, check if forward strand is present */
        if ((query_length = BLAST_GetQueryLength(query_info, context)) < 0)
            continue;

        context_offset = query_info->context_offsets[context];
        buffer = &query_blk->sequence[context_offset];

        if (!phi_align &&
           (status = BLAST_ScoreBlkFill(sbp, (char *) buffer,
                                        query_length, context))) {
            Blast_MessageWrite(blast_message, BLAST_SEV_ERROR, 2, 1, 
                   "Query completely filtered; nothing left to search");
            return status;
        }
    }


    /* Fills in block for gapped blast. */
    if (phi_align) {
       PHIScoreBlkFill(sbp, scoring_options, blast_message);
    } else if (scoring_options->gapped_calculation) {
       status = BlastScoreBlkGappedFill(sbp, scoring_options, 
                                        program_number, query_info);
       if (status) {
          Blast_MessageWrite(blast_message, BLAST_SEV_ERROR, 2, 1, 
                             "Unable to initialize scoring block");
          return status;
       }
    }
   
    /* Get "ideal" values if the calculated Karlin-Altschul params bad. */
    if (program_number == blast_type_blastx ||
        program_number == blast_type_tblastx ||
        program_number == blast_type_rpstblastn) {
        /* Adjust the ungapped Karlin parameters */
        sbp->kbp = sbp->kbp_std;
        Blast_KarlinBlkStandardCalc(sbp, query_info->first_context,
                                    query_info->last_context);
    }

    if (program_number == blast_type_blastx ||
        program_number == blast_type_tblastx) {
        /* Adjust the gapped Karlin parameters, if it is a gapped search */
        if (scoring_options->gapped_calculation) {
           sbp->kbp = sbp->kbp_gap_std;
          Blast_KarlinBlkStandardCalc(sbp, query_info->first_context,
                                       query_info->last_context);
        }
    }

    /* Why are there so many Karlin block names?? */
    sbp->kbp = sbp->kbp_std;
    if (scoring_options->gapped_calculation)
       sbp->kbp_gap = sbp->kbp_gap_std;

    return 0;
}

Int2 BLAST_MainSetUp(Uint1 program_number,
                     const QuerySetUpOptions * qsup_options,
                     const BlastScoringOptions * scoring_options,
                     const BlastHitSavingOptions * hit_options,
                     BLAST_SequenceBlk * query_blk,
                     BlastQueryInfo * query_info,
                     BlastSeqLoc ** lookup_segments, BlastMaskLoc * *filter_out,
                     BlastScoreBlk * *sbpp, Blast_Message * *blast_message)
{
    Boolean mask_at_hash = FALSE; /* mask only for making lookup table? */
    Int2 status = 0;            /* return value */
    BlastMaskLoc *filter_maskloc = NULL;   /* Local variable for mask locs. */


    if ((status=BlastSetUp_GetFilteringLocations(query_blk, query_info, program_number, qsup_options->filter_string, 
       &filter_maskloc, &mask_at_hash, blast_message)))
    {
        return status;
    } 

    if (!mask_at_hash)
    {
        if ((status=BlastSetUp_MaskQuery(query_blk, query_info, filter_maskloc, program_number)) != 0)
            return status;
    }

    /* If there was a lower case mask, its contents have now been moved to 
     * filter_maskloc and are no longer needed in the query block.
    */
    query_blk->lcase_mask = NULL;

    if (filter_out)
       *filter_out = filter_maskloc;
    else 
        filter_maskloc = BlastMaskLocFree(filter_maskloc);

    if (program_number == blast_type_blastx && scoring_options->is_ooframe) {
        BLAST_InitDNAPSequence(query_blk, query_info);
    }

    BLAST_ComplementMaskLocations(program_number, query_info, filter_maskloc, lookup_segments);


    if ((status=BlastSetup_GetScoreBlock(query_blk, query_info, scoring_options, program_number, 
           hit_options->phi_align, sbpp, blast_message)) > 0)
        return status;

    return 0;
}


Int2 BLAST_CalcEffLengths (Uint1 program_number, 
   const BlastScoringOptions* scoring_options,
   const BlastEffectiveLengthsParameters* eff_len_params, 
   const BlastScoreBlk* sbp, BlastQueryInfo* query_info)
{
   double alpha=0, beta=0; /*alpha and beta for new scoring system */
   Int4 length_adjustment = 0;  /* length adjustment for current iteration. */
   Int4 index;		/* loop index. */
   Int4	db_num_seqs;	/* number of sequences in database. */
   Int8	db_length;	/* total length of database. */
   Blast_KarlinBlk* *kbp_ptr; /* Array of Karlin block pointers */
   Int4 query_length;   /* length of an individual query sequence */
   Int8 effective_search_space = 0; /* Effective search space for a given 
                                   sequence/strand/frame */
   const BlastEffectiveLengthsOptions* eff_len_options = eff_len_params->options;

   if (sbp == NULL)
      return 1;

   /* use overriding value from effective lengths options or the real value
      from effective lengths parameters. */
   if (eff_len_options->db_length > 0)
      db_length = eff_len_options->db_length;
   else
      db_length = eff_len_params->real_db_length;

   /* If database (subject) length is not available at this stage, 
      do nothing */
   if (db_length == 0)
      return 0;

   if (program_number == blast_type_tblastn || 
       program_number == blast_type_tblastx)
      db_length = db_length/3;	

   if (eff_len_options->dbseq_num > 0)
      db_num_seqs = eff_len_options->dbseq_num;
   else
      db_num_seqs = eff_len_params->real_num_seqs;
   
   if (program_number != blast_type_blastn) {
      if (scoring_options->gapped_calculation) {
         BLAST_GetAlphaBeta(sbp->name,&alpha,&beta,TRUE, 
            scoring_options->gap_open, scoring_options->gap_extend);
      }
   }
   
   if (scoring_options->gapped_calculation) 
      kbp_ptr = sbp->kbp_gap_std;
   else
      kbp_ptr = sbp->kbp_std; 
   
   for (index = query_info->first_context;
        index <= query_info->last_context;
        index++) {
       Blast_KarlinBlk * kbp;   /* statistical parameters for the
                                   current context */
       kbp = kbp_ptr[index];

      if (eff_len_options->searchsp_eff) {
         effective_search_space = eff_len_options->searchsp_eff;
      } else {
         if ( (query_length = BLAST_GetQueryLength(query_info, index)) <= 0) {
             continue;
         }
         /* Use the correct Karlin block. For blastn, two identical Karlin
            blocks are allocated for each sequence (one per strand), but we
            only need one of them.
         */
         if (program_number != blast_type_blastn &&
             scoring_options->gapped_calculation) {
             BLAST_ComputeLengthAdjustment(kbp->K, kbp->logK,
                                           alpha/kbp->Lambda, beta,
                                           query_length, db_length,
                                           db_num_seqs, &length_adjustment);
         } else {
             BLAST_ComputeLengthAdjustment(kbp->K, kbp->logK, 1/kbp->H, 0,
                                           query_length, db_length,
                                           db_num_seqs, &length_adjustment);
         }        
     
         effective_search_space =
             (query_length - length_adjustment) * 
             (db_length - db_num_seqs*length_adjustment);

         /* For translated RPS blast, the DB size is left unchanged
            and the query size is divided by 3 (for conversion to 
            a protein sequence) and multiplied by 6 (for 6 frames) */

         if (program_number == blast_type_rpstblastn)
            effective_search_space *= (Int8)(NUM_FRAMES / CODON_LENGTH);
      }
      query_info->eff_searchsp_array[index] = effective_search_space;
      query_info->length_adjustments[index] = length_adjustment;

   }

   return 0;
}

Int2 
BLAST_GapAlignSetUp(Uint1 program_number,
                    const BlastSeqSrc* seq_src,
                    const BlastScoringOptions* scoring_options,
                    const BlastEffectiveLengthsOptions* eff_len_options,
                    const BlastExtensionOptions* ext_options,
                    const BlastHitSavingOptions* hit_options,
                    BlastQueryInfo* query_info, 
                    BlastScoreBlk* sbp, 
                    BlastExtensionParameters** ext_params,
                    BlastHitSavingParameters** hit_params,
                    BlastEffectiveLengthsParameters** eff_len_params,
                    BlastGapAlignStruct** gap_align)
{
   Int2 status = 0;
   Uint4 max_subject_length;
   Int8 total_length;
   Int4 num_seqs;

   total_length = BLASTSeqSrcGetTotLen(seq_src);
   
   if (total_length > 0) {
      num_seqs = BLASTSeqSrcGetNumSeqs(seq_src);
   } else {
      /* Not a database search; each subject sequence is considered
         individually */
      num_seqs = 1;
   }

   /* Initialize the effective length parameters with real values of
      database length and number of sequences */
   BlastEffectiveLengthsParametersNew(eff_len_options, total_length, num_seqs, 
                                      eff_len_params);
   if ((status = BLAST_CalcEffLengths(program_number, scoring_options, 
                    *eff_len_params, sbp, query_info)) != 0)
      return status;

   BlastExtensionParametersNew(program_number, ext_options, sbp, 
                               query_info, ext_params);

   BlastHitSavingParametersNew(program_number, hit_options, *ext_params, 
                               sbp, query_info, hit_params);

   /* To initialize the gapped alignment structure, we need to know the 
      maximal subject sequence length */
   max_subject_length = BLASTSeqSrcGetMaxSeqLen(seq_src);

   if ((status = BLAST_GapAlignStructNew(scoring_options, *ext_params, 
                    max_subject_length, query_info->max_length, sbp, 
                    gap_align)) != 0) {
      return status;
   }

   return status;
}

Int2 BLAST_OneSubjectUpdateParameters(Uint1 program_number,
                    Uint4 subject_length,
                    const BlastScoringOptions* scoring_options,
                    BlastQueryInfo* query_info, 
                    BlastScoreBlk* sbp, 
                    const BlastExtensionParameters* ext_params,
                    BlastHitSavingParameters* hit_params,
                    BlastInitialWordParameters* word_params,
                    BlastEffectiveLengthsParameters* eff_len_params)
{
   Int2 status = 0;
   eff_len_params->real_db_length = subject_length;
   if ((status = BLAST_CalcEffLengths(program_number, scoring_options, 
                                      eff_len_params, sbp, query_info)) != 0)
      return status;
   /* Update cutoff scores in hit saving parameters */
   BlastHitSavingParametersUpdate(program_number, ext_params,
                                  sbp, query_info, hit_params);
   
   if (word_params) {
      /* Update cutoff scores in initial word parameters */
      BlastInitialWordParametersUpdate(program_number, hit_params, ext_params,
         sbp, query_info, subject_length, word_params);
   }
   return status;
}
