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

#ifndef ALGO_BLAST_CORE__PHI_LOOKUP_H
#define ALGO_BLAST_CORE__PHI_LOOKUP_H

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the pattern items structure, serving as a "pseudo" lookup table
 * in a PHI BLAST search.
 * @param pattern String describing the pattern to search for [in]
 * @param is_dna boolean describing whether the strings are DNA or protein [in]
 * @param sbp Scoring block with statistical parameters [in]
 * @param pattern_blk The initialized structure [out]
 * @param error_msg Error message, if any.
 * @return 0 on success, -1 on failure.
 */
Int2 SPHIPatternSearchBlkNew(char* pattern, Boolean is_dna, BlastScoreBlk* sbp, 
                            SPHIPatternSearchBlk* *pattern_blk, 
                            Blast_Message* *error_msg);

/** Deallocate memory for the PHI BLAST lookup table.
 * @param pattern_blk The structure to deallocate [in]
 * @return NULL.
 */
SPHIPatternSearchBlk* 
SPHIPatternSearchBlkFree(SPHIPatternSearchBlk* pattern_blk);


/**
 * Scans the subject sequence from "offset" to the end of the sequence.
 * Copies at most array_size hits.
 * Returns the number of hits found.
 * If there isn't enough room to copy all the hits, return early, and update
 * "offset". 
 *
 * @param lookup_wrap contains the pseudo lookup table with offsets of pattern
 *                    occurrences in query [in]
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

#endif /* ALGO_BLAST_CORE__PHI_LOOKUP_H */
