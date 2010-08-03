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
* File Description:
*   Unit test module for code to split query sequences
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include "test_objmgr.hpp"

#include <blast_objmgr_priv.hpp>
#include <algo/blast/core/split_query.h>
#include "blast_aux_priv.hpp"
#include "split_query_aux_priv.hpp"
#include <algo/blast/api/blast_options_handle.hpp>
#include "split_query.hpp"
#include <algo/blast/api/objmgr_query_data.hpp>
#include <util/random_gen.hpp>
#include <objtools/simple/simple_om.hpp>

/* IMPORTANT NOTE: If you have made changes to the query splitting code, the
 * data in the configuration file (split_query.ini) might need to be updated.
 * To aid in this, the xblast library supports tracing messages that output the
 * internal data structure's contents to facilitate updating this file. To
 * enable this, please run the unit_test application with the DIAG_TRACE
 * environment variable set.
 */

typedef vector<vector<Uint4> > TSplitQueryChunkMap;

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

/// Calculate and assign the maximum length field in the BlastQueryInfo
/// structure
static void s_CalculateMaxLength(BlastQueryInfo* query_info)
{
    query_info->max_length = 0;
    for (int i = query_info->first_context; i <= query_info->last_context; i++)
    {
        BOOST_REQUIRE(query_info->contexts[i].query_length >= 0);
        query_info->max_length = 
            max<Uint4>(query_info->max_length, 
                       query_info->contexts[i].query_length);
    }
}

/// Pair for gis and their length (in that order)
typedef pair<int, size_t> TGiLenPair;
/// Vector containing pairs of gis and their length
typedef vector<TGiLenPair> TGiLengthVector;

/// Convert a vector of GIs with its lengths into a TSeqLocVector
/// @param gi_length vector of TGiLenPair containing GIs and their length [in]
/// @param retval the return value of this function [out]
/// @param tot_length total length of sequence data contained in gi_length
/// (optional) [in]
/// @param strands vector of strands to use (optional), if provided it must
/// match the size of the gi_length vector [in]
/// @param masks vector of masks (optional), if provided it must match the size
/// of the gi_length vector [in]
static void 
s_ConvertToBlastQueries(const TGiLengthVector& gi_length, 
                        TSeqLocVector& retval, 
                        size_t* tot_length = NULL,
                        vector<ENa_strand>* strands = NULL,
                        const TSeqLocInfoVector* masks = NULL)
{
    if (tot_length) {
        *tot_length = 0;
    }
    retval.clear();
    retval.reserve(gi_length.size());

    if (strands) {
        BOOST_REQUIRE(strands->size() == gi_length.size());
    }
    if (masks) {
        BOOST_REQUIRE(masks->size() == gi_length.size());
    }

    for (size_t i = 0; i < gi_length.size(); i++) {
        CRef<CSeq_loc> loc(new CSeq_loc());
        if (strands) {
            CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, gi_length[i].first));
            loc->SetInt().SetFrom(0);
            loc->SetInt().SetTo(gi_length[i].second-1);
            loc->SetId(*id);
            loc->SetStrand((*strands)[i]);
        } else {
            loc->SetWhole().SetGi(gi_length[i].first);
        }
        CRef<CScope> scope(CSimpleOM::NewScope());
        retval.push_back(SSeqLoc(loc, &*scope));
        if (tot_length) {
            *tot_length += gi_length[i].second;
        }
    }

    if (masks == NULL) {
        return;
    }

    for (size_t i = 0; i < masks->size(); i++) {
        const TMaskedQueryRegions& single_query_masks = (*masks)[i];
        // FIXME: don't make the distinction between single and multiple masks
        CRef<CSeq_loc> m(new CSeq_loc);

        if (single_query_masks.size() == 1) {
            const CSeq_interval& interval = 
                single_query_masks.front()->GetInterval();
            m->SetInt(const_cast<CSeq_interval&>(interval));
        } else {
            ITERATE(TMaskedQueryRegions, mask, single_query_masks) {
                const CSeq_interval& interval = (*mask)->GetInterval();
                m->SetPacked_int().AddInterval(interval);
            }
        }
        BOOST_REQUIRE(m->IsInt() || m->IsPacked_int());
        retval[i].mask = m;
    }
}

class CSplitQueryTestFixture {
public:
    /// This represents the split_query.ini configuration file
    CRef<CNcbiRegistry> m_Config;
    /// Default value used when a field is not present in the config file
    static const int kDefaultIntValue = -1;

    CSplitQueryTestFixture() {
        // read the configuration file if it hasn't been read yet
        if (m_Config.Empty()) {
            const IRegistry::TFlags flags =
                IRegistry::fNoOverride | 
                IRegistry::fTransient |
                IRegistry::fNotJustCore |
                IRegistry::fTruncate;

            const string fname("data/split_query.ini");
            ifstream config_file(fname.c_str());
            m_Config.Reset(new CNcbiRegistry(config_file, flags));

            if (m_Config->Empty()) {
                throw runtime_error("Failed to read configuration file" +
                                    fname);
            }
        }
    }

    ~CSplitQueryTestFixture() {
        BOOST_REQUIRE(m_Config.NotEmpty());
    }

    /// Populate a BLAST_SequenceBlk and BlastQueryInfo structures out of an
    /// array of GIs
    /// @param gis array of GIs, last element must be -1 indicating the end of
    /// the array [in]
    /// @param program program for which the query data will be created [in]
    /// @param seq_blk BLAST_SequenceBlk structure to populate [out]
    /// @param qinfo BlastQueryInfo structure to populate [out]
    /// @param strand strand to use (optional) [in]
    void x_PrepareBlastQueryStructures(int gis[],
                                       EProgram program,
                                       BLAST_SequenceBlk** seq_blk,
                                       BlastQueryInfo** qinfo,
                                       ENa_strand* strand = NULL)
    {
        BOOST_REQUIRE(seq_blk);
        BOOST_REQUIRE(qinfo);
        TSeqLocVector queries;

        for (int i = 0; gis[i] != -1; i++) {
            CRef<CSeq_loc> loc(new CSeq_loc());
            loc->SetWhole().SetGi(gis[i]);
            CScope* scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
            scope->AddDefaults();
            queries.push_back(SSeqLoc(loc, scope));
        }

        CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(program));

        TSearchMessages msgs;

        const CBlastOptions& kOpts = opts->GetOptions();
        EBlastProgramType prog = kOpts.GetProgramType();
        ENa_strand strand_opt = (strand != NULL)
            ? *strand : kOpts.GetStrandOption();
  
