/* 
**************************************************************************
*                                                                         *
*                             COPYRIGHT NOTICE                            *
*                                                                         *
* This software/database is categorized as "United States Government      *
* Work" under the terms of the United States Copyright Act.  It was       *
* produced as part of the author's official duties as a Government        *
* employee and thus can not be copyrighted.  This software/database is    *
* freely available to the public for use without a copyright notice.      *
* Restrictions can not be placed on its present or future use.            *
*                                                                         *
* Although all reasonable efforts have been taken to ensure the accuracy  *
* and reliability of the software and data, the National Library of       *
* Medicine (NLM) and the U.S. Government do not and can not warrant the   *
* performance or results that may be obtained by using this software,     *
* data, or derivative works thereof.  The NLM and the U.S. Government     *
* disclaim any and all warranties, expressed or implied, as to the        *
* performance, merchantability or fitness for any particular purpose or   *
* use.                                                                    *
*                                                                         *
* In any work or product derived from this material, proper attribution   *
* of the author(s) as the source of the software or data would be         *
* appreciated.                                                            *
*                                                                         *
**************************************************************************
 *
 * $Log$
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
 * Use BlastKarlinkGapBlkFill, PrintMatrixMessage, and PrintAllowedValuesMessage
 *
 * Revision 1.15  2003/02/26 15:42:50  madden
 * const CharPtr becomes const Char *, add BlastExtensionOptionsValidate
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
 *
 *
*/

static char const rcsid[] = "$Id$";

#include <blast_options.h>
#include <blast_gapalign.h>
#include <blast_filter.h>

QuerySetUpOptionsPtr
BlastQuerySetUpOptionsFree(QuerySetUpOptionsPtr options)

{
   BlastMaskFree(options->lcase_mask);
   sfree(options->filter_string);

   sfree(options);
   return NULL;
}

Int2
BlastQuerySetUpOptionsNew(QuerySetUpOptionsPtr *options)
{
   *options = (QuerySetUpOptionsPtr) calloc(1, sizeof(QuerySetUpOptions));
   
   if (*options == NULL)
      return 1;
   
   (*options)->genetic_code = BLAST_GENETIC_CODE;

   return 0;
}

Int2 BLAST_FillQuerySetUpOptions(QuerySetUpOptionsPtr options,
        Uint1 program, const char *filter_string, Uint1 strand_option)
{
   if (options == NULL)
      return 1;
   
   if (strand_option && program == blast_type_blastn) {
      options->strand_option = strand_option;
   }
   /* "L" indicates low-complexity (seg for proteins, 
      dust for nucleotides). */
   if (!filter_string || !strcasecmp(filter_string, "T")) {
      if (program == blast_type_blastn)
         options->filter_string = strdup("D");
      else
         options->filter_string = strdup("S");
   } else {
      options->filter_string = strdup(filter_string); 
   }

   return 0;
}

BlastInitialWordOptionsPtr
BlastInitialWordOptionsFree(BlastInitialWordOptionsPtr options)

{

	sfree(options);

	return NULL;
}


Int2
BlastInitialWordOptionsNew(Uint1 program, 
   BlastInitialWordOptionsPtr *options)
{
   *options = 
      (BlastInitialWordOptionsPtr) calloc(1, sizeof(BlastInitialWordOptions));
   if (*options == NULL)
      return 1;

   if (program != blast_type_blastn) {	/* protein-protein options. */
      (*options)->window_size = BLAST_WINDOW_SIZE_PROT;
      (*options)->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_PROT;
   } else {
      (*options)->window_size = BLAST_WINDOW_SIZE_NUCL;
   }

   return 0;
}

