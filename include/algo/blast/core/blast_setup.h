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
* Made BlastScoreBlkGappedFill static
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

#ifndef __BLAST_SETUP__
#define __BLAST_SETUP__

#ifdef __cplusplus
extern "C" {
#endif

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_stat.h>
#include <algo/blast/core/blast_extend.h>
#include <algo/blast/core/blast_gapalign.h>

/** "Main" setup routine for BLAST. Calculates all information for BLAST search
 * that is dependent on the ASN.1 structures.
 * @param program_number Type of BLAST program (0=blastn, ...). [in]
 * @param qsup_options options for query setup. [in]
 * @param scoring_options options for scoring. [in]
 * @param lookup_options options for lookup table. [in]
 * @param hit_options options for saving hits. [in]
 * @param query_blk BLAST_SequenceBlk* for the query. [in]
 * @param query_info The query information block [in]
 * @param lookup_segments Start/stop locations for non-masked query 
 *                        segments [out]
 * @param filter_slp_out Filtering/masking locations. [out]
 * @param sbpp Contains scoring information. [out]
 * @param blast_message error or warning [out] 
 */
Int2 BLAST_MainSetUp(Uint1 program_number,
        const QuerySetUpOptions* qsup_options,
        const BlastScoringOptions* scoring_options,
        const LookupTableOptions* lookup_options,	
        const BlastHitSavingOptions* hit_options,
        BLAST_SequenceBlk* query_blk,
        BlastQueryInfo* query_info, BlastSeqLoc* *lookup_segments,
        BlastMask* *filter_slp_out,
        BlastScoreBlk* *sbpp, Blast_Message* *blast_message);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_SETUP__ */
