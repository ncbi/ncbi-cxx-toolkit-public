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

/** @file blast_options.c
 *  The structures and functions in blast_options.[ch] should be used to specify 
 *  user preferences.  The options structures should not be changed by the BLAST code
 *  but rather be read to determine user preferences.  When possible these structures
 *  should be passed in as "const".
 *
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_filter.h>

const int kUngappedHSPNumMax = 400;
const double kPSSM_NoImpalaScaling = 1.0;

SDustOptions* SDustOptionsFree(SDustOptions* dust_options)
{
    if (dust_options)
      sfree(dust_options);
    return NULL;
}

Int2 SDustOptionsNew(SDustOptions* *dust_options)
{
    if (dust_options == NULL)
        return 1;

    *dust_options = (SDustOptions*) malloc(sizeof(SDustOptions));
    (*dust_options)->level = kDustLevel;
    (*dust_options)->window = kDustWindow;
    (*dust_options)->linker = kDustLinker;

    return 0;
}

SSegOptions* SSegOptionsFree(SSegOptions* seg_options)
{
    if (seg_options)
      sfree(seg_options);
    return NULL;
}

Int2 SSegOptionsNew(SSegOptions* *seg_options)
{
    if (seg_options == NULL)
        return 1;

    *seg_options = (SSegOptions*) malloc(sizeof(SSegOptions));
    (*seg_options)->window = kSegWindow;
    (*seg_options)->locut = kSegLocut;
    (*seg_options)->hicut = kSegHicut;

    return 0;
}

SRepeatFilterOptions* SRepeatFilterOptionsFree(SRepeatFilterOptions* repeat_options)
{
    if (repeat_options)
    {
        sfree(repeat_options->database);
        sfree(repeat_options);
    }
    return NULL;
}

Int2 SRepeatFilterOptionsNew(SRepeatFilterOptions* *repeat_options)
{

    const char* kRepeatDB = "humrep";
    if (repeat_options == NULL)
        return 1;

    *repeat_options = (SRepeatFilterOptions*) calloc(1, sizeof(SRepeatFilterOptions));
    if (*repeat_options == NULL)
        return 1;

    (*repeat_options)->database = strdup(kRepeatDB);

    return 0;
}

Int2 SRepeatFilterOptionsResetDB(SRepeatFilterOptions* *repeat_options, const char* db)
{
    Int2 status=0;

    if (*repeat_options == NULL)
      status = SRepeatFilterOptionsNew(repeat_options);

    if (status)
      return status;

    sfree((*repeat_options)->database);
    (*repeat_options)->database = strdup(db);

    return status;
}

SBlastFilterOptions* SBlastFilterOptionsFree(SBlastFilterOptions* filter_options)
{
    if (filter_options)
    {
        filter_options->dustOptions = SDustOptionsFree(filter_options->dustOptions);
        filter_options->segOptions = SSegOptionsFree(filter_options->segOptions);
        filter_options->repeatFilterOptions = SRepeatFilterOptionsFree(filter_options->repeatFilterOptions);
        sfree(filter_options);
    }

    return NULL;
}

Int2 SBlastFilterOptionsNew(SBlastFilterOptions* *filter_options,  EFilterOptions type)
{
    Int2 status = 0;

    if (filter_options)
    {
        *filter_options = (SBlastFilterOptions*) calloc(1, sizeof(SBlastFilterOptions));
        (*filter_options)->mask_at_hash = FALSE;
        if (type == eSeg)
          SSegOptionsNew(&((*filter_options)->segOptions)); 
        if (type == eDust || type == eDustRepeats)
          SDustOptionsNew(&((*filter_options)->dustOptions)); 
        if (type == eRepeats || type == eDustRepeats)
          SRepeatFilterOptionsNew(&((*filter_options)->repeatFilterOptions)); 
    }
    else
        status = 1;

    return status;
}

Boolean SBlastFilterOptionsMaskAtHash(const SBlastFilterOptions* filter_options)
{
       if (filter_options == NULL)
          return FALSE;
      
       return filter_options->mask_at_hash;
}

Int2 SBlastFilterOptionsValidate(EBlastProgramType program_number, const SBlastFilterOptions* filter_options, Blast_Message* *blast_message)
{
       Int2 status = 0;

       if (filter_options == NULL)
       {
           Blast_MessageWrite(blast_message, eBlastSevWarning, 2, 1, "SBlastFilterOptionsValidate: NULL filter_options");
           return 1;
       }

       if (filter_options->repeatFilterOptions)
       {
           if (program_number != eBlastTypeBlastn)
           {
               if (blast_message)
                  Blast_MessageWrite(blast_message, eBlastSevWarning, 2, 1, 
                   "SBlastFilterOptionsValidate: Repeat filtering only supported with blastn");
               return 1;
           }
           if (filter_options->repeatFilterOptions->database == NULL)
           {
               if (blast_message)
                  Blast_MessageWrite(blast_message, eBlastSevWarning, 2, 1, 
                   "SBlastFilterOptionsValidate: No repeat database specified for repeat filtering");
               return 1;
           }
       }

       if (filter_options->dustOptions)
       {
           if (program_number != eBlastTypeBlastn)
           {
               if (blast_message)
                  Blast_MessageWrite(blast_message, eBlastSevWarning, 2, 1, 
                   "SBlastFilterOptionsValidate: Dust filtering only supported with blastn");
               return 1;
           }
       }
  
       if (filter_options->segOptions)
       {
           if (program_number == eBlastTypeBlastn)
           {
               if (blast_message)
                  Blast_MessageWrite(blast_message, eBlastSevWarning, 2, 1, 
                   "SBlastFilterOptionsValidate: SEG filtering is not supported with blastn");
               return 1;
           }
       }

       return status;
}


QuerySetUpOptions*
BlastQuerySetUpOptionsFree(QuerySetUpOptions* options)

{
   if (options)
   {
       sfree(options->filter_string);
       options->filtering_options = SBlastFilterOptionsFree(options->filtering_options);
       sfree(options);
   }
   return NULL;
}

Int2
BlastQuerySetUpOptionsNew(QuerySetUpOptions* *options)
{
   Int2 status = 0;

   if (options == NULL)
      return 1;

   *options = (QuerySetUpOptions*) calloc(1, sizeof(QuerySetUpOptions));
   
   if (*options == NULL)
      return 1;

   (*options)->genetic_code = BLAST_GENETIC_CODE;

   /** @todo the code below should be deprecated */
   status = SBlastFilterOptionsNew(&((*options)->filtering_options), eEmpty);
   
   return status;
}

Int2 BLAST_FillQuerySetUpOptions(QuerySetUpOptions* options,
        EBlastProgramType program, const char *filter_string, Uint1 strand_option)
{
   Int2 status = 0;

   if (options == NULL)
      return 1;
   
   if (strand_option && 
       (program == eBlastTypeBlastn || program == eBlastTypePhiBlastn || 
        program == eBlastTypeBlastx || program == eBlastTypeTblastx)) {
      options->strand_option = strand_option;
   }

   if (filter_string) {
       /* Free whatever filter string has been set before. */
       sfree(options->filter_string);
       /* Free whatever filtering options have been set. */
       options->filtering_options =  SBlastFilterOptionsFree(options->filtering_options);
       /* Parse the filter_string for options, do not save the string. */
       status = BlastFilteringOptionsFromString(program, filter_string, 
          &options->filtering_options, NULL);
   }
   return status;
}

BlastInitialWordOptions*
BlastInitialWordOptionsFree(BlastInitialWordOptions* options)

{

	sfree(options);

	return NULL;
}


Int2
BlastInitialWordOptionsNew(EBlastProgramType program, 
   BlastInitialWordOptions* *options)
{
   *options = 
      (BlastInitialWordOptions*) calloc(1, sizeof(BlastInitialWordOptions));
   if (*options == NULL)
      return 1;

   if (program != eBlastTypeBlastn &&
       program != eBlastTypePhiBlastn) {	/* protein-protein options. */
      (*options)->window_size = BLAST_WINDOW_SIZE_PROT;
      (*options)->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_PROT;
      (*options)->gap_trigger = BLAST_GAP_TRIGGER_PROT;
   } else {
      (*options)->window_size = BLAST_WINDOW_SIZE_NUCL;
      (*options)->gap_trigger = BLAST_GAP_TRIGGER_NUCL;
      (*options)->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_NUCL;
   }
   /* Except in one special case of greedy gapped extension, we always do 
      ungapped extension. Let the special case unset this option. */
   (*options)->ungapped_extension = TRUE;

   return 0;
}


