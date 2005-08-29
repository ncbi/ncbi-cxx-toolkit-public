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
 * Author: Tom Madden
 *
 */

/** @file blast_setup.c
 * Utilities initialize/setup BLAST.
 */


#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_filter.h>
#include <algo/blast/core/blast_encoding.h>
#include <algo/blast/core/phi_lookup.h>

/* See description in blast_setup.h */
Int2
Blast_ScoreBlkKbpGappedCalc(BlastScoreBlk * sbp,
                            const BlastScoringOptions * scoring_options,
                            EBlastProgramType program, 
                            const BlastQueryInfo * query_info,
                            Blast_Message** error_return)
{
    Int2 index = 0;

    if (sbp == NULL || scoring_options == NULL)
        return 1;

    /* Allocate and fill values for a gapped Karlin block, given the scoring
       options, then copy it for all the query contexts. */
    for (index = query_info->first_context;
         index <= query_info->last_context; index++) {
        Int2 retval = 0;
        sbp->kbp_gap_std[index] = Blast_KarlinBlkNew();

        /* At this stage query sequences are nucleotide only for blastn */
        if (program == eBlastTypeBlastn) {
            retval = 
                Blast_KarlinBlkNuclGappedCalc(sbp->kbp_gap_std[index],
                    scoring_options->gap_open, scoring_options->gap_extend, 
                    scoring_options->reward, scoring_options->penalty, 
                    sbp->kbp_std[index], error_return);
        } else {
            retval = 
                Blast_KarlinBlkGappedCalc(sbp->kbp_gap_std[index],
                    scoring_options->gap_open, scoring_options->gap_extend,
                    scoring_options->decline_align, sbp->name, error_return);
        }
        if (retval)
            return retval;

        /* For right now, copy the contents from kbp_gap_std to 
         * kbp_gap_psi (as in old code - BLASTSetUpSearchInternalByLoc) */
        if (program != eBlastTypeBlastn) {
            sbp->kbp_gap_psi[index] = Blast_KarlinBlkNew();
            Blast_KarlinBlkCopy(sbp->kbp_gap_psi[index], 
                                sbp->kbp_gap_std[index]);
        }
    }

    /* Set gapped Blast_KarlinBlk* alias */
    sbp->kbp_gap = (program == eBlastTypePsiBlast) ? 
        sbp->kbp_gap_psi : sbp->kbp_gap_std;

    return 0;
}

/** Fills a scoring block structure for a PHI BLAST search. 
 * @param sbp Scoring block structure [in] [out]
 * @param options Scoring options structure [in]
 * @param blast_message Structure for reporting errors [out]
 */
