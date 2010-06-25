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

/** @file split_query_cxx.cpp
 * Defines CQuerySplitter, a class to split the query sequence(s)
 */

#include <ncbi_pch.hpp>
#include "split_query.hpp"
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/blast_options.hpp>

#include <objtools/simple/simple_om.hpp>
#include <objmgr/util/sequence.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>

#include "split_query_aux_priv.hpp"
#include "blast_setup.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CQuerySplitter::CQuerySplitter(CRef<IQueryFactory> query_factory,
                               const CBlastOptions* options)
    : m_QueryFactory(query_factory), m_Options(options), m_NumChunks(0),
    m_LocalQueryData(0), m_TotalQueryLength(0), m_ChunkSize(0)
{
    m_ChunkSize = SplitQuery_GetChunkSize(m_Options->GetProgram());
    m_LocalQueryData = m_QueryFactory->MakeLocalQueryData(m_Options);
    m_TotalQueryLength = m_LocalQueryData->GetSumOfSequenceLengths();
    m_NumChunks = SplitQuery_CalculateNumChunks(m_Options->GetProgramType(), 
        &m_ChunkSize, m_TotalQueryLength, m_LocalQueryData->GetNumQueries());
    x_ExtractCScopesAndMasks();
}

ostream& operator<<(ostream& out, const CQuerySplitter& rhs)
{
    ILocalQueryData* query_data = 
        const_cast<ILocalQueryData*>(&*rhs.m_LocalQueryData);
    const size_t kNumQueries = query_data->GetNumQueries();
    const size_t kNumChunks = rhs.GetNumberOfChunks();

    out << endl << "; This is read by x_ReadQueryBoundsPerChunk" 
        << endl << "; Format: query start, query end, strand" << endl;

    // for all query indices {
    //     iterate every chunk and collect the coords, print them out
    // }
    for (size_t query_index = 0; query_index < kNumQueries; query_index++) {
        CConstRef<CSeq_id> query_id
            (query_data->GetSeq_loc(query_index)->GetId());
        _ASSERT(query_id);
        
        for (size_t chunk_index = 0; chunk_index < kNumChunks; chunk_index++) {
            CRef<CBlastQueryVector> queries_in_chunk = 
                rhs.m_SplitQueriesInChunk[chunk_index];

            for (size_t qidx = 0; qidx < queries_in_chunk->Size(); qidx++) {
                CConstRef<CSeq_loc> query_loc_in_chunk =
                    queries_in_chunk->GetQuerySeqLoc(qidx);
                _ASSERT(query_loc_in_chunk);
                CConstRef<CSeq_id> query_id_in_chunk
                    (query_loc_in_chunk->GetId());
                _ASSERT(query_id_in_chunk);

                if (query_id->Match(*query_id_in_chunk)) {
                    const CSeq_loc::TRange& range = 
                        query_loc_in_chunk->GetTotalRange();
                    out << "Chunk" << chunk_index << "Query" << query_index
                        << " = " << range.GetFrom() << ", " 
                        << range.GetToOpen() << ", " 
                        << (int)query_loc_in_chunk->GetStrand() << endl;
                }
            }
        }
        out << endl;
    }

    return out;
}

void
CQuerySplitter::x_ExtractCScopesAndMasks()
{
    _ASSERT(m_LocalQueryData.NotEmpty());
    _ASSERT(m_Scopes.empty());
    _ASSERT(m_UserSpecifiedMasks.empty());

    const size_t num_queries = m_LocalQueryData->GetNumQueries();

    CObjMgr_QueryFactory* objmgr_qf = NULL;
    if ( (objmgr_qf = dynamic_cast<CObjMgr_QueryFactory*>(&*m_QueryFactory)) ) {
        // extract the scopes and masks ...
        m_Scopes = objmgr_qf->ExtractScopes();
        m_UserSpecifiedMasks = objmgr_qf->ExtractUserSpecifiedMasks();
        _ASSERT(m_Scopes.size() == num_queries);
    } else {
        m_NumChunks = 1;
        m_UserSpecifiedMasks.assign(num_queries, TMaskedQueryRegions());
    }
    _ASSERT(m_UserSpecifiedMasks.size() == num_queries);
}

