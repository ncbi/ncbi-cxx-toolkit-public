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
 * Implementaion of spliced alignment methods
 *
 */


#include <algo/blast/core/spliced_hits.h>


/* Create HSPContainer and take ownership of the HSP */
HSPContainer* HSPContainerNew(BlastHSP** hsp)
{
    HSPContainer* retval = calloc(1, sizeof(HSPContainer));
    if (!retval) {
        return NULL;
    }
    retval->hsp = *hsp;
    /* take ownership of this HSP */
    *hsp = NULL;

    return retval;
}

/* Free the list of HSPs, along with the stored HSPs */
HSPContainer* HSPContainerFree(HSPContainer* hc)
{
    HSPContainer* h = hc;
    while (h) {
        HSPContainer* next = h->next;
        if (h->hsp) {
            Blast_HSPFree(h->hsp);
        }

        sfree(h);
        h = next;
    }

    return NULL;
}

/* Clone a list of HSP containers */
HSPContainer* HSPContainerDup(HSPContainer* inh)
{
    HSPContainer* retval = NULL;
    HSPContainer* hi = NULL, *ho = NULL;
    BlastHSP* new_hsp = NULL;

    if (!inh || !inh->hsp) {
        return NULL;
    }

    new_hsp = Blast_HSPClone(inh->hsp);
    if (!new_hsp) {
        return NULL;
    }
    retval = HSPContainerNew(&new_hsp);
    if (!retval) {
        Blast_HSPFree(new_hsp);
        return NULL;
    }

    hi = inh->next;
    ho = retval;
    for (; hi; hi = hi->next, ho = ho->next) {
        new_hsp = Blast_HSPClone(hi->hsp);
        if (!new_hsp) {
            Blast_HSPFree(new_hsp);
            HSPContainerFree(retval);
            return NULL;
        }
        ho->next = HSPContainerNew(&new_hsp);
        if (!ho->next) {
            Blast_HSPFree(new_hsp);
            HSPContainerFree(retval);
            return NULL;
        }
    }

    return retval;
}


HSPChain* HSPChainFree(HSPChain* chain_list)
{
    HSPChain* chain = chain_list;
    while (chain) {
        HSPChain* next = chain->next;
        if (chain->pair) {
            chain->pair->pair = NULL;
        }
        ASSERT(chain->hsps);
        chain->hsps = HSPContainerFree(chain->hsps);
        sfree(chain);
        chain = next;
    }

    return NULL;
}

HSPChain* HSPChainNew(Int4 context)
{
    HSPChain* retval = calloc(1, sizeof(HSPChain));
    if (!retval) {
        return NULL;
    }
    retval->context = context;
    retval->adapter = -1;

    return retval;
}

/* Clone a single HSP chain */
HSPChain* CloneChain(const HSPChain* chain)
{
    HSPChain* retval = NULL;

    if (!chain) {
        return NULL;
    }
    
    retval = HSPChainNew(chain->context);
    if (!retval) {
        return NULL;
    }
    retval->hsps = HSPContainerDup(chain->hsps);
    if (!retval->hsps) {
        HSPChainFree(retval);
        return NULL;
    }
    retval->oid = chain->oid;
    retval->score = chain->score;
    retval->adapter = chain->adapter;
    retval->polyA = chain->polyA;

    return retval;
}

BlastMappingResults* Blast_MappingResultsNew(void)
{
    BlastMappingResults* retval = calloc(1, sizeof(BlastMappingResults));
    return retval;
}

BlastMappingResults* Blast_MappingResultsFree(BlastMappingResults* results)
{
    if (!results) {
        return NULL;
    }

    if (results->chain_array) {
        Int4 i;
        for (i = 0;i < results->num_queries;i++) {
            HSPChainFree(results->chain_array[i]);
        }
        sfree(results->chain_array);
    }
    sfree(results);

    return NULL;
}


