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
 * Use BlastKarlinkGapBlkFill, PrintMatrixMessage, and PrintAllowedValuesMessage
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
 *
 *
*/

static char const rcsid[] = "$Id$";

#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/blast_filter.h>
#include <algo/blast/core/blast_encoding.h>

QuerySetUpOptions*
BlastQuerySetUpOptionsFree(QuerySetUpOptions* options)

{
   sfree(options->filter_string);

   sfree(options);
   return NULL;
}

Int2
BlastQuerySetUpOptionsNew(QuerySetUpOptions* *options)
{
   *options = (QuerySetUpOptions*) calloc(1, sizeof(QuerySetUpOptions));
   
   if (*options == NULL)
      return 1;
   
   (*options)->genetic_code = BLAST_GENETIC_CODE;

   return 0;
}

Int2 BLAST_FillQuerySetUpOptions(QuerySetUpOptions* options,
        Uint1 program, const char *filter_string, Uint1 strand_option)
{
   if (options == NULL)
      return 1;
   
   if (strand_option && 
       (program == blast_type_blastn || program == blast_type_blastx ||
       program == blast_type_tblastx)) {
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

BlastInitialWordOptions*
BlastInitialWordOptionsFree(BlastInitialWordOptions* options)

{

	sfree(options);

	return NULL;
}


Int2
BlastInitialWordOptionsNew(Uint1 program, 
   BlastInitialWordOptions* *options)
{
   *options = 
      (BlastInitialWordOptions*) calloc(1, sizeof(BlastInitialWordOptions));
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
BLAST_FillInitialWordOptions(BlastInitialWordOptions* options, 
   Uint1 program, Boolean greedy, Int4 window_size, 
   Boolean variable_wordsize, Boolean ag_blast, Boolean mb_lookup,
   double xdrop_ungapped)
{
   if (!options)
      return 1;

   /* Ungapped extension is performed in all cases except when greedy
      gapped extension is used */
   if (program != blast_type_blastn) {
      options->ungapped_extension = TRUE;
      options->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_PROT;
   } else if (!greedy) {
      options->ungapped_extension = TRUE;
      options->x_dropoff = BLAST_UNGAPPED_X_DROPOFF_NUCL;
   } else {
      options->ungapped_extension = FALSE;
   }

   if (window_size != 0)
      options->window_size = window_size;
   if (xdrop_ungapped != 0)
      options->x_dropoff = xdrop_ungapped;

   options->variable_wordsize = variable_wordsize;

   if (ag_blast) {
      options->extension_method = eRightAndLeft;
   } else {
      options->extension_method = eRight;
      if (mb_lookup)
         options->container_type = eMbStacks;
      else
         options->container_type = eDiagArray;
   }

   return 0;
}



BlastInitialWordParameters*
BlastInitialWordParametersFree(BlastInitialWordParameters* parameters)

{
	sfree(parameters);
	return NULL;

}

static double GetUngappedCutoff(Uint1 program)
{
   switch(program) {
   case blast_type_blastn:
      return UNGAPPED_CUTOFF_E_BLASTN;
   case blast_type_blastp: 
   case blast_type_rpsblast: 
      return UNGAPPED_CUTOFF_E_BLASTP;
   case blast_type_blastx: 
      return UNGAPPED_CUTOFF_E_BLASTX;
   case blast_type_tblastn:
   case blast_type_rpstblastn:
      return UNGAPPED_CUTOFF_E_TBLASTN;
   case blast_type_tblastx:
      return UNGAPPED_CUTOFF_E_TBLASTX;
   }
   return 0;
}

Int2
BlastInitialWordParametersNew(Uint1 program_number, 
   const BlastInitialWordOptions* word_options, 
   const BlastHitSavingParameters* hit_params, 
   const BlastExtensionParameters* ext_params, BlastScoreBlk* sbp, 
   BlastQueryInfo* query_info, 
   Uint4 subject_length,
   BlastInitialWordParameters* *parameters)
{
   Int2 status = 0;

   /* If parameters pointer is NULL, there is nothing to fill, 
      so don't do anything */
   if (!parameters)
      return 0;

   ASSERT(sbp);
   ASSERT(word_options);

   *parameters = (BlastInitialWordParameters*) 
      calloc(1, sizeof(BlastInitialWordParameters));

   (*parameters)->options = (BlastInitialWordOptions *) word_options;

   ASSERT(sbp->kbp_std[query_info->first_context]);

   (*parameters)->x_dropoff = (Int4)
      ceil(word_options->x_dropoff*NCBIMATH_LN2/
           sbp->kbp_std[query_info->first_context]->Lambda);

   status = BlastInitialWordParametersUpdate(program_number,
               hit_params, ext_params, sbp, query_info,
               subject_length, *parameters);

   return status;
}

Int2
BlastInitialWordParametersUpdate(Uint1 program_number, 
   const BlastHitSavingParameters* hit_params, 
   const BlastExtensionParameters* ext_params, BlastScoreBlk* sbp, 
   BlastQueryInfo* query_info, Uint4 subj_length,
   BlastInitialWordParameters* parameters)
{
   Blast_KarlinBlk* kbp;
   Boolean gapped_calculation = TRUE;

   ASSERT(sbp);
   ASSERT(hit_params);
   ASSERT(ext_params);
   ASSERT(query_info);

   /* kbp_gap is only non-NULL for gapped searches! */
   if (sbp->kbp_gap) {
      kbp = sbp->kbp_gap[query_info->first_context];
   } else {
      kbp = sbp->kbp_std[query_info->first_context];
      gapped_calculation = FALSE;
   }

   ASSERT(kbp);
   /* For non-blastn programs cutoff score should not be larger than 
      gap trigger. */
   if (gapped_calculation && program_number != blast_type_blastn) {
      parameters->cutoff_score = 
         MIN(ext_params->gap_trigger, hit_params->cutoff_score);
   } else {
      Int4 s2 = 0;
      double e2;
      double qlen;
      /* Calculate score cutoff corresponding to a fixed e-value (0.05);
         If it is smaller, then use this one */
      qlen = query_info->context_offsets[query_info->last_context+1] - 1;
      
      e2 = GetUngappedCutoff(program_number);

      BLAST_Cutoffs(&s2, &e2, kbp, MIN(subj_length, qlen)*subj_length, TRUE, 
                    hit_params->gap_decay_rate);
      parameters->cutoff_score = MIN(hit_params->cutoff_score, s2);
   }

   return 0;
}

BlastExtensionOptions*
BlastExtensionOptionsFree(BlastExtensionOptions* options)

{

	sfree(options);

	return NULL;
}



Int2
BlastExtensionOptionsNew(Uint1 program, BlastExtensionOptions* *options)

{
	*options = (BlastExtensionOptions*) 
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
BLAST_FillExtensionOptions(BlastExtensionOptions* options, 
   Uint1 program, Boolean greedy, double x_dropoff, double x_dropoff_final)
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
   const BlastExtensionOptions* options, Blast_Message* *blast_msg)

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
			Blast_MessageWrite(blast_msg, BLAST_SEV_WARNING, code, subcode, 
                            "Greedy extension only supported for BLASTN");
			return (Int2) code;
		}
	}

	return 0;
}

