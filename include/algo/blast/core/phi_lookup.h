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
 * ===========================================================================
 *
 * Author: Ilya Dondoshansky
 *
 */

/** @file phi_lookup.h
 * Pseudo lookup table structure and database scanning functions used in 
 * PHI-BLAST
 */

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_options.h>
#include <algo/blast/core/pattern.h>
#include <algo/blast/core/lookup_wrap.h>

#ifndef PHI_LOOKUP__H
#define PHI_LOOKUP__H

#ifdef __cplusplus
extern "C" {
#endif

/** The following 3 flags define 3 options for running the program */
#define SEED_FLAG 1         
#define PATTERN_FLAG 2
#define PAT_SEED_FLAG 3
#define PAT_MATCH_FLAG 4
#define PATTERN_TOO_LONG  2

/** Original allocation size for the starts and lengths arrays for pattern 
 * occurrencies in the query.
 */
#define MIN_PHI_LOOKUP_SIZE 1000

/** Pseudo lookup table structure for PHI-BLAST. Contains starting and ending
 * offsets of pattern occurrences in the query sequence. 
 */
typedef struct BlastPHILookupTable {
   patternSearchItems* pattern_info;
   Boolean is_dna;
   Int4 num_matches;
   Int4 allocated_size;
   Int4* start_offsets;
   Int4* lengths;
} BlastPHILookupTable;

/** Initialize the pseudo lookup table for PHI BLAST */
Int2 PHILookupTableNew(const LookupTableOptions* opt, 
                       BlastPHILookupTable* * lut,
                       Boolean is_dna, BlastScoreBlk* sbp);

/** Deallocate memory for the PHI BLAST lookup table */
BlastPHILookupTable* PHILookupTableDestruct(BlastPHILookupTable* lut);

/** Find all occurrencies of a pattern in query, and save starts/stops in the
 * PHILookupTable structure.
 */
Int4 PHIBlastIndexQuery(BlastPHILookupTable* lookup,
        BLAST_SequenceBlk* query, BlastSeqLoc* location, Boolean is_dna);


/**
 * Scans the subject sequence from "offset" to the end of the sequence.
 * Copies at most array_size hits.
 * Returns the number of hits found.
 * If there isn't enough room to copy all the hits, return early, and update
 * "offset". 
 *
 * @param lookup_wrap contains the pseudo lookup table with offsets of pattern
 *                    occurrencies in query [in]
 * @param query_blk the query sequence [in]
 * @param subject the subject sequence [in]
 * @param offset the offset in the subject at which to begin scanning [in/out]
 * @param offset_pairs Array of start and end positions of pattern in subject [out]
 * @param array_size length of the offset arrays [in]
 * @return The number of hits found.
 */
Int4 PHIBlastScanSubject(const LookupTableWrap* lookup_wrap,
        const BLAST_SequenceBlk *query_blk, const BLAST_SequenceBlk *subject, 
        Int4* offset, BlastOffsetPair* NCBI_RESTRICT offset_pairs,
        Int4 array_size);

#ifdef __cplusplus
}
#endif

#endif /* PHI_LOOKUP__H */
