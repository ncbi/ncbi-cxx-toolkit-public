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
#include <sstream>

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

/// Returns the optimal chunk size for a given task
/// @param program BLAST task [in]
size_t
SplitQuery_GetChunkSize(EProgram program);

/// Calculate the number of chunks that a query will be split into
/// @param program BLAST program type [in]
/// @param chunk_size size of each of the query chunks, may be adjusted [in|out]
/// @param concatenated_query_length length of the concatenated query [in]
/// @param num_queries number of queries to split [in]
Uint4 
SplitQuery_CalculateNumChunks(EBlastProgramType program,
                              size_t *chunk_size, 
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
    /// Constructor
    /// @param sqb Split query block structure [in]
    /// @param query_chunk_factories query factories corresponding to each of
    /// the chunks needed to report unit testing data (optional) [in]
    /// @param options BLAST options, also needed to report unit test data
    /// (optional) [in]
    CContextTranslator(const CSplitQueryBlk& sqb,
               vector< CRef<IQueryFactory> >* query_chunk_factories = NULL,
               const CBlastOptions* options = NULL);

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

    /// Print this object so that its contents can be directly used to update
    /// split_query.ini (for unit testing)
    /// @param out stream to print this object [in|out]
    /// @param rhs object to print [in]
    friend ostream& operator<<(ostream& out, const CContextTranslator& rhs);

private:
    /// Each element in this vector represents a chunk, and it contains the
    /// contexts numbers that correspond in the full concatenated query
    vector< vector<int> > m_ContextsPerChunk;

    vector< vector<int> > m_StartingChunks;
    vector< vector<int> > m_AbsoluteContexts;
};

/// Auxiliary class to determine information about the query that was split
/// into chunks.
class CQueryDataPerChunk {
public:
    /** 
     * @brief Constructor
     * 
     * @param sqb Split query block structure [in]
     * @param program BLAST program type [in]
     * @param local_query_data source of query data [in]
     */
    CQueryDataPerChunk(const CSplitQueryBlk& sqb,
                       EBlastProgramType program,
                       CRef<ILocalQueryData> local_query_data);

    /** 
     * @brief Get the length of the query
     * 
     * @param chunk_num chunk number where query is found [in]
     * @param context_in_chunk which context within this chunk contains query
     * [in]
     * 
     * @return length of query
     */
    size_t GetQueryLength(size_t chunk_num, int context_in_chunk) const;
    /** 
     * @brief Get the length of the query
     * 
     * @param global_query_index index of the query in the context of the
     * full,non-split query [in]
     * 
     * @return length of query
     */
    size_t GetQueryLength(int global_query_index) const;

    /** 
     * @brief get the last chunk where query identified with global_query_index
     * is found
     * 
     * @param global_query_index index of the query in the context of the
     * full,non-split query [in]
     * 
     * @return chunk number where query is last found
     */
    int GetLastChunk(int global_query_index);
    /** 
     * @brief get the last chunk where query identified with global_query_index
     * is found
     * 
     * @param chunk_num chunk number where query is found [in]
     * @param context_in_chunk which context within this chunk contains query
     * [in]
     * 
     * @return chunk number where query is last found
     */
    int GetLastChunk(size_t chunk_num, int context_in_chunk);

private:
    /** 
     * @brief Convert a context in a chunk to a query index (within the chunk)
     * 
     * @param context_in_chunk context number [in]
     * 
     * @return query index
     */
    size_t x_ContextInChunkToQueryIndex(int context_in_chunk) const;

    /// BLAST program type
    EBlastProgramType        m_Program;

    /// Each element in this vector represents a chunk, and it contains the
    /// query indices that correspond in the full concatenated query
    vector< vector<size_t> > m_QueryIndicesPerChunk;

    /// Lengths of the queries
    vector<size_t>           m_QueryLengths;

    /// Lists the last chunk where the query can be found
    vector<int>              m_LastChunkForQueryCache;
    /// Initial value of all entries in the above cache
    enum { kUninitialized = -1 };
};

/// Auxiliary function to print a vector
/// @param data2print vector to print [in]
template <class T>
string s_PrintVector(const vector<T>& data2print) 
{
    ostringstream os;

    if (data2print.empty()) {
        return kEmptyStr;
    }

    os << data2print.front();
    for (size_t i = 1; i < data2print.size(); i++) {
        os << ", " << data2print[i];
    }
    return os.str();
}

END_SCOPE(BLAST)
END_NCBI_SCOPE

/* @} */

#endif /* ALGO_BLAST_API__SPLIT_QUERY_AUX_PRIV__HPP */