Int2
BlastInitialWordOptionsValidate(EBlastProgramType program_number,
   const BlastInitialWordOptions* options, 
   Blast_Message* *blast_msg)
{
   Int4 code=2;
   Int4 subcode=1;

   ASSERT(options);

   /* For some blastn variants (i.e., megablast), and for PHI BLAST there is no
    * ungapped extension. */
   if (program_number != eBlastTypeBlastn  &&
       (!Blast_ProgramIsPhiBlast(program_number)) &&
       options->x_dropoff <= 0.0)
   {
      Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode,
                            "x_dropoff must be greater than zero");
         return (Int2) code;
   }
   
   return 0;
}


Int2
BLAST_FillInitialWordOptions(BlastInitialWordOptions* options, 
   EBlastProgramType program, Boolean greedy, Int4 window_size, 
   double xdrop_ungapped)
{
   if (!options)
      return 1;

   /* Ungapped extension is performed in all cases except when greedy
      gapped extension is used. */
   if (program == eBlastTypeBlastn && greedy)
       options->ungapped_extension = FALSE;

   if (window_size != 0)
      options->window_size = window_size;
   if (xdrop_ungapped != 0)
      options->x_dropoff = xdrop_ungapped;

   return 0;
}

BlastExtensionOptions*
BlastExtensionOptionsFree(BlastExtensionOptions* options)

{

	sfree(options);

	return NULL;
}

Int2
BlastExtensionOptionsNew(EBlastProgramType program, BlastExtensionOptions* *options)

{
	*options = (BlastExtensionOptions*) 
           calloc(1, sizeof(BlastExtensionOptions));

	if (*options == NULL)
		return 1;

	if (program != eBlastTypeBlastn &&
        program != eBlastTypePhiBlastn) /* protein-protein options. */
	{
		(*options)->gap_x_dropoff = BLAST_GAP_X_DROPOFF_PROT;
		(*options)->gap_x_dropoff_final = 
                   BLAST_GAP_X_DROPOFF_FINAL_PROT;
    } else {
        (*options)->gap_x_dropoff = BLAST_GAP_X_DROPOFF_NUCL;
        (*options)->gap_x_dropoff_final = BLAST_GAP_X_DROPOFF_FINAL_NUCL;
    }

    (*options)->ePrelimGapExt = eDynProgExt;
    (*options)->eTbackExt = eDynProgTbck;

    /** @todo how to determine this for PSI-BLAST bootstrap run (i.e. when
     * program is blastp? */
    if (program == eBlastTypePsiBlast) {
        (*options)->compositionBasedStats = TRUE;
    }

	return 0;
}

Int2
BLAST_FillExtensionOptions(BlastExtensionOptions* options, 
   EBlastProgramType program, Int4 greedy, double x_dropoff, 
   double x_dropoff_final)
{
   if (!options)
      return 1;

   if (program == eBlastTypeBlastn ||
       program == eBlastTypePhiBlastn) {
      switch (greedy) {
      case 1:
         options->gap_x_dropoff = BLAST_GAP_X_DROPOFF_GREEDY;
         options->ePrelimGapExt = eGreedyWithTracebackExt;
         break;
      case 2:
         options->gap_x_dropoff = BLAST_GAP_X_DROPOFF_GREEDY;
         options->gap_x_dropoff_final = BLAST_GAP_X_DROPOFF_FINAL_NUCL;
         options->ePrelimGapExt = eGreedyExt;
         options->eTbackExt = eGreedyTbck;
         break;
      default: /* Non-greedy */
         options->gap_x_dropoff = BLAST_GAP_X_DROPOFF_NUCL;
         options->gap_x_dropoff_final = BLAST_GAP_X_DROPOFF_FINAL_NUCL;
         options->ePrelimGapExt = eDynProgExt;
         options->eTbackExt = eDynProgTbck;
         break;
      }
   }

   if (program == eBlastTypePsiBlast) {
       options->compositionBasedStats = TRUE;
   }

   if (x_dropoff)
      options->gap_x_dropoff = x_dropoff;
   if (x_dropoff_final) {
      options->gap_x_dropoff_final = x_dropoff_final;
   } else {
      /* Final X-dropoff can't be smaller than preliminary X-dropoff */
      options->gap_x_dropoff_final = 
         MAX(options->gap_x_dropoff_final, x_dropoff);
   }

   return 0;

}

Int2 
BlastExtensionOptionsValidate(EBlastProgramType program_number, 
   const BlastExtensionOptions* options, Blast_Message* *blast_msg)

{
	if (options == NULL)
		return 1;

	if (program_number != eBlastTypeBlastn)
	{
		if (options->ePrelimGapExt == eGreedyWithTracebackExt || 
          options->ePrelimGapExt == eGreedyExt ||
          options->eTbackExt == eGreedyTbck)
		{
			Int4 code=2;
			Int4 subcode=1;
			Blast_MessageWrite(blast_msg, eBlastSevWarning, code, subcode, 
                            "Greedy extension only supported for BLASTN");
			return (Int2) code;
		}
	}

	return 0;
}

BlastScoringOptions*
BlastScoringOptionsFree(BlastScoringOptions* options)

{
	if (options == NULL)
		return NULL;

	sfree(options->matrix);
   sfree(options->matrix_path);
	sfree(options);

	return NULL;
}

Int2 
BlastScoringOptionsNew(EBlastProgramType program_number, BlastScoringOptions* *options)
{
   *options = (BlastScoringOptions*) calloc(1, sizeof(BlastScoringOptions));

   if (*options == NULL)
      return 1;
   
   if (program_number != eBlastTypeBlastn &&
       program_number != eBlastTypePhiBlastn) {	/* protein-protein options. */
      (*options)->shift_pen = INT2_MAX;
      (*options)->is_ooframe = FALSE;
      (*options)->gap_open = BLAST_GAP_OPEN_PROT;
      (*options)->gap_extend = BLAST_GAP_EXTN_PROT;
      (*options)->matrix = strdup(BLAST_DEFAULT_MATRIX);
   } else {	/* nucleotide-nucleotide options. */
      (*options)->penalty = BLAST_PENALTY;
      (*options)->reward = BLAST_REWARD;
      /* This is correct except when greedy extension is used. In that case 
         these values would have to be reset. */
      (*options)->gap_open = BLAST_GAP_OPEN_NUCL;
      (*options)->gap_extend = BLAST_GAP_EXTN_NUCL;
   }
   (*options)->decline_align = INT2_MAX;
   (*options)->gapped_calculation = TRUE;
   
   return 0;
}

Int2 
BLAST_FillScoringOptions(BlastScoringOptions* options, 
   EBlastProgramType program_number, Boolean greedy_extension, Int4 penalty, Int4 reward, 
   const char *matrix, Int4 gap_open, Int4 gap_extend)
{
   if (!options)
      return 1;

   if (program_number != eBlastTypeBlastn &&
       program_number != eBlastTypePhiBlastn) {	/* protein-protein options. */
      /* If matrix name is not provided, keep the default "BLOSUM62" value filled in 
         BlastScoringOptionsNew, otherwise reset it. */
      if (matrix)
          BlastScoringOptionsSetMatrix(options, matrix);
   } else {	/* nucleotide-nucleotide options. */
      if (penalty)
         options->penalty = penalty;
      if (reward)
         options->reward = reward;

      if (greedy_extension) {
         options->gap_open = BLAST_GAP_OPEN_MEGABLAST;
         options->gap_extend = BLAST_GAP_EXTN_MEGABLAST;
      }	else {
         options->gap_open = BLAST_GAP_OPEN_NUCL;
         options->gap_extend = BLAST_GAP_EXTN_NUCL;
      }
   }
   if (gap_open >= 0)
      options->gap_open = gap_open;
   if (gap_extend >= 0)
      options->gap_extend = gap_extend;

   return 0;
}

Int2 
BlastScoringOptionsValidate(EBlastProgramType program_number, 
   const BlastScoringOptions* options, Blast_Message* *blast_msg)

