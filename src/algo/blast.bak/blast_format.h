/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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

File name: blast_format.h

Author: Ilya Dondoshansky

Contents: Functions needed for formatting of BLAST results

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_FORMAT__
#define __BLAST_FORMAT__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NCBI_C_TOOLKIT
#define NCBI_C_TOOLKIT
#endif

#include <ncbi.h>
#include <asn.h>
#include <bxmlobj.h>
#include <readdb.h>
#include <blast_options.h>
#include <blast_hits.h>

/** Options for formatting BLAST results 
 */
typedef struct BlastFormattingOptions {
   Uint1 align_view;
   Uint4 align_options;
   Uint4 print_options;
   Boolean believe_query;
   Boolean html;
   FILE *outfp;
   Int4 number_of_descriptions;
   Int4 number_of_alignments;
   Boolean ungapped; /**< Should this be here????? */
} BlastFormattingOptions;

/** Initialize the formatting options structure.
 * @param program Number of the BLAST program [in]
 * @param file_out Name of the file where output is printed [in]
 * @param num_descriptions Number of definition lines to report per query [in]
 * @param num_alignments Number of alignments to show per query [in]
 * @param align_view What kind of formatted output to show? [in]
 * @param format_options_ptr The initialized structure [out]
*/
Int2 BlastFormattingOptionsNew(Uint1 program, char* file_out, 
        Int4 num_descriptions, Int4 num_alignments, Int4 align_view,
        BlastFormattingOptions** format_options_ptr);

/** Deallocate memory for formatting options. In particular,
 * close the output file.
 * @param format_options Structure to free [in]
 */
BlastFormattingOptions* 
BlastFormattingOptionsFree(BlastFormattingOptions* format_options);

/** Used for pruning SeqALigns that are too big. */
typedef struct BlastPruneSapStruct {
   SeqAlignPtr sap;
   Int4	original_number; /**< how may unique hits were there originally. */
   Int4 number;		 /**< How many hits on SeqALignPtr above. */
   Boolean allocated; /**< If FALSE, SeqAlignPtr above does NOT belong to this 
                         struc.*/
} BlastPruneSapStruct;

typedef struct MBXml {
   BlastOutputPtr boutp;
   AsnIoPtr   aip;
   AsnTypePtr atp;
   AsnTypePtr BlastOutput;
   AsnTypePtr BlastOutput_iterations;
   AsnTypePtr BlastOutput_mbstat;
} MBXml;

/** Print formatted output.
 * @param head Results in the SeqAlign form (freed after use) [in]
 * @param blast_database BLAST database name [in]
 * @param blast_program BLAST program name [in]
 * @param num_queries Number of query sequences [in]
 * @param query_slp Linked list of query SeqLocs [in]
 * @param mask_loc Masking locations for all queries (freed after use) [in]
 * @param format_options Formatting options [in]
 * @param is_ooframe Are frame shifts allowed in alignments? [in]
 */
Int2 BLAST_FormatResults(SeqAlignPtr head, char* blast_database,
        char* blast_program, Int4 num_queries, 
        SeqLocPtr query_slp, BlastMask* mask_loc, 
        BlastFormattingOptions* format_options, Boolean is_ooframe);

/** Print the summary at the end of the BLAST report.
 * @param program_number Type of BLAST program [in]
 * @param format_options Formatting options [in]
 * @param score_options Scoring options [in]
 * @param sbp Statistical information [in]
 * @param lookup_options Lookup table options [in]
 * @param word_options Word finding options and parameters [in]
 * @param ext_options Extension options and parameters [in]
 * @param hit_options Hit saving options [in]
 * @param query_info Query information [in]
 * @param rdfp BLAST database information [in]
 * @param return_stats Data about this run [in]
 */
Int2 PrintOutputFooter(Uint1 program_number, 
        BlastFormattingOptions* format_options, 
        BlastScoringOptions* score_options, BlastScoreBlk* sbp,
        LookupTableOptions* lookup_options,
        BlastInitialWordOptions* word_options,
        BlastExtensionOptions* ext_options,
        BlastHitSavingOptions* hit_options,
        BlastQueryInfo* query_info, ReadDBFILEPtr rdfp, 
        BlastReturnStat* return_stats); 

Int2 BLAST_PrintOutputHeader(BlastFormattingOptions* format_options,
        Boolean greedy_extension, char* blast_database, Boolean db_is_na);

void BLAST_PrintIntermediateResults(BlastResults* results, 
        BlastQueryInfo* query_info, SeqLocPtr query_slp, 
        ReadDBFILEPtr rdfp, SeqIdPtr seqid, BlastScoreBlk* sbp, 
        char* filename);


#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_FORMAT__ */