        SetupQueryInfo(queries, prog, strand_opt, qinfo);
        SetupQueries(queries, *qinfo, seq_blk, 
                     prog, strand_opt, msgs);
        BOOST_REQUIRE(msgs.HasMessages() == false);
    }

    void x_TestCContextTranslator(TGiLengthVector& gi_length,
                                  size_t chunk_size,
                                  size_t num_chunks,
                                  blast::EProgram program,
                                  vector< vector<int> >& starting_chunks,
                                  vector< vector<int> >& absolute_contexts,
                                  vector< vector<size_t> >* context_offsets,
                                  ENa_strand strand,
                                  vector<ENa_strand>* query_strands = NULL) {

        if (query_strands) {
            BOOST_REQUIRE_EQUAL(gi_length.size(), query_strands->size());
        }

        size_t tot_length;
        TSeqLocVector queries;
        s_ConvertToBlastQueries(gi_length, queries, &tot_length, query_strands);

        size_t nc = SplitQuery_CalculateNumChunks(
                                      EProgramToEBlastProgramType(program), 
                                      &chunk_size, tot_length, queries.size());
        BOOST_REQUIRE_EQUAL(num_chunks, nc);

        CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(queries));
        CRef<CBlastOptionsHandle> opts_h(CBlastOptionsFactory::Create(program));
        CRef<CBlastOptions> opts(&opts_h->SetOptions());
        if ( !query_strands ) {
            opts->SetStrandOption(strand);
        }
        CRef<ILocalQueryData> query_data(qf->MakeLocalQueryData(&*opts));

        CAutoEnvironmentVariable tmp_env("CHUNK_SIZE",
                                         NStr::IntToString(chunk_size,
                                                           NStr::fConvErr_NoThrow));
        CRef<CQuerySplitter> splitter(new CQuerySplitter(qf, &*opts));
        CRef<CSplitQueryBlk> sqb = splitter->Split();

        BOOST_REQUIRE_EQUAL((size_t)splitter->GetNumberOfChunks(), num_chunks);

        CContextTranslator ctx_translator(*sqb);

        ostringstream os;
        for (size_t chunk_num = 0; chunk_num < num_chunks; chunk_num++) {
            // Test the starting chunks
            vector<int>& st_chunks = starting_chunks[chunk_num];
            for (size_t context_in_chunk = 0;
                 context_in_chunk < st_chunks.size(); 
                 context_in_chunk++) {
                os.str("");
                os << "Starting chunks: ";
                os << "Chunk " << chunk_num << ", context " << context_in_chunk;
                int sc = ctx_translator.GetStartingChunk(chunk_num, 
                                                         context_in_chunk);
                BOOST_REQUIRE_MESSAGE(st_chunks[context_in_chunk]==sc,os.str());
            }

            // Test the absolute contexts
            vector<int>& abs_ctxts = absolute_contexts[chunk_num];
            for (size_t context_in_chunk = 0;
                 context_in_chunk < abs_ctxts.size(); 
                 context_in_chunk++) {
                os.str("");
                os << "Absolute contexts: ";
                os << "Chunk " << chunk_num << ", context " << context_in_chunk;
                int abs_ctx = 
                    ctx_translator.GetAbsoluteContext(chunk_num, 
                                                      context_in_chunk);
                BOOST_REQUIRE_MESSAGE(abs_ctxts[context_in_chunk]==abs_ctx,os.str());
            }
        }

        // Check the context offsets
        if ( !context_offsets ) {
            return;
        }

        const BLAST_SequenceBlk* global_seq = query_data->GetSequenceBlk();
        const BlastQueryInfo* global_qinfo = query_data->GetQueryInfo();
        CRef<CSplitQueryBlk> split_query_blk = splitter->m_SplitBlk;
        for (size_t chunk_num = 0; chunk_num < num_chunks; chunk_num++) {
            vector<size_t> test_ctx_off = 
                split_query_blk->GetContextOffsets(chunk_num);
            const vector<size_t>& ref_ctx_off = (*context_offsets)[chunk_num];

            os.str("");
            os << "Number of context offsets in chunk " << chunk_num;
            BOOST_REQUIRE_MESSAGE(ref_ctx_off.size()==test_ctx_off.size(),os.str());

            CRef<IQueryFactory> chunk_qf = 
                splitter->GetQueryFactoryForChunk(chunk_num);
            CRef<ILocalQueryData> chunk_qd(chunk_qf->MakeLocalQueryData(opts));
            const BLAST_SequenceBlk* chunk_seq = chunk_qd->GetSequenceBlk();
            const BlastQueryInfo* chunk_qinfo = chunk_qd->GetQueryInfo();

            for (size_t i = 0; i < ref_ctx_off.size(); i++) {
                size_t correction = ref_ctx_off[i];
                os.str("");
                os << "Context correction in chunk " << chunk_num 
                   << ", context " << i << " value now " << test_ctx_off[i]
                   << " not " << correction;
                BOOST_REQUIRE_MESSAGE(correction==test_ctx_off[i],os.str());

                int absolute_context =
                    ctx_translator.GetAbsoluteContext(chunk_num, i);
                if (absolute_context == kInvalidContext) {
                    continue;
                }
                
                int global_offset = 
                    global_qinfo->contexts[absolute_context].query_offset +
                    correction;
                int chunk_offset = chunk_qinfo->contexts[i].query_offset;
                int num_bases2compare = 
                    min(10, chunk_qinfo->contexts[i].query_length);

                os.str("");
                os << "Sequence data in chunk " << chunk_num 
                    << ", context " << i;
                bool rv = 
                    x_CmpSequenceData(&global_seq->sequence[global_offset], 
                                      &chunk_seq->sequence[chunk_offset], 
                                      num_bases2compare);
                BOOST_REQUIRE_MESSAGE(rv,os.str());
            }

        }
    }

    /** Auxiliary function that compares bytes of sequence data to validate the
     * context offset corrections 
     * @param global global query sequence data [in]
     * @param chunk sequence data for chunk [in]
     * @param len length of the data to compare [in] 
     * @return true if sequence data is identical, false otherwise
     */
    bool x_CmpSequenceData(const Uint1* global, const Uint1* chunk, size_t len)
    {
        for (size_t i = 0; i < len; i++) {
            if (global[i] != chunk[i]) {
                return false;
            }
        }
        return true;
    }

    void QuerySplitter_BlastnSingleQueryMultiChunk(const string& kTestName, 
                                                   ENa_strand strand)
    {
        CBlastQueryVector query;
        CSeq_id id(CSeq_id::e_Gi, 112422322); // 122347 bases long
        query.AddQuery(CTestObjMgr::Instance().CreateBlastSearchQuery(id));

        CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(query));
        CRef<CBlastOptionsHandle> opts_h(CBlastOptionsFactory::Create(eBlastn));
        CRef<CBlastOptions> opts(&opts_h->SetOptions());
        opts->SetStrandOption(strand);
        CRef<ILocalQueryData> query_data(qf->MakeLocalQueryData(&*opts));

        CRef<CQuerySplitter> splitter(new CQuerySplitter(qf, &*opts));
        CRef<CSplitQueryBlk> sqb = splitter->Split();

        CQuerySplitter::TSplitQueryVector split_query_vector;
        x_ReadQueryBoundsPerChunk(kTestName, sqb, split_query_vector);
        x_ValidateQuerySeqLocsPerChunk(splitter, split_query_vector);

        x_ValidateChunkBounds(splitter->GetChunkSize(),
                              query_data->GetSumOfSequenceLengths(),
                              *sqb, opts->GetProgramType());

        const size_t kNumChunks = (size_t)m_Config->GetInt(kTestName, 
                                                           "NumChunks", 
                                                           kDefaultIntValue);
        BOOST_REQUIRE_EQUAL(kNumChunks, (size_t)splitter->GetNumberOfChunks());
        BOOST_REQUIRE_EQUAL(kNumChunks, sqb->GetNumChunks());

        vector< vector<size_t> > queries_per_chunk;
        x_ReadVectorOfVectorsForTest(kTestName, "Queries", queries_per_chunk);
        x_ValidateQueriesPerChunkAssignment(*sqb, queries_per_chunk);

        vector< vector<int> > ctxs_per_chunk;
        x_ReadVectorOfVectorsForTest(kTestName, "Contexts", ctxs_per_chunk);
        x_ValidateQueryContextsPerChunkAssignment(*sqb, ctxs_per_chunk);

        vector< vector<size_t> > ctx_offsets_per_chunk;
        x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets", 
                                     ctx_offsets_per_chunk);
        x_ValidateContextOffsetsPerChunkAssignment(*sqb, ctx_offsets_per_chunk);

        vector<BlastQueryInfo*> split_query_info;
        x_ReadSplitQueryInfoForTest(kTestName, opts->GetProgramType(),
                                    split_query_info);
        x_ValidateLocalQueryData(splitter, &*opts, split_query_info);
        NON_CONST_ITERATE(vector<BlastQueryInfo*>, itr, split_query_info) {
            *itr = BlastQueryInfoFree(*itr);
        }
    }
    
    void QuerySplitter_BlastnMultiQueryMultiChunk(const string& kTestName,
                                                  ENa_strand strand,
                                                  vector<ENa_strand>*
                                                  query_strands = NULL) 
    {
        TGiLengthVector gi_length;
        gi_length.push_back(make_pair<int, size_t>(112258880, 362959));
        gi_length.push_back(make_pair<int, size_t>(112253843, 221853));
        gi_length.push_back(make_pair<int, size_t>(112193060, 194837));
        gi_length.push_back(make_pair<int, size_t>(112193059, 204796));
        if (query_strands) {
            BOOST_REQUIRE_EQUAL(gi_length.size(), query_strands->size());
        }

        size_t tot_length;
        TSeqLocVector queries;
        s_ConvertToBlastQueries(gi_length, queries, &tot_length, query_strands);

        CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(queries));
        CRef<CBlastOptionsHandle> opts_h(CBlastOptionsFactory::Create(eBlastn));
        CRef<CBlastOptions> opts(&opts_h->SetOptions());
        if ( !query_strands ) {
            opts->SetStrandOption(strand);
        }
        CRef<ILocalQueryData> query_data(qf->MakeLocalQueryData(&*opts));

        CRef<CQuerySplitter> splitter(new CQuerySplitter(qf, &*opts));
        CRef<CSplitQueryBlk> sqb = splitter->Split();

        CQuerySplitter::TSplitQueryVector split_query_vector;
        x_ReadQueryBoundsPerChunk(kTestName, sqb, split_query_vector);
        x_ValidateQuerySeqLocsPerChunk(splitter, split_query_vector);

        x_ValidateChunkBounds(splitter->GetChunkSize(),
                              query_data->GetSumOfSequenceLengths(),
                              *sqb, opts->GetProgramType());

        const size_t kNumChunks = (size_t)m_Config->GetInt(kTestName, 
                                                           "NumChunks", 
                                                           kDefaultIntValue);
        BOOST_REQUIRE_EQUAL(kNumChunks, (size_t)splitter->GetNumberOfChunks());
        BOOST_REQUIRE_EQUAL(kNumChunks, sqb->GetNumChunks());

        vector< vector<size_t> > queries_per_chunk;
        x_ReadVectorOfVectorsForTest(kTestName, "Queries", queries_per_chunk);
        x_ValidateQueriesPerChunkAssignment(*sqb, queries_per_chunk);

        vector< vector<int> > ctxs_per_chunk;
        x_ReadVectorOfVectorsForTest(kTestName, "Contexts", ctxs_per_chunk);
        x_ValidateQueryContextsPerChunkAssignment(*sqb, ctxs_per_chunk);

        vector< vector<size_t> > ctx_offsets_per_chunk;
        x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets", 
                                     ctx_offsets_per_chunk);
        x_ValidateContextOffsetsPerChunkAssignment(*sqb, ctx_offsets_per_chunk);

        vector<BlastQueryInfo*> split_query_info;
        x_ReadSplitQueryInfoForTest(kTestName, opts->GetProgramType(),
                                    split_query_info);
        x_ValidateLocalQueryData(splitter, &*opts, split_query_info);
        NON_CONST_ITERATE(vector<BlastQueryInfo*>, itr, split_query_info) {
            *itr = BlastQueryInfoFree(*itr);
        }
    }

    void QuerySplitter_BlastxSingleQueryMultiChunk(const string& kTestName, 
                                                   ENa_strand strand)
    {
        const size_t kLength = 122347;  // length of the sequence below
        CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 63122693));
        TSeqRange range(0, kLength);
        TSeqLocVector query;
        query.push_back(*CTestObjMgr::Instance().
                        CreateSSeqLoc(*id, range, strand));

        CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(query));
        CRef<CBlastOptionsHandle> opts_h(CBlastOptionsFactory::Create(eBlastx));
        CRef<CBlastOptions> opts(&opts_h->SetOptions());
        CRef<ILocalQueryData> query_data(qf->MakeLocalQueryData(&*opts));

        CRef<CQuerySplitter> splitter(new CQuerySplitter(qf, &*opts));
        CRef<CSplitQueryBlk> sqb = splitter->Split();

        BOOST_REQUIRE_EQUAL(m_Config->GetInt(kTestName, "ChunkSize",
                                              kDefaultIntValue),
                             (int)splitter->GetChunkSize());

        x_ValidateChunkBounds(splitter->GetChunkSize(),
                              query_data->GetSumOfSequenceLengths(),
                              *sqb, opts->GetProgramType());

        const size_t kNumChunks = (size_t)m_Config->GetInt(kTestName, 
                                                           "NumChunks", 
                                                           kDefaultIntValue);
        BOOST_REQUIRE_EQUAL(kNumChunks, (size_t)splitter->GetNumberOfChunks());
        BOOST_REQUIRE_EQUAL(kNumChunks, sqb->GetNumChunks());

        vector< vector<size_t> > queries_per_chunk;
        x_ReadVectorOfVectorsForTest(kTestName, "Queries", queries_per_chunk);
        x_ValidateQueriesPerChunkAssignment(*sqb, queries_per_chunk);

        vector< vector<int> > ctxs_per_chunk;
        x_ReadVectorOfVectorsForTest(kTestName, "Contexts", ctxs_per_chunk);
        x_ValidateQueryContextsPerChunkAssignment(*sqb, ctxs_per_chunk);

        vector< vector<size_t> > ctx_offsets_per_chunk;
        x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets", 
                                     ctx_offsets_per_chunk);
        x_ValidateContextOffsetsPerChunkAssignment(*sqb, ctx_offsets_per_chunk);

        vector<BlastQueryInfo*> split_query_info;
        x_ReadSplitQueryInfoForTest(kTestName, opts->GetProgramType(), 
                                    split_query_info);
        x_ValidateLocalQueryData(splitter, &*opts, split_query_info);
        NON_CONST_ITERATE(vector<BlastQueryInfo*>, itr, split_query_info) {
            *itr = BlastQueryInfoFree(*itr);
        }
    }

    void QuerySplitter_BlastxMultiQueryMultiChunk(const string& kTestName,
                                                  ENa_strand strand,
                                                  vector<ENa_strand>*
                                                  query_strands = NULL) 
    {
        TGiLengthVector gi_length;
        gi_length.push_back(make_pair<int, size_t>(112817621, 5567));
        gi_length.push_back(make_pair<int, size_t>(112585373, 5987));
        gi_length.push_back(make_pair<int, size_t>(112585216, 5531));
        gi_length.push_back(make_pair<int, size_t>(112585119, 5046));
        if (query_strands) {
            BOOST_REQUIRE_EQUAL(gi_length.size(), query_strands->size());
        }

        size_t tot_length;
        TSeqLocVector queries;
        s_ConvertToBlastQueries(gi_length, queries, &tot_length, query_strands);

        CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(queries));
        CRef<CBlastOptionsHandle> opts_h(CBlastOptionsFactory::Create(eBlastx));
        CRef<CBlastOptions> opts(&opts_h->SetOptions());
        if ( !query_strands ) {
            opts->SetStrandOption(strand);
        }
        CRef<ILocalQueryData> query_data(qf->MakeLocalQueryData(&*opts));

        CRef<CQuerySplitter> splitter(new CQuerySplitter(qf, &*opts));
        CRef<CSplitQueryBlk> sqb = splitter->Split();

        BOOST_REQUIRE_EQUAL(m_Config->GetInt(kTestName, "ChunkSize",
                                              kDefaultIntValue),
                             (int)splitter->GetChunkSize());

        BOOST_REQUIRE_EQUAL(tot_length, query_data->GetSumOfSequenceLengths());
        x_ValidateChunkBounds(splitter->GetChunkSize(),
                              query_data->GetSumOfSequenceLengths(),
                              *sqb, opts->GetProgramType());

        const size_t kNumChunks = (size_t)m_Config->GetInt(kTestName, 
                                                           "NumChunks", 
                                                           kDefaultIntValue);
        BOOST_REQUIRE_EQUAL(kNumChunks, (size_t)splitter->GetNumberOfChunks());
        BOOST_REQUIRE_EQUAL(kNumChunks, sqb->GetNumChunks());

        vector< vector<size_t> > queries_per_chunk;
        x_ReadVectorOfVectorsForTest(kTestName, "Queries", queries_per_chunk);
        x_ValidateQueriesPerChunkAssignment(*sqb, queries_per_chunk);

        vector< vector<int> > ctxs_per_chunk;
        x_ReadVectorOfVectorsForTest(kTestName, "Contexts", ctxs_per_chunk);
        x_ValidateQueryContextsPerChunkAssignment(*sqb, ctxs_per_chunk);

        vector< vector<size_t> > ctx_offsets_per_chunk;
        x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets", 
                                     ctx_offsets_per_chunk);
        x_ValidateContextOffsetsPerChunkAssignment(*sqb, ctx_offsets_per_chunk);

        vector<BlastQueryInfo*> split_query_info;
        x_ReadSplitQueryInfoForTest(kTestName, opts->GetProgramType(),
                                    split_query_info);
        x_ValidateLocalQueryData(splitter, &*opts, split_query_info);
        NON_CONST_ITERATE(vector<BlastQueryInfo*>, itr, split_query_info) {
            *itr = BlastQueryInfoFree(*itr);
        }
    }

    /************ Auxiliary functions **********************************/

    /// Incrementally compute the query chunk bounds. This will have a direct
    /// impact on the success of x_ValidateChunkBounds. This function assumes
    /// that the chunk size doesn't vary between each invocation and that the
    /// first time this function is called, the chunk_range is initialized with
    /// its default constructor (e.g.: TChunkRange::GetEmpty())
    /// @param chunk_range range of the query chunk [in|out]
    /// @param chunk_size size of the chunk [in]
    /// @param concatenated_query_length length of the full query [in]
    /// @param overlap length of the overlap region between each chunk [in]
    void x_ComputeQueryChunkBounds(TChunkRange& chunk_range,
                                   size_t chunk_size,
                                   size_t concatenated_query_length,
                                   size_t overlap)
    {
        if (chunk_range == TChunkRange::GetEmpty()) {
            chunk_range.SetFrom(0);
            chunk_range.SetLength(chunk_size);
        } else {
            const TSeqPos kIncrement = chunk_size - overlap;
            chunk_range.SetFrom(chunk_range.GetFrom() + kIncrement);
            chunk_range.SetToOpen(chunk_range.GetToOpen() + kIncrement);
        }
        BOOST_REQUIRE(chunk_range.NotEmpty());

        if (chunk_range.GetToOpen() > concatenated_query_length) {
            chunk_range.SetToOpen(concatenated_query_length);
        }
    }

    /// This function reads values in the split_query.ini file with the format
    /// ChunkNQueryM (where N is the chunk number and M is the query number).
    /// Each of these entries should have 3 comma-separeted elements: the
    /// query's starting offset, ending offset, and its strand's enumeration
    /// value.
    /// @param kTestName name of the test to read data for [in]
    /// @param sqb CSplitQueryBlk object from which to get query indices for
    /// each chunk [in]
    /// @param split_query_vector query vector where the data from config file
    /// will be read [out]
    void x_ReadQueryBoundsPerChunk(const string& kTestName, 
                                   CConstRef<CSplitQueryBlk> sqb,
                   CQuerySplitter::TSplitQueryVector& split_query_vector)
    {
        CRef<CScope> scope(CSimpleOM::NewScope());
        TMaskedQueryRegions empty_mask;
        split_query_vector.clear();

        ostringstream os;

        const int kNumChunks = m_Config->GetInt(kTestName, "NumChunks",
                                                kDefaultIntValue);
        if (kNumChunks == kDefaultIntValue) {
            throw runtime_error("Invalid number of chunks in " + kTestName);
        }

        split_query_vector.assign(kNumChunks, CRef<CBlastQueryVector>());

        for (int i = 0; i < kNumChunks; i++) {
            os.str("");
            os << "Chunk" << i;
            const vector<size_t> kQueryIndices = sqb->GetQueryIndices(i);

            BOOST_REQUIRE( !kQueryIndices.empty() );
            split_query_vector[i].Reset(new CBlastQueryVector);

            ITERATE(vector<size_t>, itr, kQueryIndices) {
                ostringstream out;
                out << "Query" << *itr;

                const string& value = m_Config->Get(kTestName, 
                                                    os.str() + out.str());
                // This data corresponds to entries in split_query.ini of the
                // form ChunkNQueryM, and each line should contain 3 elements:
                // the start and stop for each query in each chunk and the
                // strand's enumeration value
                vector<size_t> query_data;
                x_ParseConfigLine(value, query_data);
                BOOST_REQUIRE_MESSAGE((size_t)3==query_data.size(),os.str() + out.str());

                CRef<CSeq_loc> sl(new CSeq_loc);
                sl->SetInt().SetFrom(query_data[0]);
                sl->SetInt().SetTo(query_data[1]);
                sl->SetStrand(static_cast<ENa_strand>(query_data[2]));
                CRef<CBlastSearchQuery> bsq(new CBlastSearchQuery(*sl, 
                                                                  *scope, 
                                                                  empty_mask));
                split_query_vector[i]->AddQuery(bsq);
            }
        }
    }

    /// Compare the query data (start, stop, strand) for each chunk computed by
    /// the splitter vs. the data read from the split_query.ini file 
    /// @param splitter object which performs query splitting [in]
    /// @param split_query_vector data instantiated from what was read from the
    /// split_query.ini file
    /// @param splitter CQuerySplitter object to test [in]
    /// @param split_query_vector data read from config file to test against
    /// [in]
    void x_ValidateQuerySeqLocsPerChunk(CRef<CQuerySplitter> splitter, 
              const CQuerySplitter::TSplitQueryVector& split_query_vector)
    {
        if (split_query_vector.empty()) {
            return;
        }

        ostringstream os;
        os << "Different split query vector sizes";

        BOOST_REQUIRE_MESSAGE(split_query_vector.size()==(size_t)splitter->m_NumChunks,os.str());

        for (size_t i = 0; i < splitter->m_NumChunks; i++) {
            CRef<CBlastQueryVector> ref_qvector = split_query_vector[i];
            CRef<CBlastQueryVector> test_qvector = 
                splitter->m_SplitQueriesInChunk[i];

            os.str("");
            os << "Different split query vector sizes for chunk " << i;
            BOOST_REQUIRE_MESSAGE(ref_qvector->Size()==test_qvector->Size(),os.str());

            for (size_t j = 0; j < ref_qvector->Size(); j++) {
                CConstRef<CSeq_loc> ref_qloc = ref_qvector->GetQuerySeqLoc(j);
                CConstRef<CSeq_loc> test_qloc = test_qvector->GetQuerySeqLoc(j);
                CSeq_loc::TRange ref_query_range = ref_qloc->GetTotalRange();
                CSeq_loc::TRange test_query_range = test_qloc->GetTotalRange();

                os.str("");
                os << "Starting offset for query " << j << " in chunk " << i << " is now " << test_query_range.GetFrom() << " and not " << ref_query_range.GetFrom();
                BOOST_REQUIRE_MESSAGE(ref_query_range.GetFrom()==test_query_range.GetFrom(),os.str());
                os.str("");
                os << "Ending offset for query " << j << " in chunk " << i << " is now " << test_query_range.GetToOpen() << " and not " << ref_query_range.GetTo();
                BOOST_REQUIRE_MESSAGE(ref_query_range.GetTo()==test_query_range.GetToOpen(),os.str());
                os.str("");
                os << "Strand for query " << j << " in chunk " << i << " is now " 
                    << (int)test_qloc->GetStrand() << " and not " << (int)ref_qloc->GetStrand();
                BOOST_REQUIRE_MESSAGE(ref_qloc->GetStrand()==test_qloc->GetStrand(),os.str());
            }
        }
    }

    /// Reads data to populate multiple BlastQueryInfo structures. This data is
    /// formatted in the config file as
    /// BlastQueryInfoN.X[.Y] where N is the chunk number, X is the field of
    /// the BlastQueryInfo structure and Y is the field of the BlastContextInfo
    /// structure (only applicable if X has the value contextM, where M is the
    /// context number)
    /// @param kTestName name of the test to read data for [in]
    /// @param program blast program [in]
    /// @param retval vector of BlastQueryInfo structures, there will be as
    /// many elements as there are chunks for this test. Caller is
    /// responsible for deallocating the contents of this vector [out]
    void x_ReadSplitQueryInfoForTest(const string& kTestName,
                                     EBlastProgramType program,
                                     vector<BlastQueryInfo*>& retval)
    {
        ostringstream os, errors;

        const int kNumChunks = m_Config->GetInt(kTestName, "NumChunks",
                                                kDefaultIntValue);
        if (kNumChunks == kDefaultIntValue) {
            throw runtime_error("Invalid number of chunks in " + kTestName);
        }

        retval.clear();
        retval.reserve(kNumChunks);
        retval.assign(kNumChunks, static_cast<BlastQueryInfo*>(0));

        for (int i = 0; i < kNumChunks; i++) {
            os.str("");
            os << "BlastQueryInfo" << i << ".";
            const string kPrefix(os.str());
            errors.str("Chunk ");
            errors << i << ": ";
            const int kNumQueries = m_Config->GetInt(kTestName, 
                                                     kPrefix + "num_queries",
                                                     kDefaultIntValue);
            if (kNumQueries == kDefaultIntValue) {
                string msg("Invalid BlastQueryInfo::num_queries in ");
                msg += kTestName + " or value not specified";
return; // FIXME
                //throw runtime_error(msg);
            }

            retval[i] = BlastQueryInfoNew(program, kNumQueries);
            errors << "Failed to allocate BlastQueryInfo structure"
                   << " (Number of queries=" << kNumQueries << ")";
            BOOST_REQUIRE_MESSAGE(retval[i],errors.str());

            retval[i]->first_context = m_Config->GetInt(kTestName,
                                                        kPrefix +
                                                        "first_context",
                                                        kDefaultIntValue);
            errors.str("Chunk ");
            errors << i;
            BOOST_REQUIRE_MESSAGE(retval[i]->first_context >= 0,errors.str());

            retval[i]->last_context = m_Config->GetInt(kTestName,
                                                       kPrefix +
                                                       "last_context",
                                                       kDefaultIntValue);
            BOOST_REQUIRE_MESSAGE(retval[i]->last_context >= 0,errors.str());
            BOOST_REQUIRE_MESSAGE(retval[i]->first_context <= retval[i]->last_context,errors.str());

            for (int c = retval[i]->first_context; 
                 c <= retval[i]->last_context; 
                 c++) {

                errors.str("");
                errors << "Chunk " << i << ", BlastQueryInfo::context " << c;

                ostringstream ctx;
                ctx << kPrefix << "context" << c << ".";

                retval[i]->contexts[c].query_offset =
                    m_Config->GetInt(kTestName, ctx.str() +
                                     "query_offset", kDefaultIntValue);
                BOOST_REQUIRE_MESSAGE(retval[i]->contexts[c].query_offset >= 0, 
                                      errors.str() + " query_offset >= 0");

                retval[i]->contexts[c].query_length =
                    m_Config->GetInt(kTestName, ctx.str() +
                                     "query_length", kDefaultIntValue);
                BOOST_REQUIRE_MESSAGE(retval[i]->contexts[c].query_length >= 0,
                                      errors.str() + " query_length >= 0");

                retval[i]->contexts[c].eff_searchsp =
                    m_Config->GetInt(kTestName, ctx.str() +
                                     "eff_searchsp", kDefaultIntValue);
                BOOST_REQUIRE_MESSAGE(retval[i]->contexts[c].eff_searchsp >= 0,
                                      errors.str() + " eff_searchsp >= 0");

                retval[i]->contexts[c].length_adjustment =
                    m_Config->GetInt(kTestName, ctx.str() +
                                     "length_adjustment", kDefaultIntValue);
                BOOST_REQUIRE_MESSAGE(retval[i]->contexts[c].length_adjustment >= 0,
                                      errors.str() + " length_adjustment >= 0");

                retval[i]->contexts[c].query_index =
                    m_Config->GetInt(kTestName, ctx.str() +
                                     "query_index", kDefaultIntValue);
                BOOST_REQUIRE_MESSAGE(retval[i]->contexts[c].query_index >= 0,
                                      errors.str() + " query_index");

                retval[i]->contexts[c].frame =
                    m_Config->GetInt(kTestName, ctx.str() +
                                     "frame", kDefaultIntValue);
                BOOST_REQUIRE_MESSAGE(retval[i]->contexts[c].frame == 1 
                                   || retval[i]->contexts[c].frame == 2 
                                   || retval[i]->contexts[c].frame == 3 
                                   || retval[i]->contexts[c].frame == -1 
                                   || retval[i]->contexts[c].frame == -2 
                                   || retval[i]->contexts[c].frame == -3 
                                   || retval[i]->contexts[c].frame == 0,
                                   errors.str() + " frame");

                retval[i]->contexts[c].is_valid =
                    m_Config->GetBool(kTestName, ctx.str() +
                                     "is_valid", false);
                BOOST_REQUIRE_MESSAGE(retval[i]->contexts[c].is_valid,
                                      errors.str() + " is_valid");
            }
            s_CalculateMaxLength(retval[i]);
        }
    }

    /// This method reads entries in the config file of the format
    /// ChunkNX, here N is the chunk number and X is the value of data_to_read
    /// @param kTestName name of the test to read data for [in]
    /// @param data_to_read data for a chunk to read [in]
    /// @param retval vector of vectors where the data will be returned. The
    /// first vector will contain as many elements are there are chunks, and
    /// the contained vectors will contain as many elements as there are items
    /// on the config file (comma separated values) [out]
    template <class T>
    void x_ReadVectorOfVectorsForTest(const string& kTestName,
                                      const char* data_to_read,
                                      vector< vector<T> >& retval)
    {
        ostringstream os;

        const int kNumChunks = m_Config->GetInt(kTestName, "NumChunks",
                                                kDefaultIntValue);
        if (kNumChunks == kDefaultIntValue) {
            throw runtime_error("Invalid number of chunks in " + kTestName);
        }

        retval.clear();
        retval.resize(kNumChunks);

        for (int i = 0; i < kNumChunks; i++) {
            os.str("");
            os << "Chunk" << i << data_to_read;

            const string& value = m_Config->Get(kTestName, os.str());
            x_ParseConfigLine(value, retval[i]);
        }
    }

    /// Tokenizes a string containing comma-separated values into a vector of
    /// values
    /// @param input string to tokenize [in]
    /// @param retval vector containing elements found in input string [out]
    template <class T>
    void x_ParseConfigLine(const string& input, vector<T>& retval)
    {
        retval.clear();
        vector<string> tokens;
        NStr::Tokenize(input, ",", tokens);
        retval.reserve(tokens.size());
        ITERATE(vector<string>, token, tokens) {
            retval.push_back(NStr::StringToInt(NStr::TruncateSpaces(*token)));
        }
    }

    /***************** Generic validation methods ****************************/

    /// Auxiliary method to validate the chunk bounds calculated by the
    /// CSplitQueryBlk object and the x_ComputeQueryChunkBounds method
    /// @param kChunkSize size of the chunk [in]
    /// @param kQuerySize size of the full query [in]
    /// @param sqb the CSplitQueryBlk object to test [in]
    /// @param p the program type [in]
    void x_ValidateChunkBounds(size_t kChunkSize,
                               size_t kQuerySize,
                               const CSplitQueryBlk& sqb,
                               EBlastProgramType p)
    {
        const size_t kNumChunks(sqb.GetNumChunks());
        const size_t kQueryChunkOverlapSize = SplitQuery_GetOverlapChunkSize(p);

        TChunkRange expected_chunk_range(TChunkRange::GetEmpty());
        for (size_t i = 0; i < kNumChunks; i++) {
            x_ComputeQueryChunkBounds(expected_chunk_range, kChunkSize,
                                      kQuerySize, kQueryChunkOverlapSize);
            TChunkRange chunk_range = sqb.GetChunkBounds(i);
            BOOST_REQUIRE_EQUAL(expected_chunk_range.GetFrom(),
                                 chunk_range.GetFrom());
            BOOST_REQUIRE_EQUAL(expected_chunk_range.GetToOpen(),
                                 chunk_range.GetToOpen());
            TSeqPos chunk_start = i*kChunkSize - (i*kQueryChunkOverlapSize);
            TSeqPos chunk_end = chunk_start + kChunkSize > kQuerySize
                ? kQuerySize
                : chunk_start + kChunkSize;
            BOOST_REQUIRE_EQUAL(expected_chunk_range.GetFrom(), chunk_start);
            BOOST_REQUIRE_EQUAL(expected_chunk_range.GetToOpen(), chunk_end);
            TSeqPos chunk_length = chunk_end - chunk_start;
                BOOST_REQUIRE_EQUAL(chunk_length,
                                     expected_chunk_range.GetLength());
        }
    }

    /// Validates the query sequences (by index) assigned to all the chunks
    /// This compares the data calculated by the sqb parameter to the data read
    /// from the config file in queries_per_chunk
    /// @param sqb CSplitQueryBlk object to test [in]
    /// @param queries_per_chunk data read from config file [in]
    void x_ValidateQueriesPerChunkAssignment(const CSplitQueryBlk& sqb,
                                             const vector< vector<size_t> >&
                                             queries_per_chunk)
    {
        const size_t kNumChunks = sqb.GetNumChunks();
        BOOST_REQUIRE_EQUAL(kNumChunks, queries_per_chunk.size());

        for (size_t i = 0; i < kNumChunks; i++) {
            ostringstream os;
            os << "Chunk number " << i << " has an invalid number of queries";

            vector<size_t> data2test = sqb.GetQueryIndices(i);
            BOOST_REQUIRE_MESSAGE(queries_per_chunk[i].size()==data2test.size(),os.str());

            for (size_t j = 0; j < data2test.size(); j++) {
                os.str("");
                os << "Query index mismatch in chunk number " << i 
                   << " entry number " << j;
                BOOST_REQUIRE_MESSAGE(queries_per_chunk[i][j]==data2test[j],os.str());
            }
        }
    }

    /// Validates the query contexts assigned to all the chunks
    /// @param sqb CSplitQueryBlk object to test [in]
    /// @param contexts_per_chunk data read from config file [in]
    void x_ValidateQueryContextsPerChunkAssignment(const CSplitQueryBlk& sqb,
                                             const vector< vector<int> >&
                                             contexts_per_chunk)
    {
        const size_t kNumChunks = sqb.GetNumChunks();

        BOOST_REQUIRE_EQUAL(kNumChunks, contexts_per_chunk.size());
        for (size_t i = 0; i < kNumChunks; i++) {
            ostringstream os;
            os << "Chunk number " << i << " has an invalid number of contexts";

            vector<int> data2test = sqb.GetQueryContexts(i);
            BOOST_REQUIRE_MESSAGE(contexts_per_chunk[i].size()==data2test.size(),os.str());

            for (size_t j = 0; j < data2test.size(); j++) {
                os.str("");
                os << "Context index mismatch in chunk number " << i 
                   << " entry number " << j;
                BOOST_REQUIRE_MESSAGE(contexts_per_chunk[i][j]==data2test[j],os.str());
            }
        }
    }

    /// Validates the context offsets assigned to all the chunks
    /// @param sqb CSplitQueryBlk object to test [in]
    /// @param contexts_offsets_per_chunk data read from config file [in]
    void x_ValidateContextOffsetsPerChunkAssignment(const CSplitQueryBlk& sqb,
                                             const vector< vector<size_t> >&
                                             contexts_offsets_per_chunk)
    {
        const size_t kNumChunks(sqb.GetNumChunks());
        BOOST_REQUIRE_EQUAL(kNumChunks, contexts_offsets_per_chunk.size());
        for (size_t i = 0; i < kNumChunks; i++) {
            ostringstream os;
            os << "Chunk number " << i 
               << " has an invalid number of context offsets";

            vector<size_t> data2test = sqb.GetContextOffsets(i);
            BOOST_REQUIRE_MESSAGE(contexts_offsets_per_chunk[i].size()==data2test.size(),os.str());

            for (size_t j = 0; j < data2test.size(); j++) {
                os.str("");
                os << "Context offset mismatch in chunk number " << i 
                   << " entry number " << j << " value now " << data2test[j]
                   << " not " << contexts_offsets_per_chunk[i][j];
// TLM cerr <<  "data2test " << data2test[j] << " ";
                 BOOST_REQUIRE_MESSAGE(contexts_offsets_per_chunk[i][j]==data2test[j],os.str());
            }
// TLM cerr << endl;
        }
    }

    /// Validate the query info structure generated (test) against the expected
    /// one (reference) (N.B.: this is called from x_ValidateLocalQueryData)
    /// @param reference The "good" BlastQueryInfo structure [in]
    /// @param test the BlastQueryInfo structure to test [in]
    /// @param the chunk number being tested, this is needed for error
    /// reporting purposes [in]
    void x_ValidateQueryInfoForChunk(const BlastQueryInfo* reference,
                                     const BlastQueryInfo* test,
                                     size_t chunk_num)
    {
        ostringstream os;

        os << "Chunk " << chunk_num << ": BlastQueryInfo::first_context";
        BOOST_REQUIRE_MESSAGE(reference->first_context==test->first_context,os.str());

        os.str("");
        os << "Chunk " << chunk_num << ": BlastQueryInfo::last_context";
        BOOST_REQUIRE_MESSAGE(reference->last_context==test->last_context,os.str());

        os.str("");
        os << "Chunk " << chunk_num << ": BlastQueryInfo::num_queries";
        BOOST_REQUIRE_MESSAGE(reference->num_queries==test->num_queries,os.str());

        os.str("");
        os << "Chunk " << chunk_num << ": BlastQueryInfo::max_length";
        BOOST_REQUIRE_MESSAGE(reference->max_length==test->max_length,os.str());

        os.str("");
        os << "Chunk " << chunk_num << ": BlastQueryInfo::pattern_info";
        BOOST_REQUIRE_MESSAGE(reference->pattern_info==test->pattern_info,os.str());

        for (Int4 ctx = reference->first_context;
             ctx <= reference->last_context;
             ctx++) {

            os.str("");
            os << "Chunk " << chunk_num << ", context " << ctx;
            BOOST_REQUIRE_MESSAGE(reference->contexts[ctx].query_offset==test->contexts[ctx].query_offset,
                                  os.str() + " query_offset");
            BOOST_REQUIRE_MESSAGE(reference->contexts[ctx].query_length==test->contexts[ctx].query_length,
                                  os.str() + " query_length");
            BOOST_REQUIRE_MESSAGE(reference->contexts[ctx].eff_searchsp==test->contexts[ctx].eff_searchsp,
                                  os.str() + " eff_searchsp");
            BOOST_REQUIRE_MESSAGE(reference->contexts[ctx].query_index==test->contexts[ctx].query_index,
                                  os.str() + " query_index");
            BOOST_REQUIRE_MESSAGE((int)reference->contexts[ctx].frame==(int)test->contexts[ctx].frame,
                                  os.str() + " frame");
            BOOST_REQUIRE_MESSAGE((bool)reference->contexts[ctx].is_valid==(bool)test->contexts[ctx].is_valid,
                                  os.str() + " is_valid");

        }
    }

    /// Validate the local query data for all chunks, comparing data produced
    /// by the CQuerySplitter object and the BlastQueryInfo structures read
    /// from the config file (BLAST_SequenceBlk's are not tested)
    /// @param splitter object to test [in]
    /// @param options BLAST options [in]
    /// @param split_query_info_structs the data to compare to (reference) [in]
    void x_ValidateLocalQueryData(CRef<CQuerySplitter> splitter,
                                  const CBlastOptions* options,
                                  vector<BlastQueryInfo*>
                                  split_query_info_structs)
    {
        ostringstream os;
        BOOST_REQUIRE(options);
        const size_t kNumChunks(splitter->GetNumberOfChunks());

        CRef<CSplitQueryBlk> sqb = splitter->Split();
        BOOST_REQUIRE_EQUAL(kNumChunks, split_query_info_structs.size());

        for (size_t i = 0; i < kNumChunks; i++) {
            os.str("");
            os << "Chunk " << i << ": ";
            CRef<IQueryFactory> qf = splitter->GetQueryFactoryForChunk(i);
            BOOST_REQUIRE_MESSAGE(qf.NotEmpty(),os.str() + "NULL query factory");
            CRef<ILocalQueryData> qd = qf->MakeLocalQueryData(options);
            BOOST_REQUIRE_MESSAGE(qd.NotEmpty(),os.str() + "NULL local query data");

            os << "Different number of queries";
            BOOST_REQUIRE_MESSAGE((size_t)sqb->GetNumQueriesForChunk(i)==(size_t)qd->GetNumQueries(),os.str());

            // FIXME: turned off for now
            // Validate the query info structure
            //x_ValidateQueryInfoForChunk(split_query_info_structs[i],
            //                            qd->GetQueryInfo(), i);

            //x_ValidateSequenceBlkForChunk();

            // Validate that query in this chunk is indeed valid
            //for (int qindex = 0; qindex < qd->GetNumQueries(); qindex++) {
            //    os.str("Chunk ");
            //    os << i << ": query " << qindex << " is invalid";
            //    BOOST_REQUIRE_MESSAGE(qd->IsValidQuery(qindex),os.str());
            //}

        }

    }
};