static Int2
s_PHIScoreBlkFill(BlastScoreBlk* sbp, const BlastScoringOptions* options,
   Blast_Message** blast_message)
{
   Blast_KarlinBlk* kbp;
   char buffer[1024];
   Int2 status = 0;

   sbp->read_in_matrix = TRUE;
   if ((status = Blast_ScoreBlkMatrixFill(sbp, options->matrix_path)) != 0)
      return status;
   kbp = sbp->kbp_gap_std[0] = Blast_KarlinBlkNew();
   /* Point both non-allocated Karlin block arrays to kbp_gap_std. */
   sbp->kbp_gap = sbp->kbp_gap_std;

   /* For PHI BLAST, the H value is not used, but it is not allowed to be 0, 
      so set it to 1. */
   kbp->H = 1.0;

   /* Ideal Karlin block is filled unconditionally. */
   status = Blast_ScoreBlkKbpIdealCalc(sbp);
   if (status)
      return status;

   if (0 == strcmp("BLOSUM62", options->matrix)) {
      kbp->paramC = 0.50;
      if ((11 == options->gap_open) && (1 == options->gap_extend)) {
         kbp->Lambda = 0.270;
         kbp->K = 0.047;
      } else if ((9 == options->gap_open) && (2 == options->gap_extend)) {
         kbp->Lambda = 0.285;
         kbp->K = 0.075;
      } else if ((8 == options->gap_open) && (2 == options->gap_extend)) {
         kbp->Lambda = 0.265;
         kbp->K = 0.046;
      } else if ((7 == options->gap_open) && (2 == options->gap_extend)) {
         kbp->Lambda = 0.243;
         kbp->K = 0.032;
      } else if ((12 == options->gap_open) && (1 == options->gap_extend)) {
         kbp->Lambda = 0.281;
         kbp->K = 0.057;
      } else if ((10 == options->gap_open) && (1 == options->gap_extend)) {
         kbp->Lambda = 0.250;
         kbp->K = 0.033;
      } else {
          status = -1;
      }
   } else if (0 == strcmp("PAM30", options->matrix)) { 
       kbp->paramC = 0.30;
       if ((9 == options->gap_open) && (1 == options->gap_extend)) {
           kbp->Lambda = 0.295;
           kbp->K = 0.13;
       } else if ((7 == options->gap_open) && (2 == options->gap_extend)) {
           kbp->Lambda = 0.306;
           kbp->K = 0.15;
           return status;
       } else if ((6 == options->gap_open) && (2 == options->gap_extend)) {
           kbp->Lambda = 0.292;
           kbp->K = 0.13;
       } else if ((5 == options->gap_open) && (2 == options->gap_extend)) {
           kbp->Lambda = 0.263;
           kbp->K = 0.077;
       } else if ((10 == options->gap_open) && (1 == options->gap_extend)) {
           kbp->Lambda = 0.309;
           kbp->K = 0.15;
       } else if ((8 == options->gap_open) && (1 == options->gap_extend)) {
           kbp->Lambda = 0.270;
           kbp->K = 0.070;
           return status;
       } else {
           status = -1;
       }
   } else if (0 == strcmp("PAM70", options->matrix)) { 
       kbp->paramC = 0.35;
       if ((10 == options->gap_open) && (1 == options->gap_extend)) {
           kbp->Lambda = 0.291;
           kbp->K = 0.089;
       } else if ((8 == options->gap_open) && (2 == options->gap_extend)) {
           kbp->Lambda = 0.303;
           kbp->K = 0.13;
       } else if ((7 == options->gap_open) && (2 == options->gap_extend)) {
           kbp->Lambda = 0.287;
           kbp->K = 0.095;
       } else if ((6 == options->gap_open) && (2 == options->gap_extend)) {
           kbp->Lambda = 0.269;
           kbp->K = 0.079;
       } else if ((11 == options->gap_open) && (1 == options->gap_extend)) {
           kbp->Lambda = 0.307;
           kbp->K = 0.13;
       } else if ((9 == options->gap_open) && (1 == options->gap_extend)) {
           kbp->Lambda = 0.269;
           kbp->K = 0.058;
       } else {
           status = -1;
       }
   } else if (0 == strcmp("BLOSUM80", options->matrix)) { 
       kbp->paramC = 0.40;
       if ((10 == options->gap_open) && (1 == options->gap_extend)) {
           kbp->Lambda = 0.300;
           kbp->K = 0.072;
       } else if ((8 == options->gap_open) && (2 == options->gap_extend)) {
           kbp->Lambda = 0.308;
           kbp->K = 0.089;
       } else if ((7 == options->gap_open) && (2 == options->gap_extend)) {
           kbp->Lambda = 0.295;
           kbp->K = 0.077;
       } else if ((6 == options->gap_open) && (2 == options->gap_extend)) {
           kbp->Lambda = 0.271;
           kbp->K = 0.051;
       } else if ((11 == options->gap_open) && (1 == options->gap_extend)) {
           kbp->Lambda = 0.314;
           kbp->K = 0.096;
           return status;
       } else if ((9 == options->gap_open) && (1 == options->gap_extend)) {
           kbp->Lambda = 0.277;
           kbp->K = 0.046;
       } else {
           status = -1;
       }
   } else if (0 == strcmp("BLOSUM45", options->matrix)) { 
       kbp->paramC = 0.60;
       if ((14 == options->gap_open) && (2 == options->gap_extend)) {
           kbp->Lambda = 0.199;
           kbp->K = 0.040;
       } else if ((13 == options->gap_open) && (3 == options->gap_extend)) {
           kbp->Lambda = 0.209;
           kbp->K = 0.057;
       } else if ((12 == options->gap_open) && (3 == options->gap_extend)) {
           kbp->Lambda = 0.203;
            kbp->K = 0.049;
       } else if ((11 == options->gap_open) && (3 == options->gap_extend)) {
           kbp->Lambda = 0.193;
           kbp->K = 0.037;
       } else if ((10 == options->gap_open) && (3 == options->gap_extend)) {
           kbp->Lambda = 0.182;
           kbp->K = 0.029;
       } else if ((15 == options->gap_open) && (2 == options->gap_extend)) {
           kbp->Lambda = 0.206;
           kbp->K = 0.049;
       } else if ((13 == options->gap_open) && (2 == options->gap_extend)) {
           kbp->Lambda = 0.190;
           kbp->K = 0.032;
       } else if ((12 == options->gap_open) && (2 == options->gap_extend)) {
           kbp->Lambda = 0.177;
           kbp->K = 0.023;
       } else if ((19 == options->gap_open) && (1 == options->gap_extend)) {
           kbp->Lambda = 0.209;
           kbp->K = 0.049;
       } else if ((18 == options->gap_open) && (1 == options->gap_extend)) {
           kbp->Lambda = 0.202;
           kbp->K = 0.041;
       } else if ((17 == options->gap_open) && (1 == options->gap_extend)) {
           kbp->Lambda = 0.195;
           kbp->K = 0.034;
       } else if ((16 == options->gap_open) && (1 == options->gap_extend)) {
           kbp->Lambda = 0.183;
           kbp->K = 0.024;
       } else {
           status = -1;
       }
   } else {
       status = -2;
   }

   if (status == -1) {
       sprintf(buffer, "The combination %d for gap opening cost and %d for "
               "gap extension is not supported in PHI-BLAST with matrix %s\n",
               options->gap_open, options->gap_extend, options->matrix);
   } else if (status == -2) {
       sprintf(buffer, "Matrix %s not allowed in PHI-BLAST\n", options->matrix);
   }
   if (status) 
       Blast_MessageWrite(blast_message, eBlastSevWarning, 2, 1, buffer);
   else {
       /* Put a copy the Karlin block into the kbp_std array */
       sbp->kbp_std[0] = (Blast_KarlinBlk*) 
           BlastMemDup(sbp->kbp_gap_std[0], sizeof(Blast_KarlinBlk));
       sbp->kbp = sbp->kbp_std;
   }

   return status;
}

