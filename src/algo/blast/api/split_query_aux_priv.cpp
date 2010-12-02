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

/** @file split_query_aux_priv.cpp
 * Auxiliary functions and classes to assist in query splitting
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/effsearchspace_calc.hpp>
#include "blast_setup.hpp"
#include "blast_aux_priv.hpp"
#include "split_query_aux_priv.hpp"

#include <objects/scoremat/PssmWithParameters.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

size_t
SplitQuery_GetOverlapChunkSize(EBlastProgramType program)
{
    size_t retval = 100;
    // used for experimentation purposes
    char* overlap_sz_str = getenv("OVERLAP_CHUNK_SIZE");
    if (overlap_sz_str && !NStr::IsBlank(overlap_sz_str)) {
        retval = NStr::StringToInt(overlap_sz_str);
        _TRACE("DEBUG: Using overlap chunk size " << retval);
        return retval;
    }

    if (Blast_QueryIsTranslated(program)) {
        // N.B.: this value must be divisible by 3 to work with translated
        // queries, as we split them in nucleotide coordinates and then do the
        // translation
        retval = 297;
    }
    _TRACE("Using overlap chunk size " << retval);
    return retval;
}

size_t
SplitQuery_GetChunkSize(EProgram program)
{
    size_t retval = 0;

    // used for experimentation purposes
    char* chunk_sz_str = getenv("CHUNK_SIZE");
    if (chunk_sz_str && !NStr::IsBlank(chunk_sz_str)) {
        retval = NStr::StringToInt(chunk_sz_str);
        _TRACE("DEBUG: Using query chunk size " << retval);
    } else {

        switch (program) {
        case eBlastn:
            retval = 1000000;
            break;
        case eMegablast:
        case eDiscMegablast:
            retval = 5000000;
            break;
        case eTblastn:
            retval = 20000;
            break;
        // if the query will be translated, round the chunk size up to the next
        // multiple of 3, that way, when the nucleotide sequence(s) get(s)
        // split, context N%6 in one chunk will have the same frame as context
        // N%6 in the next chunk
        case eBlastx:
        case eTblastx:
            // N.B.: the splitting is done on the nucleotide query sequences,
            // then each of these chunks is translated
            retval = 10002;
            break;
        case eBlastp:
        default:
            retval = 10000;
            break;
        }

        _TRACE("Using query chunk size " << retval);
    }

    const EBlastProgramType prog_type(EProgramToEBlastProgramType(program));
    if (Blast_QueryIsTranslated(prog_type) && !Blast_SubjectIsPssm(prog_type) &&
        (retval % CODON_LENGTH) != 0) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Split query chunk size must be divisible by 3");
    }

    return retval;
}

bool
SplitQuery_ShouldSplit(EBlastProgramType program,
                       size_t chunk_size,
                       size_t concatenated_query_length,
                       size_t num_queries)
{
    // TODO: need to model mem usage and when it's advantageous to split
    bool retval = true;

    // if ((concatenated_query_length <= chunk_size+SplitQuery_GetOverlapChunkSize(program)) ||
   //  if ((concatenated_query_length <= chunk_size) ||
        // do not split RPS-BLAST
    if (Blast_SubjectIsPssm(program) ||
        // the current implementation does NOT support splitting for multiple
        // blastx queries, loop over queries individually here...
        (program == eBlastTypeBlastx && num_queries > 1)) {
        retval = false;
    }

    return retval;
}

Uint4 
SplitQuery_CalculateNumChunks(EBlastProgramType program,
                              size_t *chunk_size, 
                              size_t concatenated_query_length,
                              size_t num_queries)
{
    if ( !SplitQuery_ShouldSplit(program, *chunk_size, 
                                 concatenated_query_length, num_queries)) {
        _TRACE("Not splitting queries");
        return 1;
    }

    size_t overlap_size = SplitQuery_GetOverlapChunkSize(program);
    Uint4 num_chunks = 0;

    _DEBUG_ARG(size_t target_chunk_size = *chunk_size);

    // For translated queries the chunk size should be divisible by CODON_LENGTH
    if (Blast_QueryIsTranslated(program)) {
        size_t chunk_size_delta = ((*chunk_size) % CODON_LENGTH);
        *chunk_size -= chunk_size_delta;
        _ASSERT((*chunk_size % CODON_LENGTH) == 0);
    }

    // Fix for small query size
    if ((*chunk_size) > overlap_size) {
       num_chunks = concatenated_query_length / ((*chunk_size) - overlap_size);
    }

    // Only one chunk, just return;
    if (num_chunks <= 1) {
       *chunk_size = concatenated_query_length;
       return 1;
    }

    // Re-adjust the chunk_size to make load even
    if (!Blast_QueryIsTranslated(program)) {
       *chunk_size = (concatenated_query_length + (num_chunks - 1) * overlap_size) / num_chunks;
       // Round up only if this will not decrease the number of chunks
       if (num_chunks < (*chunk_size) - overlap_size ) (*chunk_size)++;
    }

    _TRACE("Number of chunks: " << num_chunks << "; "
           "Target chunk size: " << target_chunk_size << "; "
           "Returned chunk size: " << *chunk_size);

    return num_chunks;
}


void
SplitQuery_SetEffectiveSearchSpace(CRef<CBlastOptions> options,
                                   CRef<IQueryFactory> full_query_fact,
                                   CRef<SInternalData> full_data)
{
    _ASSERT(full_data);
    _ASSERT(full_data->m_SeqSrc);

    // If the effective search options have been set, we don't need to
    // recompute those...
    if (options->GetEffectiveSearchSpace() != 0) {
        return;
    }

    const BlastSeqSrc* seqsrc = full_data->m_SeqSrc->GetPointer();
    Int8 total_length = BlastSeqSrcGetTotLenStats(seqsrc);
    if (total_length <= 0)
        total_length = BlastSeqSrcGetTotLen(seqsrc);
    Int4 num_seqs = BlastSeqSrcGetNumSeqsStats(seqsrc);
    if (num_seqs <= 0)
        num_seqs = BlastSeqSrcGetNumSeqs(seqsrc);

    CEffectiveSearchSpaceCalculator calc(full_query_fact, *options, 
                                         num_seqs, total_length, 
                                         full_data->m_ScoreBlk->GetPointer());
    BlastQueryInfo* qinfo = full_data->m_QueryInfo;
    _ASSERT(qinfo);

    vector<Int8> eff_searchsp;
    for (size_t index = 0; index <= (size_t)qinfo->last_context; index++) {
        eff_searchsp.push_back(calc.GetEffSearchSpaceForContext(index));
    }
    options->SetEffectiveSearchSpace(eff_searchsp);
}

CRef<SInternalData>
SplitQuery_CreateChunkData(CRef<IQueryFactory> qf,
                           CRef<CBlastOptions> options,
                           CRef<SInternalData> full_data,
                           bool is_multi_threaded /* = false */)
{
    BlastSeqSrc* seqsrc = 
        BlastSeqSrcCopy(full_data->m_SeqSrc->GetPointer());
    CRef<SBlastSetupData> setup_data = 
        BlastSetupPreliminarySearchEx(
                qf, options, 
                CRef<objects::CPssmWithParameters>(),
                seqsrc, is_multi_threaded);
    BlastSeqSrcResetChunkIterator(seqsrc);
    setup_data->m_InternalData->m_SeqSrc.Reset(new TBlastSeqSrc(seqsrc, 
                                               BlastSeqSrcFree));
    
    _ASSERT(setup_data->m_QuerySplitter->IsQuerySplit() == false);

    if (full_data->m_ProgressMonitor->Get()) {
        setup_data->m_InternalData->m_FnInterrupt = full_data->m_FnInterrupt;
        SBlastProgress* bp =
             SBlastProgressNew(full_data->m_ProgressMonitor->Get()->user_data);
        setup_data->m_InternalData->m_ProgressMonitor.Reset(new CSBlastProgress(bp));
    }
    return setup_data->m_InternalData;
}