Int2 BlastExtensionParametersNew(Uint1 program_number, 
        const BlastExtensionOptions* options, BlastScoreBlk* sbp,
        BlastQueryInfo* query_info, BlastExtensionParameters* *parameters)
{
   Blast_KarlinBlk* kbp,* kbp_gap;
   BlastExtensionParameters* params;

   /* If parameters pointer is NULL, there is nothing to fill, 
      so don't do anything */
   if (!parameters)
      return 0;

   if (sbp->kbp) {
      kbp = sbp->kbp[query_info->first_context];
   } else {
      /* The Karlin block is not found, can't do any calculations */
      *parameters = NULL;
      return -1;
   }

   if (sbp->kbp_gap) {
      kbp_gap = sbp->kbp_gap[query_info->first_context];
   } else {
      kbp_gap = kbp;
   }
   
   *parameters = params = (BlastExtensionParameters*) 
      malloc(sizeof(BlastExtensionParameters));

   params->options = (BlastExtensionOptions *) options;
   params->gap_x_dropoff = 
      (Int4) (options->gap_x_dropoff*NCBIMATH_LN2 / kbp_gap->Lambda);
   params->gap_x_dropoff_final = (Int4) 
      (options->gap_x_dropoff_final*NCBIMATH_LN2 / kbp_gap->Lambda);

   params->gap_trigger = (Int4)
      ((options->gap_trigger*NCBIMATH_LN2 + kbp->logK) / kbp->Lambda);

   return 0;
}

