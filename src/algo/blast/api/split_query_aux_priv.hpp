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
 * Author: Christiam Camacho
 *
 */

/** @file split_query_aux_priv.hpp
 * Auxiliary functions and classes to assist in query splitting
 */

#ifndef ALGO_BLAST_API__SPLIT_QUERY_AUX_PRIV_HPP
#define ALGO_BLAST_API__SPLIT_QUERY_AUX_PRIV_HPP

#include <corelib/ncbiobj.hpp>
#include <algo/blast/core/blast_query_info.h>
#include <algo/blast/api/split_query.hpp>
#include <algo/blast/api/query_data.hpp>
#include <algo/blast/api/setup_factory.hpp>
#include "split_query_blk.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

/// Determines if the input query sequence(s) should be split, whether because
/// it makes sense to do so or whether because it is supported by the current
/// implementation.
/// @param program BLAST program type [in]
/// @param chunk_size size of each of the query chunks [in]
/// @param concatenated_query_length length of the concatenated query [in]
/// @param num_queries number of queries to split [in]
bool
SplitQuery_ShouldSplit(EBlastProgramType program,
                       size_t chunk_size,
                       size_t concatenated_query_length,
                       size_t num_queries);

/// Size of the region that overlaps in between each query chunk
/// @param program BLAST program type [in]
size_t 
SplitQuery_GetOverlapChunkSize(EBlastProgramType program);

/// Returns the optimal chunk size for a given program
/// @param program BLAST program type [in]
size_t
SplitQuery_GetChunkSize(EBlastProgramType program);

/// Calculate the number of chunks that a query will be split into
/// @param program BLAST program type [in]
/// @param chunk_size size of each of the query chunks [in]
/// @param concatenated_query_length length of the concatenated query [in]
/// @param num_queries number of queries to split [in]
Uint4 
SplitQuery_CalculateNumChunks(EBlastProgramType program,
                              size_t chunk_size, 
                              size_t concatenated_query_length,
                              size_t num_queries);

/// Function used by search class to retrieve a query factory for a given chunk
CRef<SInternalData>
SplitQuery_CreateChunkData(CRef<IQueryFactory> qf,
                           CRef<CBlastOptions> options,
                           CRef<SInternalData> full_data,
                           bool is_multi_threaded = false);

/// this might supercede the function below...
void
SplitQuery_SetEffectiveSearchSpace(CRef<CBlastOptions> options,
                                   CRef<IQueryFactory> full_query_fact,
                                   CRef<SInternalData> full_data);


/** 
 * @brief Auxiliary class to provide convenient and efficient access to
 * conversions between contexts local to query split chunks and the absolute
 * (full, unsplit) query
 */
class CContextTranslator {
public:
    CContextTranslator(const CSplitQueryBlk& sqb);

    /** 
     * @brief Get the context number in the absolute (i.e.: unsplit) query
     * 
     * @param chunk_num Chunk number where the context is found in the split
     * query [in]
     * @param context_in_chunk Context in the split query [in]
     * 
     * @return the appropriate context, or if the context is invalid
     * kInvalidContext 
     */
    int GetAbsoluteContext(size_t chunk_num, Int4 context_in_chunk) const;
    
    /** 
     * @brief Get the context number in the split query chunk. This function is
     * basically doing the reverse lookup that GetAbsoluteContext does
     * 
     * @param chunk_num Chunk number to search for this context [in]
     * @param absolute_context context number in the absolute (i.e.: unsplit)
     * query [in]
     * 
     * @return the appropriate context if found, else kInvalidContext
     *
     * @sa GetAbsoluteContext
     */
    int GetContextInChunk(size_t chunk_num, int absolute_context) const;

    /** 
     * @brief Get the chunk number where context_in_chunk starts (i.e.:
     * location of its first chunk).
     * 
     * @param curr_chunk Chunk where the context_in_chunk is found [in]
     * @param context_in_chunk Context in the split query [in]
     * 
     * @return the appropriate chunk number or kInvalidContext if the context
     * is not valid in the query chunk (i.e.: strand not searched)
     */
    int GetStartingChunk(size_t curr_chunk, Int4 context_in_chunk) const;
private:
    vector< vector<int> > m_ContextsPerChunk;
};

/// Auxiliary class to determine information about the query that was split
/// into chunks.
class CQueryDataPerChunk {
public:
    CQueryDataPerChunk(const CSplitQueryBlk& sqb,
                       EBlastProgramType program,
                       CRef<ILocalQueryData> local_query_data);

    size_t GetQueryLength(size_t chunk_num, int context_in_chunk) const;
    size_t GetQueryLength(int global_query_index) const;

    int GetLastChunk(int global_query_index);
    int GetLastChunk(size_t chunk_num, int context_in_chunk);

private:
    size_t x_ContextInChunkToQueryIndex(int context_in_chunk) const;

    EBlastProgramType        m_Program;
    vector< vector<size_t> > m_QueryIndicesPerChunk;

    /// Lengths of the queries
    vector<size_t>           m_QueryLengths;

    /// Lists the last chunk where the query can be found
    vector<int>              m_LastChunkForQueryCache;
    /// Initial value of all entries in the above cache
    enum { kUninitialized = -1 };
};

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API__SPLIT_QUERY_AUX_PRIV__HPP */

