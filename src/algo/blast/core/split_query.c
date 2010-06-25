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
 * Author:  Christiam Camacho
 *
 */

/** @file split_query.c
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/split_query.h>
#include <algo/blast/core/blast_def.h>      /* needed for sfree */
#include "blast_dynarray.h"

const Int2 kBadParameter = -1;
const Int2 kOutOfMemory = -2;
const Int4 kInvalidContext = -1;

SSplitQueryBlk* 
SplitQueryBlkNew(Uint4 num_chunks, Boolean gapped_merge)
{
    SSplitQueryBlk* retval = NULL;

    if (num_chunks == 0) {
        return retval;
    }

    retval = (SSplitQueryBlk*) calloc(1, sizeof(SSplitQueryBlk));
    if ( !retval ) {
        return SplitQueryBlkFree(retval);
    }
    retval->num_chunks = num_chunks;
    retval->gapped_merge = gapped_merge;

    retval->chunk_query_map = 
        (SQueriesPerChunk**) calloc(num_chunks, sizeof(SQueriesPerChunk*));
    if ( !retval->chunk_query_map ) {
        return SplitQueryBlkFree(retval);
    }

    {{
        Uint4 i;
        for (i = 0; i < retval->num_chunks; i++) {
            retval->chunk_query_map[i] = DynamicUint4ArrayNew();
            if ( !retval->chunk_query_map[i] ) {
                return SplitQueryBlkFree(retval);
            }
        }
    }}

    retval->chunk_ctx_map = 
        (SContextsPerChunk**) calloc(num_chunks, sizeof(SContextsPerChunk*));
    if ( !retval->chunk_ctx_map ) {
        return SplitQueryBlkFree(retval);
    }

    {{
        Uint4 i;
        for (i = 0; i < retval->num_chunks; i++) {
            retval->chunk_ctx_map[i] = DynamicInt4ArrayNew();
            if ( !retval->chunk_ctx_map[i] ) {
                return SplitQueryBlkFree(retval);
            }
        }
    }}

    retval->chunk_offset_map = 
        (SContextOffsetsPerChunk**) calloc(num_chunks, 
                                           sizeof(SContextOffsetsPerChunk*));
    if ( !retval->chunk_offset_map ) {
        return SplitQueryBlkFree(retval);
    }

    {{
        Uint4 i;
        for (i = 0; i < retval->num_chunks; i++) {
            retval->chunk_offset_map[i] = DynamicUint4ArrayNew();
            if ( !retval->chunk_offset_map[i] ) {
                return SplitQueryBlkFree(retval);
            }
        }
    }}

    retval->chunk_bounds = 
        (SQueryChunkBoundary*) calloc(num_chunks, sizeof(SQueryChunkBoundary));
    if ( !retval->chunk_bounds ) {
        return SplitQueryBlkFree(retval);
    }

    return retval;
}

SSplitQueryBlk* 
SplitQueryBlkFree(SSplitQueryBlk* squery_blk)
{
    if ( !squery_blk ) {
        return NULL;
    }

    if (squery_blk->chunk_query_map) {
        Uint4 i;
        for (i = 0; i < squery_blk->num_chunks; i++) {
            DynamicUint4ArrayFree(squery_blk->chunk_query_map[i]);
        }
        sfree(squery_blk->chunk_query_map);
    }

    if (squery_blk->chunk_ctx_map) {
        Uint4 i;
        for (i = 0; i < squery_blk->num_chunks; i++) {
            DynamicInt4ArrayFree(squery_blk->chunk_ctx_map[i]);
        }
        sfree(squery_blk->chunk_ctx_map);
    }

    if (squery_blk->chunk_offset_map) {
        Uint4 i;
        for (i = 0; i < squery_blk->num_chunks; i++) {
            DynamicUint4ArrayFree(squery_blk->chunk_offset_map[i]);
        }
        sfree(squery_blk->chunk_offset_map);
    }

    if (squery_blk->chunk_bounds) {
        sfree(squery_blk->chunk_bounds);
    }

    sfree(squery_blk);
    return NULL;
}

Boolean SplitQueryBlk_AllowGap(SSplitQueryBlk* squery_blk) 
{
    if ( squery_blk && squery_blk->gapped_merge) return TRUE;
    return FALSE;
}

Int2 SplitQueryBlk_SetChunkBounds(SSplitQueryBlk* squery_blk,
                                  Uint4 chunk_num,
                                  Uint4 starting_offset,
                                  Uint4 ending_offset)

{
    if ( !squery_blk || chunk_num >= squery_blk->num_chunks) {
        return kBadParameter;
    }
    squery_blk->chunk_bounds[chunk_num].left = starting_offset;
    squery_blk->chunk_bounds[chunk_num].right = ending_offset;
    return 0;
}

Int2 SplitQueryBlk_GetChunkBounds(const SSplitQueryBlk* squery_blk,
                                  Uint4 chunk_num, 
                                  size_t* starting_offset, 
                                  size_t* ending_offset)
{
    if ( !squery_blk || !starting_offset || !ending_offset ||
         chunk_num >= squery_blk->num_chunks) {
        return kBadParameter;
    }
    *starting_offset = (size_t)squery_blk->chunk_bounds[chunk_num].left;
    *ending_offset = (size_t)squery_blk->chunk_bounds[chunk_num].right;
    return 0;
}