BOOST_FIXTURE_TEST_SUITE(split_query, CSplitQueryTestFixture)

/*********** Actual unit tests ***************************************/
BOOST_AUTO_TEST_CASE(SplitQueriesIn1Chunk) {
    CRef<CSplitQueryBlk> sqb(new CSplitQueryBlk(1));
    Int2 rv;

    rv = SplitQueryBlk_AddQueryToChunk(sqb->GetCStruct(), 41, 2);
    BOOST_REQUIRE_EQUAL(kBadParameter, rv);

    /// This will be reused for both query indices and contexts
    vector<Int4> query_indices_expected;
    query_indices_expected.push_back(45);
    query_indices_expected.push_back(0);
    query_indices_expected.push_back(7);

    ITERATE(vector<Int4>, qi, query_indices_expected) {
        rv = SplitQueryBlk_AddQueryToChunk(sqb->GetCStruct(), *qi, 0);
        BOOST_REQUIRE_EQUAL((Int2)0, rv);
        rv = SplitQueryBlk_AddContextToChunk(sqb->GetCStruct(), *qi, 0);
        BOOST_REQUIRE_EQUAL((Int2)0, rv);
    }

    Uint4* query_indices = NULL;
    rv = SplitQueryBlk_GetQueryIndicesForChunk(sqb->GetCStruct(), 0, 
                                               &query_indices);
    BOOST_REQUIRE_EQUAL((Int2)0, rv);
    for (int i = 0; query_indices[i] != UINT4_MAX; i++) {
        BOOST_REQUIRE_EQUAL(query_indices_expected[i], 
                             (Int4)query_indices[i]);
    }
    sfree(query_indices);

    Int4* query_contexts = NULL;
    Uint4 num_query_contexts = 0;
    rv = SplitQueryBlk_GetQueryContextsForChunk(sqb->GetCStruct(), 0, 
                                                &query_contexts,
                                                &num_query_contexts);
    BOOST_REQUIRE_EQUAL((Int2)0, rv);
    for (Uint4 i = 0; i < num_query_contexts; i++) {
        BOOST_REQUIRE_EQUAL(query_indices_expected[i], query_contexts[i]);
    }
    sfree(query_contexts);

    size_t num_queries(0);
    rv = SplitQueryBlk_GetNumQueriesForChunk(sqb->GetCStruct(), 0,
                                             &num_queries);
    BOOST_REQUIRE_EQUAL((Int2)0, rv);
    BOOST_REQUIRE_EQUAL(query_indices_expected.size(), num_queries);
}

