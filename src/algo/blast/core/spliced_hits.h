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


/* Create HSPContainer and take ownership of the HSP */
HSPContainer* HSPContainerNew(BlastHSP** hsp);

/* Free the list of HSPs, along with the stored HSPs */
HSPContainer* HSPContainerFree(HSPContainer* hc);

/* Clone a list of HSP containers */
HSPContainer* HSPContainerDup(HSPContainer* inh);

/* A chain of HSPs: spliced alignment */
typedef struct HSPChain
{
    Int4 context;  /* query context */
    Int4 oid;      /* subject oid */
    Int4 score;    /* score for the whole chain */
    HSPContainer* hsps;  /* list of HSPs that belong to this chain */
    Int4 compartment;    /* compartment number for the chain (needed
                            for reporting results) */
    
    Int4 count;    /* number of chains with the same or larger score found
                      for the same query */
    struct HSPChain* pair;  /* pointer to the pair (for paired reads) */
    Uint1 pair_conf;        /* pair configuration */

    Int4 adapter;  /* adapter start position */
    Int4 polyA;    /* start of polyA tail */
    struct HSPChain* next;
} HSPChain;


HSPChain* HSPChainFree(HSPChain* chain_list);

HSPChain* HSPChainNew(Int4 context);

/* Clone a single HSP chain */
HSPChain* CloneChain(const HSPChain* chain);


#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__SPLICED_HITS__H */
