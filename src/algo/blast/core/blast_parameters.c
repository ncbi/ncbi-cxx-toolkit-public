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
 */

/** @file blast_parameters.c
 * Definitions for functions dealing with BLAST CORE parameter structures.
 */

#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/ncbi_math.h>
#include <algo/blast/core/blast_nalookup.h>
#include <algo/blast/core/blast_hits.h>

/** Returns true if the Karlin-Altschul block doesn't have its lambda, K, and H
 * fields set to negative values. -1 is the sentinel used to mark them as
 * invalid. This can happen if a query sequence is completely masked for
 * example.
 * @param kbp Karlin-Altschul block to examine [in]
 * @return TRUE if its valid, else FALSE
 */
static Boolean s_BlastKarlinBlkIsValid(const Blast_KarlinBlk* kbp)
{
    if ( !kbp ) {
        return FALSE;
    } else {
        return (kbp->Lambda > 0 && kbp->K > 0 && kbp->H > 0);
    }
}

/** Returns the first valid Karlin-Altchul block from the list of blocks.
 * @sa s_BlastKarlinBlkIsValid
 * @param kbp_in array of Karlin blocks to be searched [in]
 * @param query_info information on number of queries (specifies number of
 * elements in above array) [in]
 * @param kbp_ret the object to be pointed at [out]
 * @return zero on success, 1 if no valid block found
 */

static Int2
s_BlastFindValidKarlinBlk(Blast_KarlinBlk** kbp_in, const BlastQueryInfo* query_info, Blast_KarlinBlk** kbp_ret)
{
    Int4 i;   /* Look for the first valid kbp. */

    /* BLASTERR_NOVALIDKARLINALTSCHUL means no valid block found (blast_message.h) */
    Int2 status=BLASTERR_NOVALIDKARLINALTSCHUL;  

    ASSERT(kbp_in && query_info && kbp_ret);

    for (i=query_info->first_context; i<=query_info->last_context; i++) {
         ASSERT(s_BlastKarlinBlkIsValid(kbp_in[i]) ==
                query_info->contexts[i].is_valid);
         if (s_BlastKarlinBlkIsValid(kbp_in[i])) {
              *kbp_ret = kbp_in[i];
              status = 0;
              break;
         }
    }
    return status;
}

/** Returns the smallest lambda value from a collection
 *  of Karlin-Altchul blocks
 * @param kbp_in array of Karlin blocks to be searched [in]
 * @param query_info information on number of queries (specifies number of
 * elements in above array) [in]
 * @param kbp_out Karlin blocks with smallest lambda [out]
 * @return The smallest lambda value
 */
static double
s_BlastFindSmallestLambda(Blast_KarlinBlk** kbp_in, 
                          const BlastQueryInfo* query_info,
                          Blast_KarlinBlk** kbp_out)
{
    Int4 i;
    double min_lambda = (double) INT4_MAX;

    ASSERT(kbp_in && query_info);

    for (i=query_info->first_context; i<=query_info->last_context; i++) {
        ASSERT(s_BlastKarlinBlkIsValid(kbp_in[i]) ==
               query_info->contexts[i].is_valid);
        if (s_BlastKarlinBlkIsValid(kbp_in[i])) {
            if (min_lambda > kbp_in[i]->Lambda)
            {
                min_lambda = kbp_in[i]->Lambda;
                if (kbp_out)
                  *kbp_out = kbp_in[i];
            }
        }
    }

    ASSERT(min_lambda > 0.0);
    return min_lambda;
}

BlastInitialWordParameters*
BlastInitialWordParametersFree(BlastInitialWordParameters* parameters)

{
	sfree(parameters->cutoffs);
	sfree(parameters);
	return NULL;
}

/** Compute the default cutoff expect value for ungapped extensions
 * @param program The blast program type
 * @return The default per-program expect value
 */
static double 
s_GetCutoffEvalue(EBlastProgramType program)
{
   switch(program) {
   case eBlastTypeMapping:
   case eBlastTypeBlastn:
      return CUTOFF_E_BLASTN;
   case eBlastTypeBlastp: 
   case eBlastTypeRpsBlast: 
      return CUTOFF_E_BLASTP;
   case eBlastTypeBlastx: 
      return CUTOFF_E_BLASTX;
   case eBlastTypeTblastn:
   case eBlastTypePsiTblastn:
   case eBlastTypeRpsTblastn:
      return CUTOFF_E_TBLASTN;
   case eBlastTypeTblastx:
      return CUTOFF_E_TBLASTX;
   case eBlastTypeUndefined:
   default:
      abort(); /* should never happen */
   }
   return 0.0;
}