Int2
BLAST_FillInitialWordOptions(BlastInitialWordOptionsPtr options, 
   Uint1 program, Boolean greedy, Int4 window_size, 
   Boolean variable_wordsize, Boolean ag_blast, Boolean mb_lookup,
   FloatHi xdrop_ungapped)
{
   if (!options)
      return 1;

   /* Ungapped extension is performed in all cases except when greedy
      gapped extension is used */
   if (program != blast_type_blastn) {
      options->extend_word_method |= EXTEND_WORD_UNGAPPED;
      options->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_PROT;
   } else if (!greedy) {
      options->extend_word_method |= EXTEND_WORD_UNGAPPED;
      options->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_NUCL;
   }

   if (window_size != 0)
      options->window_size = window_size;
   if (xdrop_ungapped != 0)
      options->x_dropoff = xdrop_ungapped;
   if (variable_wordsize) 
      options->extend_word_method |= EXTEND_WORD_VARIABLE_SIZE;

   if (ag_blast) {
      options->extend_word_method |= EXTEND_WORD_AG;
   } else if (mb_lookup) {
      options->extend_word_method |= EXTEND_WORD_MB_STACKS;
   }

   return 0;
}



BlastInitialWordParametersPtr
BlastInitialWordParametersFree(BlastInitialWordParametersPtr parameters)

{
	sfree(parameters);
	return NULL;

}


Int2
BlastInitialWordParametersNew(Uint1 program_number, 
   BlastInitialWordOptionsPtr word_options, 
   BlastHitSavingParametersPtr hit_params, 
   BlastExtensionParametersPtr ext_params, BLAST_ScoreBlkPtr sbp, 
   BlastQueryInfoPtr query_info, 
   BlastEffectiveLengthsOptionsPtr eff_len_options, 
   BlastInitialWordParametersPtr *parameters)
{
   Int4 context = query_info->first_context;
   Int4 cutoff_score = 0, s2 = 0;
   FloatHi e2 = UNGAPPED_CUTOFF_EVALUE;
   BLAST_KarlinBlkPtr kbp;
   FloatHi qlen;
   BlastHitSavingOptionsPtr hit_options;
   FloatHi avglen;

   if (!word_options || !hit_params || !sbp || !sbp->kbp_std[context])
      return 8;

   *parameters = (BlastInitialWordParametersPtr) 
      calloc(1, sizeof(BlastInitialWordParameters));

   hit_options = hit_params->options;

   (*parameters)->options = word_options;

   (*parameters)->x_dropoff = (Int4)
      ceil(word_options->x_dropoff*NCBIMATH_LN2/sbp->kbp_std[context]->Lambda);

   if (hit_options->is_gapped && 
       program_number != blast_type_blastn)
      kbp = sbp->kbp_gap[context];
   else
      kbp = sbp->kbp_std[context];

   /* Calculate score cutoff corresponding to a fixed e-value (0.05);
      If it is smaller, then use this one */
   qlen = query_info->context_offsets[query_info->last_context+1] - 1;

   avglen = ((FloatHi) eff_len_options->db_length) / 
      eff_len_options->dbseq_num;

   BlastCutoffs(&s2, &e2, kbp, MIN(avglen, qlen), avglen, TRUE);

   cutoff_score = MIN(hit_params->cutoff_score, s2);

   /* For non-blastn programs, the cutoff score should not be larger than 
      gap trigger */
   if (hit_options->is_gapped && 
       program_number != blast_type_blastn) {
      (*parameters)->cutoff_score = 
         MIN((Int4)ext_params->gap_trigger, cutoff_score);
   } else {
      (*parameters)->cutoff_score = cutoff_score;
   }

   return 0;
}

BlastExtensionOptionsPtr
BlastExtensionOptionsFree(BlastExtensionOptionsPtr options)

{

	sfree(options);

	return NULL;
}



Int2
BlastExtensionOptionsNew(Uint1 program, BlastExtensionOptionsPtr *options)