void
CQuerySplitter::x_ComputeChunkRanges()
{
    _ASSERT(m_SplitBlk.NotEmpty());

    // Note that this information might not need to be stored in the
    // SSplitQueryBlk structure, as these ranges can be calculated as follows:
    // chunk_start = (chunk_num*chunk_size) - (chunk_num*overlap_size);
    // chunk_end = chunk_start + chunk_size > query_size ? query_size :
    // chunk_start + chunk_size;

    size_t chunk_start = 0;
    const size_t kOverlapSize =
        SplitQuery_GetOverlapChunkSize(m_Options->GetProgramType());
    for (size_t chunk_num = 0; chunk_num < m_NumChunks; chunk_num++) {
        size_t chunk_end = chunk_start + m_ChunkSize;

        // if the chunk end is larger than the sequence ...
        if (chunk_end >= m_TotalQueryLength ||
            // ... or this is the last chunk and it didn't make it to the end
            // of the sequence
            (chunk_end < m_TotalQueryLength && (chunk_num + 1) == m_NumChunks))
        {
            // ... assign this chunk's end to the end of the sequence
            chunk_end = m_TotalQueryLength;
        }

        m_SplitBlk->SetChunkBounds(chunk_num, 
                                   TChunkRange(chunk_start, chunk_end));
        _TRACE("Chunk " << chunk_num << ": ranges from " << chunk_start 
               << " to " << chunk_end);

        chunk_start += (m_ChunkSize - kOverlapSize);
        if (chunk_start > m_TotalQueryLength || 
            chunk_end == m_TotalQueryLength) {
            break;
        }
    }

    // For purposes of having an accurate overlap size when stitching back
    // HSPs, save the overlap size
    const size_t kOverlap = 
        Blast_QueryIsTranslated(m_Options->GetProgramType()) 
        ? kOverlapSize / CODON_LENGTH : kOverlapSize;
    m_SplitBlk->SetChunkOverlapSize(kOverlap);
}

/// Auxiliary function to assign the split query's Seq-interval so that it's
/// constrained within the chunk boundaries
/// @param chunk Range for the chunk [in]
/// @param query_range Range of sequence data corresponding to the full query
/// [in]
/// @param split_query_loc Seq-loc for this query constrained by the chunk's
/// boundaries [out]
static void
s_SetSplitQuerySeqInterval(const TChunkRange& chunk, 
                           const TChunkRange& query_range, 
                           CRef<CSeq_loc> split_query_loc)
{
    _ASSERT(split_query_loc.NotEmpty());
    _ASSERT(chunk.IntersectingWith(query_range));
    CSeq_interval& interval = split_query_loc->SetInt();
    const int qstart = chunk.GetFrom() - query_range.GetFrom();
    const int qend = chunk.GetToOpen() - query_range.GetToOpen();

    interval.SetFrom(max(0, qstart));
    if (qend >= 0) {
        interval.SetTo(query_range.GetToOpen() - query_range.GetFrom());
    } else {
        interval.SetTo(chunk.GetToOpen() - query_range.GetFrom());
    }
    // Note subtraction, as Seq-intervals are assumed to be
    // open/inclusive
    interval.SetTo() -= 1;
}