BlastExtensionParameters*
BlastExtensionParametersFree(BlastExtensionParameters* parameters)
{
  sfree(parameters);
  return NULL;
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
BlastScoringOptionsNew(Uint1 program_number, BlastScoringOptions* *options)
{
   *options = (BlastScoringOptions*) calloc(1, sizeof(BlastScoringOptions));

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
BLAST_FillScoringOptions(BlastScoringOptions* options, 
   Uint1 program_number, Boolean greedy_extension, Int4 penalty, Int4 reward, 
   const char *matrix, Int4 gap_open, Int4 gap_extend)
{
   if (!options)
      return 1;

   if (program_number != blast_type_blastn) {	/* protein-protein options. */
      if (matrix) {
         unsigned int i;
         options->matrix = strdup(matrix);
         /* Make it all upper case */
         for (i=0; i<strlen(options->matrix); ++i)
            options->matrix[i] = toupper(options->matrix[i]);
      } else {
         options->matrix = strdup("BLOSUM62");
      }
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
   const BlastScoringOptions* options, Blast_Message* *blast_msg)

{
	if (options == NULL)
		return 1;

   if (program_number == blast_type_tblastx && 
              options->gapped_calculation)
   {
		Int4 code=2;
		Int4 subcode=1;
      Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR, code, subcode, 
         "Gapped search is not allowed for tblastx");
		return (Int2) code;
   }

	if (program_number == blast_type_blastn)
	{
		if (options->penalty >= 0)
		{
			Int4 code=2;
			Int4 subcode=1;
			Blast_MessageWrite(blast_msg, BLAST_SEV_WARNING, code, subcode, 
                            "BLASTN penalty must be negative");
			return (Int2) code;
		}
                if (options->gap_open > 0 && options->gap_extend == 0) 
                {
                        Int4 code=2;
                        Int4 subcode=1;
                        Blast_MessageWrite(blast_msg, BLAST_SEV_WARNING, 
                           code, subcode, 
                           "BLASTN gap extension penalty cannot be 0");
                        return (Int2) code;
                }
	}
	else
	{
		Int2 status=0;

		if ((status=Blast_KarlinkGapBlkFill(NULL, options->gap_open, 
                     options->gap_extend, options->decline_align, 
                     options->matrix)) != 0)
		{
			if (status == 1)
			{
				char* buffer;
				Int4 code=2;
				Int4 subcode=1;

				buffer = BLAST_PrintMatrixMessage(options->matrix); 
            Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR,
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
            Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR, code, subcode, 
                               buffer);
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
      Blast_MessageWrite(blast_msg, BLAST_SEV_WARNING, code, subcode, 
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
      sfree(options->phi_pattern);
   
	sfree(options);
	return NULL;
}

Int2 
LookupTableOptionsNew(Uint1 program_number, LookupTableOptions* *options)
{
   *options = (LookupTableOptions*) calloc(1, sizeof(LookupTableOptions));
   
   if (*options == NULL)
      return 1;
   
   if (program_number != blast_type_blastn) {
      (*options)->word_size = BLAST_WORDSIZE_PROT;
      (*options)->alphabet_size = BLASTAA_SIZE;
      (*options)->lut_type = AA_LOOKUP_TABLE;
      
      if (program_number == blast_type_blastp ||
          program_number == blast_type_rpsblast)
         (*options)->threshold = BLAST_WORD_THRESHOLD_BLASTP;
      else if (program_number == blast_type_blastx)
         (*options)->threshold = BLAST_WORD_THRESHOLD_BLASTX;
      else if (program_number == blast_type_tblastn ||
               program_number == blast_type_rpstblastn)
         (*options)->threshold = BLAST_WORD_THRESHOLD_TBLASTN;
      else if (program_number == blast_type_tblastx)
         (*options)->threshold = BLAST_WORD_THRESHOLD_TBLASTX;
      
   } else {
      (*options)->alphabet_size = BLASTNA_SIZE;
   }

   return 0;
}

Int4 CalculateBestStride(Int4 word_size, Boolean var_words, Int4 lut_type)
{
   Int4 lut_width;
   Int4 extra = 1;
   Uint1 remainder;
   Int4 stride;

   if (lut_type == MB_LOOKUP_TABLE)
      lut_width = 12;
   else if (word_size >= 8)
      lut_width = 8;
   else
      lut_width = 4;

   remainder = word_size % COMPRESSION_RATIO;

   if (var_words && (remainder == 0) )
      extra = COMPRESSION_RATIO;

   stride = word_size - lut_width + extra;

   remainder = stride % 4;

   if (stride > 8 || (stride > 4 && remainder == 1) ) 
      stride -= remainder;
   return stride;
}

Int2 
BLAST_FillLookupTableOptions(LookupTableOptions* options, 
   Uint1 program_number, Boolean is_megablast, Int4 threshold,
   Int2 word_size, Boolean ag_blast, Boolean variable_wordsize,
   Boolean use_pssm)
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
   } else {
      options->lut_type = AA_LOOKUP_TABLE;
   }

   /* if the supplied threshold is -1, disable neighboring words */
   if (threshold == -1)
      options->threshold = 0;

   /* if the supplied threshold is > 0, use it */
   if (threshold > 0)
      options->threshold = threshold;

   /* otherwise, use the default */

   if (use_pssm)
      options->use_pssm = use_pssm;
   if (program_number == blast_type_rpsblast ||
       program_number == blast_type_rpstblastn)
      options->lut_type = RPS_LOOKUP_TABLE;
   if (word_size)
      options->word_size = word_size;
   if (program_number == blast_type_blastn) {
      if (!ag_blast) {
         options->scan_step = COMPRESSION_RATIO;
      } else {
         options->scan_step = CalculateBestStride(options->word_size, variable_wordsize,
                                                  options->lut_type);
      }
   }
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
DiscWordOptionsValidate(Int2 word_size, Uint1 template_length,
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
LookupTableOptionsValidate(Uint1 program_number, 
   const LookupTableOptions* options, Blast_Message* *blast_msg)

{
   Int4 code=2;
   Int4 subcode=1;

	if (options == NULL)
		return 1;

        if (program_number == blast_type_rpsblast ||
            program_number == blast_type_rpstblastn)
                return 0;

	if (program_number != blast_type_blastn && options->threshold <= 0)
	{
		Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR, code, subcode, 
                         "Non-zero threshold required");
		return (Int2) code;
	}

	if (options->word_size <= 0)
	{
		Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR, code, subcode, 
                         "Word-size must be greater than zero");
		return (Int2) code;
	} else if (program_number == blast_type_blastn && options->word_size < 7)
	{
		Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR, code, subcode, 
                         "Word-size must be 7" 
                         "or greater for nucleotide comparison");
		return (Int2) code;
	} else if (program_number != blast_type_blastn && options->word_size > 5)
	{
		Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR, code, subcode, 
                         "Word-size must be less"
                         "than 6 for protein comparison");
		return (Int2) code;
	}


	if (program_number != blast_type_blastn && 
       options->lut_type == MB_LOOKUP_TABLE)
	{
		Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR, code, subcode, 
                         "Megablast lookup table only supported with blastn");
		return (Int2) code;
	}

   if (options->lut_type == MB_LOOKUP_TABLE && options->word_size < 12 && 
       options->mb_template_length == 0) {
      Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR, code, subcode, 
                         "Word size must be 12 or greater with megablast"
                         " lookup table");
      return (Int2) code;
   }

   if (program_number == blast_type_blastn && 
       options->mb_template_length > 0) {
      if (!DiscWordOptionsValidate(options->word_size,
            options->mb_template_length, options->mb_template_type)) {
         Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR, code, subcode, 
            "Invalid discontiguous template parameters");
         return (Int2) code;
      } else if (options->lut_type != MB_LOOKUP_TABLE) {
         Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR, code, subcode, 
            "Invalid lookup table type for discontiguous Mega BLAST");
         return (Int2) code;
      } else if (options->scan_step != 1 && 
                 options->scan_step != COMPRESSION_RATIO) {
         Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR, code, subcode, 
            "Invalid scanning stride for discontiguous Mega BLAST");
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