Int2
BlastInitialWordParametersNew(EBlastProgramType program_number, 
   const BlastInitialWordOptions* word_options, 
   const BlastHitSavingParameters* hit_params, 
   const LookupTableWrap* lookup_wrap,
   const BlastScoreBlk* sbp, 
   BlastQueryInfo* query_info, 
   Uint4 subject_length,
   BlastInitialWordParameters* *parameters)
{
   BlastInitialWordParameters *p;
   Blast_KarlinBlk* kbp;
   Int2 status = 0;
   Int4 context;
   const int kQueryLenForHashTable = 8000; /* For blastn, use hash table rather 
                                           than diag array for any query longer 
                                           than this */

   /* If parameters pointer is NULL, there is nothing to fill, 
      so don't do anything */
   if (!parameters)
      return 0;

   ASSERT(word_options);
   ASSERT(sbp);
   if ((status=s_BlastFindValidKarlinBlk(sbp->kbp, query_info, &kbp)) != 0)
         return status;

   p = *parameters = (BlastInitialWordParameters*)calloc(1, 
                                     sizeof(BlastInitialWordParameters));

   if (Blast_ProgramIsPhiBlast(program_number))
      p->ungapped_extension = FALSE;
   else
      p->ungapped_extension = TRUE;

   p->cutoffs = (BlastUngappedCutoffs *)calloc(
           (size_t)(query_info->last_context+1), sizeof(BlastUngappedCutoffs));

   p->options = (BlastInitialWordOptions *) word_options;

   /* for each context, compute the expected X-dropoff value */

   for (context = query_info->first_context; 
                     context <= query_info->last_context; ++context) {

      if (!(query_info->contexts[context].is_valid))
         continue;
      kbp= sbp->kbp[context];
      ASSERT(s_BlastKarlinBlkIsValid(kbp));
      if ( program_number == eBlastTypeBlastn && sbp->matrix_only_scoring )
      {
        /* In this mode our scoring system is matrix only.  Reward/Penalty
         * parameters are shutoff.  Also as the matrix parameters of
         * lambda and K are not available.  We need to treat all scores as
         * raw and avoid using/breaking KA stats. This supports the 
         * rmblastn app.
         * -RMH-
         */
        p->cutoffs[context].x_dropoff_init = word_options->x_dropoff;
      }else {
        p->cutoffs[context].x_dropoff_init =
            (Int4)(sbp->scale_factor *
                   ceil(word_options->x_dropoff * NCBIMATH_LN2 / kbp->Lambda));
      }
   }

   if (Blast_ProgramIsNucleotide(program_number) &&
       !Blast_QueryIsPattern(program_number) &&
       (query_info->contexts[query_info->last_context].query_offset +
        query_info->contexts[query_info->last_context].query_length) > kQueryLenForHashTable)
       p->container_type = eDiagHash;
   else
       p->container_type = eDiagArray;

   status = BlastInitialWordParametersUpdate(program_number,
               hit_params, sbp, query_info, subject_length, p);

   if (program_number == eBlastTypeBlastn || program_number == eBlastTypeMapping) {
      Int4 i;
      Int4 reward = sbp->reward;
      Int4 penalty = sbp->penalty;
      Int4 *table = p->nucl_score_table;

      /* nucleotide ungapped extensions are first computed
         approximately, and then recomputed exactly if the
         approximate score is high enough. The approximate
         computation considers nucleotides in batches of 4,
         so a table is needed that contains the combined scores
         of all combinations of 4 matches/mismatches */

      for (i = 0; i < 256; i++) {
         /* break the bit pattern of i into four 2-bit groups.
            If bits in a group are set, that group represents 
            a mismatch */

         Int4 score = 0;
         if (i & 3) score += penalty; else score += reward;
         if ((i >> 2) & 3) score += penalty; else score += reward;
         if ((i >> 4) & 3) score += penalty; else score += reward;
         if (i >> 6) score += penalty; else score += reward;
         table[i] = score;
      }
   }

   /* Inherit the state of the ScoreBlk matrix_only_scoring flag to
    * our BlastInitialWordParameters matrix_only_scoring flag.  This 
    * is used to disable reward/penalty scoring and bypass KA stats
    * usage.
    * -RMH-
    */
   if ( program_number == eBlastTypeBlastn && sbp->matrix_only_scoring )
   {
     p->matrix_only_scoring = TRUE;
   }else {
     p->matrix_only_scoring = FALSE;
   }

   return status;
}