BOOST_AUTO_TEST_CASE(SplitQueriesRandomly) {
    CRandom random(time(0));
    const Uint4 kNumChunks(random.GetRand(1, 100));
    TSplitQueryChunkMap map;
    map.resize(kNumChunks);
    Uint4 query_index = 0;

    // Set up the artificial data
    for (Uint4 chunk_num = 0; chunk_num < kNumChunks; chunk_num++) {
        const Uint4 kQueriesPerChunk(random.GetRand(1, 365));
        for (Uint4 i = 0; i < kQueriesPerChunk; i++) {
            map[chunk_num].push_back(query_index++);
        }
    }

    // Set up the SplitQueryBlk structure
    CRef<CSplitQueryBlk> sqb(new CSplitQueryBlk(kNumChunks));
    for (size_t chunk_num = 0; chunk_num < map.size(); chunk_num++) {
        ITERATE(vector<Uint4>, qi, map[chunk_num]) {
            Int2 rv = SplitQueryBlk_AddQueryToChunk(sqb->GetCStruct(), *qi,
                                                    chunk_num);
            BOOST_REQUIRE_EQUAL((Int2)0, rv);
        }
    }

    for (Uint4 chunk_num = 0; chunk_num < kNumChunks; chunk_num++) {
        vector<Uint4> query_indices_expected = map[chunk_num];

        Uint4* query_indices = NULL;
        Int2 rv = SplitQueryBlk_GetQueryIndicesForChunk(sqb->GetCStruct(),
                                                        chunk_num,
                                                        &query_indices);
        BOOST_REQUIRE_EQUAL((Int2)0, rv);
        BOOST_REQUIRE(query_indices != NULL);

        size_t i;
        for (i = 0; i < query_indices_expected.size(); i++) {
            BOOST_REQUIRE_EQUAL(query_indices_expected[i],
                                 query_indices[i]);
        }
        BOOST_REQUIRE_EQUAL((Uint4)UINT4_MAX, query_indices[i]);
        sfree(query_indices);

        size_t num_queries(0);
        rv = SplitQueryBlk_GetNumQueriesForChunk(sqb->GetCStruct(), chunk_num,
                                                 &num_queries);
        BOOST_REQUIRE_EQUAL((Int2)0, rv);
        BOOST_REQUIRE_EQUAL(query_indices_expected.size(), num_queries);
    }
}