Int2 BlastHitSavingOptionsNew(Uint1 program_number, 
        BlastHitSavingOptions* *options)
{
   *options = (BlastHitSavingOptions*) calloc(1, sizeof(BlastHitSavingOptions));
   
   if (*options == NULL)
      return 1;

   (*options)->hitlist_size = 500;
   (*options)->prelim_hitlist_size = 500;
   (*options)->expect_value = BLAST_EXPECT_VALUE;

   /* other stuff?? */
   
   return 0;

}

Int2
BLAST_FillHitSavingOptions(BlastHitSavingOptions* options, 
                           double evalue, Int4 hitlist_size)
{
   if (!options)
      return 1;

   if (hitlist_size)
      options->hitlist_size = options->prelim_hitlist_size = hitlist_size;
   if (evalue)
      options->expect_value = evalue;

   return 0;

}

Int2
BlastHitSavingOptionsValidate(Uint1 program_number,
   const BlastHitSavingOptions* options, Blast_Message* *blast_msg)
{
	if (options == NULL)
		return 1;

	if (options->hitlist_size < 1 || options->prelim_hitlist_size < 1)
	{
		Int4 code=1;
		Int4 subcode=1;
		Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR, code, subcode, 
                         "No hits are being saved");
		return (Int2) code;
	}

	if (options->expect_value <= 0.0 && options->cutoff_score <= 0)
	{
		Int4 code=2;
		Int4 subcode=1;
		Blast_MessageWrite(blast_msg, BLAST_SEV_ERROR, code, subcode, 
         "expect value or cutoff score must be greater than zero");
		return (Int2) code;
	}	

	return 0;
}


