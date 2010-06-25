#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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

/** @file split_query_blk.cpp
 * Implementation of C++ wrapper for SSplitQueryBlk
 */

#include <ncbi_pch.hpp>
#include "split_query_blk.hpp"
#include <algo/blast/core/blast_def.h>      // for sfree
#include <algo/blast/api/blast_exception.hpp>
#include "split_query_aux_priv.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CSplitQueryBlk::CSplitQueryBlk(Uint4 num_chunks, bool gapped_merge)
{
    if ( !(m_SplitQueryBlk = SplitQueryBlkNew(num_chunks, gapped_merge))) {
        NCBI_THROW(CBlastSystemException, eOutOfMemory, "SplitQueryBlkNew");
    }
}

CSplitQueryBlk::~CSplitQueryBlk()
{
    m_SplitQueryBlk = SplitQueryBlkFree(m_SplitQueryBlk);
}

size_t 
CSplitQueryBlk::GetNumChunks() const 
{
    return m_SplitQueryBlk->num_chunks;
}

size_t 
CSplitQueryBlk::GetNumQueriesForChunk(size_t chunk_num) const 
{
    size_t retval = 0;
    Int2 rv = SplitQueryBlk_GetNumQueriesForChunk(m_SplitQueryBlk,
                                                  chunk_num,
                                                  &retval);
    if (rv != 0) {
        throw runtime_error("SplitQueryBlk_GetNumQueriesForChunk");
    }
    return retval;
}

vector<size_t> 
CSplitQueryBlk::GetQueryIndices(size_t chunk_num) const 
{
    vector<size_t> retval;
    Uint4* query_indices = NULL;
    Int2 rv = SplitQueryBlk_GetQueryIndicesForChunk(m_SplitQueryBlk,
                                                    chunk_num,
                                                    &query_indices);
    if (rv != 0) {
        throw runtime_error("SplitQueryBlk_GetQueryIndicesForChunk");
    }
    for (int i = 0; query_indices[i] != UINT4_MAX; i++) {
        retval.push_back(query_indices[i]);
    }
    sfree(query_indices);
    return retval;
}

vector<int> 
CSplitQueryBlk::GetQueryContexts(size_t chunk_num) const 
{
    vector<int> retval;
    Int4* query_contexts = NULL;
    Uint4 num_contexts = 0;
    Int2 rv = SplitQueryBlk_GetQueryContextsForChunk(m_SplitQueryBlk,
                                                     chunk_num,
                                                     &query_contexts,
                                                     &num_contexts);
    if (rv != 0) {
        throw runtime_error("SplitQueryBlk_GetQueryContextsForChunk");
    }
    for (Uint4 i = 0; i < num_contexts; i++) {
        retval.push_back(query_contexts[i]);
    }
    sfree(query_contexts);
    return retval;
}

vector<size_t> 
CSplitQueryBlk::GetContextOffsets(size_t chunk_num) const 
{
    vector<size_t> retval;
    Uint4* context_offsets = NULL;
    Int2 rv = SplitQueryBlk_GetContextOffsetsForChunk(m_SplitQueryBlk,
                                                      chunk_num,
                                                      &context_offsets);
    if (rv != 0) {
        throw runtime_error("SplitQueryBlk_GetContextOffsetsForChunk");
    }
    for (int i = 0; context_offsets[i] != UINT4_MAX; i++) {
        retval.push_back(context_offsets[i]);
    }
    sfree(context_offsets);
    return retval;
}

TChunkRange 
CSplitQueryBlk::GetChunkBounds(size_t chunk_num) const 
{
    TChunkRange retval;
    pair<size_t, size_t> chunk_bounds(0, 0);
    Int2 rv = SplitQueryBlk_GetChunkBounds(m_SplitQueryBlk, chunk_num, 
                                           &chunk_bounds.first, 
                                           &chunk_bounds.second);
    if (rv != 0) {
        throw runtime_error("SplitQueryBlk_GetChunkBounds");
    }
    retval.SetOpen(chunk_bounds.first, chunk_bounds.second);
    return retval;
}

void 
CSplitQueryBlk::SetChunkBounds(size_t chunk_num, 
                               const TChunkRange& chunk_range) 
{
    Int2 rv = SplitQueryBlk_SetChunkBounds(m_SplitQueryBlk,
                                           chunk_num,
                                           chunk_range.GetFrom(),
                                           chunk_range.GetToOpen());
    if (rv != 0) {
        throw runtime_error("SplitQueryBlk_SetChunkBounds");
    }
}

void 
CSplitQueryBlk::AddQueryToChunk(size_t chunk_num, Int4 query_index) 
{
    Int2 rv = SplitQueryBlk_AddQueryToChunk(m_SplitQueryBlk, query_index, 
                                            chunk_num);
    if (rv != 0) {
        throw runtime_error("Failed to add query to SplitQueryBlk");
    }
}

void 
CSplitQueryBlk::AddContextToChunk(size_t chunk_num, Int4 context_index) 
{
    Int2 rv = SplitQueryBlk_AddContextToChunk(m_SplitQueryBlk, 
                                              context_index, chunk_num);
    if (rv != 0) {
        throw runtime_error("Failed to add context to SplitQueryBlk");
    }
}

void 
CSplitQueryBlk::AddContextOffsetToChunk(size_t chunk_num, Int4 context_offset) 
{
    Int2 rv = SplitQueryBlk_AddContextOffsetToChunk(m_SplitQueryBlk, 
                                              context_offset, chunk_num);
    if (rv != 0) {
        throw runtime_error("Failed to add context offset to "
                            "SplitQueryBlk");
    }
}

SSplitQueryBlk* 
CSplitQueryBlk::GetCStruct() const
{
    return m_SplitQueryBlk;
}

void
CSplitQueryBlk::SetChunkOverlapSize(size_t size)
{
    Int2 rv = SplitQueryBlk_SetChunkOverlapSize(m_SplitQueryBlk, size);
    if (rv != 0) {
        throw runtime_error("Failed to set chunk overlap size in "
                            "SplitQueryBlk");
    }
}

size_t
CSplitQueryBlk::GetChunkOverlapSize() const
{
    size_t retval = SplitQueryBlk_GetChunkOverlapSize(m_SplitQueryBlk);
    if (retval == 0) {
        ERR_POST(Warning << "Query-splitting Chunk overlap size was not set");
    }
    return retval;
}

ostream& operator<<(ostream& out, const CSplitQueryBlk& rhs)
{
    const size_t kNumChunks = rhs.GetNumChunks();

    out << endl << "NumChunks = " << kNumChunks << endl;
    for (size_t chunk_num = 0; chunk_num < kNumChunks; chunk_num++) {
        out << "Chunk" << chunk_num << "Queries = " 
            << s_PrintVector(rhs.GetQueryIndices(chunk_num)) << endl;
    }
    out << endl;
    for (size_t chunk_num = 0; chunk_num < kNumChunks; chunk_num++) {
        out << "Chunk" << chunk_num << "Contexts = " 
            << s_PrintVector(rhs.GetQueryContexts(chunk_num)) << endl;
    }
    out << endl;
    for (size_t chunk_num = 0; chunk_num < kNumChunks; chunk_num++) {
        out << "Chunk" << chunk_num << "ContextOffsets = " 
            << s_PrintVector(rhs.GetContextOffsets(chunk_num)) << endl;
    }

    return out;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

