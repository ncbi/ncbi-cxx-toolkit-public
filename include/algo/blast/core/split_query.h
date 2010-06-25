#ifndef ALGO_BLAST_CORE__SPLIT_QUERY_H
#define ALGO_BLAST_CORE__SPLIT_QUERY_H

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

/** @file split_query.h
 */

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Forward declaration of dynamic array */
typedef struct SDynamicUint4Array SQueriesPerChunk;
/** Forward declaration of dynamic array */
typedef struct SDynamicInt4Array SContextsPerChunk;
/** Forward declaration of dynamic array */
typedef struct SDynamicUint4Array SContextOffsetsPerChunk;
/** Convenience typedef */
typedef struct SSeqRange SQueryChunkBoundary;

/** Structure to keep track of which query sequences are allocated to each
 * query chunk */
typedef struct SSplitQueryBlk {
    Uint4 num_chunks;       /**< Total number of chunks */
    SQueriesPerChunk** chunk_query_map; /**< Mapping of chunk number->query
                                          indices */
    SContextsPerChunk** chunk_ctx_map;  /**< Mapping of chunk number->context
                                          number */
    SContextOffsetsPerChunk** chunk_offset_map;/**< Mapping of chunk 
                                                 number->context offsets for
                                                 each of the contexts in that
                                                 chunk (used to correct HSP
                                                 ranges) */
    SQueryChunkBoundary* chunk_bounds;  /**< This chunk's boundaries */
    size_t chunk_overlap_sz;            /**< Size (# of bases/residues) of
                                          overlap between query chunks */
    Boolean gapped_merge;   /**< Allows merging HSPs with gap */
} SSplitQueryBlk;

/** Allocate a new split query chunk structure
 * @param num_chunks number of chunks in which the query will be split [in]
 * @return newly allocated structure or NULL in case of memory allocation
 * failure
 */
NCBI_XBLAST_EXPORT
SSplitQueryBlk* SplitQueryBlkNew(Uint4 num_chunks, Boolean gapped_merge);

/** Deallocate a split query chunk structure
 * @param squery_blk structure to deallocate [in]
 * @return NULL
 */
NCBI_XBLAST_EXPORT
SSplitQueryBlk* SplitQueryBlkFree(SSplitQueryBlk* squery_blk);

/** Set the query chunk's bounds with respect to the full sized concatenated 
 * query 
 * @param squery_blk split query block structure [in]
 * @param chunk_num number of chunk to assign bounds [in]
 * @param starting_offset starting offset of this chunk [in]
 * @param ending_offset ending offset of this chunk [in]
 * @return 0 on success or kBadParameter if chunk_num is invalid
 */
NCBI_XBLAST_EXPORT
Int2 SplitQueryBlk_SetChunkBounds(SSplitQueryBlk* squery_blk,
                                  Uint4 chunk_num,
                                  Uint4 starting_offset,
                                  Uint4 ending_offset);

/** Get the query chunk's bounds with respect to the full sized concatenated 
 * query 
 * @param squery_blk split query block structure [in]
 * @param chunk_num number of chunk to assign bounds [in]
 * @param starting_offset starting offset of this chunk [in|out]
 * @param ending_offset ending offset of this chunk [in|out]
 * @return 0 on success or kBadParameter if chunk_num is invalid
 */
NCBI_XBLAST_EXPORT
Int2 SplitQueryBlk_GetChunkBounds(const SSplitQueryBlk* squery_blk,
                                  Uint4 chunk_num, 
                                  size_t* starting_offset, 
                                  size_t* ending_offset);

/** Add a query index to a given chunk
 * @param squery_blk split query block structure [in]
 * @param query_index query index to assign [in]
 * @param chunk_num number of chunk to assign query index [in]
 * @return 0 on success or kOutOfMemory
 */
NCBI_XBLAST_EXPORT
Int2 SplitQueryBlk_AddQueryToChunk(SSplitQueryBlk* squery_blk,
                                   Uint4 query_index,
                                   Uint4 chunk_num);

/** Add a query context index to a given chunk
 * @param squery_blk split query block structure [in]
 * @param ctx_index query context index to assign [in]
 * @param chunk_num number of chunk to assign query index [in]
 * @return 0 on success or kOutOfMemory
 */
NCBI_XBLAST_EXPORT
Int2 SplitQueryBlk_AddContextToChunk(SSplitQueryBlk* squery_blk,
                                     Int4 ctx_index,
                                     Uint4 chunk_num);