Int2
SplitQueryBlk_GetNumQueriesForChunk(const SSplitQueryBlk* squery_blk,
                                    Uint4 chunk_num,
                                    size_t* num_queries)
{
    if ( !squery_blk || !num_queries || chunk_num >= squery_blk->num_chunks) {
        return kBadParameter;
    }
    *num_queries = (size_t)squery_blk->chunk_query_map[chunk_num]->num_used;
    return 0;
}

Int2 
SplitQueryBlk_AddQueryToChunk(SSplitQueryBlk* squery_blk,
                              Uint4 query_index, Uint4 chunk_num)
{
    if ( !squery_blk || chunk_num >= squery_blk->num_chunks) {
        return kBadParameter;
    }
    return DynamicUint4Array_Append(squery_blk->chunk_query_map[chunk_num], 
                                    query_index);
}

Int2 SplitQueryBlk_AddContextToChunk(SSplitQueryBlk* squery_blk,
                                     Int4 ctx_index,
                                     Uint4 chunk_num)
{
    if ( !squery_blk || chunk_num >= squery_blk->num_chunks) {
        return kBadParameter;
    }
    return DynamicInt4Array_Append(squery_blk->chunk_ctx_map[chunk_num], 
                                   ctx_index);
}

Int2 SplitQueryBlk_AddContextOffsetToChunk(SSplitQueryBlk* squery_blk,
                                     Uint4 offset,
                                     Uint4 chunk_num)
{
    if ( !squery_blk || chunk_num >= squery_blk->num_chunks) {
        return kBadParameter;
    }
    return DynamicUint4Array_Append(
                            squery_blk->chunk_offset_map[chunk_num], 
                            offset);
}

Int2
SplitQueryBlk_GetQueryIndicesForChunk(const SSplitQueryBlk* squery_blk, 
                                      Uint4 chunk_num,
                                      Uint4** query_indices)
{
    SQueriesPerChunk* queries_per_chunk = NULL;
    Uint4* retval = NULL;

    if ( !squery_blk || chunk_num >= squery_blk->num_chunks || !query_indices) {
        return kBadParameter;
    }

    *query_indices = NULL;
    queries_per_chunk = squery_blk->chunk_query_map[chunk_num];

    /* Prepare the return value */
    retval = (Uint4*) malloc((queries_per_chunk->num_used + 1) * sizeof(Uint4));
    if ( !retval ) {
        return kOutOfMemory;
    }
    memcpy((void*) retval, (void*) queries_per_chunk->data, 
           queries_per_chunk->num_used*sizeof(*retval));
    retval[queries_per_chunk->num_used] = UINT4_MAX;
    *query_indices = retval;
    return 0;
}

Int2
SplitQueryBlk_GetQueryContextsForChunk(const SSplitQueryBlk* squery_blk, 
                                       Uint4 chunk_num,
                                       Int4** query_contexts,
                                       Uint4* num_query_contexts)
{
    SContextsPerChunk* contexts_per_chunk = NULL;
    Int4* retval = NULL;

    if ( !squery_blk || chunk_num >= squery_blk->num_chunks || 
         !query_contexts || !num_query_contexts)
    {
        return kBadParameter;
    }

    *query_contexts = NULL;
    contexts_per_chunk = squery_blk->chunk_ctx_map[chunk_num];

    /* Prepare the return value */
    *num_query_contexts = 0;
    retval = (Int4*) malloc(contexts_per_chunk->num_used * sizeof(Int4));
    if ( !retval ) {
        return kOutOfMemory;
    }
    memcpy((void*) retval, (void*) contexts_per_chunk->data, 
           contexts_per_chunk->num_used*sizeof(*retval));
    *num_query_contexts = contexts_per_chunk->num_used;
    *query_contexts = retval;
    return 0;
}

Int2
SplitQueryBlk_GetContextOffsetsForChunk(const SSplitQueryBlk* squery_blk, 
                                        Uint4 chunk_num,
                                        Uint4** context_offsets)
{
    SContextOffsetsPerChunk* offsets_per_chunk = NULL;
    Uint4* retval = NULL;

    if (!squery_blk || chunk_num >= squery_blk->num_chunks)
    {
        return kBadParameter;
    }

    *context_offsets = NULL;
    offsets_per_chunk = squery_blk->chunk_offset_map[chunk_num];

    /* Prepare the return value */
    retval = (Uint4*) malloc((offsets_per_chunk->num_used + 1) * 
                             sizeof(Uint4));
    if ( !retval ) {
        return kOutOfMemory;
    }
    memcpy((void*) retval, (void*) offsets_per_chunk->data, 
           offsets_per_chunk->num_used*sizeof(*retval));
    retval[offsets_per_chunk->num_used] = UINT4_MAX;
    *context_offsets = retval;
    return 0;
}

Int2
SplitQueryBlk_SetChunkOverlapSize(SSplitQueryBlk* squery_blk, size_t size)
{
    if ( !squery_blk ) {
        return kBadParameter;
    }
    squery_blk->chunk_overlap_sz = size;
    return 0;
}

size_t
SplitQueryBlk_GetChunkOverlapSize(const SSplitQueryBlk* squery_blk)
{
    if ( !squery_blk ) {
        return kBadParameter;
    }
    return squery_blk->chunk_overlap_sz;
}