Int2
BlastInitialWordParametersUpdate(EBlastProgramType program_number, 
   const BlastHitSavingParameters* hit_params, 
   const BlastScoreBlk* sbp, 
   BlastQueryInfo* query_info, Uint4 subj_length,
   BlastInitialWordParameters* parameters)
{
   Blast_KarlinBlk** kbp_array;
   Boolean gapped_calculation = TRUE;
   double gap_decay_rate = 0.0;
   Int4 cutoff_min = INT4_MAX;
   Int4 xdrop_max = 0;
   Int4 context;
   const BlastInitialWordOptions* kOptions = parameters->options;

   ASSERT(sbp);
   ASSERT(hit_params);
   ASSERT(query_info);

   if (sbp->kbp_gap) {
      kbp_array = sbp->kbp_gap;
   } else if (sbp->kbp_std) {
      kbp_array = sbp->kbp_std;
      gapped_calculation = FALSE;
   } else {
       return -1;
   }

   /** @todo FIXME hit_params->link_hsp_params is NULL for 
       gapped blastn, and so is gap_decay_rate. */

   if (hit_params && hit_params->link_hsp_params)
      gap_decay_rate = hit_params->link_hsp_params->gap_decay_rate;
      
   /* determine the cutoff values for each context, and
      the smallest cutoff score across all contexts */

   for (context = query_info->first_context; 
                     context <= query_info->last_context; ++context) {

      Int4 gap_trigger = INT4_MAX;
      Blast_KarlinBlk* kbp;
      Int4 new_cutoff = 1;
      BlastUngappedCutoffs *curr_cutoffs = parameters->cutoffs + context;

       if (!(query_info->contexts[context].is_valid)) {
          /* either this context was never valid, or it was
             valid at the beginning of the search but is not
             valid now. The latter means that ungapped
             alignments can still occur to this context,
             so we set the cutoff score to be infinite */
          curr_cutoffs->cutoff_score = INT4_MAX;
          continue;
       }

      /* We calculate the gap_trigger value here and use it as a cutoff 
         to save ungapped alignments in a gapped search. The gap trigger
         is also a maximum value for the cutoff in an ungapped search.  
         Ungapped blastn is an exception, and it's not clear that it 
         should be exceptional. */

      if (sbp->kbp_std) {     /* this may not be set for gapped blastn */
         kbp = sbp->kbp_std[context];
         if (s_BlastKarlinBlkIsValid(kbp)) {
            gap_trigger = (Int4)((kOptions->gap_trigger * NCBIMATH_LN2 + 
                                     kbp->logK) / kbp->Lambda);
         }
      }

      if (!gapped_calculation || sbp->matrix_only_scoring) {
         double cutoff_e = s_GetCutoffEvalue(program_number);
         Int4 query_length = query_info->contexts[context].query_length;

         /* include the length of reverse complement for blastn searchs. */
         ASSERT(query_length > 0);
         if (program_number == eBlastTypeBlastn ||
             program_number == eBlastTypeMapping)
            query_length *= 2;
      
         kbp = kbp_array[context];
         ASSERT(s_BlastKarlinBlkIsValid(kbp));
         BLAST_Cutoffs(&new_cutoff, &cutoff_e, kbp, 
                       MIN((Uint8)subj_length, 
                           (Uint8)query_length)*((Uint8)subj_length), 
                       TRUE, gap_decay_rate);

         /* Perform this check for compatibility with the old code */
         if (program_number != eBlastTypeBlastn)  
            new_cutoff = MIN(new_cutoff, gap_trigger);
      } else {
         new_cutoff = gap_trigger;
      }
      new_cutoff *= (Int4)sbp->scale_factor;
      new_cutoff = MIN(new_cutoff, 
                       hit_params->cutoffs[context].cutoff_score_max);
      curr_cutoffs->cutoff_score = new_cutoff;

      /* Note that x_dropoff_init stays constant throughout the search,
         but the cutoff_score and x_dropoff parameters may be updated 
         multiple times, if every subject sequence is treated individually */

      if (curr_cutoffs->x_dropoff_init == 0)
         curr_cutoffs->x_dropoff = new_cutoff;
      else
         curr_cutoffs->x_dropoff = curr_cutoffs->x_dropoff_init;

     if ( program_number == eBlastTypeBlastn && sbp->matrix_only_scoring )
     {
        /* In this mode our scoring system is matrix only.  Reward/Penalty
         * parameters are shutoff.  Also as the matrix parameters of
         * lambda and K are not available.  We need to treat all scores as
         * raw and avoid using KA stats. This supports the rmblastn app.
         * Override the modification of x_dropoff and hold it at the 
         * initial value.
         * -RMH-
         */
         curr_cutoffs->x_dropoff = curr_cutoffs->x_dropoff_init;
      }

      /* Check if this is the smallest cutoff seen so far, and
         save both the cutoff and its associated X-drop value if so */

      if (new_cutoff < cutoff_min) {
         cutoff_min = new_cutoff;
      }

      if (xdrop_max < curr_cutoffs->x_dropoff)
         xdrop_max = curr_cutoffs->x_dropoff;

      /* Nucleotide searches first compute an approximate ungapped
         alignment and compare it to a reduced ungapped cutoff score */
      if (program_number == eBlastTypeBlastn ||
          program_number == eBlastTypeMapping) {
         curr_cutoffs->reduced_nucl_cutoff_score = (Int4)(0.8 * new_cutoff);
      }
   }

   parameters->cutoff_score_min = cutoff_min;
   parameters->x_dropoff_max = xdrop_max;
   return 0;
}