Int2
Blast_ScoreBlkMatrixInit(EBlastProgramType program_number, 
                  const BlastScoringOptions* scoring_options,
                  BlastScoreBlk* sbp)
{
    Int2 status = 0;

    if ( !sbp || !scoring_options ) {
        return 1;
    }

    if (program_number == eBlastTypeBlastn) {

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
 
     } else {
        sbp->read_in_matrix = TRUE;
        BLAST_ScoreSetAmbigRes(sbp, 'X');
        sbp->name = BLAST_StrToUpper(scoring_options->matrix);
    }
    status = Blast_ScoreBlkMatrixFill(sbp, scoring_options->matrix_path);
    if (status) {
        return status;
    }

    return status;
}

Int2 
BlastSetup_ScoreBlkInit(BLAST_SequenceBlk* query_blk, 
                        const BlastQueryInfo* query_info, 
                        const BlastScoringOptions* scoring_options, 
                        EBlastProgramType program_number, 
                        BlastScoreBlk* *sbpp, 
                        double scale_factor, 
                        Blast_Message* *blast_message)
{
    BlastScoreBlk* sbp;
    Int2 status=0;      /* return value. */

    if (sbpp == NULL)
       return 1;

    if (program_number == eBlastTypeBlastn)
       sbp = BlastScoreBlkNew(BLASTNA_SEQ_CODE, query_info->last_context + 1);
    else
       sbp = BlastScoreBlkNew(BLASTAA_SEQ_CODE, query_info->last_context + 1);

    if (!sbp)
       return 1;

    *sbpp = sbp;
    sbp->scale_factor = scale_factor;

    status = Blast_ScoreBlkMatrixInit(program_number, scoring_options, sbp);
    if (status) {
        *blast_message = Blast_Perror(status);
        return status;
    }

    /* Fills in block for gapped blast. */
    if (Blast_ProgramIsPhiBlast(program_number)) {
       status = s_PHIScoreBlkFill(sbp, scoring_options, blast_message);
    } else {
       if ((status = Blast_ScoreBlkKbpUngappedCalc(program_number, sbp, 
                        query_blk->sequence, query_info)) != 0) {
          Blast_MessageWrite(blast_message, eBlastSevError, 2, 1, 
             "Could not calculate ungapped Karlin-Altschul parameters due "
             "to an invalid query sequence. Please verify the query "
             "sequence(s) and/or filtering options");
          return status;
       }

       if (scoring_options->gapped_calculation) {
          status = 
              Blast_ScoreBlkKbpGappedCalc(sbp, scoring_options, program_number,
                                          query_info, blast_message);
       }
    }

    return status;
}

