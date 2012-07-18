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
 * Author:  Aleksandr Morgulis
 *
 */

/** @file mb_indexed_lookup.h
 * Declarations for functions that extract hits from indexed blast
 * databases (specialized for megablast)
 */

#ifndef __MB_INDEXED_LOOKUP__
#define __MB_INDEXED_LOOKUP__

#include <algo/blast/core/blast_parameters.h>
#include <algo/blast/core/blast_extend.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LAST_VOL_IDX_INIT (-1)
#define LAST_VOL_IDX_NULL (-2)

enum EOidState {
    eNoResults = 0,
    eHasResults,
    eNotIndexed
};

/** Function pointer type to retrieve hits from an indexed database */
typedef unsigned long (*T_MB_IdbGetResults)(/* void * idb, */Int4 oid, Int4 chunk,
                                   BlastInitHitList * init_hitlist);

/** Function pointer type to check index seeds availability for oid. */
typedef int (*T_MB_IdbCheckOid)( Int4 oid, Int4 * last_vol_id );

/** Function pointer type to indicate end of preliminary search by the
    thread to the indexing library.
*/
typedef void (*T_MB_IdxEndSearchIndication)( Int4 last_vol_id );

/** Finds all runs of a specified number of exact matches between 
 * two nucleotide sequences. Assumes the subject sequence is part
 * of a previously indexed database
 * @param subject The subject sequence [in]
 * @param query Ignored
 * @param query_info Ignored
 * @param lookup_wrap Pointer to the (wrapper) lookup table structure [in]
 * @param matrix Ignored
 * @param word_params Ignored
 * @param ewp Ignored
 * @param offset_pairs Ignored
 * @param max_hits Ignored
 * @param init_hitlist Structure to hold all hits information. Has to be 
 *        allocated up front [out]
 * @param ungapped_stats Ignored
 */
extern Int2 MB_IndexedWordFinder( 
        BLAST_SequenceBlk * subject,
        BLAST_SequenceBlk * query,
        BlastQueryInfo * query_info,
        LookupTableWrap * lookup_wrap,
        Int4 ** matrix,
        const BlastInitialWordParameters * word_params,
        Blast_ExtendWord * ewp,
        BlastOffsetPair * offset_pairs,
        Int4 max_hits,
        BlastInitHitList * init_hitlist,
        BlastUngappedStats * ungapped_stats );

#ifdef __cplusplus
}
#endif

#endif /* __MB_INDEXED_LOOKUP__ */