Int2 BlastExtensionParametersNew(EBlastProgramType program_number, 
        const BlastExtensionOptions* options, BlastScoreBlk* sbp,
        BlastQueryInfo* query_info, BlastExtensionParameters* *parameters)
{
   BlastExtensionParameters* params;

   /* If parameters pointer is NULL, there is nothing to fill, 
      so don't do anything */
   if (!parameters)
      return 0;
 
   if (sbp->kbp) {
      Blast_KarlinBlk* kbp = NULL;
      Int2 status=0;
      if ((status=s_BlastFindValidKarlinBlk(sbp->kbp, query_info, &kbp)) != 0)
         return status;
   } else {
      /* The Karlin block is not found, can't do any calculations */
      *parameters = NULL;
      return -1;
   }

   *parameters = params = (BlastExtensionParameters*) 
      calloc(1, sizeof(BlastExtensionParameters));

   params->options = (BlastExtensionOptions *) options;

   /* Set gapped X-dropoffs only if it is a gapped search. */

   /** @todo FIXME there are no per-context X-dropoff structures
    * because the gapped lambda values happen to be the same for 
    * all contexts. That may change in the future
    */
   if (sbp->kbp_gap) {
      double min_lambda = s_BlastFindSmallestLambda(sbp->kbp_gap, query_info, NULL);
      params->gap_x_dropoff = (Int4) 
          (options->gap_x_dropoff*NCBIMATH_LN2 / min_lambda);
      /* Note that this conversion from bits to raw score is done prematurely 
         when rescaling and composition based statistics is applied, as we
         lose precision. Therefore this is redone in Kappa_RedoAlignmentCore */
      params->gap_x_dropoff_final = (Int4) 
          MAX(options->gap_x_dropoff_final*NCBIMATH_LN2 / min_lambda, params->gap_x_dropoff);
   }
   
   if (sbp->scale_factor > 1.0) {
       ASSERT(Blast_ProgramIsRpsBlast(program_number));
       params->gap_x_dropoff *= (Int4)sbp->scale_factor;
       params->gap_x_dropoff_final *= (Int4)sbp->scale_factor;
   }

   if ( program_number == eBlastTypeBlastn && sbp->matrix_only_scoring )
   {
      /* In this mode our scoring system is matrix only.  Reward/Penalty
       * parameters are shutoff.  Also as the matrix parameters of
       * lambda and K are not available.  We need to treat all scores as
       * raw and avoid using/breaking KA stats. This supports the 
       * rmblastn app.  
       * -RMH-
       */
       params->gap_x_dropoff = options->gap_x_dropoff;
       params->gap_x_dropoff_final = options->gap_x_dropoff_final;
   }

   if (program_number == eBlastTypeMapping) {
       params->gap_x_dropoff = options->gap_x_dropoff;
   }

   return 0;
}

BlastExtensionParameters*
BlastExtensionParametersFree(BlastExtensionParameters* parameters)
{
  sfree(parameters);
  return NULL;
}

BlastScoringParameters*
BlastScoringParametersFree(BlastScoringParameters* parameters)
{
	sfree(parameters);
	return NULL;
}

/* -RMH-: Added new function ( for debuging ) */
void printBlastScoringParameters( BlastScoringParameters* params )
{
  if ( params == NULL )
  {
    printf("parameters{ null }\n");
    return;
  }
  printf("BlastScoringParameters:\n");
  if ( params->options == NULL )
  {
    printf("  options = NULL\n");
  }else {
    BlastScoringOptions* options = params->options;
    printf("  options:\n");
    printf("    matrix = %s\n", options->matrix );
    printf("    matrix_path = %s\n", options->matrix_path );
    printf("    reward = %d\n", options->reward );
    printf("    penalty = %d\n", options->penalty );
    printf("    gapped_calculation = %d\n", options->gapped_calculation );
    printf("    complexity_adjusted_scoring = %d\n", options->complexity_adjusted_scoring );
    printf("    gap_open = %d\n", options->gap_open );
    printf("    gap_extend = %d\n", options->gap_extend );
    printf("    is_ooframe = %d\n", options->is_ooframe );
    printf("    shift_pen = %d\n", options->shift_pen );
    printf("    program_number = %d\n", options->program_number );
  }
  printf("  reward = %d\n", params->reward );
  printf("  penalty = %d\n", params->penalty );
  printf("  gap_open = %d\n", params->gap_open );
  printf("  gap_extend = %d\n", params->gap_extend );
  printf("  shift_pen = %d\n", params->shift_pen );
  printf("  scale_factor = %f\n\n", params->scale_factor );
}

Int2
BlastScoringParametersNew(const BlastScoringOptions* score_options, 
                          BlastScoreBlk* sbp, 
                          BlastScoringParameters* *parameters)
{
   BlastScoringParameters *params;
   double scale_factor;

   if (score_options == NULL)
      return 1;

   *parameters = params = (BlastScoringParameters*) 
                        calloc(1, sizeof(BlastScoringParameters));
   if (params == NULL)
      return 2;

   params->options = (BlastScoringOptions *)score_options;
   scale_factor = sbp->scale_factor;
   params->scale_factor = scale_factor;
   params->reward = score_options->reward;
   params->penalty = score_options->penalty;
   params->gap_open = score_options->gap_open * (Int4)scale_factor;
   params->gap_extend = score_options->gap_extend * (Int4)scale_factor;
   params->shift_pen = score_options->shift_pen * (Int4)scale_factor;
   return 0;
}


BlastEffectiveLengthsParameters*
BlastEffectiveLengthsParametersFree(BlastEffectiveLengthsParameters* parameters)

{
	sfree(parameters);

	return NULL;
}

Int2 
BlastEffectiveLengthsParametersNew(const BlastEffectiveLengthsOptions* options, 
                               Int8 db_length, Int4 num_seqs,
                               BlastEffectiveLengthsParameters* *parameters)
{
   *parameters = (BlastEffectiveLengthsParameters*) 
      calloc(1, sizeof(BlastEffectiveLengthsParameters));
   (*parameters)->options = (BlastEffectiveLengthsOptions*) options;
   (*parameters)->real_db_length = db_length;
   (*parameters)->real_num_seqs = num_seqs;
   return 0;
}

BlastLinkHSPParameters* 
BlastLinkHSPParametersFree(BlastLinkHSPParameters* parameters)
{
   sfree(parameters);
   return NULL;
}