Int2 BLAST_MainSetUp(EBlastProgramType program_number,
    const QuerySetUpOptions *qsup_options,
    const BlastScoringOptions *scoring_options,
    BLAST_SequenceBlk *query_blk,
    const BlastQueryInfo *query_info,
    double scale_factor,
    BlastSeqLoc **lookup_segments, 
    BlastMaskLoc **mask,
    BlastScoreBlk **sbpp, 
    Blast_Message **blast_message)
{
    Boolean mask_at_hash = FALSE; /* mask only for making lookup table? */
    Int2 status = 0;            /* return value */
    BlastMaskLoc *filter_maskloc = NULL;   /* Local variable for mask locs. */
    SBlastFilterOptions* filter_options = NULL;


    if (mask)
        *mask = NULL;

    if (qsup_options->filtering_options == NULL && qsup_options->filter_string)
    {
         status = BlastFilteringOptionsFromString(program_number, qsup_options->filter_string, &filter_options, blast_message);
         if (status)
            return status;
    }

    status = BlastSetUp_GetFilteringLocations(query_blk, 
                                              query_info, 
                                              program_number, 
                                              (filter_options ? filter_options : 
                                              qsup_options->filtering_options), 
                                              & filter_maskloc, 
                                              blast_message);

    if (status) {
        return status;
    } 

    mask_at_hash = 
        SBlastFilterOptionsMaskAtHash(filter_options ? filter_options : 
                                      qsup_options->filtering_options);

    filter_options = SBlastFilterOptionsFree(filter_options);

    if (!mask_at_hash)
    {
        status = BlastSetUp_MaskQuery(query_blk, query_info, filter_maskloc, 
                                      program_number);
        if (status != 0) {
            return status;
        }
    }

    if (program_number == eBlastTypeBlastx && scoring_options->is_ooframe) {
        BLAST_InitDNAPSequence(query_blk, query_info);
    }

    /* Find complement of the mask locations, for which lookup table will be
     * created. This should only be done if we do want to create a lookup table,
     * i.e. if it is a full search, not a traceback-only search. 
     */
    if (lookup_segments) {
        BLAST_ComplementMaskLocations(program_number, query_info, 
                                      filter_maskloc, lookup_segments);
    }

    if (mask)
    {
        if (program_number == eBlastTypeBlastx || 
            program_number == eBlastTypeTblastx ||
            program_number == eBlastTypeRpsTblastn) {
            /* Filter locations so far are in protein coordinates; 
               convert them back to nucleotide here. */
            BlastMaskLocProteinToDNA(filter_maskloc, query_info);
        }
        *mask = filter_maskloc;
        filter_maskloc = NULL;
    }
    else 
        filter_maskloc = BlastMaskLocFree(filter_maskloc);

    status = BlastSetup_ScoreBlkInit(query_blk, query_info, scoring_options, 
                                     program_number, sbpp, scale_factor, 
                                     blast_message);
    if (status > 0) {
        return status;
    }

    return 0;
}


