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
 * @param compress_dna Should the DNA sequence be compressed (ignored for 
 *                     proteins)? [in]
 */
Int2 BLAST_GetSubjectSequence(SeqLocPtr subject_slp, Uint1Ptr *buffer,
                              Int4 *buffer_length, Boolean compress_dna);

/** "Main" setup routine for BLAST.
 * @param query_slp query location (all queries should be represented here). [in]
 * @param program blastn, blastp, blastx, etc. [in]
 * @param qsup_options options for query setup. [in]
 * @param scoring_options options for scoring. [in]
 * @param eff_len_options  used to calc. eff len. [in]
 * @param lookup_options options for lookup table. [in]
 * @param word_options options for initial word finding. [in]
 * @param hit_options options for saving hits. [in]
 * @param concatenate concatenate all SeqLoc's if TRUE. [in]
 * @param frame frame info. (blastx/tblastx) [in]
 * @param seq_blocks BLAST_SequenceBlkPtr blocks. [out]
 * @param filter_slp_out filtering seqloc. [out]
 * @param ewp Auxiliary structure for extending initial words [out]
 * @param sbpp Contains scoring information. [out]
 * @param lookup_wrap Lookup table [out]
 * @param query_info The query information block [out]
 * @param hit_parameters Parsed hit saving options [out]
 * @param blast_message error or warning [out] 
 */

Int2 LIBCALL
BlastSetUp_Main
(SeqLocPtr query_slp,                              
const Char *program,                      
const QuerySetUpOptionsPtr qsup_options,   
const BlastScoringOptionsPtr scoring_options,
const BlastEffectiveLengthsOptionsPtr eff_len_options, 
const LookupTableOptionsPtr     lookup_options, 
const BlastInitialWordOptionsPtr word_options, 
const BlastHitSavingOptionsPtr hit_options,   
Boolean concatenate,                        
const Int4Ptr frame,                       
ValNodePtr *seq_blocks,                   
BlastQueryInfoPtr *query_info,
SeqLocPtr *filter_slp_out,               
BLAST_ExtendWordPtr *ewp,
BLAST_ScoreBlkPtr *sbpp,                
LookupTableWrapPtr *lookup_wrap,
BlastHitSavingParametersPtr *hit_parameters,  
Blast_MessagePtr *blast_message              
);