Int2 BlastLinkHSPParametersNew(EBlastProgramType program_number, 
                               Boolean gapped_calculation,
                               BlastLinkHSPParameters** link_hsp_params)
{
   BlastLinkHSPParameters* params;

   if (!link_hsp_params)
      return -1;

   params = (BlastLinkHSPParameters*)
      calloc(1, sizeof(BlastLinkHSPParameters));

   if (program_number == eBlastTypeBlastn || !gapped_calculation) {
      params->gap_prob = BLAST_GAP_PROB;
      params->gap_decay_rate = BLAST_GAP_DECAY_RATE;
   } else {
      params->gap_prob = BLAST_GAP_PROB_GAPPED;
      params->gap_decay_rate = BLAST_GAP_DECAY_RATE_GAPPED;
   }
   params->gap_size = BLAST_GAP_SIZE;
   params->overlap_size = BLAST_OVERLAP_SIZE;

   *link_hsp_params = params;
   return 0;
}

Int2 
BlastLinkHSPParametersUpdate(const BlastInitialWordParameters* word_params,
                             const BlastHitSavingParameters* hit_params,
                             Boolean gapped_calculation)
{
   if (!word_params || !hit_params)
      return -1;
   if (!hit_params->link_hsp_params)
      return 0;

   if (gapped_calculation) {
      /* FIXME, is this correct?? */
      hit_params->link_hsp_params->cutoff_small_gap = 
                                word_params->cutoff_score_min;
   } else {
      /* For all ungapped programs other than blastn, this value will be 
         recalculated anyway in CalculateLinkHSPCutoffs, but for blastn 
         this will be the final value. */
      hit_params->link_hsp_params->cutoff_small_gap = 
                                word_params->cutoff_score_min;
   }

   return 0;
}

/** Returns the estimated expect value for the pattern match with a given scoring alignment.
 *  The true expect value is calculated based upon the number of times a pattern is actually
 *  found in the database.
 * @param score score from alignment [in]
 * @param query_info provides statistical information on pattern [in]
 * @param sbp provides Karlin-Altschul statistical params [in]
 * @param effNumPatterns Number of times pattern occurs taking overlaps into account [in]
 * @return estimated expect value.
 */

static double s_GetEstimatedPhiExpect(int score, const BlastQueryInfo* query_info, 
    const BlastScoreBlk* sbp, int effNumPatterns)
{
   double paramC;
   double Lambda;
   double evalue;
   Int8 pattern_space;

   paramC = sbp->kbp[0]->paramC;
   Lambda = sbp->kbp[0]->Lambda;

   pattern_space = query_info->contexts[0].eff_searchsp;

   /* We estimate the number of times pattern will occur. */
   evalue = pattern_space*paramC*(1+Lambda*score)*
              effNumPatterns*
              query_info->pattern_info->probability*
              exp(-Lambda*score);

   return evalue;
}

/** Estimates a cutoff score for use in preliminary gapped stage of phiblast.
 * The low score must be at least 1 so that hits matching only the pattern
 * are not returned.
 * @param ethresh expect value to provide score for [in]
 * @param query_info provides statistical information on pattern [in]
 * @param sbp provides Karlin-Altschul statistical params [in]
 * @return cutoff score
 */
static Int4 s_PhiBlastCutoffScore(double ethresh, const BlastQueryInfo* query_info, const BlastScoreBlk* sbp)
{

        int lowScore = 1;
        int highScore = 100;
        int iteration=0;
        const int kMaxIter=20;
        int effNumPatterns = 0;

        ASSERT(query_info && query_info->pattern_info && sbp);

        effNumPatterns = PhiBlastGetEffectiveNumberOfPatterns(query_info);

        for (iteration=0; iteration<kMaxIter; iteration++)
        {
              int targetScore = (lowScore+highScore)/2;
              double expect = s_GetEstimatedPhiExpect(targetScore, query_info, sbp, effNumPatterns);
              if (expect > ethresh)
                  lowScore = targetScore;
              else
                  highScore = targetScore;

              if ((highScore-lowScore) <= 1)
                  break;
        }
        return lowScore;
}

BlastHitSavingParameters*
BlastHitSavingParametersFree(BlastHitSavingParameters* parameters)

{
   if (parameters) {
      sfree(parameters->cutoffs);
      sfree(parameters->link_hsp_params);
      if (parameters->low_score) {
          sfree(parameters->low_score);
      }
      sfree(parameters);
   }
   return NULL;
}