Int2 BLAST_CalcEffLengths (EBlastProgramType program_number, 
   const BlastScoringOptions* scoring_options,
   const BlastEffectiveLengthsParameters* eff_len_params, 
   const BlastScoreBlk* sbp, BlastQueryInfo* query_info)
{
   double alpha=0, beta=0; /*alpha and beta for new scoring system */
   Int4 index;		/* loop index. */
   Int4	db_num_seqs;	/* number of sequences in database. */
   Int8	db_length;	/* total length of database. */
   Blast_KarlinBlk* *kbp_ptr; /* Array of Karlin block pointers */
   const BlastEffectiveLengthsOptions* eff_len_options = eff_len_params->options;

   if (!query_info || !sbp)
      return -1;

   /* Do nothing for PHI BLAST, because search space is calculated by 
      completely different formulas there. */
   if (Blast_ProgramIsPhiBlast(program_number))
       return 0;

   /* use overriding value from effective lengths options or the real value
      from effective lengths parameters. */
   if (eff_len_options->db_length > 0)
      db_length = eff_len_options->db_length;
   else
      db_length = eff_len_params->real_db_length;

   /* If database (subject) length is not available at this stage, and
    * overriding value of effective search space is not provided by user,
    * do nothing.
    * This situation can occur in the initial set up for a non-database search,
    * where each subject is treated as an individual sequence. 
    */
   if (db_length == 0 && eff_len_options->searchsp_eff == 0)
      return 0;

   if (program_number == eBlastTypeTblastn || 
       program_number == eBlastTypeTblastx)
      db_length = db_length/3;	

   if (eff_len_options->dbseq_num > 0)
      db_num_seqs = eff_len_options->dbseq_num;
   else
      db_num_seqs = eff_len_params->real_num_seqs;
   
   /* N.B.: the old code used kbp_gap_std instead of the kbp_gap alias (which
    * could be kbp_gap_psi), hence we duplicate that behavior here */
   kbp_ptr = (scoring_options->gapped_calculation ? sbp->kbp_gap_std : sbp->kbp);
   
   for (index = query_info->first_context;
        index <= query_info->last_context;
        index++) {
      Blast_KarlinBlk * kbp; /* statistical parameters for the current context */
      Int4 length_adjustment = 0; /* length adjustment for current iteration. */
      Int8 effective_search_space = 0; /* Effective search space for a given 
                                          sequence/strand/frame */
      Int4 query_length;   /* length of an individual query sequence */
      
      kbp = kbp_ptr[index];
      
      if ((query_length = query_info->contexts[index].query_length) > 0) {
         /* Use the correct Karlin block. For blastn, two identical Karlin
          * blocks are allocated for each sequence (one per strand), but we
          * only need one of them.
          */
         if (program_number == eBlastTypeBlastn) {
             Blast_GetNuclAlphaBeta(scoring_options->reward, 
                                    scoring_options->penalty, 
                                    scoring_options->gap_open, 
                                    scoring_options->gap_extend, 
                                    sbp->kbp_std[index], 
                                    scoring_options->gapped_calculation,
                                    &alpha, &beta);
         } else {
             BLAST_GetAlphaBeta(sbp->name, &alpha, &beta,
                                scoring_options->gapped_calculation, 
                                scoring_options->gap_open, 
                                scoring_options->gap_extend, 
                                sbp->kbp_std[index]);
         }
         BLAST_ComputeLengthAdjustment(kbp->K, kbp->logK,
                                       alpha/kbp->Lambda, beta,
                                       query_length, db_length,
                                       db_num_seqs, &length_adjustment);

         if (eff_len_options->searchsp_eff)
            effective_search_space = eff_len_options->searchsp_eff;
         else
            effective_search_space =
               (query_length - length_adjustment) * 
               (db_length - db_num_seqs*length_adjustment);
      }
      query_info->contexts[index].eff_searchsp = effective_search_space;
      query_info->contexts[index].length_adjustment = length_adjustment;
   }
   return 0;
}

Int2 
BLAST_GapAlignSetUp(EBlastProgramType program_number,
    const BlastSeqSrc* seq_src,
    const BlastScoringOptions* scoring_options,
    const BlastEffectiveLengthsOptions* eff_len_options,
    const BlastExtensionOptions* ext_options,
    const BlastHitSavingOptions* hit_options,
    BlastQueryInfo* query_info, 
    BlastScoreBlk* sbp, 
    BlastScoringParameters** score_params,
    BlastExtensionParameters** ext_params,
    BlastHitSavingParameters** hit_params,
    BlastEffectiveLengthsParameters** eff_len_params,
    BlastGapAlignStruct** gap_align)
{
   Int2 status = 0;
   Uint4 max_subject_length;
   Int8 total_length;
   Int4 num_seqs;

   total_length = BlastSeqSrcGetTotLen(seq_src);
   
   if (total_length > 0) {
      num_seqs = BlastSeqSrcGetNumSeqs(seq_src);
   } else {
      /* Not a database search; each subject sequence is considered
         individually */
      Int4 oid=0;  /* Get length of first sequence. */
      if ((total_length=BlastSeqSrcGetSeqLen(seq_src, (void*) &oid)) < 0)
          return -1;
      num_seqs = 1;
   }

   /* Initialize the effective length parameters with real values of
      database length and number of sequences */
   BlastEffectiveLengthsParametersNew(eff_len_options, total_length, num_seqs, 
                                      eff_len_params);
   /* Effective lengths are calculated for all programs except PHI BLAST. */
   if ((status = BLAST_CalcEffLengths(program_number, scoring_options, 
                     *eff_len_params, sbp, query_info)) != 0)
      return status;

   BlastScoringParametersNew(scoring_options, sbp, score_params);

   BlastExtensionParametersNew(program_number, ext_options, sbp, 
                               query_info, ext_params);

   BlastHitSavingParametersNew(program_number, hit_options, sbp, query_info, 
                               (Int4)(total_length/num_seqs), hit_params);

   /* To initialize the gapped alignment structure, we need to know the 
      maximal subject sequence length */
   max_subject_length = BlastSeqSrcGetMaxSeqLen(seq_src);

   if ((status = BLAST_GapAlignStructNew(*score_params, *ext_params, 
                    max_subject_length, sbp, gap_align)) != 0) {
      return status;
   }

   return status;
}