void
CQuerySplitter::x_ComputeQueryIndicesForChunks()
{
    const int kNumQueries = m_LocalQueryData->GetNumQueries();
    const EBlastProgramType kProgram = m_Options->GetProgramType();
    const ENa_strand kStrandOption = m_Options->GetStrandOption();

    // Build vector of query ranges along the concatenated query
    vector<TChunkRange> query_ranges;
    query_ranges.reserve(kNumQueries);
    query_ranges.push_back(TChunkRange(0, m_LocalQueryData->GetSeqLength(0)));
    _TRACE("Query 0: " << query_ranges.back().GetFrom() << "-" <<
           query_ranges.back().GetToOpen());
    for (int i = 1; i < kNumQueries; i++) {
        TSeqPos query_start = query_ranges[i-1].GetTo() + 1;
        TSeqPos query_end = query_start + m_LocalQueryData->GetSeqLength(i);
        query_ranges.push_back(TChunkRange(query_start, query_end));
        _TRACE("Query " << i << ": " << query_ranges.back().GetFrom() 
               << "-" << query_ranges.back().GetToOpen());
    }

    m_SplitQueriesInChunk.assign(m_NumChunks, CRef<CBlastQueryVector>());
    _ASSERT(m_UserSpecifiedMasks.size() == (size_t)kNumQueries);

    // determine intersection between query ranges and chunk ranges
    for (size_t chunk_num = 0; chunk_num < m_NumChunks; chunk_num++) {
        const TChunkRange chunk = m_SplitBlk->GetChunkBounds(chunk_num);
        // FIXME: can be optimized to avoid examining those that have already
        // been assigned to a given chunk
        for (size_t qindex = 0; qindex < query_ranges.size(); qindex++) {
            const TChunkRange& query_range = query_ranges[qindex];
            if ( !chunk.IntersectingWith(query_range) ) {
                continue;
            }
            m_SplitBlk->AddQueryToChunk(chunk_num, qindex);
            if (m_SplitQueriesInChunk[chunk_num].Empty()) {
                m_SplitQueriesInChunk[chunk_num].Reset(new CBlastQueryVector);
            }

            // Build the split query seqloc for this query
            CRef<CSeq_loc> split_query_loc(new CSeq_loc);
            s_SetSplitQuerySeqInterval(chunk, query_range, split_query_loc);

            CConstRef<CSeq_loc>
                query_seqloc(m_LocalQueryData->GetSeq_loc(qindex));
            split_query_loc->SetId(*query_seqloc->GetId());
            const ENa_strand kStrand = 
                BlastSetup_GetStrand(*query_seqloc, kProgram, kStrandOption);
            split_query_loc->SetStrand(kStrand);
            _TRACE("Chunk " << chunk_num << ": query " << qindex << " ("
                   << split_query_loc->GetId()->AsFastaString() << ")"
                   << " " << split_query_loc->GetInt().GetFrom()
                   << "-" << split_query_loc->GetInt().GetTo()
                   << " strand " << (int)split_query_loc->GetStrand());

            // retrieve the split mask corresponding to this chunk of the query
            TMaskedQueryRegions split_mask = 
                m_UserSpecifiedMasks[qindex].RestrictToSeqInt
                (split_query_loc->GetInt());

            // retrieve the scope to retrieve this query
            CRef<CScope> scope(m_Scopes[qindex]);
            _ASSERT(scope.NotEmpty());

            // our split query chunk :)
            CRef<CBlastSearchQuery> split_query
                (new CBlastSearchQuery(*split_query_loc, *scope, split_mask));
            m_SplitQueriesInChunk[chunk_num]->AddQuery(split_query);
        }
    }
}

/** Adds the necessary shift to the context to record the query contexts for
 * the query chunks
 * @param context query context [in]
 * @param shift shift to add [in]
 */
static inline unsigned int
s_AddShift(unsigned int context, int shift)
{
    _ASSERT(context == 3 || context == 4 || context == 5);
    _ASSERT(shift == 0 || shift == 1 || shift == -1);

    unsigned int retval;
    if (shift == 0) {
        retval = context;
    } else if (shift == 1) {
        retval = context == 3 ? 5 : context - shift;
    } else if (shift == -1) {
        retval = context == 5 ? 3 : context - shift;
    } else {
        abort();
    }
    return retval;
}

/** 
 * @brief Retrieve the shift for the negative strand
 * 
 * @param query_length length of the query [in]
 * 
 * @return shift (either 1, -1, or 0)
 */
static inline int
s_GetShiftForTranslatedNegStrand(size_t query_length)
{
    int retval;
    switch (query_length % CODON_LENGTH) {
    case 1: retval = -1; break;
    case 2: retval = 1; break;
    case 0: default: retval = 0; break;
    }
    return retval;
}