CContextTranslator::CContextTranslator(const CSplitQueryBlk& sqb,
           vector< CRef<IQueryFactory> >* query_chunk_factories /* = NULL */,
           const CBlastOptions* options /* = NULL */)
{
    const size_t kNumChunks(sqb.GetNumChunks());
    m_ContextsPerChunk.reserve(kNumChunks);
    for (size_t i = 0; i < kNumChunks; i++) {
        m_ContextsPerChunk.push_back(sqb.GetQueryContexts(i));
    }

    if (query_chunk_factories == NULL || options == NULL) {
        return;
    }

    /// Populate the data to print out
    m_StartingChunks.resize(kNumChunks);
    m_AbsoluteContexts.resize(kNumChunks);
    for (size_t i = 0; i < kNumChunks; i++) {
        CRef<IQueryFactory> chunk_qf((*query_chunk_factories)[i]);
        CRef<ILocalQueryData> chunk_qd(chunk_qf->MakeLocalQueryData(options));
        BlastQueryInfo* chunk_qinfo = chunk_qd->GetQueryInfo();
        for (Int4 ctx = chunk_qinfo->first_context;
             ctx <= chunk_qinfo->last_context; ctx++) {
            m_StartingChunks[i].push_back(GetStartingChunk(i, ctx));
            m_AbsoluteContexts[i].push_back(GetAbsoluteContext(i, ctx));
        }
    }
}

int
CContextTranslator::GetAbsoluteContext(size_t chunk_num, 
                                       Int4 context_in_chunk) const
{
    _ASSERT(chunk_num < m_ContextsPerChunk.size());
    _ASSERT(context_in_chunk < (Int4)m_ContextsPerChunk[chunk_num].size());
    return m_ContextsPerChunk[chunk_num][context_in_chunk];
}

int
CContextTranslator::GetContextInChunk(size_t chunk_num,
                                      int absolute_context) const
{
    _ASSERT(chunk_num < m_ContextsPerChunk.size());
    const vector<int>& context_indices = m_ContextsPerChunk[chunk_num];
    vector<int>::const_iterator itr = find(context_indices.begin(),
                                           context_indices.end(),
                                           absolute_context);
    if (itr == context_indices.end()) {
        return kInvalidContext;
    }
    return itr - context_indices.begin();
}

