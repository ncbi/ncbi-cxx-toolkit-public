/*  $Id$
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
 * Author:  Tom Madden
 *
 */

/** @file blast_setup.h
 * Utilities initialize/setup BLAST.
 */

#ifndef __BLAST_SETUP__
#define __BLAST_SETUP__

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_gapalign.h>
#include <algo/blast/core/pattern.h>

#ifdef __cplusplus
extern "C" {
#endif

/** "Main" setup routine for BLAST. Calculates all information for BLAST search
 * that is dependent on the ASN.1 structures.
 * @todo FIXME: this function only filters query and sets up score block structure
 * @param program_number Type of BLAST program (0=blastn, ...). [in]
 * @param qsup_options options for query setup. [in]
 * @param scoring_options options for scoring. [in]
 * @param hit_options options for saving hits. [in]
 * @param query_blk BLAST_SequenceBlk* for the query. [in]
 * @param query_info The query information block [in]
 * @param scale_factor Multiplier for cutoff and dropoff scores [in]
 * @param lookup_segments Start/stop locations for non-masked query 
 *                        segments [out]
 * @param maskInfo masking location information, mask_at_hash value. [out]
 * @param sbpp Contains scoring information. [out]
 * @param blast_message error or warning [out] 
 */
NCBI_XBLAST_EXPORT
Int2 BLAST_MainSetUp(EBlastProgramType program_number,
        const QuerySetUpOptions* qsup_options,
        const BlastScoringOptions* scoring_options,
        const BlastHitSavingOptions* hit_options,
        BLAST_SequenceBlk* query_blk,
        const BlastQueryInfo* query_info, 
        double scale_factor,
        BlastSeqLoc* *lookup_segments,
        BlastMaskInformation* maskInfo,
        BlastScoreBlk* *sbpp, 
        Blast_Message* *blast_message);

/** Blast_ScoreBlkKbpGappedCalc, fills the ScoreBlkPtr for a gapped search.  
 *      Should be moved to blast_stat.c in the future.
 * @param sbp Contains fields to be set, should not be NULL. [out]
 * @param scoring_options Scoring_options [in]
 * @param program Used to set fields on sbp [in]
 * @param query_info Query information containing context information [in]
 * @param error_return Pointer to structure for returning errors. [in][out]
 * @return Status.
 */
NCBI_XBLAST_EXPORT
Int2 Blast_ScoreBlkKbpGappedCalc(BlastScoreBlk * sbp,
                                 const BlastScoringOptions * scoring_options,
                                 EBlastProgramType program, 
                                 const BlastQueryInfo * query_info,
                                 Blast_Message** error_return);

/** Function to calculate effective query length and db length as well as
 * effective search space. 
 * @param program_number blastn, blastp, blastx, etc. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_params Used to calculate effective lengths [in]
 * @param sbp Karlin-Altschul parameters [out]
 * @param query_info The query information block, which stores the effective
 *                   search spaces for all queries [in] [out]
*/
NCBI_XBLAST_EXPORT
Int2 BLAST_CalcEffLengths (EBlastProgramType program_number, 
   const BlastScoringOptions* scoring_options,
   const BlastEffectiveLengthsParameters* eff_len_params, 
   const BlastScoreBlk* sbp, BlastQueryInfo* query_info);

/** Set up the auxiliary structures for gapped alignment / traceback only 
 * @param program_number blastn, blastp, blastx, etc. [in]
 * @param seq_src Sequence source information, with callbacks to get 
 *                sequences, their lengths, etc. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_options Options overriding real database sizes for
 *                        calculating effective lengths [in]
 * @param ext_options options for gapped extension. [in]
 * @param hit_options options for saving hits. [in]
 * @param query_info The query information block [in]
 * @param sbp Contains scoring information. [in]
 * @param score_params Parameters for scoring [out]
 * @param ext_params Parameters for gapped extension [out]
 * @param hit_params Parameters for saving hits [out]
 * @param eff_len_params Parameters for search space calculations [out]
 * @param gap_align Gapped alignment information and allocated memory [out]
 */
NCBI_XBLAST_EXPORT
Int2 BLAST_GapAlignSetUp(EBlastProgramType program_number,
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
   BlastGapAlignStruct** gap_align);

/** Recalculates the parameters that depend on an individual sequence, if
 * this is not a database search.
 * @param program_number BLAST program [in]
 * @param subject_length Length of the current subject sequence [in]
 * @param scoring_options Scoring options [in]
 * @param query_info The query information structure. Effective lengths
 *                   are recalculated here. [in] [out]
 * @param sbp Scoring statistical parameters [in]
 * @param hit_params Parameters for saving hits. Score cutoffs are recalculated
 *                   here [in] [out]
 * @param word_params Parameters for ungapped extension. Score cutoffs are
 *                    recalculated here [in] [out]
 * @param eff_len_params Parameters for effective lengths calculation. Reset
 *                       with the current sequence data [in] [out]
 */
NCBI_XBLAST_EXPORT
Int2 BLAST_OneSubjectUpdateParameters(EBlastProgramType program_number,
    Uint4 subject_length,
    const BlastScoringOptions* scoring_options,
    BlastQueryInfo* query_info, 
    BlastScoreBlk* sbp, 
    BlastHitSavingParameters* hit_params,
    BlastInitialWordParameters* word_params,
    BlastEffectiveLengthsParameters* eff_len_params);

/** Initializes the substitution matrix in the BlastScoreBlk according to the
 * scoring options specified.
 * @todo Should be moved to blast_stat.c in the future.
 * @param program_number Used to set fields on sbp [in]
 * @param scoring_options Scoring_options [in]
 * @param sbp Contains fields to be set, should not be NULL. [out]
 *
*/
NCBI_XBLAST_EXPORT
Int2 Blast_ScoreBlkMatrixInit(EBlastProgramType program_number, 
    const BlastScoringOptions* scoring_options,
    BlastScoreBlk* sbp);

/** Initializes the score block structure.
 * @param query_blk Query sequence(s) [in]
 * @param query_info Additional query information [in]
 * @param scoring_options Scoring options [in]
 * @param program_number BLAST program type [in]
 * @param sbpp Initialized score block [out]
 * @param scale_factor Matrix scaling factor for this search [in]
 * @param blast_message Error message [out]
 */
NCBI_XBLAST_EXPORT
Int2 BlastSetup_ScoreBlkInit(BLAST_SequenceBlk* query_blk, 
    const BlastQueryInfo* query_info, 
    const BlastScoringOptions* scoring_options, 
    EBlastProgramType program_number, 
    BlastScoreBlk* *sbpp, 
    double scale_factor, 
    Blast_Message* *blast_message);


/** Adjusts the mask locations coordinates to a sequence interval. Removes those
 * mask locations that do not intersect the interval. Can do this either for all 
 * queries or only for the first one.
 * @param mask Structure containing a mask location. [in] [out]
 * @param from Starting offset of a sequence interval [in]
 * @param to Ending offset of a sequence interval [in]
 */
void
BlastSeqLoc_RestrictToInterval(BlastSeqLoc* *mask, Int4 from, Int4 to);


/** In a PHI BLAST search, adds pattern information to the BlastQueryInfo 
 * structure.
 * @param program Type of PHI BLAST program [in]
 * @param pattern_blk Auxiliary pattern items structure [in]
 * @param query Query sequence [in]
 * @param lookup_segments Locations on query sequence to find pattern on [in]
 * @param query_info Query information structure, where pattern occurrences
 *                   will be saved. [in][out]
 * @return Status, 0 on success, -1 on error.
 */
Int2 
Blast_SetPHIPatternInfo(EBlastProgramType            program,
                        const SPHIPatternSearchBlk * pattern_blk,
                        const BLAST_SequenceBlk    * query,
                        const BlastSeqLoc          * lookup_segments,
                        BlastQueryInfo             * query_info);

/** Calculates pattern space, which is used instead of search space in PHI BLAST
 * e-value calculations. Pattern space is equal to the product of the number of 
 * effective occurrences of pattern in query, and the number of pattern 
 * occurrences in database.
 * @param query_info The query information structure [in][out]
 * @param diagnostics The diagnostics structure containing the pattern count in 
 *                    database [in]
 */
void
PHIPatternSpaceCalc(BlastQueryInfo* query_info, 
                    const BlastDiagnostics* diagnostics);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_SETUP__ */

/*
 *
* $Log$
* Revision 1.54  2005/08/15 16:10:21  dondosha
* Added error return argument to Blast_ScoreBlkKbpGappedCalc
*
* Revision 1.53  2005/07/18 19:38:33  bealer
* - Apply const in several more places.
*
* Revision 1.52  2005/07/12 22:57:03  bealer
* - Change "BlastQueryInfo*" to "const BlastQueryInfo*" in several places.
*
* Revision 1.51  2005/04/27 19:49:01  dondosha
* Added Blast_SetPHIPatternInfo function for PHI BLAST query pattern occurrences calculation, and
* PHIPatternSpaceCalc for calculation of pattern space after all database occurrences are found.
*
* Revision 1.50  2004/12/29 13:32:43  madden
* Replaced include of blast_options.h with include of blast_parameters.h
*
* Revision 1.49  2004/12/09 21:16:26  camacho
* Removed inaccurate comment for Blast_ScoreBlkMatrixInit
*
* Revision 1.48  2004/12/09 15:21:32  dondosha
* Renamed some functions dealing with BlastScoreBlk and Blast_KarlinBlk structures
*
* Revision 1.47  2004/11/30 16:54:43  dondosha
* Added BlastSeqLoc_RestrictToInterval
*
* Revision 1.46  2004/11/23 21:46:05  camacho
* kbp_ideal field of BlastScoreBlk is initialized unconditionally
*
* Revision 1.45  2004/11/02 18:18:20  madden
* Remove BlastExtensionParameters from BLAST_OneSubjectUpdateParameters prototype
*
* Revision 1.44  2004/08/11 11:58:43  ivanov
* Added more export specifiers NCBI_XBLAST_EXPORT
*
* Revision 1.43  2004/08/10 14:52:00  ivanov
* Added export specifier NCBI_XBLAST_EXPORT
*
* Revision 1.42  2004/07/06 15:35:12  dondosha
* Use EBlastProgramType enumeration type instead of Uint1 for program argument in all functions
*
* Revision 1.41  2004/06/28 13:38:30  madden
* Change BLAST_MainSetUp to use BlastMaskInformation rather than BlastMaskLoc
*
* Revision 1.40  2004/06/16 14:53:03  dondosha
* Moved extern "C" after the #includes
*
* Revision 1.39  2004/05/19 14:52:01  camacho
* 1. Added doxygen tags to enable doxygen processing of algo/blast/core
* 2. Standardized copyright, CVS $Id string, $Log and rcsid formatting and i
*    location
* 3. Added use of @todo doxygen keyword
*
* Revision 1.38  2004/05/17 16:38:08  camacho
* Make function declarations more readable
*
* Revision 1.37  2004/05/07 15:36:40  papadopo
* add scale factor as input argument to BlastMainSetup and GetScoreBlk
*
* Revision 1.36  2004/03/30 15:49:07  madden
* Add prototype for BlastSetup_GetScoreBlock
*
* Revision 1.35  2004/03/16 18:55:36  camacho
* Moved log to end of file
*
* Revision 1.34  2004/03/15 19:54:51  dondosha
* Removed unneeded arguments in BLAST_GapAlignSetUp
*
* Revision 1.33  2004/03/11 18:31:06  papadopo
* fix doxygen warnings
*
* Revision 1.32  2004/03/10 18:00:20  camacho
* Remove outdated references to blastkar
*
* Revision 1.31  2004/03/09 22:37:11  dondosha
* Added const qualifiers to parameter arguments wherever relevant
*
* Revision 1.30  2004/03/09 18:39:35  dondosha
* Pass around effective lengths parameters instead of options; added BLAST_OneSubjectUpdateParameters to recalculate cutoffs and eff. lengths when each subject is an individual sequence
*
* Revision 1.29  2004/02/27 15:56:35  papadopo
* Mike Gertz' modifications to unify handling of gapped Karlin blocks for protein and nucleotide searches. Also modified BLAST_MainSetUp to allocate gapped Karlin blocks last
*
* Revision 1.28  2004/02/24 17:59:03  dondosha
* Moved BLAST_CalcEffLengths from blast_engine.h; Added BLAST_GapAlignSetUp to set up only gapped alignment related structures
*
* Revision 1.27  2004/02/10 20:05:14  dondosha
* Made Blast_ScoreBlkKbpGappedCalc external again: needed in unit test
*
* Revision 1.26  2003/12/03 16:31:46  dondosha
* Renamed BlastMask to BlastMaskLoc, BlastResults to BlastHSPResults, to avoid name conflicts
*
* Revision 1.25  2003/09/10 19:43:05  dondosha
* Changed #includes in accordance with lookup table code reorganization
*
* Revision 1.24  2003/09/09 14:21:14  coulouri
* change blastkar.h to blast_stat.h
*
* Revision 1.23  2003/08/11 14:57:16  dondosha
* Added algo/blast/core path to all #included headers
*
* Revision 1.22  2003/08/01 22:33:32  dondosha
* Made Blast_ScoreBlkKbpGappedCalc static
*
* Revision 1.21  2003/08/01 17:20:39  dondosha
* Renamed BLAST_ScoreBlk to BlastScoreBlk
*
* Revision 1.20  2003/07/31 14:31:37  camacho
* Replaced Char for char
*
* Revision 1.19  2003/07/31 00:32:35  camacho
* Eliminated Ptr notation
*
* Revision 1.18  2003/07/24 20:49:36  camacho
* Remove unnecessary #include
*
* Revision 1.17  2003/06/26 21:30:10  dondosha
* Removed const from an integral type function parameters
*
* Revision 1.16  2003/06/19 18:58:21  dondosha
* Moved all functions dealing with SeqLocs to blast_seq.h
*
* Revision 1.15  2003/06/17 21:09:26  dondosha
* Moved file reading from BLAST_SetUpSubject to Main
*
* Revision 1.14  2003/06/11 16:14:02  dondosha
* Added number of queries argument to BLAST_SetUpQuery
*
* Revision 1.13  2003/06/06 20:36:12  dondosha
* Removed unneeded arguments from BLAST_GetTranslatedSeqLoc
*
* Revision 1.12  2003/06/06 17:51:28  dondosha
* Comments fixes for doxygen
*
* Revision 1.11  2003/06/05 18:33:39  dondosha
* Compiler warnings fixes
*
* Revision 1.10  2003/06/05 17:16:04  dondosha
* SeqLoc is no longer used for query masking/filtering
*
* Revision 1.9  2003/05/18 21:56:04  camacho
* Use Uint1 for program name whenever possible
*
* Revision 1.8  2003/05/06 21:28:09  dondosha
* Added functions previously static in blast_driver.c
*
* Revision 1.7  2003/05/01 17:09:07  dondosha
* BLAST_SetUpAuxStructures made static in blast_engine.c
*
* Revision 1.6  2003/05/01 16:57:02  dondosha
* Fixes for strict compiler warnings
*
* Revision 1.5  2003/05/01 15:31:54  dondosha
* Reorganized the setup of BLAST search
*
* Revision 1.4  2003/04/18 22:28:15  dondosha
* Separated ASN.1 generated structures from those used in the main BLAST engine and traceback
*
* Revision 1.3  2003/04/03 14:17:45  coulouri
* fix warnings, remove unused parameter
*
* Revision 1.2  2003/04/02 17:21:23  dondosha
* Changed functions parameters to accommodate calculation of ungapped cutoff score
*
* Revision 1.1  2003/03/31 18:18:31  camacho
* Moved from parent directory
*
* Revision 1.23  2003/03/24 20:39:17  dondosha
* Added BlastExtensionParameters structure to hold raw gapped X-dropoff values
*
* Revision 1.22  2003/03/24 17:27:42  dondosha
* Improvements and additions for doxygen
*
* Revision 1.21  2003/03/14 16:55:09  dondosha
* Minor corrections to eliminate strict compiler warnings
*
* Revision 1.20  2003/03/07 20:42:23  dondosha
* Added ewp argument to be initialized in BlastSetup_Main
*
* Revision 1.19  2003/03/05 21:01:40  dondosha
* Added query information block to output arguments of BlastSetUp_Main
*
* Revision 1.18  2003/02/26 15:42:19  madden
* const charPtr becomes const char *
*
* Revision 1.17  2003/02/25 20:03:02  madden
* Remove BlastSetUp_Concatenate and BlastSetUp_Standard
*
* Revision 1.16  2003/02/14 17:37:24  madden
* Doxygen compliant comments
*
* Revision 1.15  2003/02/13 21:40:40  madden
* Validate options, send back message if problem
*
* Revision 1.14  2003/02/04 14:45:50  camacho
* Reformatted comments for doxygen
*
* Revision 1.13  2003/01/30 20:08:07  dondosha
* Added a header file for nucleotide lookup tables
*
* Revision 1.12  2003/01/28 15:14:21  madden
* BlastSetUp_Main gets additional args for parameters
*
* Revision 1.11  2003/01/10 18:50:11  madden
* Version of BlastSetUp_Main that does not require num_seqs or dblength
*
* Revision 1.10  2002/12/24 16:21:40  madden
* BlastSetUp_Mega renamed to BlastSetUp_Concatenate, unused arg frame removed
*
* Revision 1.9  2002/12/24 14:48:23  madden
* Create lookup table for proteins
*
* Revision 1.8  2002/12/20 20:55:19  dondosha
* Blast_ScoreBlkKbpGappedCalc made external (probably temporarily)
*
* Revision 1.7  2002/12/19 21:22:39  madden
* Add BlastSetUp_Mega prototype
*
* Revision 1.6  2002/12/04 13:53:47  madden
* Move BLAST_SequenceBlk from blsat_setup.h to blast_def.h
*
* Revision 1.5  2002/10/24 14:08:04  madden
* Add DNA length to call to BlastSetUp_Standard
*
* Revision 1.4  2002/10/23 22:42:34  madden
* Save context and frame information
*
* Revision 1.3  2002/10/22 15:50:46  madden
* fix translating searches
*
* Revision 1.2  2002/10/07 21:04:37  madden
* prototype change
*

*/