BOOST_AUTO_TEST_CASE(Split4QueriesIn3Chunks) {
    const Uint4 kNumChunks = 3;
    TSplitQueryChunkMap map;
    map.resize(kNumChunks);
    map[0].push_back(0);
    map[0].push_back(1);
    map[1].push_back(2);
    map[2].push_back(3);

    CRef<CSplitQueryBlk> sqb(new CSplitQueryBlk(kNumChunks));

    for (Uint4 chunk_num = 0; chunk_num < map.size(); chunk_num++) {
        ITERATE(vector<Uint4>, qi, map[chunk_num]) {
            Int2 rv = SplitQueryBlk_AddQueryToChunk(sqb->GetCStruct(), *qi, 
                                                    chunk_num);
            BOOST_REQUIRE_EQUAL((Int2)0, rv);
        }
    }

    for (Uint4 chunk_num = 0; chunk_num < kNumChunks; chunk_num++) {
        vector<Uint4> query_indices_expected = map[chunk_num];

        Uint4* query_indices = NULL;
        Int2 rv = SplitQueryBlk_GetQueryIndicesForChunk(sqb->GetCStruct(),
                                                        chunk_num,
                                                        &query_indices);
        BOOST_REQUIRE_EQUAL((Int2)0, rv);
        BOOST_REQUIRE(query_indices != NULL);

        size_t i;
        for (i = 0; i < query_indices_expected.size(); i++) {
            BOOST_REQUIRE_EQUAL(query_indices_expected[i],
                                 query_indices[i]);
        }
        BOOST_REQUIRE_EQUAL((Uint4)UINT4_MAX, query_indices[i]);
        sfree(query_indices);

        size_t num_queries(0);
        rv = SplitQueryBlk_GetNumQueriesForChunk(sqb->GetCStruct(), chunk_num,
                                                 &num_queries);
        BOOST_REQUIRE_EQUAL((Int2)0, rv);
        BOOST_REQUIRE_EQUAL(query_indices_expected.size(), num_queries);
    }
}