{
	if (options == NULL)
		return 1;

   if (program_number == eBlastTypeTblastx && 
              options->gapped_calculation)
   {
		Int4 code=2;
		Int4 subcode=1;
      Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode, 
         "Gapped search is not allowed for tblastx");
		return (Int2) code;
   }

	if (program_number == eBlastTypeBlastn ||
        program_number == eBlastTypePhiBlastn)
	{
		if (options->penalty >= 0)
		{
			Int4 code=2;
			Int4 subcode=1;
			Blast_MessageWrite(blast_msg, eBlastSevWarning, code, subcode, 
                            "BLASTN penalty must be negative");
			return (Int2) code;
		}
                if (options->gapped_calculation && options->gap_open > 0 && options->gap_extend == 0) 
                {
                        Int4 code=2;
                        Int4 subcode=1;
                        Blast_MessageWrite(blast_msg, eBlastSevWarning, 
                           code, subcode, 
                           "BLASTN gap extension penalty cannot be 0");
                        return (Int2) code;
                }
	}
	else
	{
                if (options->gapped_calculation && !Blast_ProgramIsRpsBlast(program_number))
                {
                    Int2 status=0;
                    if ((status=Blast_KarlinBlkGappedLoadFromTables(NULL, options->gap_open,
                     options->gap_extend, options->decline_align,
                     options->matrix)) != 0)
                     {
			if (status == 1)
			{
				char* buffer;
				Int4 code=2;
				Int4 subcode=1;

				buffer = BLAST_PrintMatrixMessage(options->matrix); 
            Blast_MessageWrite(blast_msg, eBlastSevError,
                               code, subcode, buffer);
				sfree(buffer);
				return (Int2) code;
				
			}
			else if (status == 2)
			{
				char* buffer;
				Int4 code=2;
				Int4 subcode=1;

				buffer = BLAST_PrintAllowedValues(options->matrix, 
                        options->gap_open, options->gap_extend, 
                        options->decline_align); 
            Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode, 
                               buffer);
				sfree(buffer);
				return (Int2) code;
			}
                    }
	       }
	}

	if (program_number != eBlastTypeBlastx && 
       program_number != eBlastTypeTblastn && options->is_ooframe)
	{
      Int4 code=2;
      Int4 subcode=1;
      Blast_MessageWrite(blast_msg, eBlastSevWarning, code, subcode, 
         "Out-of-frame only permitted for blastx and tblastn");
      return (Int2) code;
	}

	return 0;

}

Int2 
BlastScoringOptionsDup(BlastScoringOptions* *new_opt, const BlastScoringOptions* old_opt)
{
    if (old_opt == NULL || new_opt == NULL)
       return -1;

    *new_opt = (BlastScoringOptions*) BlastMemDup(old_opt, sizeof(BlastScoringOptions));
    if (*new_opt == NULL)
       return -1;

    if (old_opt->matrix)
       (*new_opt)->matrix = strdup(old_opt->matrix);

    if (old_opt->matrix_path)
       (*new_opt)->matrix_path = strdup(old_opt->matrix_path);

    return 0;
}

Int2 BlastScoringOptionsSetMatrix(BlastScoringOptions* opts,
                                  const char* matrix_name)
{
    Uint4 i;

    if (matrix_name) {
        sfree(opts->matrix);
        opts->matrix = strdup(matrix_name);
        /* Make it all upper case */
        for (i=0; i<strlen(opts->matrix); ++i)
            opts->matrix[i] = toupper((unsigned char) opts->matrix[i]);
    }
    return 0;
}

BlastEffectiveLengthsOptions*
BlastEffectiveLengthsOptionsFree(BlastEffectiveLengthsOptions* options)

{
	sfree(options);

	return NULL;
}


Int2 
BlastEffectiveLengthsOptionsNew(BlastEffectiveLengthsOptions* *options)

{
   *options = (BlastEffectiveLengthsOptions*)
      calloc(1, sizeof(BlastEffectiveLengthsOptions));

   if (*options == NULL)
      return 1;
   
   return 0;
}

Int2 
BLAST_FillEffectiveLengthsOptions(BlastEffectiveLengthsOptions* options, 
   Int4 dbseq_num, Int8 db_length, Int8 searchsp_eff)
{
   if (!options)
      return 1;

   if (searchsp_eff) {	
      /* dbnum_seq and dblen are used to calculate effective search space, so 
         if it is already set don't bother with those. */
      options->searchsp_eff = searchsp_eff;
      return 0;
   }

   options->dbseq_num = dbseq_num;
   options->db_length = db_length;

   return 0;
}

LookupTableOptions*
LookupTableOptionsFree(LookupTableOptions* options)

{

      if (options == NULL)
          return NULL;

      sfree(options->phi_pattern);
   
	sfree(options);
	return NULL;
}

Int2 
LookupTableOptionsNew(EBlastProgramType program_number, LookupTableOptions* *options)
{
   *options = (LookupTableOptions*) calloc(1, sizeof(LookupTableOptions));
   
   if (*options == NULL)
      return 1;
   
   switch (program_number) {
   case eBlastTypeBlastn:
       /* Blastn default is megablast. */
       (*options)->word_size = BLAST_WORDSIZE_MEGABLAST;
       (*options)->lut_type = MB_LOOKUP_TABLE;
       (*options)->max_positions = INT4_MAX;
       /* Discontig mb scanning default is one byte at at time. */
       (*options)->full_byte_scan = TRUE; 
       break;
   case eBlastTypeRpsBlast: case eBlastTypeRpsTblastn:
       (*options)->word_size = BLAST_WORDSIZE_PROT;
       (*options)->lut_type = RPS_LOOKUP_TABLE;
       
       if (program_number == eBlastTypeRpsBlast)
           (*options)->threshold = BLAST_WORD_THRESHOLD_BLASTP;
       else 
           (*options)->threshold = BLAST_WORD_THRESHOLD_TBLASTN;
       break;
   case eBlastTypePhiBlastn:
       (*options)->lut_type = PHI_NA_LOOKUP;
       break;
   case eBlastTypePhiBlastp:
       (*options)->lut_type = PHI_AA_LOOKUP;
       break;
   default:
       (*options)->word_size = BLAST_WORDSIZE_PROT;
       (*options)->lut_type = AA_LOOKUP_TABLE;
       
       if (program_number == eBlastTypeBlastp)
           (*options)->threshold = BLAST_WORD_THRESHOLD_BLASTP;
       else if (program_number == eBlastTypeBlastx)
           (*options)->threshold = BLAST_WORD_THRESHOLD_BLASTX;
       else if (program_number == eBlastTypeTblastn)
           (*options)->threshold = BLAST_WORD_THRESHOLD_TBLASTN;
       else if (program_number == eBlastTypeTblastx)
           (*options)->threshold = BLAST_WORD_THRESHOLD_TBLASTX;
       break;
   }

   return 0;
}

Int2 
BLAST_FillLookupTableOptions(LookupTableOptions* options, 
   EBlastProgramType program_number, Boolean is_megablast, Int4 threshold,
   Int4 word_size, Boolean variable_wordsize)
{
   if (!options)
      return 1;

   if (program_number == eBlastTypeBlastn) {
      if (is_megablast)	{
         options->word_size = BLAST_WORDSIZE_MEGABLAST;
         options->lut_type = MB_LOOKUP_TABLE;
         options->max_positions = INT4_MAX;
      }	else {
         options->lut_type = NA_LOOKUP_TABLE;
         options->word_size = BLAST_WORDSIZE_NUCL;
      }
   } else {
      options->lut_type = AA_LOOKUP_TABLE;
   }

   /* if the supplied threshold is -1, disable neighboring words */
   if (threshold == -1)
      options->threshold = 0;

   /* if the supplied threshold is > 0, use it otherwise, use the default */
   if (threshold > 0)
      options->threshold = threshold;

   if (Blast_ProgramIsRpsBlast(program_number))
      options->lut_type = RPS_LOOKUP_TABLE;
   if (word_size)
      options->word_size = word_size;
   if (program_number == eBlastTypeBlastn) {
      options->variable_wordsize = variable_wordsize;
   }
   return 0;
}

