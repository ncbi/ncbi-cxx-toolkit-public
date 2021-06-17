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
 * Author:  Greg Boratyn
 *
 * Structures and methods for spliced alignments
 *
 */

#ifndef ALGO_BLAST_CORE__SPLICED_HITS__H
#define ALGO_BLAST_CORE__SPLICED_HITS__H

#include <algo/blast/core/blast_hits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A container to create a list of HSPs */
typedef struct HSPContainer
{
    BlastHSP* hsp;
    struct HSPContainer* next;    
} HSPContainer;


/** Create HSPContainer and take ownership of the HSP */
HSPContainer* HSPContainerNew(BlastHSP** hsp);

/** Free the list of HSPs, along with the stored HSPs */
HSPContainer* HSPContainerFree(HSPContainer* hc);

/** Clone a list of HSP containers */
HSPContainer* HSPContainerDup(HSPContainer* inh);

/** A chain of HSPs: spliced alignment */
typedef struct HSPChain
{
    Int4 context;           /**< Contex number of query sequence */
    Int4 oid;               /**< Subject oid */
    Int4 score;             /**< Alignment score for the chain */
    HSPContainer* hsps;     /**< A list of HSPs that belong to this chain */
    
    Int4 count;             /**< Number of placements for the read */
    struct HSPChain* pair;  /**< Pointer to mapped mate alignmemt
                               (for paired reads) */
    Uint1 pair_conf;        /**< Pair configuration */

    Int4 adapter;   /**< Position of detected adapter sequence in the query */
    Int4 polyA;     /**< Position of detected PolyA sequence in the query */
    struct HSPChain* next;  /**< Pointer to the next chain in a list */
} HSPChain;


/** Deallocate a chain or list of chains */
NCBI_XBLAST_EXPORT
HSPChain* HSPChainFree(HSPChain* chain_list);

/** Allocate a chain */
NCBI_XBLAST_EXPORT
HSPChain* HSPChainNew(Int4 context);

/** Clone a single HSP chain */
HSPChain* CloneChain(const HSPChain* chain);


/** Structure that contains BLAST mapping results */
typedef struct BlastMappingResults
{
    Int4 num_queries;
    HSPChain** chain_array;
} BlastMappingResults;


/** Initialize BlastMappingResults structure
 */
NCBI_XBLAST_EXPORT
BlastMappingResults* Blast_MappingResultsNew(void);

/** Free BlastMappingResults structure
 */
NCBI_XBLAST_EXPORT
BlastMappingResults* Blast_MappingResultsFree(BlastMappingResults* results);



#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__SPLICED_HITS__H */