{
	*options = (BlastExtensionOptionsPtr) 
           calloc(1, sizeof(BlastExtensionOptions));

	if (*options == NULL)
		return 1;

	if (program != blast_type_blastn) /* protein-protein options. */
	{
		(*options)->gap_x_dropoff = BLAST_GAP_X_DROPOFF_PROT;
		(*options)->gap_x_dropoff_final = 
                   BLAST_GAP_X_DROPOFF_FINAL_PROT;
		(*options)->gap_trigger = BLAST_GAP_TRIGGER_PROT;
		(*options)->algorithm_type = EXTEND_DYN_PROG;
	}
	else
	{
		(*options)->gap_trigger = BLAST_GAP_TRIGGER_NUCL;
	}

	return 0;
}

Int2
BLAST_FillExtensionOptions(BlastExtensionOptionsPtr options, 
   Uint1 program, Boolean greedy, FloatHi x_dropoff, FloatHi x_dropoff_final)
{
   if (!options)
      return 1;

   if (program == blast_type_blastn) {
      if (greedy) {
         options->gap_x_dropoff = BLAST_GAP_X_DROPOFF_GREEDY;
         options->algorithm_type = EXTEND_GREEDY;
      }	else {
         options->gap_x_dropoff = BLAST_GAP_X_DROPOFF_NUCL;
         options->gap_x_dropoff_final = BLAST_GAP_X_DROPOFF_FINAL_NUCL;
         options->algorithm_type = EXTEND_DYN_PROG;
      }
   }

   if (x_dropoff)
      options->gap_x_dropoff = x_dropoff;
   if (x_dropoff_final)
      options->gap_x_dropoff_final = x_dropoff_final;

   return 0;

}

Int2 
BlastExtensionOptionsValidate(Uint1 program_number, 
   BlastExtensionOptionsPtr options, Blast_MessagePtr *blast_msg)

{
	if (options == NULL)
		return 1;

	if (program_number != blast_type_blastn)
	{
		if (options->algorithm_type == EXTEND_GREEDY || 
            	options->algorithm_type == EXTEND_GREEDY_NO_TRACEBACK)
		{
			Int4 code=2;
			Int4 subcode=1;
			Blast_MessageWrite(blast_msg, 1, code, subcode, "Greedy extension only supported for BLASTN");
			return (Int2) code;
		}
	}

	return 0;
}

Int2 BlastExtensionParametersNew(Uint1 program_number, 
        BlastExtensionOptionsPtr options, BLAST_ScoreBlkPtr sbp,
        BlastQueryInfoPtr query_info, BlastExtensionParametersPtr *parameters)
{
   BLAST_KarlinBlkPtr kbp, kbp_gap;
   BlastExtensionParametersPtr params;

   if (sbp->kbp) {
      kbp = sbp->kbp[query_info->first_context];
   } else {
      /* The Karlin block is not found, can't do any calculations */
      *parameters = NULL;
      return -1;
   }

   if ((program_number != blast_type_blastn) && sbp->kbp_gap) {
      kbp_gap = sbp->kbp_gap[query_info->first_context];
   } else {
      kbp_gap = kbp;
   }
   
   *parameters = params = (BlastExtensionParametersPtr) 
      malloc(sizeof(BlastExtensionParameters));

   params->options = options;
   params->gap_x_dropoff = 
      (Int4) (options->gap_x_dropoff*NCBIMATH_LN2 / kbp_gap->Lambda);
   params->gap_x_dropoff_final = (Int4) 
      (options->gap_x_dropoff_final*NCBIMATH_LN2 / kbp_gap->Lambda);

   params->gap_trigger = 
      (options->gap_trigger*NCBIMATH_LN2 + kbp->logK) / kbp->Lambda;

   return 0;
}

BlastExtensionParametersPtr
BlastExtensionParametersFree(BlastExtensionParametersPtr parameters)
{
  sfree(parameters);
  return NULL;
}


BlastScoringOptionsPtr
BlastScoringOptionsFree(BlastScoringOptionsPtr options)

{
	if (options == NULL)
		return NULL;

	sfree(options->matrix);

	sfree(options);

	return NULL;
}