Int2 BLAST_GetSuggestedThreshold(EBlastProgramType program_number, const char* matrixName, Int4* threshold)
{

    const Int4 kB62_threshold = 11;

    if (program_number == eBlastTypeBlastn)
      return 0;

    if (matrixName == NULL)
      return -1;

    if(strcasecmp(matrixName, "BLOSUM62") == 0)
        *threshold = kB62_threshold;
    else if(strcasecmp(matrixName, "BLOSUM45") == 0)
        *threshold = 14;
    else if(strcasecmp(matrixName, "BLOSUM62_20") == 0)
        *threshold = 100;
    else if(strcasecmp(matrixName, "BLOSUM80") == 0)
        *threshold = 12;
    else if(strcasecmp(matrixName, "PAM30") == 0)
        *threshold = 16;
    else if(strcasecmp(matrixName, "PAM70") == 0)
        *threshold = 14;
    else
        *threshold = kB62_threshold;

    if (Blast_SubjectIsTranslated(program_number) == TRUE)
        *threshold += 2;  /* Covers tblastn, tblastx, psi-tblastn rpstblastn. */
    else if (Blast_QueryIsTranslated(program_number) == TRUE)
        *threshold += 1;

    return 0;
}

Int2 BLAST_GetSuggestedWindowSize(EBlastProgramType program_number, const char* matrixName, Int4* window_size)
{
    const Int4 kB62_windowsize = 40;

    if (program_number == eBlastTypeBlastn)
      return 0;

    if (matrixName == NULL)
      return -1;

    if(strcasecmp(matrixName, "BLOSUM62") == 0)
        *window_size = kB62_windowsize;
    else if(strcasecmp(matrixName, "BLOSUM45") == 0)
        *window_size = 60;
    else if(strcasecmp(matrixName, "BLOSUM80") == 0)
        *window_size = 25;
    else if(strcasecmp(matrixName, "PAM30") == 0)
        *window_size = 15;
    else if(strcasecmp(matrixName, "PAM70") == 0)
        *window_size = 20;
    else
        *window_size = kB62_windowsize;

    return 0;
}

/** Validate options for the discontiguous word megablast
 * Word size must be 11 or 12; template length 16, 18 or 21; 
 * template type 0, 1 or 2.
 * @param word_size Word size option [in]
 * @param template_length Discontiguous template length [in]
 * @param template_type Discontiguous template type [in]
 * @return TRUE if options combination valid.
 */
static Boolean 
s_DiscWordOptionsValidate(Int4 word_size, Uint1 template_length,
                        Uint1 template_type)
{
   if (template_length == 0)
      return TRUE;

   if (word_size != 11 && word_size != 12)
      return FALSE;
   if (template_length != 16 && template_length != 18 && 
       template_length != 21)
      return FALSE;
   if (template_type > 2)
      return FALSE;

   return TRUE;
}

Int2 
LookupTableOptionsValidate(EBlastProgramType program_number, 
   const LookupTableOptions* options, Blast_Message* *blast_msg)

{
   Int4 code=2;
   Int4 subcode=1;
   const Boolean kPhiBlast = Blast_ProgramIsPhiBlast(program_number);

	if (options == NULL)
		return 1;

    if (options->phi_pattern && !kPhiBlast) {
        Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode, 
            "PHI pattern can be specified only for blastp and blastn");
        return (Int2) code;
    }

    /* For PHI BLAST, the subsequent word size tests are not needed. */
    if (kPhiBlast)
        return 0;

	if (program_number != eBlastTypeBlastn && 
        (!Blast_ProgramIsRpsBlast(program_number)) &&
        options->threshold <= 0)
	{
		Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode, 
                         "Non-zero threshold required");
		return (Int2) code;
	}

	if (options->word_size <= 0)
	{
        if ( !Blast_ProgramIsRpsBlast(program_number)) {
            Blast_MessageWrite(blast_msg, eBlastSevError, 
                                       code, subcode, 
                                     "Word-size must be greater than zero");
            return (Int2) code;
        }
	} else if (program_number == eBlastTypeBlastn && options->word_size < 4)
	{
		Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode, 
                         "Word-size must be 4" 
                         "or greater for nucleotide comparison");
		return (Int2) code;
	} else if (program_number != eBlastTypeBlastn && options->word_size > 5)
	{
		Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode, 
                         "Word-size must be less"
                         "than 6 for protein comparison");
		return (Int2) code;
	}


        /* FIXME: is this really needed?? */
        if (options->variable_wordsize && 
          ((options->word_size % 4) != 0) ) {
         Blast_MessageWrite(blast_msg, eBlastSevWarning, code, subcode, 
                            "Word size must be divisible by 4 if only full "
                            "bytes of subject sequences are matched to query");
         return (Int2) code;
      }

	if (program_number != eBlastTypeBlastn && 
       options->lut_type == MB_LOOKUP_TABLE)
	{
		Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode, 
                         "Megablast lookup table only supported with blastn");
		return (Int2) code;
	}

   if (program_number == eBlastTypeBlastn && options->mb_template_length > 0) {
      if (!s_DiscWordOptionsValidate(options->word_size,
              options->mb_template_length, options->mb_template_type)) {
         Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode, 
                            "Invalid discontiguous template parameters");
         return (Int2) code;
      } else if (options->lut_type != MB_LOOKUP_TABLE) {
         Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode, 
            "Invalid lookup table type for discontiguous Mega BLAST");
         return (Int2) code;
      } 
   }
	return 0;
}

BlastHitSavingOptions*
BlastHitSavingOptionsFree(BlastHitSavingOptions* options)

{
  sfree(options);
  return NULL;
}


Int2 BlastHitSavingOptionsNew(EBlastProgramType program_number, 
        BlastHitSavingOptions* *options)
{
   *options = (BlastHitSavingOptions*) calloc(1, sizeof(BlastHitSavingOptions));
   
   if (*options == NULL)
      return 1;

   (*options)->hitlist_size = BLAST_HITLIST_SIZE;
   (*options)->expect_value = BLAST_EXPECT_VALUE;

   return 0;

}

Int2
BLAST_FillHitSavingOptions(BlastHitSavingOptions* options, 
                           double evalue, Int4 hitlist_size,
                           Boolean is_gapped, Int4 culling_limit)
{
   if (!options)
      return 1;

   if (hitlist_size)
      options->hitlist_size = hitlist_size;
   if (evalue)
      options->expect_value = evalue;
   if(!is_gapped)
     options->hsp_num_max = kUngappedHSPNumMax;
   options->culling_limit = culling_limit;

   return 0;

}

Int2
BlastHitSavingOptionsValidate(EBlastProgramType program_number,
   const BlastHitSavingOptions* options, Blast_Message* *blast_msg)
{
	if (options == NULL)
		return 1;

	if (options->hitlist_size < 1)
	{
		Int4 code=1;
		Int4 subcode=1;
		Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode, 
                         "No hits are being saved");
		return (Int2) code;
	}

	if (options->expect_value <= 0.0 && options->cutoff_score <= 0)
	{
		Int4 code=2;
		Int4 subcode=1;
		Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode, 
         "expect value or cutoff score must be greater than zero");
		return (Int2) code;
	}	

   if (options->longest_intron != 0 && 
       program_number != eBlastTypeTblastn && 
       program_number != eBlastTypeBlastx) {
		Int4 code=2;
		Int4 subcode=1;
		Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode, 
         "Uneven gap linking of HSPs is allowed for blastx and tblastn only");
		return (Int2) code;
   }

	if (options->culling_limit < 0)
	{
		Int4 code=2;
		Int4 subcode=1;
		Blast_MessageWrite(blast_msg, eBlastSevError, code, subcode, 
                    "culling limit must be greater than or equal to zero");
		return (Int2) code;
	}	

	return 0;
}

Int2 PSIBlastOptionsNew(PSIBlastOptions** psi_options)
{
   PSIBlastOptions* options = NULL;

   if ( !psi_options )
      return 1;

   options = (PSIBlastOptions*)calloc(1, sizeof(PSIBlastOptions));
   if ( !options ) 
       return 1;

   *psi_options = options;
   options->inclusion_ethresh = PSI_INCLUSION_ETHRESH;
   options->pseudo_count = PSI_PSEUDO_COUNT_CONST;
   options->use_best_alignment = TRUE;

   options->nsg_compatibility_mode = FALSE;
   options->impala_scaling_factor = kPSSM_NoImpalaScaling;
   
   return 0;
}

Int2 PSIBlastOptionsValidate(const PSIBlastOptions* psi_options,
                             Blast_Message** blast_msg)
{
    Int2 retval = 1;    /* assume failure */

    if ( !psi_options ) {
        return retval;
    }

    if (psi_options->pseudo_count <= 0) {
        Blast_MessageWrite(blast_msg, eBlastSevError, 0, 0,
                           "Pseudo count must be greater than 0");
        return retval;
    }

    if (psi_options->inclusion_ethresh <= 0.0) {
        Blast_MessageWrite(blast_msg, eBlastSevError, 0, 0,
                           "Inclusion threshold must be greater than 0");
        return retval;
    }

    retval = 0;
    return retval;
}