/// Tests query splitting for blastn of both strands of a single query into
/// multiple chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastnSingleQueryMultiChunk_BothStrands) {
    CAutoEnvironmentVariable tmp_env("CHUNK_SIZE", "40000");
    const string
        kTestName("QuerySplitter_BlastnSingleQueryMultiChunk_BothStrands");

    QuerySplitter_BlastnSingleQueryMultiChunk(kTestName, eNa_strand_both);
}

/// Tests query splitting for blastn of the plus strands of a single query
/// into multiple chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastnSingleQueryMultiChunk_PlusStrand) {
    CAutoEnvironmentVariable tmp_env("CHUNK_SIZE", "40000");
    const string
        kTestName("QuerySplitter_BlastnSingleQueryMultiChunk_PlusStrand");

    QuerySplitter_BlastnSingleQueryMultiChunk(kTestName, eNa_strand_plus);
}

/// Tests query splitting for blastn of the minus strands of a single query
/// into multiple chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastnSingleQueryMultiChunk_MinusStrand) {
    CAutoEnvironmentVariable tmp_env("CHUNK_SIZE", "40000");
    const string
        kTestName("QuerySplitter_BlastnSingleQueryMultiChunk_MinusStrand");

    QuerySplitter_BlastnSingleQueryMultiChunk(kTestName, eNa_strand_minus);
}

/// Tests query splitting for blastn of the plus strands of multiple queries
/// into multiple chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastnMultiQueryMultiChunk_PlusStrand) {
    CAutoEnvironmentVariable tmp_env("CHUNK_SIZE", "40000");
    const string
        kTestName("QuerySplitter_BlastnMultiQueryMultiChunk_PlusStrand");

    QuerySplitter_BlastnMultiQueryMultiChunk(kTestName, eNa_strand_plus);
}

/// Tests query splitting for blastn of the minus strands of multiple
/// queries into multiple chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastnMultiQueryMultiChunk_MinusStrand) {
    CAutoEnvironmentVariable tmp_env("CHUNK_SIZE", "40000");
    const string
        kTestName("QuerySplitter_BlastnMultiQueryMultiChunk_MinusStrand");

    QuerySplitter_BlastnMultiQueryMultiChunk(kTestName, eNa_strand_minus);
}

/// Tests query splitting for blastn of both strands of multiple
/// queries into multiple chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastnMultiQueryMultiChunk_BothStrands) {
    CAutoEnvironmentVariable tmp_env("CHUNK_SIZE", "40000");
    const string 
        kTestName("QuerySplitter_BlastnMultiQueryMultiChunk_BothStrands");
    QuerySplitter_BlastnMultiQueryMultiChunk(kTestName, eNa_strand_both);
}

/// Tests query splitting for blastn with multiple queries in multiple
/// chunks with each query using different strands
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastnMultiQueryMultiChunk_MixedStrands) {
    CAutoEnvironmentVariable tmp_env("CHUNK_SIZE", "40000");
    const string 
        kTestName("QuerySplitter_BlastnMultiQueryMultiChunk_MixedStrands");
    vector<ENa_strand> query_strands;
    query_strands.reserve(4);
    query_strands.push_back(eNa_strand_plus);
    query_strands.push_back(eNa_strand_both);
    query_strands.push_back(eNa_strand_minus);
    query_strands.push_back(eNa_strand_unknown);

    QuerySplitter_BlastnMultiQueryMultiChunk(kTestName, 
                                             eNa_strand_unknown,
                                             &query_strands);
}

/*********  This functionality has not been implemented  **************/
#if 0
/// Tests blastx of both strands of a single query into multiple chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastxSingleQueryMultiChunk_BothStrands) {
    const string
        kTestName("QuerySplitter_BlastxSingleQueryMultiChunk_BothStrands");

    QuerySplitter_BlastxSingleQueryMultiChunk(kTestName, eNa_strand_both);
}

/// Tests blastx of the plus strand of a single query into multiple chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastxSingleQueryMultiChunk_PlusStrand) {
    const string
        kTestName("QuerySplitter_BlastxSingleQueryMultiChunk_PlusStrand");

    QuerySplitter_BlastxSingleQueryMultiChunk(kTestName, eNa_strand_plus);
}

/// Tests blastx of the minus strand of a single query into multiple chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastxSingleQueryMultiChunk_MinusStrand) {
    const string
        kTestName("QuerySplitter_BlastxSingleQueryMultiChunk_MinusStrand");

    QuerySplitter_BlastxSingleQueryMultiChunk(kTestName, eNa_strand_minus);
}


/// Tests blastx of the plus strand of multiple queries into multiple chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastxMultiQueryMultiChunk_PlusStrand) {
    const string
        kTestName("QuerySplitter_BlastxMultiQueryMultiChunk_PlusStrand");

    QuerySplitter_BlastxMultiQueryMultiChunk(kTestName, eNa_strand_plus);
}

/// Tests blastx of the minus strand of multiple queries into multiple
/// chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastxMultiQueryMultiChunk_MinusStrand) {
    const string 
        kTestName("QuerySplitter_BlastxMultiQueryMultiChunk_MinusStrand");

    QuerySplitter_BlastxMultiQueryMultiChunk(kTestName, eNa_strand_minus);
}

/// Tests blastx of both strands of multiple queries into multiple
/// chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastxMultiQueryMultiChunk_BothStrands) {
    const string 
        kTestName("QuerySplitter_BlastxMultiQueryMultiChunk_BothStrands");

    QuerySplitter_BlastxMultiQueryMultiChunk(kTestName, eNa_strand_both);
}

BOOST_AUTO_TEST_CASE(QuerySplitter_BlastxMultiQueryMultiChunk_MixedStrands) {
    const string 
        kTestName("QuerySplitter_BlastxMultiQueryMultiChunk_MixedStrands");
    vector<ENa_strand> query_strands;
    query_strands.reserve(4);
    query_strands.push_back(eNa_strand_unknown);
    query_strands.push_back(eNa_strand_plus);
    query_strands.push_back(eNa_strand_both);
    query_strands.push_back(eNa_strand_minus);

    QuerySplitter_BlastxMultiQueryMultiChunk(kTestName, eNa_strand_unknown,
                                             &query_strands);
}

#endif

/// Tests blastp of a single query into multiple chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastpSingleQueryMultiChunk) {
    const string kTestName("QuerySplitter_BlastpSingleQueryMultiChunk");

    const size_t kLength = 33423;    // query length
    CBlastQueryVector query;
    CSeq_id id(CSeq_id::e_Gi, 110349719);
    query.AddQuery(CTestObjMgr::Instance().CreateBlastSearchQuery(id));

    CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(query));
    CRef<CBlastOptionsHandle> opts_h(CBlastOptionsFactory::Create(eBlastp));
    CRef<CBlastOptions> opts(&opts_h->SetOptions());
    CRef<ILocalQueryData> query_data(qf->MakeLocalQueryData(&*opts));

    CRef<CQuerySplitter> splitter(new CQuerySplitter(qf, &*opts));
    CRef<CSplitQueryBlk> sqb = splitter->Split();

    BOOST_REQUIRE_EQUAL(m_Config->GetInt(kTestName, "ChunkSize",
                                          kDefaultIntValue),
                         (int)splitter->GetChunkSize());

    CQuerySplitter::TSplitQueryVector split_query_vector;
    x_ReadQueryBoundsPerChunk(kTestName, sqb, split_query_vector);
    x_ValidateQuerySeqLocsPerChunk(splitter, split_query_vector);

    BOOST_REQUIRE_EQUAL(kLength, query_data->GetSumOfSequenceLengths());
    x_ValidateChunkBounds(splitter->GetChunkSize(),
                          query_data->GetSumOfSequenceLengths(),
                          *sqb, opts->GetProgramType());

    const size_t kNumChunks = (size_t)m_Config->GetInt(kTestName, 
                                                       "NumChunks", 
                                                       kDefaultIntValue);
    BOOST_REQUIRE_EQUAL(kNumChunks, (size_t)splitter->GetNumberOfChunks());
    BOOST_REQUIRE_EQUAL(kNumChunks, sqb->GetNumChunks());

    vector< vector<size_t> > queries_per_chunk;
    x_ReadVectorOfVectorsForTest(kTestName, "Queries", queries_per_chunk);
    x_ValidateQueriesPerChunkAssignment(*sqb, queries_per_chunk);

    vector< vector<int> > ctxs_per_chunk;
    x_ReadVectorOfVectorsForTest(kTestName, "Contexts", ctxs_per_chunk);
    x_ValidateQueryContextsPerChunkAssignment(*sqb, ctxs_per_chunk);

    vector< vector<size_t> > ctx_offsets_per_chunk;
    x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets", 
                                 ctx_offsets_per_chunk);
    x_ValidateContextOffsetsPerChunkAssignment(*sqb, ctx_offsets_per_chunk);

    vector<BlastQueryInfo*> split_query_info;
    x_ReadSplitQueryInfoForTest(kTestName, opts->GetProgramType(),
                                split_query_info);
    x_ValidateLocalQueryData(splitter, &*opts, split_query_info);
    NON_CONST_ITERATE(vector<BlastQueryInfo*>, itr, split_query_info) {
        *itr = BlastQueryInfoFree(*itr);
    }
}