Int2 BLAST_OneSubjectUpdateParameters(EBlastProgramType program_number,
                    Uint4 subject_length,
                    const BlastScoringOptions* scoring_options,
                    BlastQueryInfo* query_info, 
                    BlastScoreBlk* sbp, 
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
   BlastHitSavingParametersUpdate(program_number, sbp, query_info, subject_length, 
                                  hit_params);
   
   if (word_params) {
      /* Update cutoff scores in initial word parameters */
      BlastInitialWordParametersUpdate(program_number, hit_params, sbp, query_info, 
           subject_length, word_params);
      /* Update the parameters for linking HSPs, if necessary. */
      BlastLinkHSPParametersUpdate(word_params, hit_params, scoring_options->gapped_calculation);
   }
   return status;
}

void
BlastSeqLoc_RestrictToInterval(BlastSeqLoc* *mask, Int4 from, Int4 to)
{
   BlastSeqLoc* head_loc = NULL, *last_loc = NULL, *next_loc, *seqloc;
   
   to = MAX(to, 0);

   /* If there is no mask, or if both coordinates passed are 0, which indicates
      the full sequence, just return - there is nothing to be done. */
   if (mask == NULL || *mask == NULL || (from == 0 && to == 0))
      return;

   for (seqloc = *mask; seqloc; seqloc = next_loc) {
      next_loc = seqloc->next;
      seqloc->ssr->left = MAX(0, seqloc->ssr->left - from);
      seqloc->ssr->right = MIN(seqloc->ssr->right, to) - from;
      /* If this mask location does not intersect the [from,to] interval,
         do not add it to the newly constructed list and free its contents. */
      if (seqloc->ssr->left > seqloc->ssr->right) {
         /* Shift the pointer to the next link in chain and free this link. */
         if (last_loc)
            last_loc->next = seqloc->next;
         sfree(seqloc->ssr);
         sfree(seqloc);
      } else if (!head_loc) {
         /* First time a mask was found within the range. */
         head_loc = last_loc = seqloc;
      } else {
         /* Append to the previous masks. */
         last_loc->next = seqloc;
         last_loc = last_loc->next;
      }
   }
   *mask = head_loc;
}

Int2 
Blast_SetPHIPatternInfo(EBlastProgramType            program,
                        const SPHIPatternSearchBlk * pattern_blk,
                        const BLAST_SequenceBlk    * query,
                        const BlastSeqLoc          * lookup_segments,
                        BlastQueryInfo             * query_info)
{
    const Boolean kIsNa = (program == eBlastTypePhiBlastn);
    
    ASSERT(Blast_ProgramIsPhiBlast(program));
    ASSERT(query_info && pattern_blk);
    
    query_info->pattern_info = SPHIQueryInfoNew();
    
    /* If pattern is not found in query, return failure status. */
    if (!PHIGetPatternOccurrences(pattern_blk, query, lookup_segments, kIsNa,
                                  query_info->pattern_info))
        return -1;
    
    /* Save pattern probability, because it needs to be passed back to
       formatting stage, where lookup table will not be available. */
    query_info->pattern_info->probability = pattern_blk->patternProbability;
    
    /* Save minimal pattern length in the length adjustment field, because 
       that is essentially its meaning. */
    query_info->contexts[0].length_adjustment = 
        pattern_blk->minPatternMatchLength;
    
    return 0;
}

/** Count the number of occurrences of pattern in sequence, which
 * do not overlap by more than half the pattern match length. 
 * @param query_info Query information structure, containing pattern info. [in]
 */
static Int4
s_GetEffectiveNumberOfPatterns(const BlastQueryInfo *query_info)
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

void
PHIPatternSpaceCalc(BlastQueryInfo* query_info, 
                    const BlastDiagnostics* diagnostics)
{
    Int4 num_query_patterns;
    Int8 num_db_patterns;

    ASSERT(query_info && diagnostics && diagnostics->ungapped_stat);

    num_query_patterns = s_GetEffectiveNumberOfPatterns(query_info);
    num_db_patterns = diagnostics->ungapped_stat->lookup_hits;
    query_info->contexts[0].eff_searchsp = num_query_patterns * num_db_patterns;
}