void
CQuerySplitter::x_ComputeQueryContextsForChunks()
{
    const EBlastProgramType kProgram = m_Options->GetProgramType();
    const unsigned int kNumContexts = GetNumberOfContexts(kProgram);
    const ENa_strand kStrandOption = m_Options->GetStrandOption();
    auto_ptr<CQueryDataPerChunk> qdpc;
    
    if (Blast_QueryIsTranslated(kProgram)) {
        qdpc.reset(new CQueryDataPerChunk(*m_SplitBlk, kProgram, 
                                          m_LocalQueryData));
    }

    for (size_t chunk_num = 0; chunk_num < m_NumChunks; chunk_num++) {
        vector<size_t> queries = m_SplitBlk->GetQueryIndices(chunk_num);

        for (size_t i = 0; i < queries.size(); i++) {
            CConstRef<CSeq_loc> sl = m_LocalQueryData->GetSeq_loc(queries[i]);
            const ENa_strand kStrand = 
                BlastSetup_GetStrand(*sl, kProgram, kStrandOption);

            if (Blast_QueryIsTranslated(kProgram)) {
                size_t qlength = qdpc->GetQueryLength(queries[i]);
                int last_query_chunk = qdpc->GetLastChunk(queries[i]);
                _ASSERT(last_query_chunk != -1);
                int shift = s_GetShiftForTranslatedNegStrand(qlength);

                for (unsigned int ctx = 0; ctx < kNumContexts; ctx++) {
                    // handle the plus strand...
                    if (ctx % NUM_FRAMES < CODON_LENGTH) {
                        if (kStrand == eNa_strand_minus) {
                            m_SplitBlk->AddContextToChunk(chunk_num,
                                                          kInvalidContext);
                        } else {
                            m_SplitBlk->AddContextToChunk(chunk_num, 
                                              kNumContexts*queries[i]+ctx);
                        }
                    } else { // handle the negative strand
                        if (kStrand == eNa_strand_plus) {
                            m_SplitBlk->AddContextToChunk(chunk_num,
                                                          kInvalidContext);
                        } else {
                            if (chunk_num == (size_t)last_query_chunk) {
                                // last chunk doesn't have shift
                                m_SplitBlk->AddContextToChunk(chunk_num,
                                          kNumContexts*queries[i]+ctx);
                            } else {
                                m_SplitBlk->AddContextToChunk(chunk_num,
                                          kNumContexts*queries[i]+
                                          s_AddShift(ctx, shift));
                            }
                        }
                    }
                }
            } else if (Blast_QueryIsNucleotide(kProgram)) {

                for (unsigned int ctx = 0; ctx < kNumContexts; ctx++) {
                    // handle the plus strand...
                    if (ctx % NUM_STRANDS == 0) {
                        if (kStrand == eNa_strand_minus) {
                            m_SplitBlk->AddContextToChunk(chunk_num,
                                                          kInvalidContext);
                        } else {
                            m_SplitBlk->AddContextToChunk(chunk_num, 
                                              kNumContexts*queries[i]+ctx);
                        }
                    } else { // handle the negative strand
                        if (kStrand == eNa_strand_plus) {
                            m_SplitBlk->AddContextToChunk(chunk_num,
                                                          kInvalidContext);
                        } else {
                            m_SplitBlk->AddContextToChunk(chunk_num, 
                                              kNumContexts*queries[i]+ctx);
                        }
                    }
                }

            } else if (Blast_QueryIsProtein(kProgram)) {
                m_SplitBlk->AddContextToChunk(chunk_num, 
                                              kNumContexts*queries[i]);
            } else {
                abort();
            }
        }
    }
}

/** 
 * @brief Determine whether a given context corresponds to the plus or minus
 * strand
 * 
 * @param qinfo BlastQueryInfo structure to determine the strand [in]
 * @param context_number Context number in the BlastQueryInfo structure (index
 * into the BlastContextInfo array) [in]
 */
static inline bool
s_IsPlusStrand(const BlastQueryInfo* qinfo, Int4 context_number)
{
    return qinfo->contexts[context_number].frame >= 0;
}

/** 
 * @brief Get the length of a context in absolute terms (i.e.: in the context
 * of the full, non-split sequence)
 * 
 * @param chunk_qinfo vector of BlastQueryInfo structures corresponding to the
 * various query chunks [in]
 * @param chunk_num Chunk number, index into the vector above [in]
 * @param ctx_translator auxiliary context translator object [in]
 * @param absolute_context context in the full, non-split query
 * 
 * @return length of the requested context
 */
static size_t
s_GetAbsoluteContextLength(const vector<const BlastQueryInfo*>& chunk_qinfo, 
                           int chunk_num,
                           const CContextTranslator& ctx_translator,
                           int absolute_context)
{
    if (chunk_num < 0) {
        return 0;
    }

    int pos = ctx_translator.GetContextInChunk((size_t)chunk_num,
                                               absolute_context);
    if (pos != kInvalidContext) {
        return chunk_qinfo[chunk_num]->contexts[pos].query_length;
    }
    return 0;
}

//#define DEBUG_COMPARE_SEQUENCES 1

#ifdef DEBUG_COMPARE_SEQUENCES

/// Convert a sequence into its printable representation
/// @param seq sequence data [in]
/// @param len length of sequence to print [in]
/// @param is_prot whether the sequence is protein or not [in]
static string s_GetPrintableSequence(const Uint1* seq, size_t len, bool is_prot)
{
    string retval;
    for (size_t i = 0; i < len; i++) {
        retval.append(1, (is_prot 
                      ? NCBISTDAA_TO_AMINOACID[seq[i]] 
                      : BLASTNA_TO_IUPACNA[seq[i]]));
    }
    return retval;
}

