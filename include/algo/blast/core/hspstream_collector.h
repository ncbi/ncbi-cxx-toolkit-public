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
 * Author:  Ilya Dondoshansky
 *
 */

/** @file hspstream_collector.h
 * Default implementation of the BlastHSPStream interface to save hits from
 * a BLAST search, and subsequently return them in sorted order.
 */

#ifndef HSPSTREAM_COLLECTOR_H
#define HSPSTREAM_COLLECTOR_H

#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/blast_hits.h>
#include <algo/blast/core/blast_hspstream.h>
#include <connect/ncbi_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Default implementation of BlastHSPStream */
typedef struct BlastHSPListCollectorData {
   EBlastProgramType program;           /**< BLAST program type */
   const BlastHitSavingOptions* hit_options; /**< Hit saving options */
   BlastHSPResults* results;/**< Structure for saving HSP lists */
   Boolean results_sorted;  /**< Have the results already been sorted? 
                               Set to true after the first read call. */
   Boolean sort_on_read;    /**< Should the results be sorted on the first
                               read call? */
   Int4 first_query_index;  /**< Index of the first query to try getting 
                               results from. */
   MT_LOCK x_lock;   /**< Mutex for writing and reading results. */
                                  
} BlastHSPListCollectorData;

/** Initialize the collector HSP stream for a multi-threaded search. The 
 * locking facility must be instantiated before this function is called. 
 * @param program Type of BlAST program [in]
 * @param hit_options Hit saving options containing limits on numbers of 
 *                    results to save[in]
 * @param num_queries Number of query sequences in this BLAST search [in]
 * @param sort_on_read Should results be sorted on the first read call? [in]
 * @param lock        Pointer to locking structure for writing by multiple
 *                    threads. Locking will not be performed if NULL. [in]
 */
BlastHSPStream* 
Blast_HSPListCollectorInitMT(EBlastProgramType program, 
                             const BlastHitSavingOptions* hit_options, 
                             Int4 num_queries, Boolean sort_on_read, 
                             MT_LOCK lock);

/** Initialize the collector HSP stream for a single-threaded search, i.e. 
 * no locking is done when reading/writing from/to the stream.
 * @param program Type of BlAST program [in]
 * @param hit_options Hit saving options containing limits on numbers of 
 *                    results to save[in]
 * @param num_queries Number of query sequences in this BLAST search [in]
 * @param sort_on_read Should results be sorted on the first read call? [in]
 */
BlastHSPStream* 
Blast_HSPListCollectorInit(EBlastProgramType program, 
                           const BlastHitSavingOptions* hit_options, 
                           Int4 num_queries, Boolean sort_on_read);

#ifdef __cplusplus
}
#endif

#endif /* HSPSTREAM_COLLECTOR_H */
