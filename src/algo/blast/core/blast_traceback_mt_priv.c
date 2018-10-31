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

/** @file blast_traceback_mt_priv.c
 *  Private interface to support the multi-threaded traceback
 */

#include <algo/blast/core/blast_setup.h>
#include "blast_traceback_mt_priv.h"
#include "blast_hspstream_mt_utils.h"

SThreadLocalData* SThreadLocalDataNew()
{
    return (SThreadLocalData*)calloc(1, sizeof(SThreadLocalData));
}

SThreadLocalData* SThreadLocalDataFree(SThreadLocalData* tld)
{
    if (tld) {
        /* Do not destruct score block here, it was just assigned */
        if (tld->gap_align != NULL) {
            tld->gap_align->sbp = NULL;
        }
        tld->gap_align = BLAST_GapAlignStructFree(tld->gap_align);
        tld->score_params = BlastScoringParametersFree(tld->score_params);
        tld->ext_params = BlastExtensionParametersFree(tld->ext_params);
        tld->hit_params = BlastHitSavingParametersFree(tld->hit_params);
        tld->eff_len_params = BlastEffectiveLengthsParametersFree(tld->eff_len_params);
        tld->query_info = BlastQueryInfoFree(tld->query_info);
        tld->seqsrc = BlastSeqSrcFree(tld->seqsrc);
        tld->results = Blast_HSPResultsFree(tld->results);
        sfree(tld);
    }
    return NULL;
}

void SThreadLocalDataArrayTrim(SThreadLocalDataArray* array, Uint4 actual_num_threads)
{
    Uint4 i = 0;

    if (!array)
        return;

    ASSERT(actual_num_threads < array->num_elems);
    for (i = actual_num_threads; i < array->num_elems; i++) {
        array->tld[i] = SThreadLocalDataFree(array->tld[i]);
    }
    array->num_elems = actual_num_threads;
}

SThreadLocalDataArray* SThreadLocalDataArrayNew(Uint4 num_threads)
{
    SThreadLocalDataArray* retval = NULL;
    Uint4 i = 0;
    if ( !(retval = (SThreadLocalDataArray*)malloc(sizeof(SThreadLocalDataArray)))) {
        return NULL;
    }
    retval->num_elems = num_threads;
    if ( !(retval->tld = (SThreadLocalData**)calloc(num_threads, sizeof(*retval->tld)))) {
        return SThreadLocalDataArrayFree(retval);
    }
    for (i = 0; i < retval->num_elems; i++) {
        retval->tld[i] = SThreadLocalDataNew();
        if ( !retval->tld[i] ) {
            return SThreadLocalDataArrayFree(retval);
        }
    }
    return retval;
}

SThreadLocalDataArray* SThreadLocalDataArrayFree(SThreadLocalDataArray* array)
{
    if (array) {
        if (array->tld) {
            Uint4 i;
            for (i = 0; i < array->num_elems; i++) {
                array->tld[i] = SThreadLocalDataFree(array->tld[i]);
            }
            sfree(array->tld);
        }
        sfree(array);
    }
    return NULL;
}

static Uint4* s_CountHspListsPerQuery(const SThreadLocalDataArray* data, Int4* num_queries)
{
    Uint4 i = 0;
    Uint4* retval = NULL;

    if (!data || !num_queries)
        return retval;
    *num_queries = data->tld[0]->results->num_queries;

    if ( !(retval = (Uint4*)calloc(*num_queries, sizeof(*retval))) ) {
        return NULL;
    }

    for (i = 0; i < data->num_elems; i++) {
        Int4 query_idx;
        const BlastHSPResults* thread_results = data->tld[i]->results;
        ASSERT(*num_queries == thread_results->num_queries);
        for (query_idx = 0; query_idx < *num_queries; query_idx++) {
            const BlastHitList* hitlist = thread_results->hitlist_array[query_idx];
            if (hitlist) {
                retval[query_idx] += hitlist->hsplist_count;
            }
        }

    }
    return retval;
}