/** Auxiliary function to validate the context offset corrections 
 * @param global global query sequence data [in]
 * @param chunk sequence data for chunk [in]
 * @param len length of the data to compare [in] 
 * @param is_prot whether the sequence is protein or not [in]
 * @return true if sequence data is identical, false otherwise
 */
static bool cmp_sequence(const Uint1* global, const Uint1* chunk, size_t len,
                         bool is_prot)
{
    bool retval = true;

    for (size_t i = 0; i < len; i++) {
        if (global[i] != chunk[i]) {
            retval = false;
            break;
        }
    }

    if (retval == false) {
        _TRACE("Comparing global: '" 
               << s_GetPrintableSequence(global, len, is_prot) << "'");
        _TRACE("with chunk: '" 
               << s_GetPrintableSequence(chunk, len, is_prot) << "'");
    }

    return retval;
}
#endif

/* ----------------------------------------------------------------------------
 * To compute the offset correction for the plus strand we need to add the
 * length of all the contexts spread across the various chunks and subtract
 * the corresponding overlap regions.
 *
 * Assumptions: chunk size and overlap (in nucleotide coordinates) are
 * divisible by CODON_LENGTH.
 *
 * let C = current split query chunk
 * let SC = starting chunk such that the context in question is found in this
 * chunk
 * let ctx = context number in split query chunk C
 * let O = overlap size in final sequence data coordinates (aa for blastx, na
 * for blastn)
 * let x = ctx equivalent in the global (absolute) unsplit query
 * let ctxlen(a, b) be a function which computes the length of context a (given
 * in global coordinates) in chunk b. If a is not found in b or if b doesn't
 * exist (i.e.: is negative), 0 is returned.
 * let corr = correction to be added to the global query offset for chunk C,
 * context ctx
 *
 * corr = 0;
 * for (; C != SC; C--) {
 *       let prev_len = ctxlen(x, C - 1);
 *       let curr_len = ctxlen(x, C);
 *       let overlap = min(O, curr_len);
 *       corr += prev_len - min(overlap, prev_len);
 * }
 *
 * ----------------------------------------------------------------------------
 * To compute the offset correction for the negative strand we need to subtract
 * from the query length how far we have advanced into the query's negative
 * strand  (starting from the right) minus the overlap regions.
 *
 * Define the same variables as for the plus strand (except corr, defined
 * below):
 * let L = length of context x in the absolute query
 * let corr = L - S, where S is computed as follows:
 *
 * S = 0;
 * for (; C >= SC; C--) {
 *       let prev_len = ctxlen(x, C - 1);
 *       let curr_len = ctxlen(x, C);
 *       let overlap = min(O, curr_len);
 *       corr += curr_len - min(overlap, prev_len);
 * }
 * 
*/

void
CQuerySplitter::x_ComputeContextOffsetsForChunks()
{
    if (Blast_QueryIsTranslated(m_Options->GetProgramType())) {
        x_ComputeContextOffsets_TranslatedQueries();
    } else {
        x_ComputeContextOffsets_NonTranslatedQueries();
    }
}