Int2
BlastHitSavingParametersNew(EBlastProgramType program_number, 
   const BlastHitSavingOptions* options, 
   const BlastScoreBlk* sbp, 
   const BlastQueryInfo* query_info, 
   Int4 avg_subj_length,
   Int4 compositionBasedStats,
   BlastHitSavingParameters* *parameters)
{
   Boolean gapped_calculation = TRUE;
   Int2 status = 0;
   BlastHitSavingParameters* params;

   /* If parameters pointer is NULL, there is nothing to fill, 
      so don't do anything */
   if (!parameters)
      return 0;

   *parameters = NULL;

   ASSERT(options);
   ASSERT(sbp);
   
   if (!sbp->kbp_gap)
      gapped_calculation = FALSE;

   if (options->do_sum_stats && gapped_calculation && avg_subj_length <= 0)
       return 1;
       

   /* If parameters have not yet been created, allocate and fill all
      parameters that are constant throughout the search */
   *parameters = params = (BlastHitSavingParameters*) 
      calloc(1, sizeof(BlastHitSavingParameters));

   if (params == NULL)
      return 1;

   // -RMH-  Initialize mask_level to default
   params->mask_level = 101;

   params->do_sum_stats = options->do_sum_stats;
   params->options = (BlastHitSavingOptions *) options;
   /* Each context gets its own gapped cutoff data */
   params->cutoffs = (BlastGappedCutoffs *)calloc(
                                        (size_t)(query_info->last_context+1),
                                        sizeof(BlastGappedCutoffs));

   if (params->do_sum_stats) {
      BlastLinkHSPParametersNew(program_number, gapped_calculation,
                                &params->link_hsp_params);

      if((Blast_QueryIsTranslated(program_number) ||
	  Blast_SubjectIsTranslated(program_number)) &&
	 program_number != eBlastTypeTblastx) {
          /* The program may use Blast_UnevenGapLinkHSPs find significant
             collections of distinct alignments */
          Int4 max_protein_gap; /* the largest gap permitted in the
                               * translated sequence */

          max_protein_gap = (options->longest_intron - 2)/3;
          if(gapped_calculation) {
              if(options->longest_intron == 0) {
                  /* a zero value of longest_intron invokes the
                   * default behavior, which for gapped calculation is
                   * to set longest_intron to a predefined value. */
                  params->link_hsp_params->longest_intron =
                      (DEFAULT_LONGEST_INTRON - 2) / 3;
              } else if(max_protein_gap <= 0) {
                  /* A nonpositive value of max_protein_gap disables linking */
                  params->link_hsp_params =
                      BlastLinkHSPParametersFree(params->link_hsp_params);
                  params->do_sum_stats = FALSE;
              } else { /* the value of max_protein_gap is positive */
                  params->link_hsp_params->longest_intron = max_protein_gap;
              }
          } else { /* This is an ungapped calculation. */
              /* For ungapped calculations, we preserve the old behavior
               * of the longest_intron parameter to maintain
               * backward-compatibility with older versions of BLAST. */
              params->link_hsp_params->longest_intron =
                MAX(max_protein_gap, 0);
          }
      }
   }

   if (options->low_score_perc > 0.00001)
       params->low_score = calloc((size_t) query_info->num_queries, sizeof(int));
   else 
       params->low_score = NULL;

   status = BlastHitSavingParametersUpdate(program_number, sbp, query_info, 
                                           avg_subj_length, compositionBasedStats, params);

   return status;
}

