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
 *
 */

/** @file blast_diagnostics.h
 * Various diagnostics (hit counts, etc.) returned from the BLAST engine
 */

#ifndef __BLAST_DIAGNOSTICS__
#define __BLAST_DIAGNOSTICS__

#include <algo/blast/core/ncbi_std.h>
#include <connect/ncbi_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Structure holding raw cutoff and gap-x-drop values. */
typedef struct BlastRawCutoffs {
   Int4 x_drop_ungapped; /**< Raw value of the x-dropoff for ungapped 
                            extensions */
   Int4 x_drop_gap; /**< Raw value of the x-dropoff for preliminary gapped 
                       extensions */
   Int4 x_drop_gap_final; /**< Raw value of the x-dropoff for gapped 
                             extensions with traceback */
   Int4 ungapped_cutoff; /**< Minimal raw score for starting gapped extension */
   Int4 cutoff_score; /**< Cutoff score corresponding to given evalue. */
} BlastRawCutoffs;

/** Structure containing hit counts from the ungapped stage of a BLAST 
 * search */
typedef struct BlastUngappedStats {
   Int8 lookup_hits; /**< Number of successful lookup table hits */
   Int4 num_seqs_lookup_hits; /**< Number of sequences which had at least one 
                                 lookup table hit. */
   Int4 init_extends; /**< Number of initial words found and extended */
   Int4 good_init_extends; /**< Number of successful initial extensions,
                              i.e. number of HSPs saved after ungapped stage.*/
   Int4 num_seqs_passed; /**< Number of sequences with at least one HSP saved
                            after ungapped stage. */
} BlastUngappedStats;

/** Structure containing hit counts from the gapped stage of a BLAST 
 * search */
typedef struct BlastGappedStats {
   Int4 seqs_ungapped_passed; /**< Number of sequences with top HSP 
                                 after ungapped extension passing the
                                 e-value threshold. */
   Int4 extra_extensions; /**< Number of extra gapped extensions performed for
                             ungapped HSPs above the e-value threshold. */
   Int4 extensions; /**< Total number of gapped extensions performed. */
   Int4 good_extensions; /**< Number of HSPs below the e-value threshold after
                            gapped extension */
   Int4 num_seqs_passed; /**< Number of sequences with top HSP passing the
                            e-value threshold. */
} BlastGappedStats;

/** Return statistics from the BLAST search */
typedef struct BlastDiagnostics {
   BlastUngappedStats* ungapped_stat; /**< Ungapped extension counts */
   BlastGappedStats* gapped_stat; /**< Gapped extension counts */
   BlastRawCutoffs* cutoffs; /**< Various raw values for the cutoffs */
   MT_LOCK mt_lock; /**< Mutex for updating diagnostics data in a 
                       multi-threaded search. */
} BlastDiagnostics;

/** Free the BlastDiagnostics structure and all substructures. */
BlastDiagnostics* Blast_DiagnosticsFree(BlastDiagnostics* diagnostics);

/** Initialize the BlastDiagnostics structure and all its substructures. */
BlastDiagnostics* Blast_DiagnosticsInit(void);

/** Initialize the BlastDiagnostics structure for a multi-threaded search.
 * @param mt_lock Mutex locking mechanism to be used for updates. [in]
 */
BlastDiagnostics* Blast_DiagnosticsInitMT(MT_LOCK mt_lock);

/** Fill data in the ungapped hits diagnostics structure */
void Blast_UngappedStatsUpdate(BlastUngappedStats* ungapped_stats, 
                               Int4 total_hits, Int4 extended_hits,
                               Int4 saved_hits);

/** In a multi-threaded run, update global diagnostics data with the data
 * coming from one of the preliminary search threads.
 * @param diag_global Diagnostics for the entire BLAST search [in] [out]
 * @param diag_local Diagnostics from one of the preliminary search threads [in]
 */
void 
Blast_DiagnosticsUpdate(BlastDiagnostics* diag_global,
                        BlastDiagnostics* diag_local);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_DIAGNOSTICS__ */