// Record the correction needed to synchronize with the complete query all the
// query offsets within HSPs that fall within this context
void
CQuerySplitter::x_ComputeContextOffsets_NonTranslatedQueries()
{
    _ASSERT( !m_QueryChunkFactories.empty() );

    const EBlastProgramType kProgram = m_Options->GetProgramType();
    _ASSERT( !Blast_QueryIsTranslated(kProgram) );
    const BlastQueryInfo* global_qinfo = m_LocalQueryData->GetQueryInfo();
#ifdef DEBUG_COMPARE_SEQUENCES
    const BLAST_SequenceBlk* global_seq = m_LocalQueryData->GetSequenceBlk();
#endif
    const size_t kOverlap = SplitQuery_GetOverlapChunkSize(kProgram);
    CContextTranslator ctx_translator(*m_SplitBlk, &m_QueryChunkFactories,
                                      m_Options);
    vector<const BlastQueryInfo*> chunk_qinfo(m_NumChunks, 0);

    for (size_t chunk_num = 0; chunk_num < m_NumChunks; chunk_num++) {
        CRef<IQueryFactory> chunk_qf(m_QueryChunkFactories[chunk_num]);
        CRef<ILocalQueryData> chunk_qd(chunk_qf->MakeLocalQueryData(m_Options));
#ifdef DEBUG_COMPARE_SEQUENCES
        const BLAST_SequenceBlk* chunk_seq = chunk_qd->GetSequenceBlk();
#endif

        // BlastQueryInfo structure corresponding to chunk number chunk_num
        chunk_qinfo[chunk_num] = chunk_qd->GetQueryInfo();
        _ASSERT(chunk_qinfo[chunk_num]);

        // In case the first context differs from 0, for consistency with the
        // other data returned by this class...
        for (Int4 ctx = 0; ctx < chunk_qinfo[chunk_num]->first_context; ctx++) {
            m_SplitBlk->AddContextOffsetToChunk(chunk_num, INT4_MAX);
        }

        for (Int4 ctx = chunk_qinfo[chunk_num]->first_context; 
             ctx <= chunk_qinfo[chunk_num]->last_context; 
             ctx++) {

            size_t correction = 0;
            const int starting_chunk =
                ctx_translator.GetStartingChunk(chunk_num, ctx);
            const int absolute_context =
                ctx_translator.GetAbsoluteContext(chunk_num, ctx);

            if (absolute_context == kInvalidContext || 
                starting_chunk == kInvalidContext) {
                _ASSERT( !chunk_qinfo[chunk_num]->contexts[ctx].is_valid );
                // INT4_MAX is the sentinel value for invalid contexts
                m_SplitBlk->AddContextOffsetToChunk(chunk_num, INT4_MAX);
                continue;
            }

            if (s_IsPlusStrand(chunk_qinfo[chunk_num], ctx)) {

                for (int c = chunk_num; c != starting_chunk; c--) {
                    size_t prev_len = s_GetAbsoluteContextLength(chunk_qinfo, 
                                                         c - 1,
                                                         ctx_translator,
                                                         absolute_context);
                    size_t curr_len = s_GetAbsoluteContextLength(chunk_qinfo, c,
                                                         ctx_translator,
                                                         absolute_context);
                    size_t overlap = min(kOverlap, curr_len);
                    correction += prev_len - min(overlap, prev_len);
                }

            } else {

                size_t subtrahend = 0;

                for (int c = chunk_num; c >= starting_chunk && c >= 0; c--) {
                    size_t prev_len = s_GetAbsoluteContextLength(chunk_qinfo, 
                                                         c - 1,
                                                         ctx_translator,
                                                         absolute_context);
                    size_t curr_len = s_GetAbsoluteContextLength(chunk_qinfo,
                                                         c,
                                                         ctx_translator,
                                                         absolute_context);
                    size_t overlap = min(kOverlap, curr_len);
                    subtrahend += (curr_len - min(overlap, prev_len));
                }
                correction =
                    global_qinfo->contexts[absolute_context].query_length -
                    subtrahend;

            }
            _ASSERT((chunk_qinfo[chunk_num]->contexts[ctx].is_valid));
            m_SplitBlk->AddContextOffsetToChunk(chunk_num, correction);
#ifdef DEBUG_COMPARE_SEQUENCES
{
    int global_offset = global_qinfo->contexts[absolute_context].query_offset +
        correction;
    int chunk_offset = chunk_qinfo[chunk_num]->contexts[ctx].query_offset;
    if (!cmp_sequence(&global_seq->sequence[global_offset], 
                      &chunk_seq->sequence[chunk_offset], 10,
                      Blast_QueryIsProtein(kProgram))) {
        cerr << "Failed to compare sequence data!" << endl;
    }
}
#endif

        }
    }
    _TRACE("CContextTranslator contents: " << ctx_translator);
}