/** Add a context offset to a given chunk
 * @param squery_blk split query block structure [in]
 * @param offset the context offset to assign [in]
 * @param chunk_num number of chunk to assign query index [in]
 * @return 0 on success or kOutOfMemory
 */
NCBI_XBLAST_EXPORT
Int2 SplitQueryBlk_AddContextOffsetToChunk(SSplitQueryBlk* squery_blk,
                                           Uint4 offset,
                                           Uint4 chunk_num);

/** Retrieve the number of queries that correspond to chunk number chunk_num
 * @param squery_blk split query block structure [in]
 * @param chunk_num number of chunk to retrieve query indices [in]
 * @param num_queries the return value of this function [in|out]
 * @return 0 on success or kBadParameter
 */
NCBI_XBLAST_EXPORT
Int2
SplitQueryBlk_GetNumQueriesForChunk(const SSplitQueryBlk* squery_blk, 
                                    Uint4 chunk_num,
                                    size_t* num_queries);

/** Retrieve an array of query indices for the requested chunk
 * @param squery_blk split query block structure [in]
 * @param chunk_num number of chunk to retrieve query indices [in]
 * @param query_indices indices for the chunk requested, this will be allocated
 * in this function and the last element in the array will be assigned the
 * value UINT4_MAX. Caller is responsible for deallocating this.
 * @return 0 on success or kOutOfMemory
 */
NCBI_XBLAST_EXPORT
Int2
SplitQueryBlk_GetQueryIndicesForChunk(const SSplitQueryBlk* squery_blk, 
                                      Uint4 chunk_num,
                                      Uint4** query_indices);

/** Value to represent an invalid context */
NCBI_XBLAST_EXPORT extern const Int4 kInvalidContext;

/** Retrieve an array of query contexts for the requested chunk
 * @param squery_blk split query block structure [in]
 * @param chunk_num number of chunk to retrieve query indices [in]
 * @param query_contexts contexts for the chunk requested, this will be 
 * allocated in this function, caller is responsible for deallocating this.
 * @param num_query_contexts size of the returned array in query_contexts
 * @return 0 on success or kOutOfMemory
 */
NCBI_XBLAST_EXPORT
Int2
SplitQueryBlk_GetQueryContextsForChunk(const SSplitQueryBlk* squery_blk, 
                                       Uint4 chunk_num,
                                       Int4** query_contexts,
                                       Uint4* num_query_contexts);

/** Retrieve an array of context offsets for the requested chunk. Each
 * offset is the correction needed for the query range of HSPs found
 * in the specified chunk
 * @param squery_blk split query block structure [in]
 * @param chunk_num number of chunk to retrieve query indices [in]
 * @param context_offsets offsets for the contexts in the requested chunk, 
 *                   this will be allocated in this function and the last 
 *                   element in the array will be assigned the value UINT4_MAX.
 *                   Caller is responsible for deallocating this.
 * @return 0 on success or kOutOfMemory
 */
NCBI_XBLAST_EXPORT
Int2
SplitQueryBlk_GetContextOffsetsForChunk(const SSplitQueryBlk* squery_blk,
                                        Uint4 chunk_num,
                                        Uint4** context_offsets);

/** Sets the query chunk overlap size 
 * @param squery_blk split query block structure [in]
 * @param size size of the chunk overlap size [in]
 * @return 0 on success or kBadParameter is squery_blk is NULL
 */
NCBI_XBLAST_EXPORT
Int2
SplitQueryBlk_SetChunkOverlapSize(SSplitQueryBlk* squery_blk, size_t size);

/** Returns the query chunk overlap size 
 * @param squery_blk split query block structure [in]
 * @return chunk overlap size, 0 if value wasn't set or kBadParameter if
 * squery_blk is NULL
 */
NCBI_XBLAST_EXPORT
size_t
SplitQueryBlk_GetChunkOverlapSize(const SSplitQueryBlk* squery_blk);

/* Return values */

/** Invalid parameter used in a function call */
NCBI_XBLAST_EXPORT
extern const Int2 kBadParameter;
/** Failure due to out-of-memory condition */
NCBI_XBLAST_EXPORT
extern const Int2 kOutOfMemory;

#ifdef __cplusplus
}
#endif

#endif /* ALGO_BLAST_CORE__SPLIT_QUERY_H */
