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

File name: blast_setup.h

Author: Tom Madden

Contents: Utilities initialize/setup BLAST.

$Revision$

******************************************************************************/

/*
 *
* $Log$
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
* const CharPtr becomes const Char *
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
* BlastScoreBlkGappedFill made external (probably temporarily)
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


#include <ncbi.h>
#include <blast_options.h>
#include <blastkar.h>
#include <blast_def.h>
#include <mb_lookup.h>
#include <aa_lookup.h>
#include <na_lookup.h>
#include <blast_extend.h>
#include <blast_gapalign.h>

/** BlastScoreBlkGappedFill, fills the ScoreBlkPtr for a gapped search.  
 *      Should be moved to blastkar.c (or it's successor) in the future.
 * @param sbp contains fields to be set, should not be NULL. [out]
 * @param scoring_options scoring_options [in]
 * @param program_name used to set fields on sbp [in]
 *
*/
Int2
BlastScoreBlkGappedFill(BLAST_ScoreBlkPtr sbp,
const BlastScoringOptionsPtr scoring_options, const Char *program_name);


/** BlastSetUp_GetSequence 
 * Purpose:     Get the sequence for the BLAST engine, put in a Uint1 buffer
 * @param slp SeqLoc to extract sequence for [in]
 * @param use_blastna If TRUE use blastna alphabet (ignored for proteins) [in]
 * @param concatenate If TRUE do all SeqLoc's, otherwise only first [in]
 * @param buffer Buffer to hold plus strand or protein [out] 
 * @param buffer_length Length of buffer allocated [out]
 * @param selcys_pos Positions where selenocysteine was replaced by X [out]
 */
Int2 LIBCALL 
BlastSetUp_GetSequence(SeqLocPtr slp, Boolean use_blastna, Boolean concatenate,
   Uint1Ptr *buffer, Int4 *buffer_length, ValNodePtr *selcys_pos);

/** Get the subject sequence from its SeqLoc for the two sequences case 
 * @param subject_slp The subject SeqLoc [in]
 * @param buffer Buffer containing sequence; compressed if nucleotide [out]
 * @param buffer_length Length of the sequence buffer [out]
 * @param encoding What type of sequence encoding to use? [in]
 */
Int2 BLAST_GetSubjectSequence(SeqLocPtr subject_slp, Uint1Ptr *buffer,
                              Int4 *buffer_length, Uint1 encoding);

/** Read the query sequences from a file, return a SeqLoc list.
 * @param infp The input file [in]
 * @param query_is_na Are sequences nucleotide (or protein)? [in]
 * @param lcase_mask The lower case masking locations (no lower case masking 
 *                   if NULL [out]
 * @param query_slp List of query SeqLocs [out]
 * @param ctr_start Number from which to start counting local ids [in]
 * @return Have all sequences been read?
 */
Boolean
BLAST_GetQuerySeqLoc(FILE *infp, Boolean query_is_na, 
   SeqLocPtr PNTR lcase_mask, SeqLocPtr PNTR query_slp, Int4 ctr_start);

/** Set up the subject sequence block in case of two sequences BLAST.
 * @param file_name File with the subject sequence FASTA [in]
 * @param blast_program Type of BLAST program [in]
 * @param subject_slp SeqLoc for the subject sequence [out]
 * @param subject Subject sequence block [out]
 */
Int2 BLAST_SetUpSubject(CharPtr file_name, CharPtr blast_program, 
        SeqLocPtr PNTR subject_slp, BLAST_SequenceBlkPtr PNTR subject);

/** "Main" setup routine for BLAST. Calculates all information for BLAST search
 * that is dependent on the ASN.1 structures.
 * @param query_slp Linked list of all query SeqLocs. [in]
 * @param program blastn, blastp, blastx, etc. [in]
 * @param qsup_options options for query setup. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_options  used to calc. eff len. [in]
 * @param lookup_options options for lookup table. [in]
 * @param word_options options for initial word finding. [in]
 * @param hit_options options for saving hits. [in]
 * @param frame frame info. (blastx/tblastx) [in]
 * @param query_blk_ptr BLAST_SequenceBlkPtr for the query. [out]
 * @param lookup_segments Start/stop locations for non-masked query 
 *                        segments [out]
 * @param query_info The query information block [out]
 * @param filter_slp_out filtering seqloc. [out]
 * @param sbpp Contains scoring information. [out]
 * @param blast_message error or warning [out] 
 */
Int2 BLAST_MainSetUp(SeqLocPtr query_slp, Char *program,
        const QuerySetUpOptionsPtr qsup_options,
        const BlastScoringOptionsPtr scoring_options,
        const BlastEffectiveLengthsOptionsPtr eff_len_options,
        const LookupTableOptionsPtr lookup_options,	
        const BlastInitialWordOptionsPtr word_options,
        const BlastHitSavingOptionsPtr hit_options,
        const Int4Ptr frame, BLAST_SequenceBlkPtr *query_blk_ptr,
        ValNodePtr PNTR lookup_segments,
        BlastQueryInfoPtr *query_info, SeqLocPtr *filter_slp_out,
        BLAST_ScoreBlkPtr *sbpp, Blast_MessagePtr *blast_message);

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
Int2 BLAST_SetUpAuxStructures(Char *program,
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
        BlastHitSavingParametersPtr PNTR hit_params);