Int2
BlastHitSavingParametersUpdate(EBlastProgramType program_number, 
   const BlastScoreBlk* sbp, const BlastQueryInfo* query_info, 
   Int4 avg_subject_length, Int4 compositionBasedStats, 
   BlastHitSavingParameters* params)
{
   BlastHitSavingOptions* options;
   Blast_KarlinBlk** kbp_array;
   double scale_factor = sbp->scale_factor;
   Boolean gapped_calculation = TRUE;
   Int4 context;

   ASSERT(params);
   ASSERT(query_info);

   options = params->options;

   /* if there is a performance benefit to doing so, perform
      approximate score-only gapped alignment. While this can
      in principle apply to any search, only blastp has been
      carefully analyzed */

   if (program_number == eBlastTypeBlastp && gapped_calculation &&
      options->expect_value <= RESTRICTED_ALIGNMENT_WORST_EVALUE) {
      params->restricted_align = TRUE;
   }

   /* Scoring options are not available here, but we can determine whether
      this is a gapped or ungapped search by checking whether gapped
      Karlin blocks have been set. */
   if (sbp->kbp_gap) {
       kbp_array = sbp->kbp_gap;
   } else if (sbp->kbp) {
       kbp_array = sbp->kbp;
       gapped_calculation = FALSE;
   } else {
       return -1;
   }
    params->prelim_evalue = options->expect_value;  /* evalue and prelim_evalue same if no CBS. */

   // Set masklevel parameter -RMH-
    if ((program_number == eBlastTypeBlastn ||
         program_number == eBlastTypeMapping) && options->mask_level >= 0 )
     params->mask_level = options->mask_level;

   /* Calculate cutoffs based on effective length information */
   if (options->cutoff_score > 0) {
      Int4 new_cutoff = options->cutoff_score * (Int4) sbp->scale_factor;
      for (context = query_info->first_context;
                          context <= query_info->last_context; ++context) {
         params->cutoffs[context].cutoff_score = new_cutoff;
         params->cutoffs[context].cutoff_score_max = new_cutoff;
         if ( program_number == eBlastTypeBlastn && sbp->matrix_only_scoring )
         {
            /* In this mode our scoring system is matrix only.  Reward/Penalty
             * parameters are shutoff.  Also as the matrix parameters of
             * lambda and K are not available.  We need to treat all scores as
             * raw and avoid using KA stats. This supports the rmblastn app.
             * Override the modification of x_dropoff and hold it at the 
             * initial value.
             * -RMH-
             */
             params->cutoffs[context].cutoff_score = options->cutoff_score;
             params->cutoffs[context].cutoff_score_max = (Int4)(options->cutoff_score/2);
          }
      }
      params->cutoff_score_min = new_cutoff;
                            
   } else if (Blast_ProgramIsPhiBlast(program_number)) {
      Int4 new_cutoff = (Int4)sbp->scale_factor *
                        s_PhiBlastCutoffScore(5 * options->expect_value, 
                                              query_info, sbp);
      for (context = query_info->first_context;
                             context <= query_info->last_context; ++context) {
         params->cutoffs[context].cutoff_score = new_cutoff;
         params->cutoffs[context].cutoff_score_max = new_cutoff;
      }
      params->cutoff_score_min = new_cutoff;
                            
   } else {
      Int4 cutoff_min = INT4_MAX;
      Blast_KarlinBlk* kbp;

      for (context = query_info->first_context;
                          context <= query_info->last_context; ++context) {
         Int8 searchsp;
         Int4 new_cutoff = 1;
         double evalue = options->expect_value;

         if (!(query_info->contexts[context].is_valid)) {
            params->cutoffs[context].cutoff_score = INT4_MAX;
            continue;
         }

         kbp = kbp_array[context];
         ASSERT(s_BlastKarlinBlkIsValid(kbp));
         searchsp = query_info->contexts[context].eff_searchsp;
   
         /* translated RPS searches must scale the search space down */
         /** @todo FIXME why only scale down rpstblastn search space?  */
         if (program_number == eBlastTypeRpsTblastn) 
            searchsp /= NUM_FRAMES;
   
	 params->prelim_evalue = evalue;  /* evalue and prelim_evalue same if no CBS. */
         /* Get cutoff_score for specified evalue. */
         if (sbp->gbp && sbp->gbp->filled) {
	     /* If cbs greater than 1 (2 or 3), then increase expect value by 5 for preliminary search. */
	     int cbs_stretch = (compositionBasedStats > 1) ? 5 : 1;
	     params->prelim_evalue = cbs_stretch*evalue;
             new_cutoff = BLAST_SpougeEtoS(cbs_stretch*evalue, kbp, sbp->gbp, 
                         query_info->contexts[context].query_length,
                         avg_subject_length);
         } else {
             BLAST_Cutoffs(&new_cutoff, &evalue, kbp, searchsp, FALSE, 0);
         }
         params->cutoffs[context].cutoff_score = new_cutoff;
         params->cutoffs[context].cutoff_score_max = new_cutoff;
      }

      /* If using sum statistics, use a modified cutoff score 
         if that turns out smaller */
      if (params->link_hsp_params && gapped_calculation) {

         double evalue_hsp = 1.0;
         Int4 concat_qlen =
             query_info->contexts[query_info->last_context].query_offset +
             query_info->contexts[query_info->last_context].query_length;
         Int4 avg_qlen = concat_qlen / (query_info->last_context + 1);
         Int8 searchsp = (Int8)MIN(avg_qlen, avg_subject_length) * 
                         (Int8)avg_subject_length;

         ASSERT(params->link_hsp_params);

         for (context = query_info->first_context;
                             context <= query_info->last_context; ++context) {
            Int4 new_cutoff = 1;

            if (!(query_info->contexts[context].is_valid))
                continue;

            kbp = kbp_array[context];
            ASSERT(s_BlastKarlinBlkIsValid(kbp));
            BLAST_Cutoffs(&new_cutoff, &evalue_hsp, kbp, searchsp,
                       TRUE, params->link_hsp_params->gap_decay_rate);
            params->cutoffs[context].cutoff_score = MIN(new_cutoff,
                                    params->cutoffs[context].cutoff_score);
         }
      }
     
      /* scale up the computed cutoffs, and find the smallest one */
      for (context = query_info->first_context;
                             context <= query_info->last_context; ++context) {

         if (query_info->contexts[context].is_valid) {
            params->cutoffs[context].cutoff_score *= (Int4) scale_factor;
            params->cutoffs[context].cutoff_score_max *= (Int4) scale_factor;
            cutoff_min = MIN(cutoff_min,
                             params->cutoffs[context].cutoff_score);
         }
      }
      params->cutoff_score_min = cutoff_min;
   }

   return 0;
}