void
CQuerySplitter::x_ComputeContextOffsets_TranslatedQueries()
{
    _ASSERT( !m_QueryChunkFactories.empty() );

    const EBlastProgramType kProgram = m_Options->GetProgramType();
    _ASSERT(Blast_QueryIsTranslated(kProgram));
    const BlastQueryInfo* global_qinfo = m_LocalQueryData->GetQueryInfo();
#ifdef DEBUG_COMPARE_SEQUENCES
    const BLAST_SequenceBlk* global_seq = m_LocalQueryData->GetSequenceBlk();
#endif
    const size_t kOverlap = 
        SplitQuery_GetOverlapChunkSize(kProgram) / CODON_LENGTH;
    CContextTranslator ctx_translator(*m_SplitBlk, &m_QueryChunkFactories,
                                      m_Options);
    CQueryDataPerChunk qdpc(*m_SplitBlk, kProgram, m_LocalQueryData);
    vector<const BlastQueryInfo*> chunk_qinfo(m_NumChunks, 0);

    for (size_t chunk_num = 0; chunk_num < m_NumChunks; chunk_num++) {
        CRef<IQueryFactory> chunk_qf(m_QueryChunkFactories[chunk_num]);
        CRef<ILocalQueryData> chunk_qd(chunk_qf->MakeLocalQueryData(m_Options));
#ifdef DEBUG_COMPARE_SEQUENCES
        const BLAST_SequenceBlk* chunk_seq = chunk_qd->GetSequenceBlk();
#endif

        // BlastQueryInfo structure corresponding to chunk number chunk_num
        chunk_qinfo[chunk_num] = chunk_qd->GetQueryInfo();
        _ASSERT(chunk_qinfo[chunk_num]);

        // In case the first context differs from 0, for consistency with the
        // other data returned by this class...
        for (Int4 ctx = 0; ctx < chunk_qinfo[chunk_num]->first_context; ctx++) {
            m_SplitBlk->AddContextOffsetToChunk(chunk_num, INT4_MAX);
        }

        for (Int4 ctx = chunk_qinfo[chunk_num]->first_context; 
             ctx <= chunk_qinfo[chunk_num]->last_context; 
             ctx++) {

            size_t correction = 0;
            const int starting_chunk =
                ctx_translator.GetStartingChunk(chunk_num, ctx);
            const int absolute_context =
                ctx_translator.GetAbsoluteContext(chunk_num, ctx);
            const int last_query_chunk = qdpc.GetLastChunk(chunk_num, ctx);

            if (absolute_context == kInvalidContext || 
                starting_chunk == kInvalidContext) {
                _ASSERT( !chunk_qinfo[chunk_num]->contexts[ctx].is_valid );
                // INT4_MAX is the sentinel value for invalid contexts
                m_SplitBlk->AddContextOffsetToChunk(chunk_num, INT4_MAX);
                continue;
            }

            // The corrections for the contexts corresponding to the negative
            // strand in the last chunk of a query sequence are all 0
            if (!s_IsPlusStrand(chunk_qinfo[chunk_num], ctx) &&
                (chunk_num == (size_t)last_query_chunk) && 
                (ctx % NUM_FRAMES >= 3)) {
                correction = 0;
                goto error_check;
            }

            // The corrections for the contexts corresponding to the plus
            // strand are always the same, so only calculate the first one
            if (s_IsPlusStrand(chunk_qinfo[chunk_num], ctx) && 
                (ctx % NUM_FRAMES == 1 || ctx % NUM_FRAMES == 2)) {
                correction = m_SplitBlk->GetContextOffsets(chunk_num).back();
                goto error_check;
            }

            // If the query length is divisible by CODON_LENGTH, the
            // corrections for all contexts corresponding to a given strand are
            // the same, so only calculate the first one
            if ((qdpc.GetQueryLength(chunk_num, ctx) % CODON_LENGTH == 0) &&
                (ctx % NUM_FRAMES != 0) && (ctx % NUM_FRAMES != 3)) {
                correction = m_SplitBlk->GetContextOffsets(chunk_num).back();
                goto error_check;
            }

            // If the query length % CODON_LENGTH == 1, the corrections for the
            // first two contexts of the negative strand are the same, and the
            // correction for the last context is one more than that.
            if ((qdpc.GetQueryLength(chunk_num, ctx) % CODON_LENGTH == 1) &&
                !s_IsPlusStrand(chunk_qinfo[chunk_num], ctx)) {

                if (ctx % NUM_FRAMES == 4) {
                    correction = 
                        m_SplitBlk->GetContextOffsets(chunk_num).back();
                    goto error_check;
                } else if (ctx % NUM_FRAMES == 5) {
                    correction = 
                        m_SplitBlk->GetContextOffsets(chunk_num).back() + 1;
                    goto error_check;
                }
            }
                
            // If the query length % CODON_LENGTH == 2, the corrections for the
            // last two contexts of the negative strand are the same, which is
            // one more that the first context on the negative strand.
            if ((qdpc.GetQueryLength(chunk_num, ctx) % CODON_LENGTH == 2) &&
                !s_IsPlusStrand(chunk_qinfo[chunk_num], ctx)) {

                if (ctx % NUM_FRAMES == 4) {
                    correction = 
                        m_SplitBlk->GetContextOffsets(chunk_num).back() + 1;
                    goto error_check;
                } else if (ctx % NUM_FRAMES == 5) {
                    correction = 
                        m_SplitBlk->GetContextOffsets(chunk_num).back();
                    goto error_check;
                }
            }

            if (s_IsPlusStrand(chunk_qinfo[chunk_num], ctx)) {

                for (int c = chunk_num; c != starting_chunk; c--) {
                    size_t prev_len = s_GetAbsoluteContextLength(chunk_qinfo, 
                                                         c - 1,
                                                         ctx_translator,
                                                         absolute_context);
                    size_t curr_len = s_GetAbsoluteContextLength(chunk_qinfo, c,
                                                         ctx_translator,
                                                         absolute_context);
                    size_t overlap = min(kOverlap, curr_len);
                    correction += prev_len - min(overlap, prev_len);
                }

            } else {

                size_t subtrahend = 0;

                for (int c = chunk_num; c >= starting_chunk && c >= 0; c--) {
                    size_t prev_len = s_GetAbsoluteContextLength(chunk_qinfo, 
                                                         c - 1,
                                                         ctx_translator,
                                                         absolute_context);
                    size_t curr_len = s_GetAbsoluteContextLength(chunk_qinfo,
                                                         c,
                                                         ctx_translator,
                                                         absolute_context);
                    size_t overlap = min(kOverlap, curr_len);
                    subtrahend += (curr_len - min(overlap, prev_len));
                }
                correction =
                    global_qinfo->contexts[absolute_context].query_length -
                    subtrahend;
            }

error_check:
            _ASSERT((chunk_qinfo[chunk_num]->contexts[ctx].is_valid));
            m_SplitBlk->AddContextOffsetToChunk(chunk_num, correction);
#ifdef DEBUG_COMPARE_SEQUENCES
{
    int global_offset = global_qinfo->contexts[absolute_context].query_offset +
        correction;
    int chunk_offset = chunk_qinfo[chunk_num]->contexts[ctx].query_offset;
    int num_bases2compare = 
        min(10, chunk_qinfo[chunk_num]->contexts[ctx].query_length);
    if (!cmp_sequence(&global_seq->sequence[global_offset], 
                      &chunk_seq->sequence[chunk_offset], 
                      num_bases2compare, Blast_QueryIsProtein(kProgram))) {
        cerr << "Failed to compare sequence data for chunk " << chunk_num
             << ", context " << ctx << endl;
    }
}
#endif
        }
    }
    _TRACE("CContextTranslator contents: " << ctx_translator);
}