PSIBlastOptions* PSIBlastOptionsFree(PSIBlastOptions* psi_options)
{
   sfree(psi_options);
   return NULL;
}

Int2 BlastDatabaseOptionsNew(BlastDatabaseOptions** db_options)
{
   BlastDatabaseOptions* options = NULL;

   if ( !db_options ) {
       return 1;
   }

   options = (BlastDatabaseOptions*) calloc(1, sizeof(BlastDatabaseOptions));
   if ( !options ) {
       return 1;
   }

   options->genetic_code = BLAST_GENETIC_CODE;
   *db_options = options;

   return 0;
}

BlastDatabaseOptions* 
BlastDatabaseOptionsFree(BlastDatabaseOptions* db_options)
{

   if (db_options == NULL)
      return NULL;

   sfree(db_options->gen_code_string);
   sfree(db_options);
   return NULL;
}

Int2 BLAST_InitDefaultOptions(EBlastProgramType program_number,
   LookupTableOptions** lookup_options,
   QuerySetUpOptions** query_setup_options, 
   BlastInitialWordOptions** word_options,
   BlastExtensionOptions** ext_options,
   BlastHitSavingOptions** hit_options,
   BlastScoringOptions** score_options,
   BlastEffectiveLengthsOptions** eff_len_options,
   PSIBlastOptions** psi_options,
   BlastDatabaseOptions** db_options)
{
   Int2 status;

   if ((status = LookupTableOptionsNew(program_number, lookup_options)))
      return status;

   if ((status=BlastQuerySetUpOptionsNew(query_setup_options)))
      return status;

   if ((status=BlastInitialWordOptionsNew(program_number, word_options)))
      return status;

   if ((status = BlastExtensionOptionsNew(program_number, ext_options)))
      return status;

   if ((status=BlastHitSavingOptionsNew(program_number, hit_options)))
      return status;

   if ((status=BlastScoringOptionsNew(program_number, score_options)))
      return status;

   if ((status=BlastEffectiveLengthsOptionsNew(eff_len_options)))
      return status;
   
   if ((status=PSIBlastOptionsNew(psi_options)))
      return status;

   if ((status=BlastDatabaseOptionsNew(db_options)))
      return status;

   return 0;

}

/**  Checks that the extension and scoring options are consistent with each other
 * @param program_number identifies the program [in]
 * @param ext_options the extension options [in]
 * @param score_options the scoring options [in]
 * @param blast_msg returns a message on errors. [in|out]
 * @return zero on success, an error code otherwise. 
 */
static Int2 s_BlastExtensionScoringOptionsValidate(EBlastProgramType program_number,
                           const BlastExtensionOptions* ext_options,
                           const BlastScoringOptions* score_options, 
                           Blast_Message* *blast_msg)
{
    if (ext_options == NULL || score_options == NULL)
        return -1;

    if (program_number == eBlastTypeBlastn)
    {
        if (score_options->gap_open == 0 && score_options->gap_extend == 0)
        {
	    if (ext_options->ePrelimGapExt != eGreedyWithTracebackExt && 
                ext_options->ePrelimGapExt != eGreedyExt && 
                ext_options->eTbackExt != eGreedyTbck)
	    {
			Int4 code=2;
			Int4 subcode=1;
			Blast_MessageWrite(blast_msg, eBlastSevWarning, code, subcode, 
                            "Greedy extension must be used if gap existence and extension options are zero");
			return (Int2) code;
	    }
	}
    }

    return 0;
}
                   