BlastHitSavingParameters*
BlastHitSavingParametersFree(BlastHitSavingParameters* parmameters)

{
  sfree(parmameters);
  return NULL;
}


Int2
BlastHitSavingParametersNew(Uint1 program_number, 
   const BlastHitSavingOptions* options, 
   const BlastExtensionParameters* ext_params, 
   BlastScoreBlk* sbp, BlastQueryInfo* query_info, 
   BlastHitSavingParameters* *parameters)
{
   Boolean gapped_calculation = TRUE;
   Int2 status = 0;
   BlastHitSavingParameters* params;

   /* If parameters pointer is NULL, there is nothing to fill, 
      so don't do anything */
   if (!parameters)
      return 0;

   ASSERT(options);
   ASSERT(sbp);
   
   if (!sbp->kbp_gap)
      gapped_calculation = FALSE;

   /* If parameters have not yet been created, allocate and fill all
      parameters that are constant throughout the search */
   *parameters = params = (BlastHitSavingParameters*) 
      calloc(1, sizeof(BlastHitSavingParameters));

   if (params == NULL)
      return 1;

   params->options = (BlastHitSavingOptions *) options;

   /* If sum statistics use is forced by the options, 
      set it in the paramters */
   params->do_sum_stats = options->do_sum_stats;
   /* Sum statistics is used anyway for all ungapped searches and all 
      translated gapped searches (except RPS translated searches) */
   if (!gapped_calculation || 
       (program_number != blast_type_blastn && 
        program_number != blast_type_blastp &&
        program_number != blast_type_rpsblast &&
        program_number != blast_type_rpstblastn))
      params->do_sum_stats = TRUE;
   if (program_number == blast_type_blastn || !gapped_calculation) {
      params->gap_prob = BLAST_GAP_PROB;
      params->gap_decay_rate = BLAST_GAP_DECAY_RATE;
   } else {
      params->gap_prob = BLAST_GAP_PROB_GAPPED;
      params->gap_decay_rate = BLAST_GAP_DECAY_RATE_GAPPED;
   }
   params->gap_size = BLAST_GAP_SIZE;
   params->cutoff_big_gap = 0;

   status = BlastHitSavingParametersUpdate(program_number, 
               ext_params, sbp, query_info, params);
   return status;
}

Int2
BlastHitSavingParametersUpdate(Uint1 program_number, 
   const BlastExtensionParameters* ext_params, 
   BlastScoreBlk* sbp, BlastQueryInfo* query_info, 
   BlastHitSavingParameters* params)
{
   BlastHitSavingOptions* options;
   Blast_KarlinBlk* kbp;
   double evalue;

   ASSERT(params);
   ASSERT(query_info);
   ASSERT(ext_params);

   options = params->options;
   evalue = options->expect_value;

   /* Scoring options are not available here, but we can determine whether
      this is a gapped or ungapped search by checking whether gapped
      Karlin blocks have been set. */
   if (sbp->kbp_gap) {
      kbp = sbp->kbp_gap[query_info->first_context];
   } else {
      kbp = sbp->kbp[query_info->first_context];
   }

   /* Calculate cutoffs based on the current effective lengths information */
   if (options->cutoff_score > 0) {
      params->cutoff_score = options->cutoff_score;
   } else if (!options->phi_align) {
      params->cutoff_score = 0;
      BLAST_Cutoffs(&params->cutoff_score, &evalue, kbp, 
         (double)query_info->eff_searchsp_array[query_info->first_context], 
         FALSE, 0);
      /* When sum statistics is used, all HSPs above the gap trigger 
         cutoff are saved until the sum statistics is applied to potentially
         link them with other HSPs and improve their e-values. 
         However this does not apply to the ungapped search! */
      if (params->do_sum_stats) {
         params->cutoff_score = 
            MIN(params->cutoff_score, ext_params->gap_trigger);
      }
   } else {
      params->cutoff_score = 0;
   }
   
   params->cutoff_small_gap = 
      MIN(params->cutoff_score, ext_params->gap_trigger);
      
   return 0;
}