CRef<CSplitQueryBlk>
CQuerySplitter::Split()
{
    if (m_SplitBlk.NotEmpty()) {
        return m_SplitBlk;
    }

    m_SplitBlk.Reset(new CSplitQueryBlk(m_NumChunks,
                                        m_Options->GetGappedMode()));
    m_QueryChunkFactories.reserve(m_NumChunks);

    if (m_NumChunks == 1) {
        m_QueryChunkFactories.push_back(m_QueryFactory);
    } else {
        _TRACE("Splitting into " << m_NumChunks << " query chunks");
        x_ComputeChunkRanges();
        x_ComputeQueryIndicesForChunks();
        x_ComputeQueryContextsForChunks();

        for (size_t chunk_num = 0; chunk_num < m_NumChunks; chunk_num++) {
            CRef<IQueryFactory> qf
                (new CObjMgr_QueryFactory(*m_SplitQueriesInChunk[chunk_num]));
            m_QueryChunkFactories.push_back(qf);
        }

        x_ComputeContextOffsetsForChunks();
    }

    _TRACE("CSplitQuerBlk contents: " << *m_SplitBlk);
    _TRACE("CQuerySplitter contents: " << *this);

    return m_SplitBlk;
}

CRef<IQueryFactory>
CQuerySplitter::GetQueryFactoryForChunk(Uint4 chunk_num)
{
    if (chunk_num >= m_NumChunks) {
        string msg("Invalid query chunk number: ");
        msg += NStr::IntToString(chunk_num) + " out of " +
            NStr::IntToString(m_NumChunks);
        throw out_of_range(msg);
    }

    if (m_SplitBlk.Empty()) {
        Split();
    }

    return m_QueryChunkFactories[chunk_num];
}

END_SCOPE(blast)
END_NCBI_SCOPE