BlastHSPResults* SThreadLocalDataArrayConsolidateResults(SThreadLocalDataArray* array)
{
    BlastHSPResults* retval = NULL;
    Int4 num_queries = 0, query_idx = 0, hitlist_size = 0;
    Uint4* num_hsplists_per_query = NULL;

    if ( !array ) {
        return retval;
    }

    num_hsplists_per_query = s_CountHspListsPerQuery(array, &num_queries);
    if ( !(retval = Blast_HSPResultsNew(num_queries)) ) {
        sfree(num_hsplists_per_query);
        return retval;
    }
    hitlist_size = array->tld[0]->hit_params->options->hitlist_size;

    for (query_idx = 0; query_idx < num_queries; query_idx++) {
        Uint4 tid = 0;
        BlastHitList* hits4query = retval->hitlist_array[query_idx] =
            Blast_HitListNew(hitlist_size);
        if ( !hits4query ) {
            retval = Blast_HSPResultsFree(retval);
            break;
        }

        hits4query->hsplist_array = (BlastHSPList**)
            calloc(num_hsplists_per_query[query_idx], sizeof(BlastHSPList*));
        if ( !hits4query->hsplist_array ) {
            retval = Blast_HSPResultsFree(retval);
            break;
        }

        /* Consolidate the results for query_idx from all threads */
        for (tid = 0; tid < array->num_elems; tid++) {
            BlastHSPResults* thread_results = array->tld[tid]->results;
            BlastHitList* thread_hitlist = thread_results->hitlist_array[query_idx];
            Int4 i;

            if ( !thread_hitlist ) {
                continue;
            }
            /* transfer the BlastHSPList to the consolidated structure */
            for (i = 0; i < thread_hitlist->hsplist_count; i++) {
                if ( !Blast_HSPList_IsEmpty(thread_hitlist->hsplist_array[i])) {
                    hits4query->hsplist_array[hits4query->hsplist_count++] =
                        thread_hitlist->hsplist_array[i];
                    thread_hitlist->hsplist_array[i] = NULL;
                }
            }
            hits4query->worst_evalue = !tid
                ? thread_hitlist->worst_evalue
                : MAX(thread_hitlist->worst_evalue, hits4query->worst_evalue);
            hits4query->low_score = !tid
                ? thread_hitlist->low_score
                : MIN(thread_hitlist->low_score, hits4query->low_score);
        }
    }

    sfree(num_hsplists_per_query);
    return retval;
}

Int2 SThreadLocalDataArraySetup(SThreadLocalDataArray* array,
                                EBlastProgramType program,
                                const BlastScoringOptions* score_options,
                                const BlastEffectiveLengthsOptions* eff_len_options,
                                const BlastExtensionOptions* ext_options,
                                const BlastHitSavingOptions* hit_options,
                                BlastQueryInfo* query_info,
                                BlastScoreBlk* sbp,
                                const BlastSeqSrc* seqsrc)
{
    Uint4 i;
    Int2 status = 0;

    if (!array)
        return BLASTERR_INVALIDPARAM;

    for (i = 0; i < array->num_elems; i++) {
        status =
           BLAST_GapAlignSetUp(program, seqsrc, score_options, eff_len_options,
              ext_options, hit_options, query_info, sbp,
              &array->tld[i]->score_params,
              &array->tld[i]->ext_params,
              &array->tld[i]->hit_params,
              &array->tld[i]->eff_len_params,
              &array->tld[i]->gap_align);
        if (status)
           return status;
        if ( !(array->tld[i]->query_info = BlastQueryInfoDup(query_info)) ) {
            return BLASTERR_MEMORY;
        }
        if ( !(array->tld[i]->seqsrc = BlastSeqSrcCopy(seqsrc)) ) {
            return BLASTERR_MEMORY;
        }
        if ( !(array->tld[i]->results = Blast_HSPResultsNew(query_info->num_queries)) ) {
            return BLASTERR_MEMORY;
        }
    }
    return 0;
}