/* FIXME, move to blast_engine.c and make private?  */
void
CalculateLinkHSPCutoffs(EBlastProgramType program, BlastQueryInfo* query_info, 
   const BlastScoreBlk* sbp, BlastLinkHSPParameters* link_hsp_params, 
   const BlastInitialWordParameters* word_params,
   Int8 db_length, Int4 subject_length)
{
    Blast_KarlinBlk* kbp = NULL;
    double gap_prob, gap_decay_rate, x_variable, y_variable;
    Int4 expected_length, window_size, query_length;
    Int8 search_sp;
    const double kEpsilon = 1.0e-9;

    if (!link_hsp_params)
        return;

    /* Get KarlinBlk for context with smallest lambda (still greater than zero) */
    s_BlastFindSmallestLambda(sbp->kbp, query_info, &kbp);
    if (!kbp)
        return;

    window_size
        = link_hsp_params->gap_size + link_hsp_params->overlap_size + 1;
    gap_prob = link_hsp_params->gap_prob = BLAST_GAP_PROB;
    gap_decay_rate = link_hsp_params->gap_decay_rate;
    /* Use average query length */
    
    query_length =
        (query_info->contexts[query_info->last_context].query_offset +
        query_info->contexts[query_info->last_context].query_length - 1)
        / (query_info->last_context + 1);
    
    if (Blast_SubjectIsTranslated(program) || program == eBlastTypeRpsTblastn) {
        /* Lengths in subsequent calculations should be on the protein scale */
        subject_length /= CODON_LENGTH;
        db_length /= CODON_LENGTH;
    }

    
    /* Subtract off the expected score. */
   expected_length = BLAST_Nint(log(kbp->K*((double) query_length)*
                                    ((double) subject_length))/(kbp->H));
   query_length = query_length - expected_length;

   subject_length = subject_length - expected_length;
   query_length = MAX(query_length, 1);
   subject_length = MAX(subject_length, 1);

   /* If this is a database search, use database length, else the single 
      subject sequence length */
   if (db_length > subject_length) {
      y_variable = log((double) (db_length)/(double) subject_length)*(kbp->K)/
         (gap_decay_rate);
   } else {
      y_variable = log((double) (subject_length + expected_length)/
                       (double) subject_length)*(kbp->K)/(gap_decay_rate);
   }

   search_sp = ((Int8) query_length)* ((Int8) subject_length);
   x_variable = 0.25*y_variable*((double) search_sp);

   /* To use "small" gaps the query and subject must be "large" compared to
      the gap size. If small gaps may be used, then the cutoff values must be
      adjusted for the "bayesian" possibility that both large and small gaps 
      are being checked for. */

   if (search_sp > 8*window_size*window_size) {
      x_variable /= (1.0 - gap_prob + kEpsilon);
      link_hsp_params->cutoff_big_gap = 
         (Int4) floor((log(x_variable)/kbp->Lambda)) + 1;
      x_variable = y_variable*(window_size*window_size);
      x_variable /= (gap_prob + kEpsilon);
      link_hsp_params->cutoff_small_gap = 
         MAX(word_params->cutoff_score_min, 
             (Int4) floor((log(x_variable)/kbp->Lambda)) + 1);
   } else {
      link_hsp_params->cutoff_big_gap = 
         (Int4) floor((log(x_variable)/kbp->Lambda)) + 1;
      /* The following is equivalent to forcing small gap rule to be ignored
         when linking HSPs. */
      link_hsp_params->gap_prob = 0;
      link_hsp_params->cutoff_small_gap = 0;
   }	

   link_hsp_params->cutoff_big_gap *= (Int4)sbp->scale_factor;
   link_hsp_params->cutoff_small_gap *= (Int4)sbp->scale_factor;
}

/* For debugging within the C core -RMH- */
void printBlastInitialWordParamters ( BlastInitialWordParameters *word_params, BlastQueryInfo *query_info )
{   
  int context;
  printf("BlastInitialWordParamters:\n");
  printf("  x_dropoff_max = %d\n", word_params->x_dropoff_max );
  printf("  cutoff_score_min = %d\n", word_params->cutoff_score_min );
  printf("  cutoffs:\n");
  for (context = query_info->first_context;
       context <= query_info->last_context; ++context)
  { 
    if (!(query_info->contexts[context].is_valid))
      continue;
    printf("    %d x_dropoff_init = %d\n", context, word_params->cutoffs[context].x_dropoff_init );
    printf("    %d x_dropoff = %d\n", context, word_params->cutoffs[context].x_dropoff );
    printf("    %d cutoff_score = %d\n", context, word_params->cutoffs[context].cutoff_score );
    printf("    %d reduced_nucl_cutoff_score = %d\n", context, word_params->cutoffs[context].reduced_nucl_cutoff_score );
  }
}

/* For debugging within the C core -RMH- */
void printBlastExtensionParameters ( BlastExtensionParameters *ext_params )
{
  printf("BlastExtensionParameters:\n");
  printf("  gap_x_dropoff = %d\n", ext_params->gap_x_dropoff );
  printf("  gap_x_dropoff_final = %d\n", ext_params->gap_x_dropoff_final );
}

/* For debugging within the C core -RMH- */
void printBlastHitSavingParameters ( BlastHitSavingParameters * hit_params,
                                     BlastQueryInfo *query_info )
{
  int context;
  printf("BlastHitSavingParameters:\n");
  printf("  cutoff_score_min = %d\n", hit_params->cutoff_score_min );
   for (context = query_info->first_context;
       context <= query_info->last_context; ++context)
  {
    if (!(query_info->contexts[context].is_valid))
      continue;
    printf("    %d cutoff_score = %d\n", context, hit_params->cutoffs[context].cutoff_score );
    printf("    %d cutoff_score_max = %d\n", context, hit_params->cutoffs[context].cutoff_score_max );
  }
}

/* For debugging within the C core -RMH- */
void printAllParameters ( BlastHitSavingParameters * hit_params,
                          BlastExtensionParameters *ext_params,
                          BlastInitialWordParameters *word_params,
                          BlastQueryInfo *query_info  )
{
  printBlastInitialWordParamters( word_params, query_info );
  printBlastExtensionParameters( ext_params );
  printBlastHitSavingParameters( hit_params, query_info );
}