/// Tests blastp of multiple queries into multiple chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_BlastpMultiQueryMultiChunk) {
    const string kTestName("QuerySplitter_BlastpMultiQueryMultiChunk");

    TGiLengthVector gi_length;
    gi_length.push_back(make_pair<int, size_t>(33624848,  6883));
    gi_length.push_back(make_pair<int, size_t>(4758794,   6669));
    gi_length.push_back(make_pair<int, size_t>(66821305,  6061));
    gi_length.push_back(make_pair<int, size_t>(109075552, 5007));

    size_t tot_length;
    TSeqLocVector queries;
    s_ConvertToBlastQueries(gi_length, queries, &tot_length);

    CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(queries));
    CRef<CBlastOptionsHandle> opts_h(CBlastOptionsFactory::Create(eBlastp));
    CRef<CBlastOptions> opts(&opts_h->SetOptions());
    CRef<ILocalQueryData> query_data(qf->MakeLocalQueryData(&*opts));

    CRef<CQuerySplitter> splitter(new CQuerySplitter(qf, &*opts));
    CRef<CSplitQueryBlk> sqb = splitter->Split();

    BOOST_REQUIRE_EQUAL(m_Config->GetInt(kTestName, "ChunkSize",
                                          kDefaultIntValue),
                         (int)splitter->GetChunkSize());

    CQuerySplitter::TSplitQueryVector split_query_vector;
    x_ReadQueryBoundsPerChunk(kTestName, sqb, split_query_vector);
    x_ValidateQuerySeqLocsPerChunk(splitter, split_query_vector);

    BOOST_REQUIRE_EQUAL(tot_length, query_data->GetSumOfSequenceLengths());
    x_ValidateChunkBounds(splitter->GetChunkSize(),
                          query_data->GetSumOfSequenceLengths(),
                          *sqb, opts->GetProgramType());

    const size_t kNumChunks = (size_t)m_Config->GetInt(kTestName, 
                                                       "NumChunks", 
                                                       kDefaultIntValue);
    BOOST_REQUIRE_EQUAL(kNumChunks, (size_t)splitter->GetNumberOfChunks());
    BOOST_REQUIRE_EQUAL(kNumChunks, sqb->GetNumChunks());

    vector< vector<size_t> > queries_per_chunk;
    x_ReadVectorOfVectorsForTest(kTestName, "Queries", queries_per_chunk);
    x_ValidateQueriesPerChunkAssignment(*sqb, queries_per_chunk);

    vector< vector<int> > ctxs_per_chunk;
    x_ReadVectorOfVectorsForTest(kTestName, "Contexts", ctxs_per_chunk);
    x_ValidateQueryContextsPerChunkAssignment(*sqb, ctxs_per_chunk);

    vector< vector<size_t> > ctx_offsets_per_chunk;
    x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets", 
                                 ctx_offsets_per_chunk);
    x_ValidateContextOffsetsPerChunkAssignment(*sqb, ctx_offsets_per_chunk);

    vector<BlastQueryInfo*> split_query_info;
    x_ReadSplitQueryInfoForTest(kTestName, opts->GetProgramType(),
                                split_query_info);
    x_ValidateLocalQueryData(splitter, &*opts, split_query_info);
    NON_CONST_ITERATE(vector<BlastQueryInfo*>, itr, split_query_info) {
        *itr = BlastQueryInfoFree(*itr);
    }
}

/// Tests the CContextTranslator class for blastn of both strands of
/// multiple queries 
BOOST_AUTO_TEST_CASE(TestCContextTranslator_BlastnMultiQuery_BothStrands) {
    const string
        kTestName("TestCContextTranslator_BlastnMultiQuery_BothStrands");
    TGiLengthVector gi_length;
    gi_length.push_back(make_pair<int, size_t>(107784911, 1000));
    gi_length.push_back(make_pair<int, size_t>(115354032, 250));
    gi_length.push_back(make_pair<int, size_t>(115381005, 2551));

    const size_t chunk_size = 500;
    const size_t num_chunks = 9;

    vector< vector<int> > starting_chunks(num_chunks);
    vector< vector<int> > absolute_contexts(num_chunks);
    vector< vector<size_t> > context_offset_corrections(num_chunks);

    x_ReadVectorOfVectorsForTest(kTestName, "StartingChunks", 
                                 starting_chunks);
    x_ReadVectorOfVectorsForTest(kTestName, "AbsoluteContexts", 
                                 absolute_contexts);
    x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets", 
                                 context_offset_corrections);

    x_TestCContextTranslator(gi_length, chunk_size, num_chunks, eBlastn,
                             starting_chunks, absolute_contexts,
                             &context_offset_corrections,
                             eNa_strand_both);
}

/// Tests the CContextTranslator class for blastn of the plus strand of
/// multiple queries 
BOOST_AUTO_TEST_CASE(TestCContextTranslator_BlastnMultiQuery_PlusStrand) {
    const string
        kTestName("TestCContextTranslator_BlastnMultiQuery_PlusStrand");
    TGiLengthVector gi_length;
    gi_length.push_back(make_pair<int, size_t>(107784911, 1000));
    gi_length.push_back(make_pair<int, size_t>(115354032, 250));
    gi_length.push_back(make_pair<int, size_t>(115381005, 2551));

    const size_t chunk_size = 500;
    const size_t num_chunks = 9;

    vector< vector<int> > starting_chunks(num_chunks);
    vector< vector<int> > absolute_contexts(num_chunks);
    vector< vector<size_t> > context_offset_corrections(num_chunks);

    x_ReadVectorOfVectorsForTest(kTestName, "StartingChunks", 
                                 starting_chunks);
    x_ReadVectorOfVectorsForTest(kTestName, "AbsoluteContexts", 
                                 absolute_contexts);
    x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets", 
                                 context_offset_corrections);

    x_TestCContextTranslator(gi_length, chunk_size, num_chunks, eBlastn, 
                             starting_chunks, absolute_contexts, 
                             &context_offset_corrections,
                             eNa_strand_plus);
}

/// Tests the CContextTranslator class for blastn of the minus strand of
/// multiple queries 
BOOST_AUTO_TEST_CASE(TestCContextTranslator_BlastnMultiQuery_MinusStrand) {
    const string
        kTestName("TestCContextTranslator_BlastnMultiQuery_MinusStrand");
    TGiLengthVector gi_length;
    gi_length.push_back(make_pair<int, size_t>(107784911, 1000));
    gi_length.push_back(make_pair<int, size_t>(115354032, 250));
    gi_length.push_back(make_pair<int, size_t>(115381005, 2551));

    const size_t chunk_size = 500;
    const size_t num_chunks = 9;

    vector< vector<int> > starting_chunks(num_chunks);
    vector< vector<int> > absolute_contexts(num_chunks);
    vector< vector<size_t> > context_offset_corrections(num_chunks);

    x_ReadVectorOfVectorsForTest(kTestName, "StartingChunks", 
                                 starting_chunks);
    x_ReadVectorOfVectorsForTest(kTestName, "AbsoluteContexts", 
                                 absolute_contexts);
    x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets", 
                                 context_offset_corrections);

    x_TestCContextTranslator(gi_length, chunk_size, num_chunks, eBlastn, 
                             starting_chunks, absolute_contexts, 
                             &context_offset_corrections,
                             eNa_strand_minus);
}

/// Tests the CContextTranslator class for blastx of both strands of
/// a single query with length divisible by CODON_LENGTH
BOOST_AUTO_TEST_CASE(TestCContextTranslator_BlastxSingleQuery_BothStrands_0) {
    const string 
        kTestName("TestCContextTranslator_BlastxSingleQuery_BothStrands_0");
    TGiLengthVector gi_length;
    gi_length.push_back(make_pair<int, size_t>(116001669, 33));

    const size_t chunk_size = 15;
    const size_t num_chunks = 3;
    CAutoEnvironmentVariable tmp_env("OVERLAP_CHUNK_SIZE", "6");

    vector< vector<int> > starting_chunks(num_chunks);
    vector< vector<int> > absolute_contexts(num_chunks);
    vector< vector<size_t> > context_offset_corrections(num_chunks);

    x_ReadVectorOfVectorsForTest(kTestName, "StartingChunks", 
                                 starting_chunks);
    x_ReadVectorOfVectorsForTest(kTestName, "AbsoluteContexts", 
                                 absolute_contexts);
    x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets",
                                 context_offset_corrections);

    x_TestCContextTranslator(gi_length, chunk_size, num_chunks, eBlastx,
                             starting_chunks, absolute_contexts,
                             &context_offset_corrections,
                             eNa_strand_both);
}

/// Tests the CContextTranslator class for blastx of both strands of
/// a single query with length not divisible by CODON_LENGTH, instead, the
/// (query length % CODON_LENGTH == 1)
BOOST_AUTO_TEST_CASE(TestCContextTranslator_BlastxSingleQuery_BothStrands_1) {
    const string 
        kTestName("TestCContextTranslator_BlastxSingleQuery_BothStrands_1");
    TGiLengthVector gi_length;
    gi_length.push_back(make_pair<int, size_t>(116001673, 34));

    const size_t chunk_size = 15;
    const size_t num_chunks = 3;
    CAutoEnvironmentVariable tmp_env("OVERLAP_CHUNK_SIZE", "6");

    vector< vector<int> > starting_chunks(num_chunks);
    vector< vector<int> > absolute_contexts(num_chunks);
    vector< vector<size_t> > context_offset_corrections(num_chunks);

    x_ReadVectorOfVectorsForTest(kTestName, "StartingChunks", 
                                 starting_chunks);
    x_ReadVectorOfVectorsForTest(kTestName, "AbsoluteContexts", 
                                 absolute_contexts);
    x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets",
                                 context_offset_corrections);

    x_TestCContextTranslator(gi_length, chunk_size, num_chunks, eBlastx,
                             starting_chunks, absolute_contexts,
                             &context_offset_corrections,
                             eNa_strand_both);
}