Int2 BLAST_ValidateOptions(EBlastProgramType program_number,
                           const BlastExtensionOptions* ext_options,
                           const BlastScoringOptions* score_options, 
                           const LookupTableOptions* lookup_options, 
                           const BlastInitialWordOptions* word_options, 
                           const BlastHitSavingOptions* hit_options,
                           Blast_Message* *blast_msg)
{
   Int2 status = 0;

   if ((status = BlastExtensionOptionsValidate(program_number, ext_options,
                                               blast_msg)) != 0)
       return status;
   if ((status = BlastScoringOptionsValidate(program_number, score_options,
                                               blast_msg)) != 0)
       return status;
   if ((status = LookupTableOptionsValidate(program_number, 
                    lookup_options, blast_msg)) != 0)   
       return status;
   if ((status = BlastInitialWordOptionsValidate(program_number, 
                    word_options, blast_msg)) != 0)   
       return status;
   if ((status = BlastHitSavingOptionsValidate(program_number, hit_options,
                                               blast_msg)) != 0)
       return status;
   if ((status = s_BlastExtensionScoringOptionsValidate(program_number, ext_options,
                                               score_options, blast_msg)) != 0)
       return status;

   

   return status;
}

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.177  2005/12/19 16:11:12  papadopo
 * no minimum value for megablast word size need be enforced; the engine will switch to a standard lookup table if the specified word size is too small
 *
 * Revision 1.176  2005/12/12 13:38:27  madden
 * Add call to s_BlastExtensionScoringOptionsValidate to BLAST_ValidateOptions to check that scoring and extension options are consistent
 *
 * Revision 1.175  2005/11/16 14:27:03  madden
 * Fix spelling in CRN
 *
 * Revision 1.174  2005/10/18 15:19:04  madden
 * Exclude rpsblast from validation of gap parameters
 *
 * Revision 1.173  2005/10/17 14:03:34  madden
 * Change convention for unset gap parameters from zero to negative number
 *
 * Revision 1.172  2005/08/29 13:51:44  madden
 * Add functions BLAST_GetSuggestedThreshold and BLAST_GetSuggestedWindowSize
 *
 * Revision 1.171  2005/06/24 12:15:40  madden
 * Add protection against NULL pointers in options free functons
 *
 * Revision 1.170  2005/06/20 13:09:36  madden
 * Rename BlastSeverity enums in line with C++ tookit convention
 *
 * Revision 1.169  2005/06/03 16:22:22  lavr
 * Explicit (unsigned char) casts in ctype routines
 *
 * Revision 1.168  2005/06/02 16:18:40  camacho
 * Remove LookupTableOptions::use_pssm
 *
 * Revision 1.167  2005/05/20 18:26:54  camacho
 * Deduce LookupTableOptions::use_pssm from program type
 *
 * Revision 1.166  2005/05/16 12:22:07  madden
 * Removal of prelim_hitlist_size as an option
 *
 * Revision 1.165  2005/05/06 14:27:26  camacho
 * + Blast_ProgramIs{Phi,Rps}Blast
 *
 * Revision 1.164  2005/05/02 19:38:47  camacho
 * Introduced constant for IMPALA-style PSSM scaling
 *
 * Revision 1.163  2005/04/27 19:53:45  dondosha
 * Added handling of PHI BLAST program enumeration values
 *
 * Revision 1.162  2005/04/27 14:46:18  papadopo
 * set culling limit in BlastHitSavingOptions, and validate it
 *
 * Revision 1.161  2005/03/21 15:20:31  papadopo
 * Allow RPS searches in LookupTableOptionsValidate
 *
 * Revision 1.160  2005/03/08 21:00:51  camacho
 * PSIBlastOptionsNew returns 0 on success only, passing in NULL is an error
 *
 * Revision 1.159  2005/03/02 13:55:26  madden
 * Rename Filtering option funcitons to standard naming convention
 *
 * Revision 1.158  2005/02/28 17:14:48  camacho
 * Move hard coded constant to header
 *
 * Revision 1.157  2005/02/24 13:43:04  madden
 * Added new functions for structured filtering options
 *
 * Revision 1.156  2005/02/22 22:48:02  camacho
 * + impala_scaling_factor, first cut
 *
 * Revision 1.155  2005/02/17 17:49:22  camacho
 * fix to BlastScoringOptionsSetMatrix
 *
 * Revision 1.154  2005/02/08 18:32:49  dondosha
 * Added check in lookup table options validation that program and PHI pattern are compatible
 *
 * Revision 1.153  2005/02/03 21:37:03  dondosha
 * Tiny memory leak fix
 *
 * Revision 1.152  2005/02/02 18:55:33  dondosha
 * Added BlastScoringOptionsSetMatrix; added setting of more default values in some of the BlastXXXOptionsNew functions
 *
 * Revision 1.151  2005/01/10 13:21:23  madden
 * Move some fields from InitialWordOptions to InitialWordParameters, remove function CalculateBestStride
 *
 * Revision 1.150  2004/12/29 13:33:47  madden
 * Removed functions relevant to parameters, move to blast_parameters.c
 *
 * Revision 1.149  2004/12/28 13:37:01  madden
 * word size changed from Int2 to Int4
 *
 * Revision 1.148  2004/12/21 17:10:58  dondosha
 * Removed eSkipTbck option
 *
 * Revision 1.147  2004/12/13 22:26:59  camacho
 * Consolidated structure group customizations in option: nsg_compatibility_mode
 *
 * Revision 1.146  2004/12/09 15:22:56  dondosha
 * Renamed some functions dealing with BlastScoreBlk and Blast_KarlinBlk structures
 *
 * Revision 1.145  2004/12/08 14:20:29  camacho
 * + PSIBlastOptionsValidate
 *
 * Revision 1.144  2004/12/02 17:22:41  camacho
 * Fix compiler error
 *
 * Revision 1.143  2004/12/02 15:55:54  bealer
 * - Change multiple-arrays to array-of-struct in BlastQueryInfo
 *
 * Revision 1.142  2004/11/22 14:38:48  camacho
 * + option to set % identity threshold to PSSM engine
 *
 * Revision 1.141  2004/11/15 16:32:37  dondosha
 * Changed constant names and static functions names in accordance with C++ toolkit guidelines
 *
 * Revision 1.140  2004/11/02 18:20:14  madden
 * 1.) Reorganization of options to move gap_trigger from BlastExtensionParameters to BlastInitialWordOptions,
 * and use ungapped_cutoff in place of gap_trigger in parameter structures.
 * 2.) restored CUTOFF_E_ to orginal values and change call to BlastCutoffs
 * 3.) cleaned up code.
 *
 * Revision 1.139  2004/11/02 17:56:48  camacho
 * Add DOXYGEN_SKIP_PROCESSING to guard rcsid string
 *
 * Revision 1.138  2004/11/01 18:37:23  madden
 *    - In BLAST_FillHitSavingOption, set hsp_num_max to a finite value
 *      (400) for ungapped searches.
 *
 *    - In CalculateLinkHSPCutoffs reset gap_prob for each subject
 *      sequence before the value of gap_prob is used.
 *
 * Revision 1.137  2004/10/25 16:27:49  coulouri
 * include the length of the reverse complement strand for blastn
 *
 * Revision 1.136  2004/10/14 17:10:35  madden
 * BlastHitSavingParametersNew and BlastHitSavingParametersUpdate changes for gapped sum statistics
 *
 * Revision 1.135  2004/10/13 17:44:17  dondosha
 * Added comment for setting of ungapped X-dropoff in BlastInitialWordParametersUpdate
 *
 * Revision 1.134  2004/09/28 16:20:17  papadopo
 * From Michael Gertz:
 * 1. Disallow setting longest intron for programs other than tblastn and
 * 	blastx; previously it was possible to set it to a negative value.
 * 2. For ungapped blastx and tblastn, if longest_intron is not set
 *         (i.e. = 0) or (longest_intron - 2)/3 is nonpositive, call
 *         link_hsps. Otherwise call new_link_hsps.
 * 3. For gapped blastx or tblastn, if longest_intron is not set
 *         (i.e. = 0), set it to 122.  Then call new_link_hsps if
 *         (longest_intron - 2)/3 is positive.  Otherwise turn off sum
 *         statistics.
 *
 * Revision 1.133  2004/09/23 15:00:29  dondosha
 * Reset gap_prob in CalculateLinkHSPCutoffs when necessary; fixed a doxygen comment
 *
 * Revision 1.132  2004/09/14 21:17:02  camacho
 * Add structure group customization to ignore query
 *
 * Revision 1.131  2004/09/13 19:33:49  camacho
 * Add comments
 *
 * Revision 1.130  2004/08/16 19:45:14  dondosha
 * Implemented uneven gap linking of HSPs for blastx
 *
 * Revision 1.129  2004/08/12 13:01:02  madden
 * Add static function BlastFindValidKarlinBlk to look for valid Karlin blocks in an array of them, prevents use of bogus (-1) Karlin-Altschul parameters
 *
 * Revision 1.128  2004/08/06 16:22:56  dondosha
 * In initial word options validation, return after first encountered error
 *
 * Revision 1.127  2004/08/05 20:41:01  dondosha
 * Implemented stacks initial word container for all blastn extension methods
 *
 * Revision 1.126  2004/08/05 19:54:12  dondosha
 * Allow stride 1 for discontiguous megablast in options validation
 *
 * Revision 1.125  2004/08/03 20:19:11  dondosha
 * Added initial word options validation; set seed container type and extension method appropriately
 *
 * Revision 1.124  2004/07/16 17:31:19  dondosha
 * When one-hit word finder is used for protein searches, reduce cutoff score, like it is done in old engine
 *
 * Revision 1.123  2004/07/07 15:05:03  camacho
 * Handle eBlastTypeUndefined in switch stmt
 *
 * Revision 1.122  2004/07/06 15:42:15  dondosha
 * Use EBlastProgramType enumeration type instead of Uint1 for program argument in all functions
 *
 * Revision 1.121  2004/06/28 21:41:02  dondosha
 * Test for NULL input in BlastHitSavingParametersFree
 *
 * Revision 1.120  2004/06/23 14:43:04  dondosha
 * Return 0 from PSIBlastOptionsNew if NULL pointer argument is provided
 *
 * Revision 1.119  2004/06/22 17:53:22  dondosha
 * Moved parameters specific to HSP linking into a independent structure
 *
 * Revision 1.118  2004/06/22 16:45:14  camacho
 * Changed the blast_type_* definitions for the TBlastProgramType enumeration.
 *
 * Revision 1.117  2004/06/17 20:46:25  camacho
 * Use consistent return values for errors
 *
 * Revision 1.116  2004/06/09 22:44:03  dondosha
 * Set sum statistics parameter to TRUE by default for ungapped blastp
 *
 * Revision 1.115  2004/06/09 22:27:44  dondosha
 * Do not reduce score cutoffs to gap_trigger value for ungapped blastn
 *
 * Revision 1.114  2004/06/09 14:11:34  camacho
 * Set default for use_best_alignment
 *
 * Revision 1.113  2004/06/08 15:12:51  dondosha
 * Removed skip_traceback option; added eSkipTbck type to traceback extension types enum
 *
 * Revision 1.112  2004/06/07 15:44:47  dondosha
 * do_sum_stats option is now an enum; set do_sum_stats parameter only if option is not set;
 *
 * Revision 1.111  2004/05/26 16:04:54  papadopo
 * fix doxygen errors
 *
 * Revision 1.110  2004/05/24 17:26:21  camacho
 * Fix PC warning
 *
 * Revision 1.109  2004/05/20 16:29:30  madden
 * Make searchsp an Int8 consistent with rest of blast
 *
 * Revision 1.108  2004/05/19 14:52:02  camacho
 * 1. Added doxygen tags to enable doxygen processing of algo/blast/core
 * 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
 *    location
 * 3. Added use of @todo doxygen keyword
 *
 * Revision 1.107  2004/05/17 15:30:20  madden
 * Int algorithm_type replaced with enum EBlastPrelimGapExt, removed include for blast_gapalign.h
 *
 * Revision 1.106  2004/05/14 17:11:03  dondosha
 * Minor correction in setting X-dropoffs
 *
 * Revision 1.105  2004/05/14 13:14:15  camacho
 * Use correct definition for inclusion threshold
 *
 * Revision 1.104  2004/05/12 12:18:06  madden
 * Clean out PSIBlast options, add fields to ExtensionOptions to support smith-waterman and composition-based stats
 *
 * Revision 1.103  2004/05/10 14:27:23  madden
 * Correction to CalculateLinkHSPCutoffs to use gap_trigger in calculation of small cutoff
 *
 * Revision 1.102  2004/05/07 15:22:15  papadopo
 * 1. add functions to allocate and free BlastScoringParameters structures
 * 2. apply a scaling factor to all cutoffs generated in HitSavingParameters
 *    or ExtentionParameters structures
 *
 * Revision 1.101  2004/04/29 17:41:05  papadopo
 * Scale down the search space when calculating the S2 cutoff score for a translated RPS search
 *
 * Revision 1.100  2004/04/29 15:08:43  madden
 * Add BlastScoringOptionsDup
 *
 * Revision 1.99  2004/04/23 14:02:25  papadopo
 * ignore validation of LookupTableOptions if performing an RPS search
 *
 * Revision 1.98  2004/04/22 22:18:03  dondosha
 * Set lookup table type correctly in BLAST_FillLookupTableOptions - needed for C driver only
 *
 * Revision 1.97  2004/04/21 17:00:59  madden
 * Removed set but not read variable
 *
 * Revision 1.96  2004/04/19 12:58:44  madden
 * Changed BLAST_KarlinBlk to Blast_KarlinBlk to avoid conflict with blastkar.h structure, renamed some functions to start with Blast_Karlin, made Blast_KarlinBlkDestruct public
 *
 * Revision 1.95  2004/04/16 14:17:06  papadopo
 * add use of RPS-specific defines, remove RPS argument to FillLookupTableOptions
 *
 * Revision 1.94  2004/04/07 03:06:16  camacho
 * Added blast_encoding.[hc], refactoring blast_stat.[hc]
 *
 * Revision 1.93  2004/03/26 20:46:00  dondosha
 * Made gap_trigger parameter an integer, as in the old code
 *
 * Revision 1.92  2004/03/22 20:11:37  dondosha
 * Do not allow small gaps cutoff to be less than gap trigger
 *
 * Revision 1.91  2004/03/17 15:19:10  camacho
 * Add missing casts
 *
 * Revision 1.90  2004/03/11 23:58:10  dondosha
 * Set cutoff_score to 0 before calling BLAST_Cutoffs, so it knows what to calculate
 *
 * Revision 1.89  2004/03/11 20:41:49  camacho
 * Remove dead code
 *
 * Revision 1.88  2004/03/10 17:33:10  papadopo
 * Make a separate lookup table type for RPS blast
 *
 * Revision 1.87  2004/03/09 22:37:26  dondosha
 * Added const qualifiers to parameter arguments wherever relevant
 *
 * Revision 1.86  2004/03/09 18:46:24  dondosha
 * Corrected how cutoffs are calculated
 *
 * Revision 1.85  2004/03/04 21:07:48  papadopo
 * add RPS BLAST functionality
 *
 * Revision 1.84  2004/02/27 15:56:33  papadopo
 * Mike Gertz' modifications to unify handling of gapped Karlin blocks for protein and nucleotide searches. Also modified BLAST_MainSetUp to allocate gapped Karlin blocks last
 *
 * Revision 1.83  2004/02/24 17:57:14  dondosha
 * Added function to combine all options validation functions for the C engine
 *
 * Revision 1.82  2004/02/19 21:16:48  dondosha
 * Use enum type for severity argument in Blast_MessageWrite
 *
 * Revision 1.81  2004/02/17 22:10:30  dondosha
 * Set preliminary hitlist size in options initialization
 *
 * Revision 1.80  2004/02/07 15:48:30  ucko
 * PSIBlastOptionsNew: rearrange slightly so that declarations come first.
 *
 * Revision 1.79  2004/02/06 22:49:30  dondosha
 * Check for NULL pointer in PSIBlastOptionsNew
 *
 * Revision 1.78  2004/02/03 18:33:39  dondosha
 * Correction to previous change: word size can be 11 if discontiguous words
 *
 * Revision 1.77  2004/02/03 16:17:33  dondosha
 * Require word size to be >= 12 with megablast lookup table
 *
 * Revision 1.76  2004/02/02 18:49:32  dondosha
 * Fixes for minor compiler warnings
 *
 * Revision 1.75  2003/12/31 20:04:47  dondosha
 * Round best stride to a number divisible by 4 for all values except 6 and 7
 *
 * Revision 1.74  2003/12/31 16:04:37  coulouri
 * use -1 to disable protein neighboring words
 *
 * Revision 1.73  2003/12/08 16:03:05  coulouri
 * Propagate protein neighboring threshold even if it is zero
 *
 * Revision 1.72  2003/11/24 23:18:32  dondosha
 * Added gap_decay_rate argument to BLAST_Cutoffs; removed BLAST_Cutoffs_simple
 *
 * Revision 1.71  2003/11/12 18:17:46  dondosha
 * Correction in calculating scanning stride
 *
 * Revision 1.70  2003/11/04 23:22:47  dondosha
 * Do not calculate hit saving cutoff score for PHI BLAST
 *
 * Revision 1.69  2003/10/30 19:34:01  dondosha
 * Removed gapped_calculation from BlastHitSavingOptions structure
 *
 * Revision 1.68  2003/10/24 20:55:10  camacho
 * Rename GetDefaultStride
 *
 * Revision 1.67  2003/10/22 16:44:33  dondosha
 * Added function to calculate default stride value for AG method
 *
 * Revision 1.66  2003/10/21 22:15:34  camacho
 * Rearranging of C options structures, fix seed extension method
 *
 * Revision 1.65  2003/10/17 18:20:20  dondosha
 * Use separate variables for different initial word extension options
 *
 * Revision 1.64  2003/10/15 16:59:43  coulouri
 * type correctness fixes
 *
 * Revision 1.63  2003/10/07 17:26:11  dondosha
 * Lower case mask moved from options to the sequence block
 *
 * Revision 1.62  2003/10/02 22:08:34  dondosha
 * Corrections for one-strand translated searches
 *
 * Revision 1.61  2003/10/01 22:36:52  dondosha
 * Correction of setting of e2 in revision 1.57 was wrong
 *
 * Revision 1.60  2003/09/24 19:28:20  dondosha
 * Correction in setting extend word method: unset options that are set by default but overridden
 *
 * Revision 1.59  2003/09/12 17:26:01  dondosha
 * Added check that gap extension option cannot be 0 when gap open is not 0
 *
 * Revision 1.58  2003/09/10 19:48:08  dondosha
 * Removed dependency on mb_lookup.h
 *
 * Revision 1.57  2003/09/09 22:12:02  dondosha
 * Minor correction for ungapped cutoff calculation; added freeing of PHI pattern
 *
 * Revision 1.56  2003/09/08 12:55:57  madden
 * Allow use of PSSM to construct lookup table
 *
 * Revision 1.55  2003/08/27 15:05:37  camacho
 * Use symbolic name for alphabet sizes
 *
 * Revision 1.54  2003/08/26 21:53:33  madden
 * Protein alphabet is 26 chars, not 25
 *
 * Revision 1.53  2003/08/11 15:01:59  dondosha
 * Added algo/blast/core to all #included headers
 *
 * Revision 1.52  2003/08/01 17:26:19  dondosha
 * Use renamed versions of functions from local blastkar.h
 *
 * Revision 1.51  2003/07/31 17:45:17  dondosha
 * Made use of const qualifier consistent throughout the library
 *
 * Revision 1.50  2003/07/31 14:31:41  camacho
 * Replaced Char for char
 *
 * Revision 1.49  2003/07/31 14:19:28  camacho
 * Replaced FloatHi for double
 *
 * Revision 1.48  2003/07/31 00:32:37  camacho
 * Eliminated Ptr notation
 *
 * Revision 1.47  2003/07/30 22:06:25  dondosha
 * Convert matrix name to upper case when filling scoring options
 *
 * Revision 1.46  2003/07/30 19:39:14  camacho
 * Remove PNTRs
 *
 * Revision 1.45  2003/07/30 18:58:10  dondosha
 * Removed unused member matrixname from lookup table options
 *
 * Revision 1.44  2003/07/30 17:15:00  dondosha
 * Minor fixes for very strict compiler warnings
 *
 * Revision 1.43  2003/07/30 16:32:02  madden
 * Use ansi functions when possible
 *
 * Revision 1.42  2003/07/29 14:42:31  coulouri
 * use strdup() instead of StringSave()
 *
 * Revision 1.41  2003/07/28 19:04:15  camacho
 * Replaced all MemNews for calloc
 *
 * Revision 1.40  2003/07/25 21:12:28  coulouri
 * remove constructions of the form "return sfree();" and "a=sfree(a);"
 *
 * Revision 1.39  2003/07/25 17:25:43  coulouri
 * in progres:
 *  * use malloc/calloc/realloc instead of Malloc/Calloc/Realloc
 *  * add sfree() macro and __sfree() helper function to util.[ch]
 *  * use sfree() instead of MemFree()
 *
 * Revision 1.38  2003/07/23 17:31:10  camacho
 * BlastDatabaseParameters struct is deprecated
 *
 * Revision 1.37  2003/07/23 16:42:01  dondosha
 * Formatting options moved from blast_options.c to blast_format.c
 *
 * Revision 1.36  2003/07/22 20:26:16  dondosha
 * Initialize BlastDatabaseParameters structure outside engine
 *
 * Revision 1.35  2003/07/22 15:32:55  dondosha
 * Removed dependence on readdb API
 *
 * Revision 1.34  2003/07/21 20:31:47  dondosha
 * Added BlastDatabaseParameters structure with genetic code string
 *
 * Revision 1.33  2003/06/26 21:38:05  dondosha
 * Program number is removed from options structures, and passed explicitly as a parameter to functions that need it
 *
 * Revision 1.32  2003/06/26 20:24:06  camacho
 * Do not free options structure in BlastExtensionParametersFree
 *
 * Revision 1.31  2003/06/23 21:49:11  dondosha
 * Possibility of linking HSPs for tblastn activated
 *
 * Revision 1.30  2003/06/20 21:40:21  dondosha
 * Added parameters for linking HSPs
 *
 * Revision 1.29  2003/06/20 15:20:21  dondosha
 * Memory leak fixes
 *
 * Revision 1.28  2003/06/18 12:21:01  camacho
 * Added proper return value
 *
 * Revision 1.27  2003/06/17 20:42:43  camacho
 * Moved comments to header file, fixed includes
 *
 * Revision 1.26  2003/06/11 16:14:53  dondosha
 * Added initialization of PSI-BLAST and database options
 *
 * Revision 1.25  2003/06/09 20:13:17  dondosha
 * Minor type casting compiler warnings fixes
 *
 * Revision 1.24  2003/06/06 17:02:30  dondosha
 * Typo fix
 *
 * Revision 1.23  2003/06/04 20:16:51  coulouri
 * make prototypes and definitions agree
 *
 * Revision 1.22  2003/06/03 15:50:39  coulouri
 * correct function pointer argument
 *
 * Revision 1.21  2003/05/30 15:52:11  coulouri
 * various lint-induced cleanups
 *
 * Revision 1.20  2003/05/21 22:31:53  dondosha
 * Added forcing of ungapped search for tblastx to option validation
 *
 * Revision 1.19  2003/05/18 21:57:37  camacho
 * Use Uint1 for program name whenever possible
 *
 * Revision 1.18  2003/05/15 22:01:22  coulouri
 * add rcsid string to sources
 *
 * Revision 1.17  2003/05/13 20:41:48  dondosha
 * Correction in assigning of number of db sequences for 2 sequence case
 *
 * Revision 1.16  2003/05/13 15:11:34  dondosha
 * Changed some char * arguments to const char *
 *
 * Revision 1.15  2003/05/07 17:44:31  dondosha
 * Assign ungapped xdropoff default correctly for protein programs
 *
 * Revision 1.14  2003/05/06 20:29:57  dondosha
 * Fix in filling effective length options
 *
 * Revision 1.13  2003/05/06 14:34:51  dondosha
 * Fix in comment
 *
 * Revision 1.12  2003/05/01 16:56:30  dondosha
 * Fixed strict compiler warnings
 *
 * Revision 1.11  2003/05/01 15:33:39  dondosha
 * Reorganized the setup of BLAST search
 *
 * Revision 1.10  2003/04/24 14:27:35  dondosha
 * Correction for latest changes
 *
 * Revision 1.9  2003/04/23 20:04:49  dondosha
 * Added a function BLAST_InitAllDefaultOptions to initialize all various options structures with only default values
 *
 * Revision 1.8  2003/04/17 21:14:41  dondosha
 * Added cutoff score hit parameters that is calculated from e-value
 *
 * Revision 1.7  2003/04/16 22:25:37  dondosha
 * Correction to previous change
 *
 * Revision 1.6  2003/04/16 22:20:24  dondosha
 * Correction in calculation of cutoff score for ungapped extensions
 *
 * Revision 1.5  2003/04/11 22:35:48  dondosha
 * Minor corrections for blastn
 *
 * Revision 1.4  2003/04/03 22:57:50  dondosha
 * Uninitialized variable fix
 *
 * Revision 1.3  2003/04/02 17:20:41  dondosha
 * Added calculation of ungapped cutoff score in correct place
 *
 * Revision 1.2  2003/04/01 17:42:33  dondosha
 * Added arguments to BlastExtensionParametersNew
 *
 * Revision 1.1  2003/03/31 18:22:30  camacho
 * Moved from parent directory
 *
 * Revision 1.30  2003/03/28 23:12:34  dondosha
 * Added program argument to BlastFormattingOptionsNew
 *
 * Revision 1.29  2003/03/27 20:54:19  dondosha
 * Moved ungapped cutoff from hit options to word options
 *
 * Revision 1.28  2003/03/25 16:30:25  dondosha
 * Strict compiler warning fixes
 *
 * Revision 1.27  2003/03/24 20:39:17  dondosha
 * Added BlastExtensionParameters structure to hold raw gapped X-dropoff values
 *
 * Revision 1.26  2003/03/19 19:52:42  dondosha
 * 1. Added strand option argument to BlastQuerySetUpOptionsNew
 * 2. Added check of discontiguous template parameters in LookupTableOptionsValidate
 *
 * Revision 1.25  2003/03/14 19:08:53  dondosha
 * Added arguments to various OptionsNew functions, so all initialization can be done inside
 *
 * Revision 1.24  2003/03/12 17:03:41  dondosha
 * Set believe_query in formatting options to FALSE by default
 *
 * Revision 1.23  2003/03/11 20:40:32  dondosha
 * Correction in assigning gap_x_dropoff_final
 *
 * Revision 1.22  2003/03/10 16:44:42  dondosha
 * Added functions for initialization and freeing of formatting options structure
 *
 * Revision 1.21  2003/03/07 20:41:08  dondosha
 * Small corrections in option initialization functions
 *
 * Revision 1.20  2003/03/06 19:25:52  madden
 * Include blast_util.h
 *
 * Revision 1.19  2003/03/05 21:19:09  coulouri
 * set NA_LOOKUP_TABLE flag
 *
 * Revision 1.18  2003/03/05 20:58:50  dondosha
 * Corrections for handling effective search space for multiple queries
 *
 * Revision 1.17  2003/03/05 15:36:34  madden
 * Moved BlastNumber2Program and BlastProgram2Number from blast_options to blast_util
 *
 * Revision 1.16  2003/03/03 14:43:21  madden
 * Use BlastKarlinBlkGappedFill, PrintMatrixMessage, and PrintAllowedValuesMessage
 *
 * Revision 1.15  2003/02/26 15:42:50  madden
 * const charPtr becomes const char *, add BlastExtensionOptionsValidate
 *
 * Revision 1.14  2003/02/14 16:30:19  dondosha
 * Get rid of a compiler warning for type mismatch
 *
 * Revision 1.13  2003/02/13 21:42:25  madden
 * Added validation functions
 *
 * Revision 1.12  2003/02/04 13:14:36  dondosha
 * Changed the macro definitions for 
 *
 * Revision 1.11  2003/01/31 17:00:32  dondosha
 * Do not set the scan step in LookupTableOptionsNew
 *
 * Revision 1.10  2003/01/28 15:13:25  madden
 * Added functions and structures for parameters
 *
 * Revision 1.9  2003/01/22 20:49:31  dondosha
 * Set decline_align for blastn too
 *
 * Revision 1.8  2003/01/22 15:09:55  dondosha
 * Correction for default penalty assignment
 *
 * Revision 1.7  2003/01/17 22:10:45  madden
 * Added functions for BlastExtensionOptions, BlastInitialWordOptions as well as defines for default values
 *
 * Revision 1.6  2003/01/10 18:36:40  madden
 * Change call to BlastEffectiveLengthsOptionsNew
 *
 * Revision 1.5  2003/01/02 17:09:35  dondosha
 * Fill alphabet size when creating lookup table options structure
 *
 * Revision 1.4  2002/12/24 14:49:00  madden
 * Set defaults for LookupTableOptions for protein-protein searches
 *
 * Revision 1.3  2002/12/04 13:38:21  madden
 * Add function LookupTableOptionsNew
 *
 * Revision 1.2  2002/10/17 15:45:17  madden
 * Make BLOSUM62 default
 *
 * Revision 1.1  2002/10/07 21:05:12  madden
 * Sets default option values
 *
 * ===========================================================================
 */