Int2 
BlastScoringOptionsNew(Uint1 program_number, BlastScoringOptionsPtr *options)
{
   *options = (BlastScoringOptionsPtr) calloc(1, sizeof(BlastScoringOptions));

   if (*options == NULL)
      return 1;
   
   if (program_number != blast_type_blastn) {	/* protein-protein options. */
      (*options)->shift_pen = INT2_MAX;
      (*options)->is_ooframe = FALSE;
      (*options)->gap_open = BLAST_GAP_OPEN_PROT;
      (*options)->gap_extend = BLAST_GAP_EXTN_PROT;
   } else {	/* nucleotide-nucleotide options. */
      (*options)->penalty = BLAST_PENALTY;
      (*options)->reward = BLAST_REWARD;
   }
   (*options)->decline_align = INT2_MAX;
   (*options)->gapped_calculation = TRUE;
   
   return 0;
}

Int2 
BLAST_FillScoringOptions(BlastScoringOptionsPtr options, 
   Uint1 program_number, Boolean greedy_extension, Int4 penalty, Int4 reward, 
   const char *matrix, Int4 gap_open, Int4 gap_extend)
{
   if (!options)
      return 1;

   if (program_number != blast_type_blastn) {	/* protein-protein options. */
      if (matrix)
         options->matrix = strdup(matrix);
      else
         options->matrix = strdup("BLOSUM62");
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
   if (gap_open)
      options->gap_open = gap_open;
   if (gap_extend)
      options->gap_extend = gap_extend;

   return 0;
}

Int2 
BlastScoringOptionsValidate(Uint1 program_number, 
   BlastScoringOptionsPtr options, Blast_MessagePtr *blast_msg)

{
	if (options == NULL)
		return 1;

	if (program_number == blast_type_blastn)
	{
		if (options->penalty >= 0)
		{
			Int4 code=2;
			Int4 subcode=1;
			Blast_MessageWrite(blast_msg, 1, code, subcode, "BLASTN penalty must be negative");
			return (Int2) code;
		}
	}
	else
	{
		Int2 status=0;

		if ((status=BlastKarlinkGapBlkFill(NULL, options->gap_open, options->gap_extend, options->decline_align, options->matrix)) != 0)
		{
			if (status == 1)
			{
				CharPtr buffer;
				Int4 code=2;
				Int4 subcode=1;

				buffer = PrintMatrixMessage(options->matrix); 
                                Blast_MessageWrite(blast_msg, 2, code, subcode, buffer);
				sfree(buffer);
				return (Int2) code;
				
			}
			else if (status == 2)
			{
				CharPtr buffer;
				Int4 code=2;
				Int4 subcode=1;

				buffer = PrintAllowedValuesMessage(options->matrix, options->gap_open, options->gap_extend, options->decline_align); 
                                Blast_MessageWrite(blast_msg, 2, code, subcode, buffer);
				sfree(buffer);
				return (Int2) code;
			}
		}
		
	}

	if (program_number != blast_type_blastx && 
       program_number != blast_type_tblastn && options->is_ooframe)
	{
      Int4 code=2;
      Int4 subcode=1;
      Blast_MessageWrite(blast_msg, 1, code, subcode, 
         "Out-of-frame only permitted for blastx and tblastn");
      return (Int2) code;
	}

	return 0;

}


BlastEffectiveLengthsOptionsPtr
BlastEffectiveLengthsOptionsFree(BlastEffectiveLengthsOptionsPtr options)

{
	sfree(options);

	return NULL;
}


Int2 
BlastEffectiveLengthsOptionsNew(BlastEffectiveLengthsOptionsPtr *options)

{
   *options = (BlastEffectiveLengthsOptionsPtr)
      calloc(1, sizeof(BlastEffectiveLengthsOptions));

   if (*options == NULL)
      return 1;
   
   return 0;
}

Int2 
BLAST_FillEffectiveLengthsOptions(BlastEffectiveLengthsOptionsPtr options, 
   CharPtr database, Boolean is_protein, Int4 dbseq_num, Int8 db_length,
   Int8 searchsp_eff)
{
   if (!options)
      return 1;

   if (searchsp_eff) {	
      /* dbnum_seq and dblen are used to calculate effective search space, so 
         if it is already set don't bother with those. */
      options->searchsp_eff = searchsp_eff;
      return 0;
   }

   options->dbseq_num = MAX(dbseq_num, 1);
   options->db_length = MAX(db_length, 1);

   return 0;
}

LookupTableOptionsPtr
LookupTableOptionsFree(LookupTableOptionsPtr options)

{

	if (options)
	{
	  sfree(options->matrixname);
	}

	sfree(options);
	return NULL;
}

Int2 
LookupTableOptionsNew(Uint1 program_number, LookupTableOptionsPtr *options)
{
   *options = (LookupTableOptionsPtr) calloc(1, sizeof(LookupTableOptions));
   
   if (*options == NULL)
      return 1;
   
   if (program_number != blast_type_blastn) {
      (*options)->word_size = BLAST_WORDSIZE_PROT;
      (*options)->alphabet_size = 25;
      (*options)->matrixname = strdup("BLOSUM62");
      (*options)->lut_type = AA_LOOKUP_TABLE;
      
      if (program_number == blast_type_blastp)
         (*options)->threshold = BLAST_WORD_THRESHOLD_BLASTP;
      else if (program_number == blast_type_blastx)
         (*options)->threshold = BLAST_WORD_THRESHOLD_BLASTX;
      else if (program_number == blast_type_tblastn)
         (*options)->threshold = BLAST_WORD_THRESHOLD_TBLASTN;
      else if (program_number == blast_type_tblastx)
         (*options)->threshold = BLAST_WORD_THRESHOLD_TBLASTX;
      
   } else {
      (*options)->alphabet_size = 16;
   }

   return 0;
}

Int2 
BLAST_FillLookupTableOptions(LookupTableOptionsPtr options, 
   Uint1 program_number, Boolean is_megablast, Int4 threshold,
   Int2 word_size, Boolean ag_blast, Boolean variable_wordsize)
{
   if (!options)
      return 1;

   if (program_number == blast_type_blastn) {
      if (is_megablast)	{
         options->word_size = BLAST_WORDSIZE_MEGABLAST;
         options->lut_type = MB_LOOKUP_TABLE;
         options->max_positions = INT4_MAX;
      }	else {
         options->lut_type = NA_LOOKUP_TABLE;
         options->word_size = BLAST_WORDSIZE_NUCL;
      }
   }

   if (threshold)
      options->threshold = threshold;
   if (word_size)
      options->word_size = word_size;
   if (program_number == blast_type_blastn) {
      if (!ag_blast) {
         options->scan_step = COMPRESSION_RATIO;
      } else if (variable_wordsize) {
         if (is_megablast)
            options->scan_step = options->word_size - 12 + COMPRESSION_RATIO;
         else
            options->scan_step = options->word_size - 8 + COMPRESSION_RATIO;
      } else {
         if (is_megablast)
            options->scan_step = options->word_size - 12 + 1;
         else
            options->scan_step = options->word_size - 8 + 1;
      }
   }
   return 0;
}

Int2 
LookupTableOptionsValidate(Uint1 program_number, 
   LookupTableOptionsPtr options, Blast_MessagePtr *blast_msg)

{
	if (options == NULL)
		return 1;

	if (program_number != blast_type_blastn && options->threshold <= 0)
	{
		Int4 code=2;
		Int4 subcode=1;
		Blast_MessageWrite(blast_msg, 2, code, subcode, "Non-zero threshold required");
		return (Int2) code;
	}

	if (options->word_size <= 0)
	{
		Int4 code=2;
		Int4 subcode=1;
		Blast_MessageWrite(blast_msg, 2, code, subcode, "Word-size must be greater than zero");
		return (Int2) code;
	} else if (program_number == blast_type_blastn && options->word_size < 7)
	{
		Int4 code=2;
		Int4 subcode=1;
		Blast_MessageWrite(blast_msg, 2, code, subcode, "Word-size must be 7" 
                         "or greater for nucleotide comparison");
		return (Int2) code;
	} else if (program_number != blast_type_blastn && options->word_size > 5)
	{
		Int4 code=2;
		Int4 subcode=1;
		Blast_MessageWrite(blast_msg, 2, code, subcode, "Word-size must be less"
                         "than 6 for protein comparison");
		return (Int2) code;
	}


	if (program_number != blast_type_blastn && 
       options->lut_type == MB_LOOKUP_TABLE)
	{
		Int4 code=2;
		Int4 subcode=1;
		Blast_MessageWrite(blast_msg, 2, code, subcode, "Megablast lookup table only supported with blastn");
		return (Int2) code;
	}

        if (program_number == blast_type_blastn && 
            options->mb_template_length > 0) {
           DiscTemplateType template_type = 
              GetDiscTemplateType(options->word_size,
                 options->mb_template_length, options->mb_template_type);

           if (template_type == TEMPL_CONTIGUOUS) {
              Int4 code=2;
              Int4 subcode=1;
              Blast_MessageWrite(blast_msg, 2, code, subcode, 
                 "Invalid discontiguous template parameters");
              return (Int2) code;
           } else {
              /* Reset the related options if they were set to incorrect 
                 defaults */
              options->lut_type = MB_LOOKUP_TABLE;
              if (options->scan_step != 1)
                 options->scan_step = COMPRESSION_RATIO;
           }
        }

	return 0;
}

BlastHitSavingOptionsPtr
BlastHitSavingOptionsFree(BlastHitSavingOptionsPtr options)

{
  sfree(options);
  return NULL;
}


Int2 BlastHitSavingOptionsNew(Uint1 program_number, 
        BlastHitSavingOptionsPtr *options)
{
   *options = (BlastHitSavingOptionsPtr) calloc(1, sizeof(BlastHitSavingOptions));
   
   if (*options == NULL)
      return 1;

   (*options)->hitlist_size = 500;
   (*options)->expect_value = BLAST_EXPECT_VALUE;

   if (program_number == blast_type_tblastn)
      (*options)->do_sum_stats = TRUE;

   /* other stuff?? */
   
   return 0;

}

Int2
BLAST_FillHitSavingOptions(BlastHitSavingOptionsPtr options, 
   Boolean is_gapped, FloatHi evalue, Int4 hitlist_size)
{
   if (!options)
      return 1;

   options->is_gapped = is_gapped;
   if (hitlist_size)
      options->hitlist_size = hitlist_size;
   if (evalue)
      options->expect_value = evalue;

   return 0;

}

Int2
BlastHitSavingOptionsValidate(Uint1 program_number,
   BlastHitSavingOptionsPtr options, Blast_MessagePtr *blast_msg)
{
	if (options == NULL)
		return 1;

        if (program_number == blast_type_tblastx && options->is_gapped)
           options->is_gapped = FALSE;

	if (options->hitlist_size < 1)
	{
		Int4 code=1;
		Int4 subcode=1;
		Blast_MessageWrite(blast_msg, 2, code, subcode, "No hits are being saved");
		return (Int2) code;
	}

	if (options->expect_value <= 0.0 && options->cutoff_score <= 0)
	{
		Int4 code=2;
		Int4 subcode=1;
		Blast_MessageWrite(blast_msg, 
			2, code, subcode, "expect value or cutoff score must be greater than zero");
		return (Int2) code;
	}	

	return 0;
}


BlastHitSavingParametersPtr
BlastHitSavingParametersFree(BlastHitSavingParametersPtr parmameters)

{
  sfree(parmameters);
  return NULL;
}


Int2
BlastHitSavingParametersNew(Uint1 program_number, 
   BlastHitSavingOptionsPtr options, 
   int (*handle_results)(VoidPtr, VoidPtr, VoidPtr, VoidPtr, VoidPtr, VoidPtr, 
           VoidPtr), BLAST_ScoreBlkPtr sbp, BlastQueryInfoPtr query_info, 
   BlastHitSavingParametersPtr *parameters)
{
   BlastHitSavingParametersPtr params;
   BLAST_KarlinBlkPtr kbp;
   FloatHi evalue = options->expect_value;

   if (!options || !parameters)
      return 1;

   *parameters = params = (BlastHitSavingParametersPtr) 
      calloc(1, sizeof(BlastHitSavingParameters));

   if (params == NULL)
      return 1;

   params->options = options;

   params->handle_results = handle_results;

   if (sbp->kbp_gap && sbp->kbp_gap[query_info->first_context])
      kbp = sbp->kbp_gap[query_info->first_context];
   else
      kbp = sbp->kbp[query_info->first_context];

   if (options->cutoff_score > 0) {
      params->cutoff_score = options->cutoff_score;
   } else {
      BlastCutoffs_simple(&(params->cutoff_score), &evalue, kbp, 
         (FloatHi)query_info->eff_searchsp_array[query_info->first_context], 
         FALSE);
   }
   
   if (program_number == blast_type_blastn || !options->is_gapped) {
      params->gap_prob = BLAST_GAP_PROB;
      params->gap_decay_rate = BLAST_GAP_DECAY_RATE;
   } else {
      params->gap_prob = BLAST_GAP_PROB_GAPPED;
      params->gap_decay_rate = BLAST_GAP_DECAY_RATE_GAPPED;
   }
   params->gap_size = BLAST_GAP_SIZE;
      
   return 0;
}

Int2 PSIBlastOptionsNew(PSIBlastOptionsPtr PNTR psi_options)
{
   PSIBlastOptionsPtr options = 
      (PSIBlastOptionsPtr) calloc(1, sizeof(PSIBlastOptions));
   *psi_options = options;
   options->ethresh = PSI_ETHRESH;
   options->maxNumPasses = PSI_MAX_NUM_PASSES;
   options->pseudoCountConst = PSI_PSEUDO_COUNT_CONST;
   options->scalingFactor = PSI_SCALING_FACTOR;
   
   return 0;
}

PSIBlastOptionsPtr PSIBlastOptionsFree(PSIBlastOptionsPtr psi_options)
{
   if (psi_options->isPatternSearch)
      sfree(psi_options->phi_pattern);
   
   sfree(psi_options);
   return NULL;
}

Int2 BlastDatabaseOptionsNew(BlastDatabaseOptionsPtr PNTR db_options)
{
   BlastDatabaseOptionsPtr options = (BlastDatabaseOptionsPtr)
      calloc(1, sizeof(BlastDatabaseOptions));

   options->genetic_code = BLAST_GENETIC_CODE;
   *db_options = options;

   return 0;
}

BlastDatabaseOptionsPtr 
BlastDatabaseOptionsFree(BlastDatabaseOptionsPtr db_options)
{
   sfree(db_options->gen_code_string);
   sfree(db_options);
   return NULL;
}

Int2 BLAST_InitDefaultOptions(Uint1 program_number,
   LookupTableOptionsPtr PNTR lookup_options,
   QuerySetUpOptionsPtr PNTR query_setup_options, 
   BlastInitialWordOptionsPtr PNTR word_options,
   BlastExtensionOptionsPtr PNTR ext_options,
   BlastHitSavingOptionsPtr PNTR hit_options,
   BlastScoringOptionsPtr PNTR score_options,
   BlastEffectiveLengthsOptionsPtr PNTR eff_len_options,
   PSIBlastOptionsPtr PNTR psi_options,
   BlastDatabaseOptionsPtr PNTR db_options)
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