/// Tests the CContextTranslator class for blastx of both strands of
/// a single query with length not divisible by CODON_LENGTH, instead, the
/// (query length % CODON_LENGTH == 2)
BOOST_AUTO_TEST_CASE(TestCContextTranslator_BlastxSingleQuery_BothStrands_2) {
    const string 
        kTestName("TestCContextTranslator_BlastxSingleQuery_BothStrands_2");
    TGiLengthVector gi_length;
    gi_length.push_back(make_pair<int, size_t>(116001668, 35));

    const size_t chunk_size = 15;
    const size_t kNumChunks = m_Config->GetInt(kTestName, "NumChunks",
                                               kDefaultIntValue);
    CAutoEnvironmentVariable tmp_env("OVERLAP_CHUNK_SIZE", "6");

    vector< vector<int> > starting_chunks(kNumChunks);
    vector< vector<int> > absolute_contexts(kNumChunks);
    vector< vector<size_t> > context_offset_corrections(kNumChunks);

    x_ReadVectorOfVectorsForTest(kTestName, "StartingChunks", 
                                 starting_chunks);
    x_ReadVectorOfVectorsForTest(kTestName, "AbsoluteContexts", 
                                 absolute_contexts);
    x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets",
                                 context_offset_corrections);

    x_TestCContextTranslator(gi_length, chunk_size, kNumChunks, eBlastx,
                             starting_chunks, absolute_contexts,
                             &context_offset_corrections,
                             eNa_strand_both);
}

/*********  This functionality has not been implemented  **************/
#if 0

BOOST_AUTO_TEST_CASE(TestCContextTranslator_BlastxMultiQuery_BothStrands) {
    const string
        kTestName("TestCContextTranslator_BlastxMultiQuery_BothStrands");
    TGiLengthVector gi_length;
    gi_length.push_back(make_pair<int, size_t>(107784911, 1000));
    gi_length.push_back(make_pair<int, size_t>(115354032, 250));
    gi_length.push_back(make_pair<int, size_t>(115381005, 2551));

    const size_t chunk_size = 501;
    const size_t num_chunks = 10;

    vector< vector<int> > starting_chunks(num_chunks);
    vector< vector<int> > absolute_contexts(num_chunks);
    vector< vector<size_t> > context_offset_corrections(num_chunks);

    x_ReadVectorOfVectorsForTest(kTestName, "StartingChunks", 
                                 starting_chunks);
    x_ReadVectorOfVectorsForTest(kTestName, "AbsoluteContexts", 
                                 absolute_contexts);
    x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets",
                                 context_offset_corrections);

    x_TestCContextTranslator(gi_length, chunk_size, num_chunks, eBlastx,
                             starting_chunks, absolute_contexts,
                             &context_offset_corrections,
                             eNa_strand_both);
}

BOOST_AUTO_TEST_CASE(TestCContextTranslator_BlastxMultiQuery_PlusStrand) {
    const string
        kTestName("TestCContextTranslator_BlastxMultiQuery_PlusStrand");
    TGiLengthVector gi_length;
    gi_length.push_back(make_pair<int, size_t>(107784911, 1000));
    gi_length.push_back(make_pair<int, size_t>(115354032, 250));
    gi_length.push_back(make_pair<int, size_t>(115381005, 2551));

    const size_t chunk_size = 500;
    const size_t num_chunks = 10;

    vector< vector<int> > starting_chunks(num_chunks);
    vector< vector<int> > absolute_contexts(num_chunks);
    vector< vector<size_t> > context_offset_corrections(num_chunks);

    x_ReadVectorOfVectorsForTest(kTestName, "StartingChunks", 
                                 starting_chunks);
    x_ReadVectorOfVectorsForTest(kTestName, "AbsoluteContexts", 
                                 absolute_contexts);
    x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets",
                                 context_offset_corrections);

    x_TestCContextTranslator(gi_length, chunk_size, num_chunks, eBlastx,
                             starting_chunks, absolute_contexts, 
                             &context_offset_corrections,
                             eNa_strand_plus);
}

BOOST_AUTO_TEST_CASE(TestCContextTranslator_BlastxMultiQuery_MinusStrand) {
    const string
        kTestName("TestCContextTranslator_BlastxMultiQuery_MinusStrand");
    TGiLengthVector gi_length;
    gi_length.push_back(make_pair<int, size_t>(107784911, 1000));
    gi_length.push_back(make_pair<int, size_t>(115354032, 250));
    gi_length.push_back(make_pair<int, size_t>(115381005, 2551));

    const size_t chunk_size = 500;
    const size_t num_chunks = 10;

    vector< vector<int> > starting_chunks(num_chunks);
    vector< vector<int> > absolute_contexts(num_chunks);
    vector< vector<size_t> > context_offset_corrections(num_chunks);

    x_ReadVectorOfVectorsForTest(kTestName, "StartingChunks", 
                                 starting_chunks);
    x_ReadVectorOfVectorsForTest(kTestName, "AbsoluteContexts", 
                                 absolute_contexts);
    x_ReadVectorOfVectorsForTest(kTestName, "ContextOffsets", 
                                 context_offset_corrections);

    x_TestCContextTranslator(gi_length, chunk_size, num_chunks, eBlastx,
                             starting_chunks, absolute_contexts,
                             &context_offset_corrections,
                             eNa_strand_minus);
}
#endif


/// Tests the CQuerySplitter class when no splitting should occur
BOOST_AUTO_TEST_CASE(QuerySplitter_NoSplit) {
    CAutoEnvironmentVariable tmp_env("CHUNK_SIZE", "40000");
    const string kTestName("QuerySplitter_NoSplit");
    CBlastQueryVector query;
    CSeq_id id(CSeq_id::e_Gi, 555);
    query.AddQuery(CTestObjMgr::Instance().CreateBlastSearchQuery(id));

    CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(query));
    CRef<CBlastOptionsHandle> opts_h(CBlastOptionsFactory::Create(eBlastn));
    CRef<CBlastOptions> opts(&opts_h->SetOptions());

    const size_t kNumChunks = m_Config->GetInt(kTestName, "NumChunks",
                                               kDefaultIntValue);
    CRef<CQuerySplitter> splitter(new CQuerySplitter(qf, &*opts));

    BOOST_REQUIRE_EQUAL(false, splitter->IsQuerySplit());
    BOOST_REQUIRE_EQUAL(m_Config->GetInt(kTestName, "ChunkSize",
                                          kDefaultIntValue),
                         (int)splitter->GetChunkSize());
    BOOST_REQUIRE_EQUAL(kNumChunks, (size_t)splitter->GetNumberOfChunks());

    CRef<CSplitQueryBlk> sqb = splitter->Split();
    BOOST_REQUIRE_EQUAL(false, splitter->IsQuerySplit());
    BOOST_REQUIRE_EQUAL(kNumChunks, sqb->GetNumChunks());

    try {
        // try passing an out-of-range index
        (void)sqb->GetNumQueriesForChunk(kNumChunks + 8);
        BOOST_REQUIRE(false);
    } catch (const runtime_error&) {
        BOOST_REQUIRE(true);
    }

    CRef<IQueryFactory> chunk_query_factory =
        splitter->GetQueryFactoryForChunk(0);
    BOOST_REQUIRE_EQUAL(qf, chunk_query_factory);
}

/// Tests the CQuerySplitter class for retrieval of IQueryFactory objects
/// for given chunks
BOOST_AUTO_TEST_CASE(QuerySplitter_ValidateQueryFactoriesBlastn) {
    CAutoEnvironmentVariable tmp_env("CHUNK_SIZE", "30000");
    TGiLengthVector gi_length;
    gi_length.push_back(make_pair<int, size_t>(95116755, 35000));
    gi_length.push_back(make_pair<int, size_t>(112123020, 35580));

    TSeqLocVector queries;
    s_ConvertToBlastQueries(gi_length, queries);

    CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(queries));
    CRef<CBlastOptionsHandle> opts_h(CBlastOptionsFactory::Create(eBlastn));
    CRef<CBlastOptions> opts(&opts_h->SetOptions());

    CRef<CQuerySplitter> splitter(new CQuerySplitter(qf, &*opts));
    const size_t kNumChunks(2);

    try {
        (void)splitter->GetQueryFactoryForChunk(kNumChunks);
        BOOST_REQUIRE(false);
    } catch (const out_of_range& ) {
        BOOST_REQUIRE(true);
    }

    CRef<IQueryFactory> chunk_0 = splitter->GetQueryFactoryForChunk(0);
    CRef<IQueryFactory> chunk_1 = splitter->GetQueryFactoryForChunk(1);

    BOOST_REQUIRE(chunk_0 != qf);
    BOOST_REQUIRE(chunk_1 != qf);

    BOOST_REQUIRE(chunk_0.NotEmpty());
    BOOST_REQUIRE(chunk_1.NotEmpty());
}

BOOST_AUTO_TEST_CASE(CalculateNumberChunks)
{
    EBlastProgramType program = eBlastTypeBlastx;
    size_t chunk_size = 10002;
    Uint4 retval = SplitQuery_CalculateNumChunks(program, 
                       &chunk_size, 10240000, 1);
    BOOST_REQUIRE_EQUAL(1055, retval);

    retval = SplitQuery_CalculateNumChunks(eBlastTypeBlastx,
                       &chunk_size, chunk_size/2, 1);

    BOOST_REQUIRE_EQUAL(1, retval);

    retval = SplitQuery_CalculateNumChunks(program,
                       &chunk_size, 
                       3*chunk_size-2*SplitQuery_GetOverlapChunkSize(program), 1);

    BOOST_REQUIRE_EQUAL(3, retval);

    retval = SplitQuery_CalculateNumChunks(program,
                       &chunk_size, 
                       1+2*chunk_size+SplitQuery_GetOverlapChunkSize(program), 1);

    BOOST_REQUIRE_EQUAL(2, retval);
}

BOOST_AUTO_TEST_CASE(InvalidChunkSizeBlastx)
{
    CAutoEnvironmentVariable tmp_env("CHUNK_SIZE", "40000");
    BOOST_REQUIRE_THROW(SplitQuery_GetChunkSize(blast::eBlastx), CBlastException);
}

BOOST_AUTO_TEST_CASE(InvalidChunkSizeTblastx) 
{
    CAutoEnvironmentVariable tmp_env("CHUNK_SIZE", "40000");
    BOOST_REQUIRE_THROW(SplitQuery_GetChunkSize(blast::eTblastx), CBlastException);
}

BOOST_AUTO_TEST_SUITE_END()