int
CContextTranslator::GetStartingChunk(size_t curr_chunk, 
                                     Int4 context_in_chunk) const
{
    int absolute_context = GetAbsoluteContext(curr_chunk, context_in_chunk);
    if (absolute_context == kInvalidContext) {
        return kInvalidContext;
    }

    size_t retval = curr_chunk;

    for (--curr_chunk; static_cast<int>(curr_chunk) >= 0; --curr_chunk) {
        if (GetContextInChunk(curr_chunk, absolute_context) == 
            kInvalidContext) {
            break;
        }
        retval = curr_chunk;
    }
    return static_cast<int>(retval);
}

ostream& operator<<(ostream& out, const CContextTranslator& rhs)
{
    if (rhs.m_StartingChunks.front().empty() ||
        rhs.m_AbsoluteContexts.front().empty()) {
        return out;
    }

    const size_t kNumChunks = rhs.m_ContextsPerChunk.size();
    out << endl << "NumChunks = " << kNumChunks << endl;

    for (size_t i = 0; i < kNumChunks; i++) {
        out << "Chunk" << i << "StartingChunks = "
            << s_PrintVector(rhs.m_StartingChunks[i]) << endl;
    }
    out << endl;
    for (size_t i = 0; i < kNumChunks; i++) {
        out << "Chunk" << i << "AbsoluteContexts = "
            << s_PrintVector(rhs.m_AbsoluteContexts[i]) << endl;
    }
    out << endl;

    return out;
}

CQueryDataPerChunk::CQueryDataPerChunk(const CSplitQueryBlk& sqb,
                                       EBlastProgramType program,
                                       CRef<ILocalQueryData> local_query_data)
    : m_Program(program)
{
    const size_t kNumChunks(sqb.GetNumChunks());
    m_QueryIndicesPerChunk.reserve(kNumChunks);

    // unique list of query indices in global query
    set<size_t> global_query_indices;   

    for (size_t i = 0; i < kNumChunks; i++) {
        m_QueryIndicesPerChunk.push_back(sqb.GetQueryIndices(i));
        const vector<size_t>& query_indices = m_QueryIndicesPerChunk.back();
        ITERATE(vector<size_t>, itr, query_indices) {
            global_query_indices.insert(*itr);
        }
    }

    m_QueryLengths.reserve(global_query_indices.size());
    ITERATE(set<size_t>, itr, global_query_indices) {
        m_QueryLengths.push_back(local_query_data->GetSeqLength(*itr));
    }
    
    m_LastChunkForQueryCache.assign(m_QueryLengths.size(), kUninitialized);
}

size_t
CQueryDataPerChunk::GetQueryLength(int global_query_index) const
{
    _ASSERT(global_query_index < (int)m_QueryLengths.size());
    return m_QueryLengths[global_query_index];
}

size_t
CQueryDataPerChunk::GetQueryLength(size_t chunk_num, int context_in_chunk) const
{
    _ASSERT(chunk_num < m_QueryIndicesPerChunk.size());
    size_t pos = x_ContextInChunkToQueryIndex(context_in_chunk);
    _ASSERT(pos < m_QueryIndicesPerChunk[chunk_num].size());
    return GetQueryLength(m_QueryIndicesPerChunk[chunk_num][pos]);
}

size_t
CQueryDataPerChunk::x_ContextInChunkToQueryIndex(int context_in_chunk) const
{
    Int4 retval = Blast_GetQueryIndexFromContext(context_in_chunk, m_Program);
    _ASSERT(retval != -1);
    return static_cast<size_t>(retval);
}

int
CQueryDataPerChunk::GetLastChunk(size_t chunk_num, int context_in_chunk)
{
    _ASSERT(chunk_num < m_QueryIndicesPerChunk.size());
    size_t pos = x_ContextInChunkToQueryIndex(context_in_chunk);
    _ASSERT(pos < m_QueryIndicesPerChunk[chunk_num].size());
    return GetLastChunk(m_QueryIndicesPerChunk[chunk_num][pos]);
}

int
CQueryDataPerChunk::GetLastChunk(int global_query_index)
{
    bool found = false;
    int retval = m_LastChunkForQueryCache[global_query_index];

    if (retval != kUninitialized) {
        return retval;
    }

    for (size_t i = 0; i < m_QueryIndicesPerChunk.size(); i++) {
        vector<size_t>::const_iterator itr = 
            find(m_QueryIndicesPerChunk[i].begin(), 
                 m_QueryIndicesPerChunk[i].end(), 
                 (size_t)global_query_index);
        if (itr == m_QueryIndicesPerChunk[i].end()) {
            if (found) { 
                break;
            } else {
                continue;
            }
        }
        found = true;
        retval = static_cast<int>(i);
    }

    if ( !found ) {
        return -1;
    }
    m_LastChunkForQueryCache[global_query_index] = retval;
    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