Int2 PSIBlastOptionsNew(PSIBlastOptions** psi_options)
{
   PSIBlastOptions* options;
   if (!psi_options)
      return 0;
   options = (PSIBlastOptions*)calloc(1, sizeof(PSIBlastOptions));
   *psi_options = options;
   options->ethresh = PSI_ETHRESH;
   options->maxNumPasses = PSI_MAX_NUM_PASSES;
   options->pseudoCountConst = PSI_PSEUDO_COUNT_CONST;
   options->scalingFactor = PSI_SCALING_FACTOR;
   
   return 0;
}

PSIBlastOptions* PSIBlastOptionsFree(PSIBlastOptions* psi_options)
{
   sfree(psi_options);
   return NULL;
}

Int2 BlastDatabaseOptionsNew(BlastDatabaseOptions** db_options)
{
   BlastDatabaseOptions* options = (BlastDatabaseOptions*)
      calloc(1, sizeof(BlastDatabaseOptions));

   options->genetic_code = BLAST_GENETIC_CODE;
   *db_options = options;

   return 0;
}

BlastDatabaseOptions* 
BlastDatabaseOptionsFree(BlastDatabaseOptions* db_options)
{
   sfree(db_options->gen_code_string);
   sfree(db_options);
   return NULL;
}

Int2 BLAST_InitDefaultOptions(Uint1 program_number,
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

Int2 BLAST_ValidateOptions(Uint1 program_number,
                           const BlastExtensionOptions* ext_options,
                           const BlastScoringOptions* score_options, 
                           const LookupTableOptions* lookup_options, 
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
   if ((status = BlastHitSavingOptionsValidate(program_number, hit_options,
                                               blast_msg)) != 0)
       return status;
   return status;
}

#define MY_EPS 1.0e-9
void
CalculateLinkHSPCutoffs(Uint1 program, BlastQueryInfo* query_info, 
   BlastScoreBlk* sbp, BlastHitSavingParameters* hit_params, 
   Int8 db_length, Int4 subject_length, 
   const PSIBlastOptions* psi_options)
{
	double gap_prob, gap_decay_rate, x_variable, y_variable;
	Blast_KarlinBlk* kbp;
	Int4 expected_length, gap_size, query_length;
	Int8 search_sp;
   Boolean translated_subject = (program == blast_type_tblastn || 
                                 program == blast_type_rpstblastn || 
                                 program == blast_type_tblastx);

	/* Do this for the first context, should this be changed?? */
	kbp = sbp->kbp[query_info->first_context];
	gap_size = hit_params->gap_size;
	gap_prob = hit_params->gap_prob;
	gap_decay_rate = hit_params->gap_decay_rate;
   /* Use average query length */
	query_length = query_info->context_offsets[query_info->last_context+1] /
      (query_info->last_context + 1);

   if (translated_subject) {
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

   if (search_sp > 8*gap_size*gap_size) {
      x_variable /= (1.0 - gap_prob + MY_EPS);
      hit_params->cutoff_big_gap = 
         (Int4) floor((log(x_variable)/kbp->Lambda)) + 1;
      x_variable = y_variable*(gap_size*gap_size);
      x_variable /= (gap_prob + MY_EPS);
      hit_params->cutoff_small_gap = 
         MAX(hit_params->cutoff_small_gap, 
             (Int4) floor((log(x_variable)/kbp->Lambda)) + 1);

      hit_params->ignore_small_gaps = FALSE;
   } else {
      hit_params->cutoff_big_gap = 
         (Int4) floor((log(x_variable)/kbp->Lambda)) + 1;
      hit_params->cutoff_small_gap = hit_params->cutoff_big_gap;
      hit_params->ignore_small_gaps = TRUE;
   }	

   if (psi_options) {
      hit_params->cutoff_big_gap *= (Int4) psi_options->scalingFactor;
      hit_params->cutoff_small_gap *= (Int4) psi_options->scalingFactor;
   }
}
