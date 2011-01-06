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
 * Author:  Ilya Dondoshansky
 *
 * File Description:
 *   Unit test for low complexity filtering
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <corelib/ncbitime.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/simple/simple_om.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <serial/iterator.hpp>
#include <util/random_gen.hpp>
#include <objmgr/util/sequence.hpp>


#include <algo/blast/api/blast_aux.hpp>
#include "blast_objmgr_priv.hpp"

#include <algo/blast/api/bl2seq.hpp>
#include <algo/blast/api/blast_options_handle.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>

#include <algo/blast/core/blast_setup.h>

// For repeats and dust filtering only
#include <algo/blast/api/repeats_filter.hpp>
#include <algo/blast/api/windowmask_filter.hpp>
#include "winmask_filter.hpp"
#include "dust_filter.hpp"
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>

#include "test_objmgr.hpp"
#include "blast_test_util.hpp"

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;

typedef vector<TSeqRange> TRangeVector;

static BlastSeqLoc*
s_RangeVector2BlastSeqLoc(const TRangeVector& rv)
{
    BlastSeqLoc* retval = NULL;

    if (rv.empty()) {
        return retval;
    }

    BlastSeqLoc* tail = NULL;
    ITERATE(TRangeVector, itr, rv) {
        tail = BlastSeqLocNew(tail ? &tail : &retval, 
                              itr->GetFrom(), 
                              itr->GetTo());
    }

    return retval;
}

static void x_TestGetSeqLocInfoVector(EBlastProgramType program, 
                                      size_t num_seqs)
{
    const string kProgName(Blast_ProgramNameFromType(program));
    typedef vector< CRef<CSeq_id> > TSeqIds;
    TSeqIds seqid_v(num_seqs);
    generate(seqid_v.begin(), seqid_v.end(),
             TestUtil::GenerateRandomSeqid_Gi);
    CPacked_seqint seqintervals;
    ITERATE(TSeqIds, seqid, seqid_v) {
        seqintervals.AddInterval(**seqid, 0, 100000);
    }

    const size_t kNumContexts(GetNumberOfContexts(program));
    CBlastMaskLoc mask(BlastMaskLocNew(num_seqs*kNumContexts));

    // Fill the masks 
    const TSeqPos kOffsetLength(30);
    for (int index = 0; index < mask->total_size; ++index) {
        mask->seqloc_array[index] = BlastSeqLocNew(NULL, index, 
                                                   index+kOffsetLength);
    }
    TSeqLocInfoVector mask_v;
    Blast_GetSeqLocInfoVector(program, seqintervals, mask, mask_v);
    BOOST_REQUIRE_EQUAL(num_seqs, mask_v.size());

    unsigned int qindex(0); // query index
    ITERATE(TSeqLocInfoVector, query_masks_list, mask_v) {
        const size_t kNumMasks = program == eBlastTypeBlastn 
            ? 1 : kNumContexts;
        BOOST_REQUIRE_MESSAGE( kNumMasks == query_masks_list->size(),
                               "Failed on " + kProgName);
        size_t context = 0;
        ITERATE(TMaskedQueryRegions, itr, *query_masks_list) {
            CNcbiOstrstream ss;
            ss << "Error in query number " << qindex << ", context " 
               << context << " ('" << kProgName << "')";
            // Validate the frame
            int frame = program == eBlastTypeBlastn
                ? CSeqLocInfo::eFrameNotSet
                : BLAST_ContextToFrame(program, context);
            BOOST_REQUIRE_MESSAGE(frame == (*itr)->GetFrame(), 
                                  (string)CNcbiOstrstreamToString(ss));

            // Validate the artificially built offsets of the mask
            const BlastSeqLoc* loc = 
                mask->seqloc_array[kNumContexts*qindex+context];
            BOOST_REQUIRE(loc != NULL);
            TSeqRange offsets(loc->ssr->left, loc->ssr->right);;
            BOOST_REQUIRE_MESSAGE
                (offsets.GetFrom() == (*itr)->GetInterval().GetFrom(), 
                 (string)CNcbiOstrstreamToString(ss));
            BOOST_REQUIRE_MESSAGE
                (offsets.GetTo() == (*itr)->GetInterval().GetTo(),
                 (string)CNcbiOstrstreamToString(ss));
            ++context;
        }
        BOOST_REQUIRE_EQUAL(kNumMasks, context);
        ++qindex;
    }
}

// Returns true if *all* bases in the range provided are masked
static bool x_AreAllBasesMasked(const Uint1* sequence, int start, int stop) 
{
    BOOST_CHECK(start <= stop);
    for (int i = start; i < stop; i++) {
        if (sequence[i] != kNuclMask) {
            return false;
        }
    }
    return true;
}

class CBlastFilterTest {
public:
    static void x_TestLowerCaseMaskWith(ENa_strand strand,
                                        bool ignore_strand_in_mask)
    {
        const int kNumLcaseLocs = 11;
        const int kLcaseStarts[kNumLcaseLocs] = 
            { 0, 78, 217, 380, 694, 1018, 1128, 2817, 3084, 3428, 3782 };
        const int kLcaseEnds[kNumLcaseLocs] = 
            { 75, 208, 316, 685, 1004, 1122, 1298, 2952, 3409, 3733, 3916 };

        int i = 0;      // loop index
        const int kQuerySize = 9180;
        vector<int> kLcaseStartsNegStrand, kLcaseEndsNegStrand;
        kLcaseStartsNegStrand.reserve(kNumLcaseLocs);
        kLcaseEndsNegStrand.reserve(kNumLcaseLocs);
        for (i = 0; i < kNumLcaseLocs; i++) {
            int start = kQuerySize - 1 - kLcaseEnds[i];
            int stop = kQuerySize - 1 - kLcaseStarts[i];
            kLcaseStartsNegStrand.push_back(start);
            kLcaseEndsNegStrand.push_back(stop);
        }

        CSeq_id id("gi|1945388");
        auto_ptr<SSeqLoc> qsl(
            CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_both));
        // Fill the lower case mask into the SSeqLoc
        CSeq_loc* seqloc = new CSeq_loc();
        for (int index = 0; index < kNumLcaseLocs; ++index) {
            seqloc->SetPacked_int().AddInterval(id, kLcaseStarts[index],
                                                kLcaseEnds[index]);
            BOOST_CHECK(!seqloc->GetPacked_int().Get().back()->CanGetStrand());
            seqloc->SetPacked_int().Set().back()->SetStrand(strand);
        }
        qsl->mask.Reset(seqloc);
        qsl->ignore_strand_in_mask = ignore_strand_in_mask;

        TSeqLocVector query_v;
        query_v.push_back(*qsl);
        CBlastNucleotideOptionsHandle nucl_handle;
        nucl_handle.SetDustFiltering(false);
        nucl_handle.SetMaskAtHash(false);

        // Run a self hit BLAST search, discard the return value, and get the
        // masked query regions
        blast::CBl2Seq blaster(*qsl.get(), *qsl.get(), nucl_handle);
        (void) blaster.Run();

        // check that the actual query sequence was masked at the proper
        // locations
        BOOST_CHECK_EQUAL(false, nucl_handle.GetMaskAtHash());
        for (i = 0; i < kNumLcaseLocs; i++) {
            const pair<int, int> range_plus(kLcaseStarts[i], kLcaseEnds[i]);
            const pair<int, int> range_minus(kLcaseStartsNegStrand[i], 
                                             kLcaseEndsNegStrand[i]);
            int starting_offset = 0;

            if (ignore_strand_in_mask || strand == eNa_strand_both) {
                starting_offset = 
                    blaster.mi_clsQueryInfo->contexts[0].query_offset;
                BOOST_CHECK(x_AreAllBasesMasked
                               (blaster.mi_clsQueries->sequence,
                                starting_offset + range_plus.first,
                                starting_offset + range_plus.second));

                starting_offset = 
                    blaster.mi_clsQueryInfo->contexts[1].query_offset;
                BOOST_CHECK(x_AreAllBasesMasked
                               (blaster.mi_clsQueries->sequence,
                                starting_offset + range_minus.first,
                                starting_offset + range_minus.second));
            } else {

                if (strand == eNa_strand_plus) {
                    starting_offset = 
                        blaster.mi_clsQueryInfo->contexts[0].query_offset;
                    BOOST_CHECK(x_AreAllBasesMasked
                                   (blaster.mi_clsQueries->sequence,
                                    starting_offset + range_plus.first,
                                    starting_offset + range_plus.second));

                    starting_offset = 
                        blaster.mi_clsQueryInfo->contexts[1].query_offset;
                    BOOST_CHECK(!x_AreAllBasesMasked
                                   (blaster.mi_clsQueries->sequence,
                                    starting_offset + range_minus.first,
                                    starting_offset + range_minus.second));
                } else if (strand == eNa_strand_minus) {
                    starting_offset = 
                        blaster.mi_clsQueryInfo->contexts[0].query_offset;
                    BOOST_CHECK(!x_AreAllBasesMasked
                                   (blaster.mi_clsQueries->sequence,
                                    starting_offset + range_plus.first,
                                    starting_offset + range_plus.second));
                    starting_offset = 
                        blaster.mi_clsQueryInfo->contexts[1].query_offset;
                    BOOST_CHECK(x_AreAllBasesMasked
                                   (blaster.mi_clsQueries->sequence,
                                    starting_offset + range_minus.first,
                                    starting_offset + range_minus.second));
                } else {
                    abort();
                }
            }
        }

        // Check that the masked regions (returned as part of the original
        // SSeqLoc.mask field or from CBl2Seq::GetFilteredQueryRegions) are
        // those on the plus strand only
        TSeqLocInfoVector masked_regions_vector = 
            blaster.GetFilteredQueryRegions();
        BOOST_CHECK(masked_regions_vector.size() == 1);
        BOOST_CHECK_EQUAL(masked_regions_vector.front().size(),
                             (size_t)kNumLcaseLocs);

        BOOST_CHECK(query_v[0].mask->IsPacked_int());
        BOOST_CHECK_EQUAL(query_v[0].mask->GetPacked_int().Get().size(),
                             masked_regions_vector.front().size());
        int loc_index = 0;
        ITERATE(list< CRef<CSeq_interval> >, itr, 
                query_v[0].mask->GetPacked_int().Get()) {
            BOOST_CHECK_EQUAL(kLcaseStarts[loc_index], (int)(*itr)->GetFrom());
            BOOST_CHECK_EQUAL(kLcaseEnds[loc_index], (int)(*itr)->GetTo());
            ++loc_index;
        }
        BOOST_CHECK_EQUAL(kNumLcaseLocs, loc_index);

        loc_index = 0;
        ITERATE(TMaskedQueryRegions, itr, masked_regions_vector[0]) {
            const CSeq_interval& intv = (*itr)->GetInterval();
            BOOST_CHECK_EQUAL(kLcaseStarts[loc_index], (int)intv.GetFrom());
            BOOST_CHECK_EQUAL(kLcaseEnds[loc_index], (int)intv.GetTo());
            BOOST_CHECK(!intv.CanGetStrand());
            BOOST_CHECK_EQUAL((*itr)->GetFrame(), 
                                 (int)CSeqLocInfo::eFrameNotSet);
            loc_index++;
        }

        BOOST_CHECK_EQUAL(kNumLcaseLocs, loc_index);
    }
};

BOOST_AUTO_TEST_SUITE(blastfilter)

static void x_TestGetFilteredQueryRegions(ENa_strand strand) {
    typedef vector< pair<TSeqPos, TSeqPos> > TSegments;
    TSegments masked_offsets;
    masked_offsets.push_back(make_pair(298U, 305U));
    masked_offsets.push_back(make_pair(875U, 882U));
    masked_offsets.push_back(make_pair(1018U, 1115U));
    masked_offsets.push_back(make_pair(1449U, 1479U));
    masked_offsets.push_back(make_pair(3113U, 3133U));
    masked_offsets.push_back(make_pair(3282U, 3298U));
    masked_offsets.push_back(make_pair(3428U, 3441U));
    masked_offsets.push_back(make_pair(3598U, 3606U));
    masked_offsets.push_back(make_pair(4704U, 4710U));
    masked_offsets.push_back(make_pair(6364U, 6373U));
    masked_offsets.push_back(make_pair(6512U, 6573U));
    masked_offsets.push_back(make_pair(7600U, 7672U));
    masked_offsets.push_back(make_pair(7766U, 7772U));
    masked_offsets.push_back(make_pair(8873U, 8880U));
    masked_offsets.push_back(make_pair(9114U, 9179U));

    const size_t kNumQueries(1);
    const size_t kNumLocs(masked_offsets.size());
    size_t index(0);

    CSeq_id id("gi|1945388");
    auto_ptr<SSeqLoc> qsl(
                          CTestObjMgr::Instance().CreateSSeqLoc(id, strand));
    TSeqLocVector query_reference(kNumQueries, *qsl);
    TSeqLocVector query_test(kNumQueries, *qsl);
    CBlastNucleotideOptionsHandle nucl_handle;

    // Filter the query regions using the C++ APIs
    Blast_FindDustFilterLoc(query_reference, &nucl_handle);
    BOOST_CHECK(query_reference[0].mask->IsPacked_int());
    const CPacked_seqint::Tdata& seqinterval_list = 
        query_reference[0].mask->GetPacked_int().Get();
    BOOST_CHECK_EQUAL(kNumLocs, seqinterval_list.size());
    // CSeq_loc_mapper returns intervals sorted in reverse order if on minus strand.
    bool reverse = IsReverse(query_reference[0].mask->GetStrand());
    index = reverse ? masked_offsets.size() - 1 : 0;
    ITERATE(CPacked_seqint::Tdata, itr, seqinterval_list) {
        BOOST_CHECK_EQUAL(masked_offsets[index].first, 
                          (*itr)->GetFrom());
        BOOST_CHECK_EQUAL(masked_offsets[index].second, 
                          (*itr)->GetTo());
        reverse ? index-- : index++;
    }

    // Run a self hit BLAST search, discard the return value, and get the
    // masked query regions
    blast::CBl2Seq blaster(query_test, query_test, nucl_handle);
    (void) blaster.Run();
    TSeqLocInfoVector masked_regions_vector = 
        blaster.GetFilteredQueryRegions();

    BOOST_CHECK_EQUAL(kNumQueries, query_reference.size());
    BOOST_CHECK_EQUAL(kNumQueries, query_test.size());
    BOOST_CHECK_EQUAL(kNumQueries, masked_regions_vector.size());

    TMaskedQueryRegions& masked_regions = *masked_regions_vector.begin();
    BOOST_CHECK_EQUAL(kNumLocs, masked_regions.size());
    index = 0;
    ITERATE(TMaskedQueryRegions, itr, masked_regions) {
        BOOST_CHECK_EQUAL(masked_offsets[index].first,
                          (*itr)->GetInterval().GetFrom());
        BOOST_CHECK_EQUAL(masked_offsets[index].second,
                          (*itr)->GetInterval().GetTo());
        index++;
    }
}

BOOST_AUTO_TEST_CASE(TSeqLocVector2Packed_seqint_TestIntervals) {

    vector< CRef<CSeq_id> > gis;
    gis.push_back(CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Gi, 6)));
    gis.push_back(CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Gi, 129295)));
    gis.push_back(CRef<CSeq_id>(new CSeq_id(CSeq_id::e_Gi, 15606659)));

    vector<TSeqRange> ranges;
    ranges.push_back(TSeqRange(10, 100));
    ranges.push_back(TSeqRange(100, 200));
    ranges.push_back(TSeqRange(50, 443));

    BOOST_REQUIRE(gis.size() == ranges.size());
    TSeqLocVector input(gis.size());
    size_t i(0);
    for (i = 0; i < gis.size(); i++) {
        CRef<CSeq_loc> seqloc(new CSeq_loc(*gis[i], 
                                           ranges[i].GetFrom(),
                                           ranges[i].GetTo()));
        input[i] = SSeqLoc(seqloc, CSimpleOM::NewScope());
    }

    CRef<CPacked_seqint> packed_seqint(TSeqLocVector2Packed_seqint(input));
    i = 0;
    ITERATE(CPacked_seqint::Tdata, query_interval, packed_seqint->Get()) {
        BOOST_REQUIRE(gis[i]->Match((*query_interval)->GetId()));
        BOOST_REQUIRE_EQUAL(ranges[i].GetFrom(), 
                            (*query_interval)->GetFrom());
        BOOST_REQUIRE_EQUAL(ranges[i].GetTo(), 
                            (*query_interval)->GetTo());
        i++;
    }
}

BOOST_AUTO_TEST_CASE(TSeqLocVector2Packed_seqint_TestNoIntervals) {
    typedef pair<int, TSeqPos> TGiLength;
    vector<TGiLength> gis;
    gis.push_back(make_pair(6, 342U));
    gis.push_back(make_pair(129295, 232U));
    gis.push_back(make_pair(15606659, 443U));

    TSeqLocVector input;
    input.reserve(gis.size());
    ITERATE(vector<TGiLength>, gi, gis) {
        CRef<CSeq_loc> seqloc(new CSeq_loc);
        seqloc->SetWhole().SetGi(gi->first);
        input.push_back(SSeqLoc(seqloc, CSimpleOM::NewScope()));
    }

    CRef<CPacked_seqint> packed_seqint(TSeqLocVector2Packed_seqint(input));
    int i(0);
    const TSeqPos kStartingPosition(0);
    ITERATE(CPacked_seqint::Tdata, query_interval, packed_seqint->Get()) {
        const TGiLength& kGiLength = gis[i++];
        const CSeq_id kTargetId(CSeq_id::e_Gi, kGiLength.first);
        BOOST_REQUIRE(kTargetId.Match((*query_interval)->GetId()));
        BOOST_REQUIRE_EQUAL(kStartingPosition, 
                            (*query_interval)->GetFrom());
        BOOST_REQUIRE_EQUAL(kGiLength.second, 
                            (*query_interval)->GetTo());
    }
}

BOOST_AUTO_TEST_CASE(TSeqLocVector2Packed_seqint_TestEmptyInput) {
    TSeqLocVector empty;
    CRef<CPacked_seqint> retval(TSeqLocVector2Packed_seqint(empty));
    BOOST_REQUIRE(retval.Empty());
}

void setupQueryStructures(TSeqLocVector& query_vector,
                          const CBlastOptions& kOpts,
                          BLAST_SequenceBlk** query_blk,
                          BlastQueryInfo** qinfo)
{
    TSearchMessages blast_msg;

    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_vector, prog, strand_opt, qinfo); 
    CBlastQueryInfo qi_tmp(*qinfo);
    SetupQueries(query_vector, qi_tmp, query_blk,
                 prog, strand_opt, blast_msg);
    qi_tmp.Release();
    ITERATE(TSearchMessages, m, blast_msg) {
        BOOST_REQUIRE(m->empty());
    }
}

BOOST_AUTO_TEST_CASE(SegFilter) {
    const int kNumLocs = 3;
    const int kSegStarts[kNumLocs] = { 15, 55, 495 };
    const int kSegEnds[kNumLocs] = { 27, 68, 513 };
    CSeq_id id("gi|3091");
    auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(id));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastp));

    setupQueryStructures(query_v, opts->GetOptions(), 
                         &query_blk, &query_info);

    BlastSeqLoc *filter_slp = NULL, *loc;
    SBlastFilterOptions* filtering_options;
    SBlastFilterOptionsNew(&filtering_options, eSeg);
    Int2 status = BlastSetUp_Filter(opts->GetOptions().GetProgramType(),
                                    query_blk->sequence,
                                    query_info->contexts[0].query_length,
                                    0,
                                    filtering_options,
                                    & filter_slp, NULL);
    filtering_options = SBlastFilterOptionsFree(filtering_options);
    BOOST_REQUIRE(status == 0);
        
    Int4 loc_index;
    SSeqRange* di;
    for (loc_index=0, loc = filter_slp; loc; loc = loc->next, ++loc_index) {
        di = loc->ssr;
        BOOST_REQUIRE_EQUAL(kSegStarts[loc_index], di->left);
        BOOST_REQUIRE_EQUAL(kSegEnds[loc_index], di->right);
    }
    BlastSeqLocFree(filter_slp);

    BOOST_REQUIRE_EQUAL(kNumLocs, loc_index);
}
    
BOOST_AUTO_TEST_CASE(RepeatsFilter) {
    const size_t kNumLocs = 5;
    const TSeqPos kRepeatStarts[kNumLocs] = { 0, 380, 1014, 2851, 3113 };
    const TSeqPos kRepeatEnds[kNumLocs] = { 212, 1003, 1297, 2953, 3730 };
    CSeq_id id("gi|1945388");
    auto_ptr<SSeqLoc> qsl(
                          CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_both));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);

    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetRepeatFiltering(true);
    Blast_FindRepeatFilterLoc(query_v, &nucl_handle);

    BOOST_REQUIRE(query_v[0].mask.NotEmpty());
    BOOST_REQUIRE(query_v[0].mask->IsPacked_int());
    const CPacked_seqint::Tdata& seqinterval_list = 
        query_v[0].mask->GetPacked_int().Get();

    size_t loc_index = 0;
    BOOST_REQUIRE_EQUAL(kNumLocs, seqinterval_list.size());
    ITERATE(CPacked_seqint::Tdata, itr,  seqinterval_list) {
        BOOST_REQUIRE_EQUAL(kRepeatStarts[loc_index], (*itr)->GetFrom());
        BOOST_REQUIRE_EQUAL(kRepeatEnds[loc_index], (*itr)->GetTo());
        BOOST_REQUIRE(!(*itr)->CanGetStrand());
        ++loc_index;
    }

    BOOST_REQUIRE_EQUAL(kNumLocs, loc_index);
}

BOOST_AUTO_TEST_CASE(WindowMasker)
{
    int pair_size = sizeof(TSeqPos) * 2;
    
    const TSeqPos intervals[] =
        { 0, 79,
          100, 122,
          146, 169,
          225, 248,
          286, 329,
          348, 366,
          373, 688,
          701, 971,
          974, 1303,
          1450, 1485,
          2858, 2887,
          3086, 3212,
          3217, 3735,
          4142, 4162,
          5423, 5443,
          5797, 5817,
          6333, 6383,
          6458, 6477,
          6519, 6539,
          7043, 7063,
          7170, 7189,
          7604, 7623,
          8454, 8476,
          8829, 8851,
          8860, 8889 };
    
    size_t num_locs = sizeof(intervals) / pair_size;
    BOOST_REQUIRE(0 == (sizeof(intervals) % pair_size));
    
    CSeq_id id("gi|1945388");
    auto_ptr<SSeqLoc>
        qsl(CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_both));
    
    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    
    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetWindowMaskerTaxId(9606);
    Blast_FindWindowMaskerLoc(query_v, &nucl_handle);
    
    BOOST_REQUIRE(query_v[0].mask.NotEmpty());
    BOOST_REQUIRE(query_v[0].mask->IsPacked_int());
    const CPacked_seqint::Tdata& seqinterval_list = 
        query_v[0].mask->GetPacked_int().Get();
    
    size_t loc_index = 0;
    BOOST_REQUIRE_EQUAL(num_locs, seqinterval_list.size());
    
    ITERATE(CPacked_seqint::Tdata, itr,  seqinterval_list) {
        BOOST_REQUIRE_EQUAL(intervals[loc_index],   (*itr)->GetFrom());
        BOOST_REQUIRE_EQUAL(intervals[loc_index+1], (*itr)->GetTo());
        BOOST_REQUIRE(! (*itr)->CanGetStrand());
        loc_index += 2;
    }
    
    BOOST_REQUIRE_EQUAL(num_locs*2, loc_index);
}

BOOST_AUTO_TEST_CASE(RepeatsFilter_OnSeqInterval) {
    vector<TSeqRange> masked_regions;
    masked_regions.push_back(TSeqRange(85028, 85528));
    masked_regions.push_back(TSeqRange(85539, 85736));
    masked_regions.push_back(TSeqRange(86334, 86461));
    masked_regions.push_back(TSeqRange(86730, 87043));
    masked_regions.push_back(TSeqRange(87313, 87370));
    masked_regions.push_back(TSeqRange(88134, 88140));
    masked_regions.push_back(TSeqRange(88171, 88247));
    masked_regions.push_back(TSeqRange(88254, 88480));
    masked_regions.push_back(TSeqRange(89032, 89152));
    masked_regions.push_back(TSeqRange(91568, 91704));
    masked_regions.push_back(TSeqRange(92355, 92545));
    masked_regions.push_back(TSeqRange(92550, 92973));
    masked_regions.push_back(TSeqRange(92983, 93384));
    masked_regions.push_back(TSeqRange(93472, 93642));
    masked_regions.push_back(TSeqRange(93702, 94026));

    CSeq_id id("gi|20196551");
    auto_ptr<SSeqLoc> qsl(
                          CTestObjMgr::Instance().CreateSSeqLoc(id, 
                                                                make_pair<TSeqPos, TSeqPos>(84999, 94637),
                                                                eNa_strand_both));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);

    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetDustFiltering(true);
    nucl_handle.SetRepeatFiltering(true);
    Blast_FindDustFilterLoc(query_v, &nucl_handle);
    Blast_FindRepeatFilterLoc(query_v, &nucl_handle);

    BOOST_REQUIRE(query_v[0].mask->IsPacked_int());
    const CPacked_seqint::Tdata& seqinterval_list = 
        query_v[0].mask->GetPacked_int().Get();

    size_t loc_index = 0;
    // BOOST_REQUIRE_EQUAL(masked_regions.size(), seqinterval_list.size());
    ITERATE(CPacked_seqint::Tdata, itr,  seqinterval_list) {
        BOOST_REQUIRE_EQUAL(masked_regions[loc_index].GetFrom(), 
                            (*itr)->GetFrom());
        BOOST_REQUIRE_EQUAL(masked_regions[loc_index].GetTo(), 
                            (*itr)->GetTo());
        BOOST_REQUIRE(!(*itr)->CanGetStrand());
        ++loc_index;
    }

    BOOST_REQUIRE_EQUAL(masked_regions.size(), loc_index);
}

BOOST_AUTO_TEST_CASE(CSeqLocInfo_EqualityOperators)
{
    CSeq_id id("gi|197670657");
    TSeqRange r(1, 100);
    CSeqLocInfo a(id, r, (int)CSeqLocInfo::eFramePlus1);
    CSeqLocInfo b(id, r, (int)CSeqLocInfo::eFramePlus1);
    BOOST_REQUIRE(a == b);

    b.SetFrame(2);
    BOOST_REQUIRE(a != b);
}

BOOST_AUTO_TEST_CASE(CombineDustAndLowerCaseMasking_WithBlastQueryVector) {
    CSeq_id id("gi|197670657");
    TSeqRange r(2, 299);
    CRef<CSeqLocInfo> lower_case_mask
        (new CSeqLocInfo(id, r, (int)CSeqLocInfo::eFramePlus1));
    CRef<blast::CBlastSearchQuery> query =
        CTestObjMgr::Instance().CreateBlastSearchQuery(id, eNa_strand_both);
    query->AddMask(lower_case_mask);
    CBlastQueryVector queries;
    queries.AddQuery(query);

    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetDustFiltering(true);
    Blast_FindDustFilterLoc(queries, 
                            nucl_handle.GetDustFilteringLevel(),
                            nucl_handle.GetDustFilteringWindow(),
                            nucl_handle.GetDustFilteringLinker());
    TMaskedQueryRegions mqr = queries.GetMaskedRegions(0);

    BOOST_REQUIRE( !mqr.empty() );
    try { CRef<CSeq_loc> masks = queries.GetMasks(0); }
    catch (const CBlastException& e) {
        BOOST_REQUIRE(e.GetErrCode() == CBlastException::eNotSupported);
        BOOST_REQUIRE(e.GetMsg().find("lossy direction") != NPOS);
    }

    CRef<CSeqLocInfo> sli = mqr.front();
    BOOST_REQUIRE(sli.NotEmpty());
    BOOST_REQUIRE(*sli == *lower_case_mask);
    BOOST_REQUIRE_EQUAL((int)2, (int)mqr.size());
    BOOST_REQUIRE(mqr.front()->GetFrame() == 1);
    BOOST_REQUIRE(mqr.back()->GetFrame() == -1);
}


BOOST_AUTO_TEST_CASE(RepeatsAndDustFilter) {

    CSeq_id id1("gi|197333738");
    auto_ptr<SSeqLoc> qsl1(CTestObjMgr::Instance().CreateSSeqLoc(id1)); 
    TSeqLocVector query_v1;
    query_v1.push_back(*qsl1);

    CSeq_id id2("gi|197333738");
    auto_ptr<SSeqLoc> qsl2(CTestObjMgr::Instance().CreateSSeqLoc(id2)); 
    TSeqLocVector query_v2;
    query_v2.push_back(*qsl2);

    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetDustFiltering(true);
    nucl_handle.SetRepeatFiltering(true);

    Blast_FindDustFilterLoc(query_v1, &nucl_handle);
    Blast_FindRepeatFilterLoc(query_v1, &nucl_handle);


    Blast_FindRepeatFilterLoc(query_v2, &nucl_handle);
    Blast_FindDustFilterLoc(query_v2, &nucl_handle);

    BOOST_REQUIRE_EQUAL(sequence::Compare(*(query_v1[0].mask), *(query_v2[0].mask), qsl1->scope), sequence::eSame); 
}

BOOST_AUTO_TEST_CASE(WindowMaskerAndDustFilter) {

    CSeq_id id1("gi|197333738");
    auto_ptr<SSeqLoc> qsl1(CTestObjMgr::Instance().CreateSSeqLoc(id1)); 
    TSeqLocVector query_v1;
    query_v1.push_back(*qsl1);

    CSeq_id id2("gi|197333738");
    auto_ptr<SSeqLoc> qsl2(CTestObjMgr::Instance().CreateSSeqLoc(id2)); 
    TSeqLocVector query_v2;
    query_v2.push_back(*qsl2);

    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetDustFiltering(true);
    nucl_handle.SetWindowMaskerTaxId(9606);

    Blast_FindDustFilterLoc(query_v1, &nucl_handle);
    Blast_FindWindowMaskerLoc(query_v1, &nucl_handle);


    Blast_FindWindowMaskerLoc(query_v2, &nucl_handle);
    Blast_FindDustFilterLoc(query_v2, &nucl_handle);

    BOOST_REQUIRE_EQUAL(sequence::Compare(*(query_v1[0].mask), *(query_v2[0].mask), qsl1->scope), sequence::eSame); 
}

BOOST_AUTO_TEST_CASE(WindowMasker_OnSeqInterval)
{
    // these are from window masker and dust
    vector<TSeqRange> masked_regions;
    masked_regions.push_back(TSeqRange(85019, 85172));
    masked_regions.push_back(TSeqRange(85190, 85345));
    masked_regions.push_back(TSeqRange(85385, 85452));
    masked_regions.push_back(TSeqRange(85483, 85505));
    masked_regions.push_back(TSeqRange(85511, 85533));
    masked_regions.push_back(TSeqRange(85575, 85596));
    masked_regions.push_back(TSeqRange(85673, 85694));
    masked_regions.push_back(TSeqRange(85725, 85745));
    
    CSeq_id id("gi|20196551");
    auto_ptr<SSeqLoc>
        qsl(CTestObjMgr::Instance().CreateSSeqLoc
            (id, make_pair<TSeqPos, TSeqPos>(85000, 86200), eNa_strand_both));
    
    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    
    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetDustFiltering(true);
    nucl_handle.SetWindowMaskerTaxId(9606);
    
    Blast_FindDustFilterLoc(query_v, &nucl_handle);
    Blast_FindWindowMaskerLoc(query_v, &nucl_handle);

    BOOST_REQUIRE(query_v[0].mask->IsPacked_int());
    const CPacked_seqint::Tdata& seqinterval_list = 
        query_v[0].mask->GetPacked_int().Get();
    
    size_t loc_index = 0;
    BOOST_REQUIRE_EQUAL(masked_regions.size(), seqinterval_list.size());
    
    ITERATE(CPacked_seqint::Tdata, itr,  seqinterval_list) {
        BOOST_REQUIRE_EQUAL(masked_regions[loc_index].GetFrom(), 
                            (*itr)->GetFrom());
        BOOST_REQUIRE_EQUAL(masked_regions[loc_index].GetTo(), 
                            (*itr)->GetTo());
        BOOST_REQUIRE(!(*itr)->CanGetStrand());
        ++loc_index;
    }
    
    BOOST_REQUIRE_EQUAL(masked_regions.size(), loc_index);
}

BOOST_AUTO_TEST_CASE(RepeatsFilter_NoHitsFound) {
    CSeq_id id("gi|33079743");
    auto_ptr<SSeqLoc> qsl(
                          CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_both));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);

    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetRepeatFiltering(true);
    nucl_handle.SetRepeatFilteringDB("repeat/repeat_9606");
    Blast_FindRepeatFilterLoc(query_v, &nucl_handle);

    BOOST_REQUIRE(query_v[0].mask.Empty());
}

BOOST_AUTO_TEST_CASE(WindowMasker_NoHitsFound) {
    CSeq_id id("gi|33079743");
    auto_ptr<SSeqLoc> qsl
        (CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_both));
    
    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    
    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetWindowMaskerTaxId(9606);
    
    Blast_FindRepeatFilterLoc(query_v, &nucl_handle);
    
    BOOST_REQUIRE(query_v[0].mask.Empty());
}

BOOST_AUTO_TEST_CASE(RepeatsFilterWithMissingParameter) {
    CSeq_id id("gi|1945388");
    auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(id));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);

    CBlastNucleotideOptionsHandle nucl_handle;
    // note the missing argument to the repeats database
    nucl_handle.SetFilterString("m L; R -d ");/* NCBI_FAKE_WARNING */
    BOOST_REQUIRE_THROW(Blast_FindRepeatFilterLoc(query_v, &nucl_handle),
                        CSeqDBException);
}

BOOST_AUTO_TEST_CASE(WindowMaskerWithMissingParameter) {
    CSeq_id id("gi|1945388");
    auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(id));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);

    CBlastNucleotideOptionsHandle nucl_handle;
    // note the missing argument to the repeats database
    nucl_handle.SetFilterString("m L; W -d ");/* NCBI_FAKE_WARNING */
    BOOST_REQUIRE_THROW(Blast_FindWindowMaskerLoc(query_v, &nucl_handle),
                        CBlastException);
}

/// Test the conversion of a BlastMaskLoc internal structure to the
/// TSeqLocInfoVector type, used in formatting.
BOOST_AUTO_TEST_CASE(TestGetFilteredQueryRegions_BothStrandsOneQuery) {
    x_TestGetFilteredQueryRegions(eNa_strand_both);
}
BOOST_AUTO_TEST_CASE(TestGetFilteredQueryRegions_PlusStrandsOneQuery) {
    x_TestGetFilteredQueryRegions(eNa_strand_plus);
}
BOOST_AUTO_TEST_CASE(TestGetFilteredQueryRegions_MinusStrandsOneQuery) {
    x_TestGetFilteredQueryRegions(eNa_strand_minus);
}

BOOST_AUTO_TEST_CASE(RestrictLowerCaseMask) {
    vector<TSeqRange> masks;
    masks.push_back(TSeqRange(0, 75));
    masks.push_back(TSeqRange(78, 208));
    masks.push_back(TSeqRange(217, 316));
    masks.push_back(TSeqRange(380, 685));
    masks.push_back(TSeqRange(694, 1004));
    masks.push_back(TSeqRange(1018, 1122));
    masks.push_back(TSeqRange(1128, 1298));
    masks.push_back(TSeqRange(2817, 2952));
    masks.push_back(TSeqRange(2084, 3409));
    masks.push_back(TSeqRange(3428, 3733));
    masks.push_back(TSeqRange(3782, 3916));

    TMaskedQueryRegions mqr;
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 1945388));
    ITERATE(vector<TSeqRange>, range, masks) {
        CRef<CSeq_interval> intv(new CSeq_interval(*id,
                                                   range->GetFrom(),
                                                   range->GetTo()));
        // N.B.: this is deliberate, because of this the return value of
        // TMaskedQueryRegions::RestrictToSeqInt() will have its strand
        // unset (see CSeq_interval parametrized constructor for that)
        BOOST_REQUIRE(intv->CanGetStrand() == false);
        CRef<CSeqLocInfo> sli(new CSeqLocInfo(intv, 
                                              CSeqLocInfo::eFrameNotSet));
        mqr.push_back(sli);
    }

    // N.B.: even a different Seq-id will work!
    CSeq_id other_id(CSeq_id::e_Gi, 555);
    CSeq_interval restriction(other_id, 0, 624);
    TMaskedQueryRegions restricted_mask;
    restricted_mask = mqr.RestrictToSeqInt(restriction);
    BOOST_REQUIRE_EQUAL((size_t)4, restricted_mask.size());
    BOOST_REQUIRE_EQUAL((TSeqPos)624, 
                        restricted_mask.back()->GetInterval().GetTo());
    BOOST_REQUIRE_EQUAL(CSeq_id::e_YES, id->Compare
                        (restricted_mask.front()->GetInterval().GetId()));
    BOOST_REQUIRE(!(restricted_mask.front()->GetInterval().CanGetStrand()));

    restriction.SetFrom(1000);
    restriction.SetTo(2000);
    restriction.SetStrand(eNa_strand_plus); // this is irrelevant
    restricted_mask = mqr.RestrictToSeqInt(restriction);
    BOOST_REQUIRE_EQUAL((size_t)3, restricted_mask.size());
    TMaskedQueryRegions::iterator itr = restricted_mask.begin();

    BOOST_REQUIRE_EQUAL((TSeqPos)1000, (*itr)->GetInterval().GetFrom());
    BOOST_REQUIRE_EQUAL((TSeqPos)1004, (*itr)->GetInterval().GetTo());
    BOOST_REQUIRE(id->Match((*itr)->GetInterval().GetId()));
    BOOST_REQUIRE(!(*itr)->GetInterval().CanGetStrand());
    BOOST_REQUIRE_EQUAL((int)CSeqLocInfo::eFrameNotSet, (*itr)->GetFrame());
    ++itr;
    BOOST_REQUIRE_EQUAL((TSeqPos)1018, (*itr)->GetInterval().GetFrom());
    BOOST_REQUIRE_EQUAL((TSeqPos)1122, (*itr)->GetInterval().GetTo());
    BOOST_REQUIRE(id->Match((*itr)->GetInterval().GetId()));
    BOOST_REQUIRE(!(*itr)->GetInterval().CanGetStrand());
    BOOST_REQUIRE_EQUAL((int)CSeqLocInfo::eFrameNotSet, (*itr)->GetFrame());
    ++itr;
    BOOST_REQUIRE_EQUAL((TSeqPos)1128, (*itr)->GetInterval().GetFrom());
    BOOST_REQUIRE_EQUAL((TSeqPos)1298, (*itr)->GetInterval().GetTo());
    BOOST_REQUIRE(id->Match((*itr)->GetInterval().GetId()));
    BOOST_REQUIRE(!(*itr)->GetInterval().CanGetStrand());
    BOOST_REQUIRE_EQUAL((int)CSeqLocInfo::eFrameNotSet, (*itr)->GetFrame());
    ++itr;
    BOOST_REQUIRE(itr == restricted_mask.end());

    restriction.SetFrom(10000);
    restriction.SetTo(20000);
    restricted_mask = mqr.RestrictToSeqInt(restriction);
    BOOST_REQUIRE(restricted_mask.empty());
}

// Inspired by JIRA SB-264
BOOST_AUTO_TEST_CASE(BlastxLowerCaseMask) {
    vector<TSeqRange> masks;
    masks.push_back(TSeqRange(0, 75));
    masks.push_back(TSeqRange(78, 208));
    masks.push_back(TSeqRange(217, 316));
    masks.push_back(TSeqRange(380, 685));
    masks.push_back(TSeqRange(694, 1004));
    masks.push_back(TSeqRange(1018, 1122));
    masks.push_back(TSeqRange(1128, 1298));
    masks.push_back(TSeqRange(2817, 2952));
    masks.push_back(TSeqRange(2084, 3409));
    masks.push_back(TSeqRange(3428, 3733));
    masks.push_back(TSeqRange(3782, 3916));

    TMaskedQueryRegions mqr;
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 1945388));
    ITERATE(vector<TSeqRange>, range, masks) {
        CRef<CSeq_interval> intv(new CSeq_interval(*id,
                                                   range->GetFrom(),
                                                   range->GetTo()));
        CRef<CSeqLocInfo> sli(new CSeqLocInfo(intv, 
                                              CSeqLocInfo::eFramePlus1));
        mqr.push_back(sli);
    }
    CBlastQueryFilteredFrames bqff(eBlastTypeBlastx, mqr);
    BOOST_REQUIRE(!bqff.Empty());
    BOOST_REQUIRE(bqff.QueryHasMultipleFrames());
    const set<CSeqLocInfo::ETranslationFrame>& frames = bqff.ListFrames();
    ITERATE(set<CSeqLocInfo::ETranslationFrame>, fr, frames) {
        BOOST_REQUIRE(bqff[*fr] != NULL);
    }
    BOOST_REQUIRE(bqff.GetNumFrames() == NUM_FRAMES);
}

// Inspired by SB-285
BOOST_AUTO_TEST_CASE(BlastnLowerCaseMask_SingleStrand) {
    TSeqRange mask(TSeqRange(0, 75));

    TMaskedQueryRegions mqr;
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 1945388));
    CRef<CSeq_interval> intv(new CSeq_interval(*id,
                                               mask.GetFrom(),
                                               mask.GetTo()));
    CRef<CSeqLocInfo> sli(new CSeqLocInfo(intv, 
                                          CSeqLocInfo::eFramePlus1));
    mqr.push_back(sli);

    CBlastQueryFilteredFrames bqff(eBlastTypeBlastn, mqr);
    BOOST_REQUIRE(!bqff.Empty());
    BOOST_REQUIRE(bqff.QueryHasMultipleFrames());
    const set<CSeqLocInfo::ETranslationFrame>& frames = bqff.ListFrames();
    const int kExpectedNumFrames = 2;
    int frame_ctr = 0;
    ITERATE(set<CSeqLocInfo::ETranslationFrame>, fr, frames) {
        BOOST_REQUIRE(bqff[*fr] != NULL);
        frame_ctr++;
    }
    BOOST_REQUIRE_EQUAL(kExpectedNumFrames, bqff.GetNumFrames());
    BOOST_REQUIRE_EQUAL(1, frame_ctr); // NOTE!!
    BOOST_REQUIRE_EQUAL(1, frames.size()); // NOTE!!
}

// Inspired by SB-285
BOOST_AUTO_TEST_CASE(BlastnLowerCaseMask_BothStrands) {
    TSeqRange mask(TSeqRange(0, 75));

    TMaskedQueryRegions mqr;
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 1945388));
    CRef<CSeq_interval> intv(new CSeq_interval(*id,
                                               mask.GetFrom(),
                                               mask.GetTo()));
    CRef<CSeqLocInfo> sli(new CSeqLocInfo(intv, 
                                          CSeqLocInfo::eFramePlus1));
    mqr.push_back(sli);
    sli.Reset(new CSeqLocInfo(intv, CSeqLocInfo::eFrameMinus1));
    mqr.push_back(sli);

    CBlastQueryFilteredFrames bqff(eBlastTypeBlastn, mqr);
    BOOST_REQUIRE(!bqff.Empty());
    BOOST_REQUIRE(bqff.QueryHasMultipleFrames());
    const set<CSeqLocInfo::ETranslationFrame>& frames = bqff.ListFrames();
    const int kExpectedNumFrames = 2;
    int frame_ctr = 0;
    ITERATE(set<CSeqLocInfo::ETranslationFrame>, fr, frames) {
        BOOST_REQUIRE(bqff[*fr] != NULL);
        frame_ctr++;
    }
    BOOST_REQUIRE_EQUAL(kExpectedNumFrames, bqff.GetNumFrames());
    BOOST_REQUIRE_EQUAL(kExpectedNumFrames, frame_ctr); // NOTE!!
    BOOST_REQUIRE_EQUAL(kExpectedNumFrames, frames.size()); // NOTE!!
}

BOOST_AUTO_TEST_CASE(LowerCaseMask_PlusStrand) {
    const bool ignore_strand_in_mask = true;
    CBlastFilterTest::x_TestLowerCaseMaskWith(eNa_strand_plus, 
                                              ignore_strand_in_mask);
}

BOOST_AUTO_TEST_CASE(LowerCaseMask_MinusStrand) {
    const bool ignore_strand_in_mask = true;
    CBlastFilterTest::x_TestLowerCaseMaskWith(eNa_strand_minus,
                                              ignore_strand_in_mask);
}

BOOST_AUTO_TEST_CASE(LowerCaseMask_BothStrands) {
    const bool ignore_strand_in_mask = true;
    CBlastFilterTest::x_TestLowerCaseMaskWith(eNa_strand_both,
                                              ignore_strand_in_mask);
}

BOOST_AUTO_TEST_CASE(LowerCaseMask_PlusStrand_Explicit) {
    const bool ignore_strand_in_mask = false;
    CBlastFilterTest::x_TestLowerCaseMaskWith(eNa_strand_plus,
                                              ignore_strand_in_mask);
}

BOOST_AUTO_TEST_CASE(LowerCaseMask_MinusStrand_Explicit) {
    const bool ignore_strand_in_mask = false;
    CBlastFilterTest::x_TestLowerCaseMaskWith(eNa_strand_minus,
                                              ignore_strand_in_mask);
}

BOOST_AUTO_TEST_CASE(LowerCaseMask_BothStrands_Explicit) {
    const bool ignore_strand_in_mask = false;
    CBlastFilterTest::x_TestLowerCaseMaskWith(eNa_strand_both,
                                              ignore_strand_in_mask);
}

BOOST_AUTO_TEST_CASE(CombineRepeatAndLowerCaseMask) {
    const int kNumLcaseLocs = 11;
    const int kLcaseStarts[kNumLcaseLocs] = 
        { 0, 78, 217, 380, 694, 1018, 1128, 2817, 3084, 3428, 3782 };
    const int kLcaseEnds[kNumLcaseLocs] = 
        { 75, 208, 316, 685, 1004, 1122, 1298, 2952, 3409, 3733, 3916 };

    const int kNumLocs = 7;
    const int kStarts[kNumLocs] = { 0, 217, 380, 1014, 2817, 3084, 3782 };
    const int kEnds[kNumLocs] = { 212, 316, 1004, 1298, 2953, 3733, 3916 };
    CSeq_id id("gi|1945388");
    auto_ptr<SSeqLoc> qsl(
                          CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_both));

    // Fill the lower case mask into the SSeqLoc
    CSeq_loc* seqloc = new CSeq_loc();
    for (int index = 0; index < kNumLcaseLocs; ++index) {
        seqloc->SetPacked_int().AddInterval(id, kLcaseStarts[index],
                                            kLcaseEnds[index]);
        BOOST_REQUIRE(!seqloc->GetPacked_int().Get().back()->CanGetStrand());
    }
    qsl->mask.Reset(seqloc);

    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetRepeatFiltering(true);
    Blast_FindRepeatFilterLoc(query_v, &nucl_handle);

    BOOST_REQUIRE(query_v[0].mask->IsPacked_int());

    int loc_index = 0;

    BOOST_REQUIRE(query_v[0].mask.NotEmpty());
    ITERATE(list< CRef<CSeq_interval> >, itr, 
            query_v[0].mask->GetPacked_int().Get()) {
        BOOST_REQUIRE_EQUAL(kStarts[loc_index], (int)(*itr)->GetFrom());
        BOOST_REQUIRE_EQUAL(kEnds[loc_index], (int)(*itr)->GetTo());
        ++loc_index;
    }

    BOOST_REQUIRE_EQUAL(kNumLocs, loc_index);
}

BOOST_AUTO_TEST_CASE(CombineRepeatAndDustFilter) {
    const int kNumLocs = 14;
    const int kStarts[kNumLocs] = 
        { 0, 298, 380, 1014, 1449, 2851, 3113, 4704, 6364, 6512, 7600, 
          7766, 8873, 9114};
    const int kEnds[kNumLocs] = 
        { 212, 305, 1003, 1297, 1479, 2953, 3730, 4710, 6373, 6573, 7672, 
          7772, 8880 , 9179};
    CSeq_id id("gi|1945388");
    auto_ptr<SSeqLoc> qsl(
                          CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_both));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);

    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetRepeatFiltering(true);
    nucl_handle.SetDustFiltering(true);
    Blast_FindDustFilterLoc(query_v, &nucl_handle);
    Blast_FindRepeatFilterLoc(query_v, &nucl_handle);

    int loc_index = 0;

    BOOST_REQUIRE(query_v[0].mask.NotEmpty());
    ITERATE(list< CRef<CSeq_interval> >, itr,
            query_v[0].mask->GetPacked_int().Get()) {
        BOOST_REQUIRE_EQUAL(kStarts[loc_index], (int)(*itr)->GetFrom());
        BOOST_REQUIRE_EQUAL(kEnds[loc_index], (int)(*itr)->GetTo());
        ++loc_index;
    }
    BOOST_REQUIRE_EQUAL(kNumLocs, loc_index);
}

BOOST_AUTO_TEST_CASE(FilterLocNuclBoth) {
    const int kNumLocs = 15;
    const int kDustStarts[kNumLocs] = 
        { 298, 875, 1018, 1449, 3113, 3282, 3428, 3598, 4704, 6364, 
          6512, 7600, 7766, 8873, 9114};
    const int kDustEnds[kNumLocs] = 
        { 305, 882, 1115, 1479, 3133, 3298, 3441, 3606, 4710, 6373, 
          6573, 7672, 7772, 8880 , 9179}; 

    CSeq_id id("gi|1945388");
    auto_ptr<SSeqLoc> qsl(
                          CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_both));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);

    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetDustFiltering(true);
    Blast_FindDustFilterLoc(query_v, &nucl_handle);

    int loc_index=0;
    ITERATE(list< CRef<CSeq_interval> >, itr, 
            query_v[0].mask->GetPacked_int().Get()) {
        BOOST_REQUIRE_EQUAL(kDustStarts[loc_index], (int)(*itr)->GetFrom());
        BOOST_REQUIRE_EQUAL(kDustEnds[loc_index], (int)(*itr)->GetTo());
        ++loc_index;
    }

    BOOST_REQUIRE_EQUAL(loc_index, kNumLocs);
}

BOOST_AUTO_TEST_CASE(FilterLocNuclPlus) {
    const int kNumLocs = 15;
    const int kDustStarts[kNumLocs] = 
        { 298, 875, 1018, 1449, 3113, 3282, 3428, 3598, 4704, 6364, 
          6512, 7600, 7766, 8873, 9114};
    const int kDustEnds[kNumLocs] = 
        { 305, 882, 1115, 1479, 3133, 3298, 3441, 3606, 4710, 6373, 
          6573, 7672, 7772, 8880 , 9179}; 

    CSeq_id id("gi|1945388");
    auto_ptr<SSeqLoc> qsl(
                          CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_plus));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);

    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetDustFiltering(true);
    Blast_FindDustFilterLoc(query_v, &nucl_handle);

    int loc_index=0;
    ITERATE(list< CRef<CSeq_interval> >, itr, 
            query_v[0].mask->GetPacked_int().Get()) {
        BOOST_REQUIRE_EQUAL(kDustStarts[loc_index], (int)(*itr)->GetFrom());
        BOOST_REQUIRE_EQUAL(kDustEnds[loc_index], (int)(*itr)->GetTo());
        ++loc_index;
    }

    BOOST_REQUIRE_EQUAL(loc_index, kNumLocs);
}

BOOST_AUTO_TEST_CASE(FilterLocNuclMinus) {
    const int kNumLocs = 15;
    const int kDustStarts[kNumLocs] = 
        { 298, 875, 1018, 1449, 3113, 3282, 3428, 3598, 4704, 6364, 
          6512, 7600, 7766, 8873, 9114};
    const int kDustEnds[kNumLocs] = 
        { 305, 882, 1115, 1479, 3133, 3298, 3441, 3606, 4710, 6373, 
          6573, 7672, 7772, 8880 , 9179}; 

    CSeq_id id("gi|1945388");
    auto_ptr<SSeqLoc> qsl(
                          CTestObjMgr::Instance().CreateSSeqLoc(id, eNa_strand_minus));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);

    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetDustFiltering(true);
    Blast_FindDustFilterLoc(query_v, &nucl_handle);
    // CSeq_loc_mapper sorts intervals in reverse order if on minus strand.
    bool reverse = IsReverse(query_v[0].mask->GetStrand());
    int loc_index = reverse ? kNumLocs - 1 : 0;
    ITERATE(list< CRef<CSeq_interval> >, itr, 
            query_v[0].mask->GetPacked_int().Get()) {
        BOOST_REQUIRE_EQUAL(kDustStarts[loc_index], (int)(*itr)->GetFrom());
        BOOST_REQUIRE_EQUAL(kDustEnds[loc_index], (int)(*itr)->GetTo());
    reverse ? --loc_index : ++loc_index;
    }

    // Check that we finished loop on reverse strand is that loc_index is -1.
    if ( !reverse ) {
        BOOST_REQUIRE_EQUAL(loc_index, kNumLocs);
    }
    else {
        BOOST_REQUIRE_EQUAL(loc_index, -1);
    }
}


BOOST_AUTO_TEST_CASE(FilterLocProtein) {
    const int kNumLocs = 3;
    const int kSegStarts[kNumLocs] = { 15, 55, 495 };
    const int kSegEnds[kNumLocs] = { 27, 68, 513 };
    CSeq_id id("gi|3091");
    auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(id));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastp));

    setupQueryStructures(query_v, opts->GetOptions(), 
                         &query_blk, &query_info);

    BlastMaskLoc* filter_out = NULL;
    Blast_Message *blast_message=NULL;
    SBlastFilterOptions* filter_options;
    SBlastFilterOptionsNew(&filter_options, eSeg);

    Int2 status = 
        BlastSetUp_GetFilteringLocations(query_blk, query_info, 
                                         eBlastTypeBlastp, filter_options,
                                         &filter_out, &blast_message);
    filter_options = SBlastFilterOptionsFree(filter_options);
    BOOST_REQUIRE(status == 0);

    BlastSeqLoc *filter_slp = filter_out->seqloc_array[0];
    Int4 loc_index;
    SSeqRange* di;
    BlastSeqLoc *loc = NULL;
    for (loc_index=0, loc = filter_slp; loc; loc = loc->next, ++loc_index) {
        di = loc->ssr;
        BOOST_REQUIRE_EQUAL(kSegStarts[loc_index], di->left);
        BOOST_REQUIRE_EQUAL(kSegEnds[loc_index], di->right);
    }

    BOOST_REQUIRE_EQUAL(kNumLocs, loc_index);

    filter_out = BlastMaskLocFree(filter_out);
}

BOOST_AUTO_TEST_CASE(MaskProteinSequence) {
    const int kNumLocs = 3;
    const int kSegStarts[kNumLocs] = { 15, 55, 495 };
    const int kSegEnds[kNumLocs] = { 27, 68, 513 };
    CSeq_id id("gi|3091");
    auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(id));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastp));

    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    setupQueryStructures(query_v, opts->GetOptions(), 
                         &query_blk, &query_info);

    BlastSeqLoc *head = NULL;
    BlastSeqLoc *last = NULL;
    for (Int4 loc_index=0; loc_index<kNumLocs; ++loc_index) {
        if (head == NULL)
            last = BlastSeqLocNew(&head, kSegStarts[loc_index], 
                                  kSegEnds[loc_index]);
        else 
            last = BlastSeqLocNew(&last, kSegStarts[loc_index], 
                                  kSegEnds[loc_index]);
    }

    BlastMaskLoc* filter_maskloc = BlastMaskLocNew(1);
    filter_maskloc->seqloc_array[0] = head;

    BlastSetUp_MaskQuery(query_blk, query_info, filter_maskloc, 
                         eBlastTypeBlastp);
    filter_maskloc = BlastMaskLocFree(filter_maskloc);

    Uint1* buffer = &query_blk->sequence[0];
    Int4 query_length = query_info->contexts[0].query_length;
    Uint4 hash = 0;
    for (int index=0; index<query_length; index++)
        {
            hash *= 1103515245;
            hash += (Uint4)buffer[index] + 12345;
        }
    BOOST_REQUIRE_EQUAL(-241853716, (int) hash);
}

BOOST_AUTO_TEST_CASE(MaskNucleotideBothStrands) {
    const int kNumLocs = 15;
    const int kDustStarts[kNumLocs] = 
        { 298, 875, 1018, 1064, 1448, 3113, 3282, 3428, 3598, 4704, 6364,
          6511, 7766, 8873, 9108 };
    const int kDustEnds[kNumLocs] = 
        { 305, 882, 1045, 1115, 1479, 3133, 3298, 3441, 3606, 4710, 6373, 
          6573, 7772, 8880, 9179 }; 
        
    CSeq_id id("gi|1945388");
    auto_ptr<SSeqLoc> qsl(CTestObjMgr::Instance().CreateSSeqLoc(id, 
                                                                eNa_strand_both));
    TSeqLocVector query_v;
    query_v.push_back(*qsl);
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastn));

    CBlastQueryInfo query_info;
    CBLAST_SequenceBlk query_blk;
    setupQueryStructures(query_v, opts->GetOptions(), 
                         &query_blk, &query_info);

    BlastSeqLoc *head = NULL;
    BlastSeqLoc *last = NULL;
    for (Int4 loc_index=0; loc_index<kNumLocs; ++loc_index) {
        if (head == NULL)
            last = BlastSeqLocNew(&head, kDustStarts[loc_index], 
                                  kDustEnds[loc_index]);
        else 
            last = BlastSeqLocNew(&last, kDustStarts[loc_index], 
                                  kDustEnds[loc_index]);
    }

    BlastMaskLoc* filter_maskloc = 
        BlastMaskLocNew(query_info->last_context+1);
    filter_maskloc->seqloc_array[0] = head;

    BlastSetUp_MaskQuery(query_blk, query_info, filter_maskloc, 
                         eBlastTypeBlastn);
    filter_maskloc = BlastMaskLocFree(filter_maskloc);

    Uint1* buffer = &query_blk->sequence[0];
    Int4 query_length = query_info->contexts[0].query_length;
    Uint4 hash = 0;
    for (int index=0; index<query_length; index++)
        {
            hash *= 1103515245;
            hash += (Uint4)buffer[index] + 12345;
        }
    BOOST_REQUIRE_EQUAL(-1261879517, (int) hash);
}

BOOST_AUTO_TEST_CASE(FilterMultipleQueriesLocNuclPlus) {
    const int kNumLocs0 = 15;
    const int kNumLocs1 = 80;
    const int kNumLocs2 = 1;

    int dust_starts0[kNumLocs0] = 
        { 298, 875, 1018, 1449, 3113, 3282, 3428, 3598, 4704, 6364, 
          6512, 7600, 7766, 8873, 9114};
    int dust_ends0[kNumLocs0] = 
        { 305, 882, 1115, 1479, 3133, 3298, 3441, 3606, 4710, 6373, 
          6573, 7672, 7772, 8880 , 9179}; 
    int dust_starts1[kNumLocs1] = 
        { 189, 862, 1717, 1880, 2301, 2850, 3074, 3301, 4865, 5231, 5397, 
          5825, 5887, 6560, 6806, 7178, 7709, 8000, 8275, 8441, 9449, 9779, 
          10297, 10457, 11033, 11242, 12271, 12410, 12727, 13803, 14743, 15052, 
          15153, 15262, 16201, 16968, 17318, 18470, 20179, 21513, 21569,
          22034, 22207, 22657, 22890, 23326, 27984, 28305, 28581, 28960, 29678, 
          30553, 31195, 32347, 33641, 33785, 34138, 34861, 34872, 35028,
          35676, 35727, 36105, 36312, 36841, 38459, 38610, 38997, 39217, 39428, 
          39629, 42243, 42584, 43157, 43346, 43619, 44040, 44617, 46791, 47213};
    int dust_ends1[kNumLocs1] = 
        { 230, 876, 1741, 1898, 2315, 2868, 3117, 3308, 4886, 5255, 5433, 5860, 
          5943, 6566, 6857, 7245, 7737, 8014, 8286, 8479, 9496, 9830, 10306,
          10581, 11082, 11255, 12277, 12432, 12748, 13809, 14750, 15121, 15171, 
          15345, 16237, 16992, 17332, 18482, 20185, 21524, 21688, 22072, 22220, 
          22672, 22898, 23348, 27996, 28311, 28626, 28998, 29690, 30596, 31220, 
          32359, 33683, 33815, 34203, 34870, 34894, 35039, 35725, 35797, 36114, 
          36318, 36869, 38497, 38632, 39035, 39223, 39477, 39635, 42249, 42591, 
          43175, 43410, 43648, 44049, 44630, 46811, 47219};
    int dust_starts2[kNumLocs2] = {156};
    int dust_ends2[kNumLocs2] = {172};

    typedef pair<int*, int*> TStartEndPair;
    TStartEndPair pair0(dust_starts0, dust_ends0);
    TStartEndPair pair1(dust_starts1, dust_ends1);
    TStartEndPair pair2(dust_starts2, dust_ends2);

    vector< TStartEndPair > start_end_v;
    start_end_v.push_back(pair0);
    start_end_v.push_back(pair1);
    start_end_v.push_back(pair2);
        
    CSeq_id qid1("gi|1945388");
    auto_ptr<SSeqLoc> qsl1(
                           CTestObjMgr::Instance().CreateSSeqLoc(qid1, eNa_strand_both));
    CSeq_id qid2("gi|2655203");
    auto_ptr<SSeqLoc> qsl2(
                           CTestObjMgr::Instance().CreateSSeqLoc(qid2, eNa_strand_both));
    CSeq_id qid3("gi|557");
    auto_ptr<SSeqLoc> qsl3(
                           CTestObjMgr::Instance().CreateSSeqLoc(qid3, eNa_strand_both));

    TSeqLocVector query_v;
        
    query_v.push_back(*qsl1);
    query_v.push_back(*qsl2);
    query_v.push_back(*qsl3);
        

    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetDustFiltering(true);
    Blast_FindDustFilterLoc(query_v, &nucl_handle);
    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastn));

    int query_number=0;
    ITERATE(vector< TStartEndPair >, vec_iter, start_end_v)
        {
            TStartEndPair local_pair = *vec_iter;
            int* start = local_pair.first;
            int* stop = local_pair.second;
            int loc_index=0;
            ITERATE(list< CRef<CSeq_interval> >, itr,
                    query_v[query_number].mask->GetPacked_int().Get()) {
                BOOST_REQUIRE_EQUAL(start[loc_index], (int)(*itr)->GetFrom());
                BOOST_REQUIRE_EQUAL(stop[loc_index], (int)(*itr)->GetTo());
                ++loc_index;
            }
            ++query_number;
        }
}

BOOST_AUTO_TEST_CASE(MaskRestrictToInterval)
{
    const int kNumLocs = 4;
    const int kMaskStarts[kNumLocs] = { 10, 20, 30, 40 };
    const int kMaskEnds[kNumLocs] = { 15, 25, 35, 45 };
    const int kRange[2] = { 12, 22 };
    BlastSeqLoc* mask_loc = NULL, *loc_var;
    int index;

    for (index = 0; index < kNumLocs; ++index) {
        BlastSeqLocNew(&mask_loc, kMaskStarts[index], kMaskEnds[index]);
    }

    // Test that restricting to a full sequence does not change anything;
    // this also checks that negative ending offset indicates full 
    // sequence.
    BlastSeqLoc_RestrictToInterval(&mask_loc, 0, -2);
    for (index = 0, loc_var = mask_loc; loc_var; 
         ++index, loc_var = loc_var->next) {
        BOOST_REQUIRE_EQUAL(kMaskStarts[index], (int)loc_var->ssr->left);
        BOOST_REQUIRE_EQUAL(kMaskEnds[index], (int)loc_var->ssr->right);
    }       
    BOOST_REQUIRE_EQUAL(kNumLocs, index);

    BlastSeqLoc_RestrictToInterval(&mask_loc, kRange[0], kRange[1]);
    for (index = 0, loc_var = mask_loc; loc_var; 
         ++index, loc_var = loc_var->next);
    BOOST_REQUIRE_EQUAL(2, index);
    BOOST_REQUIRE_EQUAL(kMaskEnds[0]-kRange[0], (int)mask_loc->ssr->right);
    BOOST_REQUIRE_EQUAL(kMaskStarts[1]-kRange[0], 
                        (int)mask_loc->next->ssr->left);
    BOOST_REQUIRE_EQUAL(kRange[1]-kRange[0], 
                        (int)mask_loc->next->ssr->right);

    BlastSeqLoc_RestrictToInterval(&mask_loc, kRange[0], kRange[1]);

    BOOST_REQUIRE(mask_loc == NULL);
} 

void setupQueryInfoForOffsetTranslation(CBlastQueryInfo &query_info)
{
    const int kNumQueries = 3;
    const int kQueryGis[kNumQueries] = { 215041, 441158, 214981 };
    const int kQueryLengths[kNumQueries] = { 1639, 1151, 1164 };

    TSeqLocVector query_v;

    for (int index = 0; index < kNumQueries; ++index) {
        CRef<CSeq_loc> loc(new CSeq_loc());
        loc->SetWhole().SetGi(kQueryGis[index]);
        CScope* scope = new CScope(CTestObjMgr::Instance().GetObjMgr());
        scope->AddDefaults();
        query_v.push_back(SSeqLoc(loc, scope));
    }

    CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::Create(eBlastx));

    const CBlastOptions& kOpts = opts->GetOptions();
    EBlastProgramType prog = kOpts.GetProgramType();
    ENa_strand strand_opt = kOpts.GetStrandOption();

    SetupQueryInfo(query_v, prog, strand_opt, &query_info); 
    for (int i = 0; i < kNumQueries; i++) {
        int len = BlastQueryInfoGetQueryLength(query_info,
                                               eBlastTypeBlastx, i);
        BOOST_REQUIRE_EQUAL(kQueryLengths[i], len);
    }
}

BOOST_AUTO_TEST_CASE(ConvertTranslatedFilterOffsets)
{
    const int kNumQueries = 3;
    CBlastQueryInfo query_info;
    const int kNumContexts = kNumQueries*NUM_FRAMES;

    setupQueryInfoForOffsetTranslation(query_info);
    BOOST_REQUIRE_EQUAL(kNumContexts, query_info->last_context + 1);

    const SSeqRange kMasks[kNumQueries] = 
        { { 660, 686 }, { 92, 119 }, { 1156, 1163 } };

    CBlastMaskLoc mask_loc(BlastMaskLocNew(kNumContexts));
    BOOST_REQUIRE_EQUAL(kNumContexts, mask_loc->total_size);

    for (int index = 0; index < kNumQueries; index++) {
        BlastSeqLoc* seqloc = mask_loc->seqloc_array[index*NUM_FRAMES] = 
            (BlastSeqLoc*) calloc(1, sizeof(BlastSeqLoc));
        seqloc->ssr = (SSeqRange*) malloc(sizeof(SSeqRange));
        seqloc->ssr->left = kMasks[index].left;
        seqloc->ssr->right = kMasks[index].right;
    }

    BlastMaskLocDNAToProtein(mask_loc, query_info);

    BOOST_REQUIRE_EQUAL(kNumContexts, mask_loc->total_size);

    const int kProtStarts[kNumContexts] = 
        { 220, 219, 219, 317, 317, 316, 30, 30, 30, 343, 343, 343, 385, 385, 
          384, 0, 0, 0 };
    const int kProtEnds[kNumContexts] = 
        { 228, 228, 228, 326, 325, 325, 39, 39, 39, 352, 352, 352, 387, 386,
          386, 2, 2, 1 };

    for (int index = 0; index < kNumContexts; ++index) {
        {{
            CNcbiOstrstream os;
            os << "Context " << index << " has no mask!";
            BOOST_REQUIRE_MESSAGE(mask_loc->seqloc_array[index], 
                                  (string)CNcbiOstrstreamToString(os));
        }}
        const SSeqRange* range = mask_loc->seqloc_array[index]->ssr;
        CNcbiOstrstream os;
        os << "Context " << index;
        BOOST_REQUIRE_MESSAGE(kProtStarts[index] == range->left,
                              (string)CNcbiOstrstreamToString(os));
        BOOST_REQUIRE_MESSAGE(kProtEnds[index] == range->right,
                              (string)CNcbiOstrstreamToString(os));
    }

    BlastMaskLocProteinToDNA(mask_loc, query_info);

    BOOST_REQUIRE_EQUAL(kNumContexts, mask_loc->total_size);
    const int kNuclStarts[kNumContexts] = 
        { 660, 658, 659, 661, 663, 662, 90, 91, 92, 95, 94, 93, 1155, 1156,
          1154, 1158, 1157, 1159 };
    const int kNuclEnds[kNumContexts] = 
        { 684, 685, 686, 687, 686, 688, 117, 118, 119, 121, 120, 119, 1161, 
          1159, 1160, 1163, 1162, 1161 };

    for (int index = 0; index < kNumContexts; ++index) {
        {{
            CNcbiOstrstream os;
            os << "Context " << index << " has no mask!";
            BOOST_REQUIRE_MESSAGE(mask_loc->seqloc_array[index],
                                  (string)CNcbiOstrstreamToString(os));
        }}
        const SSeqRange* range = mask_loc->seqloc_array[index]->ssr;
        CNcbiOstrstream os;
        os << "Context " << index;
        BOOST_REQUIRE_MESSAGE(kNuclStarts[index] == range->left,
                              (string)CNcbiOstrstreamToString(os));
        BOOST_REQUIRE_MESSAGE(kNuclEnds[index] == range->right,
                              (string)CNcbiOstrstreamToString(os));
    }

}

BOOST_AUTO_TEST_CASE(FilterOptionsToStringFromNULL)
{
    TAutoCharPtr retval = BlastFilteringOptionsToString(NULL);
    BOOST_REQUIRE(strcmp(retval.get(), "F") == 0);
}

BOOST_AUTO_TEST_CASE(FilterOptionsToStringFromMaskAtHashOnly)
{
    SBlastFilterOptions filtering_options = { '\0' };
    filtering_options.mask_at_hash = true;
    TAutoCharPtr retval = BlastFilteringOptionsToString(&filtering_options);
    BOOST_REQUIRE(strcmp(retval.get(), "m;") == 0);
}
        
BOOST_AUTO_TEST_CASE(FilterOptionsToStringLargeData)
{
    SBlastFilterOptions filtering_options = { '\0' };
    SDustOptionsNew(&filtering_options.dustOptions);
    filtering_options.dustOptions->window *= 2;
    SRepeatFilterOptionsResetDB(&filtering_options.repeatFilterOptions,
                                string(4096, 'X').c_str());

    TAutoCharPtr retval = BlastFilteringOptionsToString(&filtering_options);
    SDustOptionsFree(filtering_options.dustOptions);
    SRepeatFilterOptionsFree(filtering_options.repeatFilterOptions);
    //cerr << "FilterStr ='" << retval.get() << "'" << endl;
    BOOST_REQUIRE(NStr::StartsWith(string(retval.get()), 
                                   "D 20 128 1;R -d XXXXXXXXXXXXXXXXXXXX"));
}
    
BOOST_AUTO_TEST_CASE(FilterOptionsFromNULLString)
{
    const EBlastProgramType kProgram = eBlastTypeBlastn;
    SBlastFilterOptions* filtering_options;
    Int2 status = BlastFilteringOptionsFromString(kProgram, NULL, 
                                                  &filtering_options, NULL);
    BOOST_REQUIRE(status == 0);
    BOOST_REQUIRE(filtering_options != NULL);
    BOOST_REQUIRE_EQUAL(false, !!filtering_options->mask_at_hash);
    BOOST_REQUIRE(filtering_options->segOptions == NULL);
    BOOST_REQUIRE(filtering_options->dustOptions == NULL);
    filtering_options = SBlastFilterOptionsFree(filtering_options);
}
    
BOOST_AUTO_TEST_CASE(FilterOptionsFromStringDustMaskAtHash)
{
    const EBlastProgramType kProgram = eBlastTypeBlastn;
    SBlastFilterOptions* filtering_options;
    Int2 status = BlastFilteringOptionsFromString(kProgram, (char*) "m D",
                                                  &filtering_options, NULL);
    BOOST_REQUIRE(status == 0);
    BOOST_REQUIRE_EQUAL(true, !!filtering_options->mask_at_hash);
    BOOST_REQUIRE(filtering_options->dustOptions);
    BOOST_REQUIRE(filtering_options->segOptions == NULL);

    TAutoCharPtr retval = BlastFilteringOptionsToString(filtering_options);
    BOOST_REQUIRE_EQUAL(string("L;m;"), string(retval.get()));

    filtering_options = SBlastFilterOptionsFree(filtering_options);
}

BOOST_AUTO_TEST_CASE(FilterOptionsFromStringDust)
{
    const EBlastProgramType kProgram = eBlastTypeBlastn;
    SBlastFilterOptions* filtering_options;
    Int2 status = BlastFilteringOptionsFromString(kProgram, (char*) "D",
                                                  &filtering_options, NULL);
    BOOST_REQUIRE(status == 0);
    BOOST_REQUIRE_EQUAL(false, !!filtering_options->mask_at_hash);
    BOOST_REQUIRE(filtering_options->dustOptions);
    BOOST_REQUIRE(filtering_options->segOptions == NULL);

    TAutoCharPtr retval = BlastFilteringOptionsToString(filtering_options);
    BOOST_REQUIRE(strcmp(retval.get(), "L;") == 0);

    filtering_options = SBlastFilterOptionsFree(filtering_options);
}

BOOST_AUTO_TEST_CASE(FilterOptionsFromStringSEGWithParams)
{
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    SBlastFilterOptions* filtering_options;
    Int2 status = BlastFilteringOptionsFromString(kProgram, (char*) "S 10 1.0 1.5", &filtering_options, NULL);
    BOOST_REQUIRE(status == 0);
    BOOST_REQUIRE_EQUAL(false, !!filtering_options->mask_at_hash);
    BOOST_REQUIRE(filtering_options->dustOptions == NULL);
    BOOST_REQUIRE(filtering_options->segOptions);
    BOOST_REQUIRE_EQUAL(10, filtering_options->segOptions->window);
    BOOST_REQUIRE_CLOSE(1.0, filtering_options->segOptions->locut, 0.01);
    BOOST_REQUIRE_CLOSE(1.5, filtering_options->segOptions->hicut, 0.01);

    TAutoCharPtr retval = BlastFilteringOptionsToString(filtering_options);
    BOOST_REQUIRE(strcmp(retval.get(), "S 10 1.0 1.5;") == 0);

    filtering_options = SBlastFilterOptionsFree(filtering_options);
}

BOOST_AUTO_TEST_CASE(FilterOptionsFromBadStringSEGWithParams)
{
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    SBlastFilterOptions* filtering_options;
    // Only three numbers are allowed.
    Int2 status = BlastFilteringOptionsFromString(kProgram, (char*) "S 10 1.0 1.5 1.0", &filtering_options, NULL);
    BOOST_REQUIRE_EQUAL(1, (int) status);
    BOOST_REQUIRE(filtering_options == NULL);
}

BOOST_AUTO_TEST_CASE(FilterOptionsFromStringBlastnL)
{
    const EBlastProgramType kProgram = eBlastTypeBlastn;
    SBlastFilterOptions* filtering_options;
    Int2 status = BlastFilteringOptionsFromString(kProgram, (char*) "L", &filtering_options, NULL);
    BOOST_REQUIRE(status == 0);
    BOOST_REQUIRE_EQUAL(false, !!filtering_options->mask_at_hash);
    BOOST_REQUIRE(filtering_options->dustOptions);
    BOOST_REQUIRE(filtering_options->segOptions == NULL);

    TAutoCharPtr retval = BlastFilteringOptionsToString(filtering_options);
    BOOST_REQUIRE(strcmp(retval.get(), "L;") == 0);

    filtering_options = SBlastFilterOptionsFree(filtering_options);
}
BOOST_AUTO_TEST_CASE(FilterOptionsFromStringBlastpL)
{
    const EBlastProgramType kProgram = eBlastTypeBlastp;
    SBlastFilterOptions* filtering_options;
    Int2 status = BlastFilteringOptionsFromString(kProgram, (char*) "L", &filtering_options, NULL);
    BOOST_REQUIRE(status == 0);
    BOOST_REQUIRE_EQUAL(false, !!filtering_options->mask_at_hash);
    BOOST_REQUIRE(filtering_options->dustOptions == NULL);
    BOOST_REQUIRE(filtering_options->segOptions);

    TAutoCharPtr retval = BlastFilteringOptionsToString(filtering_options);
    BOOST_REQUIRE(strcmp(retval.get(), "L;") == 0);

    filtering_options = SBlastFilterOptionsFree(filtering_options);
}
BOOST_AUTO_TEST_CASE(FilterOptionsFromStringBlastnW)
{
    const EBlastProgramType kProgram = eBlastTypeBlastn;
    SBlastFilterOptions* filtering_options = NULL;
    Int2 status = BlastFilteringOptionsFromString(kProgram, (char*) "W -t 9606", &filtering_options, NULL);
    BOOST_REQUIRE(status == 0);
    BOOST_REQUIRE(! filtering_options->mask_at_hash);
    BOOST_REQUIRE(! filtering_options->dustOptions);
    BOOST_REQUIRE(! filtering_options->segOptions);
    BOOST_REQUIRE(! filtering_options->repeatFilterOptions);
    BOOST_REQUIRE(filtering_options->windowMaskerOptions);

    TAutoCharPtr retval = BlastFilteringOptionsToString(filtering_options);
    BOOST_REQUIRE(strcmp(retval.get(), "W -t 9606;") == 0);

    filtering_options = SBlastFilterOptionsFree(filtering_options);
}

BOOST_AUTO_TEST_CASE(FilterMerge)
{
    const int kNewLevel = 21;
    const int kNewWindow = 68;

    SBlastFilterOptions* opt1 = NULL;
    SBlastFilterOptionsNew(&opt1, eDust);
    opt1->dustOptions->level = kNewLevel;
    opt1->dustOptions->window = kNewWindow;

    SBlastFilterOptions* opt2 = NULL;
    SBlastFilterOptionsNew(&opt2, eRepeats);
    opt2->mask_at_hash = true;

    SBlastFilterOptions* result = NULL;

    Int2 status = SBlastFilterOptionsMerge(&result, opt1, opt2);
    BOOST_REQUIRE_EQUAL(0, (int) status);
    BOOST_REQUIRE(result);
    BOOST_REQUIRE_EQUAL(true, !!result->mask_at_hash);
    BOOST_REQUIRE_EQUAL(kNewLevel, result->dustOptions->level);
    BOOST_REQUIRE_EQUAL(kNewWindow, result->dustOptions->window);
    BOOST_REQUIRE(result->repeatFilterOptions);
    result = SBlastFilterOptionsFree(result);

    status = SBlastFilterOptionsMerge(&result, opt1, NULL);
    BOOST_REQUIRE_EQUAL(0, (int) status);
    BOOST_REQUIRE(result);
    BOOST_REQUIRE_EQUAL(kNewLevel, result->dustOptions->level);
    BOOST_REQUIRE_EQUAL(kNewWindow, result->dustOptions->window);
    result = SBlastFilterOptionsFree(result);

    status = SBlastFilterOptionsMerge(&result, NULL, opt2);
    BOOST_REQUIRE_EQUAL(0, (int) status);
    BOOST_REQUIRE(result);
    BOOST_REQUIRE_EQUAL(true, !!result->mask_at_hash);
    BOOST_REQUIRE(result->repeatFilterOptions);
    result = SBlastFilterOptionsFree(result);

    SBlastFilterOptionsFree(opt1);
    SBlastFilterOptionsFree(opt2);
}

BOOST_AUTO_TEST_CASE(FilterStringFalse)
{
    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetFilterString("F");/* NCBI_FAKE_WARNING */
    BOOST_REQUIRE_EQUAL(false, nucl_handle.GetMaskAtHash());
    BOOST_REQUIRE_EQUAL(false, nucl_handle.GetDustFiltering());
    BOOST_REQUIRE_EQUAL(false, nucl_handle.GetWindowMaskerTaxId());
    BOOST_REQUIRE(nucl_handle.GetWindowMaskerDatabase() == NULL);
}

BOOST_AUTO_TEST_CASE(MergeOptionHandle) {
 
    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetFilterString("R -d repeat/repeat_9606");/* NCBI_FAKE_WARNING */
    nucl_handle.SetMaskAtHash(true);
    nucl_handle.SetDustFiltering(true);
    BOOST_REQUIRE_EQUAL(true, nucl_handle.GetMaskAtHash());
    BOOST_REQUIRE_EQUAL(true, nucl_handle.GetDustFiltering());
}

BOOST_AUTO_TEST_CASE(OptionsHandleNotClear) {
    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetFilterString("R -d repeat/repeat_9606", false);/* NCBI_FAKE_WARNING */
    BOOST_REQUIRE_EQUAL(true, nucl_handle.GetDustFiltering());
    BOOST_REQUIRE_EQUAL(true, nucl_handle.GetRepeatFiltering());
}

BOOST_AUTO_TEST_CASE(OptionsHandleClear) {
    CBlastNucleotideOptionsHandle nucl_handle;
    nucl_handle.SetFilterString("R -d repeat/repeat_9606");/* NCBI_FAKE_WARNING */
    BOOST_REQUIRE_EQUAL(false, nucl_handle.GetDustFiltering());
    BOOST_REQUIRE_EQUAL(true, nucl_handle.GetRepeatFiltering());
    BOOST_REQUIRE_EQUAL(false, nucl_handle.GetWindowMaskerTaxId());
    BOOST_REQUIRE(nucl_handle.GetWindowMaskerDatabase() == NULL);
}

BOOST_AUTO_TEST_CASE(GetSeqLocInfoVector_EmptyQueryIdVector) {
    CBlastMaskLoc mask(BlastMaskLocNew(1));
    CPacked_seqint empty_seqids;
    TSeqLocInfoVector mask_v;
    BOOST_REQUIRE_THROW(
                        Blast_GetSeqLocInfoVector(eBlastTypeBlastp, empty_seqids, mask, mask_v),
                        CBlastException);
}

// Check that the conversion function will now create a vector of empty
// mask lists.
BOOST_AUTO_TEST_CASE(GetSeqLocInfoVector_EmptyMasks) {
    const EBlastProgramType kProgram = eBlastTypeBlastn;
    const size_t kNumSeqs = 10;
    CBlastMaskLoc mask
        (BlastMaskLocNew(kNumSeqs*GetNumberOfContexts(kProgram)));

    // since the masks won't have any data in them, we don't care about the
    // Seq-id's passed in
    const CPacked_seqint::TRanges ranges(kNumSeqs, TSeqRange(0, 100000));
    CSeq_id seqid(CSeq_id::e_Gi, 555);
    CPacked_seqint seqintervals(seqid, ranges);

    TSeqLocInfoVector mask_v;

    Blast_GetSeqLocInfoVector(kProgram, seqintervals, mask, mask_v);
        
    BOOST_REQUIRE_EQUAL((size_t)kNumSeqs, (size_t)mask_v.size());
    ITERATE(TSeqLocInfoVector, query_masks_list, mask_v) {
        BOOST_REQUIRE_EQUAL((size_t)0U, query_masks_list->size());
    }
}

BOOST_AUTO_TEST_CASE(BlastSeqLocCombineTest) {
    const int kNumberLocIn = 7;
    const int kLocStartIn[kNumberLocIn] = 
        { 281312, 281356, 281416, 281454, 281895, 282435, 282999};
    const int kLocEndIn[kNumberLocIn] = 
        { 281736, 281406, 281446, 281878, 282423, 282968, 283191};

    const int kNumberLocOut = 4;
    const int kLocStartOut[kNumberLocOut] = 
        { 281312, 281895, 282435, 282999};
    const int kLocEndOut[kNumberLocOut] = 
        { 281878, 282423, 282968, 283191};

    BlastSeqLoc *head = NULL;
    for (int index=0; index<kNumberLocIn; index++)
        {
            BlastSeqLocNew(&head, kLocStartIn[index], 
                           kLocEndIn[index]);
        }

    BlastSeqLocCombine(&head, 0);
    BlastSeqLoc* result = head;
    head = NULL;

    int count = 0;
    BlastSeqLoc* var = result;
    while (var)
        {
            var = var->next;
            count++;
        }
    BOOST_REQUIRE_EQUAL(count, kNumberLocOut);

    var = result;
    count = 0;
    while (var)
        {
            SSeqRange* ssr = var->ssr;
            BOOST_REQUIRE_EQUAL(ssr->left, kLocStartOut[count]);
            BOOST_REQUIRE_EQUAL(ssr->right, kLocEndOut[count]);
            var = var->next;
            count++;
        }

    result = BlastSeqLocFree(result);
}

BOOST_AUTO_TEST_CASE(GetSeqLocInfoVector_AllPrograms) {
    vector<EBlastProgramType> programs =
        TestUtil::GetAllBlastProgramTypes();

    // Generate the different number of sequences to pass to test function
    CRandom random_gen(time(0));
    vector<int> num_seqs_array;
    num_seqs_array.reserve(3);
    num_seqs_array.push_back(random_gen.GetRand(1,10));
    num_seqs_array.push_back(random_gen.GetRand(1,10));
    num_seqs_array.push_back(random_gen.GetRand(1,10));

    ITERATE(vector<EBlastProgramType>, program, programs) {
        ITERATE(vector<int>, num_seqs, num_seqs_array) {
            x_TestGetSeqLocInfoVector(*program, *num_seqs);
        }
    }

}

#if SEQLOC_MIX_QUERY_OK
    /// Test the dust filtering API on a mixed Seqloc input.
    BOOST_AUTO_TEST_CASE(DustSeqlocMix) {
        const int kNumInts = 20;
        const int kStarts[kNumInts] = 
            { 838, 1838, 6542, 7459, 9246, 10431, 14807, 16336, 19563, 
              20606, 21232, 22615, 23822, 27941, 29597, 30136, 31287, 
              31786, 33315, 35402 };
        const int kEnds[kNumInts] = 
            { 961, 2010, 6740, 7573, 9408, 10609, 15043, 16511, 19783, 
              20748, 21365, 22817, 24049, 28171, 29839, 30348, 31362, 
              31911, 33485, 37952 };
#if 0 // These are the locations produced directly by CSymDustMasker
        const int kNumMaskLocs = 7;
        const int kMaskStarts[kNumMaskLocs] = 
            { 2607, 3000, 3739, 4238, 5211, 5602, 5716 };
        const int kMaskStops[kNumMaskLocs] = 
            { 2769, 3006, 3809, 4244, 5218, 5608, 5722 };
#else // These are locations that have been mapped to the full sequence scale
        const int kNumMaskLocs = 8;
        const int kMaskStarts[kNumMaskLocs] = 
            { 29678, 30136, 31305, 35786, 36285, 37258, 37649, 37763 };
        const int kMaskStops[kNumMaskLocs] = 
            { 29839, 30136, 31311, 35856, 36291, 37265, 37655, 37769 };
#endif

        int index;

        CSeq_id qid("gi|3417288");
        CRef<CSeq_loc> qloc(new CSeq_loc());
        for (index = 0; index < kNumInts; ++index) {
            CRef<CSeq_loc> next_loc(new CSeq_loc());
            next_loc->SetInt().SetFrom(kStarts[index]);
            next_loc->SetInt().SetTo(kEnds[index]);
            next_loc->SetInt().SetId(qid);
            qloc->SetMix().Set().push_back(next_loc);
        }

        CRef<CScope> scope(new CScope(CTestObjMgr::Instance().GetObjMgr()));
        scope->AddDefaults();

        auto_ptr<SSeqLoc> query(new SSeqLoc(qloc, scope));
        TSeqLocVector query_v;
        query_v.push_back(*query);

        CBlastNucleotideOptionsHandle nucl_handle;
        nucl_handle.SetDustFiltering(true);
        Blast_FindDustFilterLoc(query_v, &nucl_handle);

        int loc_index = 0;
        ITERATE(list< CRef<CSeq_interval> >, itr,
                query_v[0].mask->GetPacked_int().Get()) {
            BOOST_REQUIRE_EQUAL(kMaskStarts[loc_index], 
                                 (int) (*itr)->GetFrom());
            BOOST_REQUIRE_EQUAL(kMaskStops[loc_index], 
                                 (int) (*itr)->GetTo());
            ++loc_index;
        }
        BOOST_REQUIRE_EQUAL(kNumMaskLocs, loc_index);
    }
#endif

BOOST_AUTO_TEST_CASE(TestBlastSeqLocCombine_MergeElems) 
{
    TRangeVector rv;
    rv.push_back(TRangeVector::value_type(10, 77));
    rv.push_back(TRangeVector::value_type(0, 100));
    rv.push_back(TRangeVector::value_type(20, 45));
    rv.push_back(TRangeVector::value_type(3, 50));
    rv.push_back(TRangeVector::value_type(10, 77));

    BlastSeqLoc* mask = s_RangeVector2BlastSeqLoc(rv);
    BlastSeqLocCombine(&mask, 0);
    TRangeVector merged_rv;
    merged_rv.push_back(TRangeVector::value_type(0, 100));

    BlastSeqLoc* mask_itr = mask;
    ITERATE(TRangeVector, itr, merged_rv) {
        BOOST_REQUIRE(mask_itr != NULL);
        BOOST_REQUIRE_EQUAL((int)itr->GetFrom(), (int)mask_itr->ssr->left);
        BOOST_REQUIRE_EQUAL((int)itr->GetTo(), (int)mask_itr->ssr->right);
        mask_itr = mask_itr->next;
    }
    BOOST_REQUIRE(mask_itr == NULL);

    mask = BlastSeqLocFree(mask);
}

BOOST_AUTO_TEST_CASE(TestBlastSeqLocCombine_MergeIdenticals) 
{
    TRangeVector rv;
    rv.push_back(TRangeVector::value_type(380, 684));
    rv.push_back(TRangeVector::value_type(0, 74));
    rv.push_back(TRangeVector::value_type(78, 207));
    rv.push_back(TRangeVector::value_type(695, 776));
    rv.push_back(TRangeVector::value_type(380, 684));
    rv.push_back(TRangeVector::value_type(78, 212));

    BlastSeqLoc* mask = s_RangeVector2BlastSeqLoc(rv);
    BlastSeqLocCombine(&mask, 0);
    TRangeVector merged_rv;
    merged_rv.push_back(TRangeVector::value_type(0, 74));
    merged_rv.push_back(TRangeVector::value_type(78, 212));
    merged_rv.push_back(TRangeVector::value_type(380, 684));
    merged_rv.push_back(TRangeVector::value_type(695, 776));

    BlastSeqLoc* mask_itr = mask;
    ITERATE(TRangeVector, itr, merged_rv) {
        BOOST_REQUIRE(mask_itr != NULL);
        BOOST_REQUIRE_EQUAL((int)itr->GetFrom(), (int)mask_itr->ssr->left);
        BOOST_REQUIRE_EQUAL((int)itr->GetTo(), (int)mask_itr->ssr->right);
        mask_itr = mask_itr->next;
    }
    BOOST_REQUIRE(mask_itr == NULL);

    mask = BlastSeqLocFree(mask);
}

BOOST_AUTO_TEST_CASE(TestBlastSeqLocCombine_NoMerging) 
{
    TRangeVector rv;
    rv.push_back(TRangeVector::value_type(10, 77));
    rv.push_back(TRangeVector::value_type(250, 3400));
    rv.push_back(TRangeVector::value_type(3, 8));

    BlastSeqLoc* mask = s_RangeVector2BlastSeqLoc(rv);
    BlastSeqLocCombine(&mask, 0);
    TRangeVector merged_rv;
    merged_rv.push_back(TRangeVector::value_type(3, 8));
    merged_rv.push_back(TRangeVector::value_type(10, 77));
    merged_rv.push_back(TRangeVector::value_type(250, 3400));

    BlastSeqLoc* mask_itr = mask;
    ITERATE(TRangeVector, itr, merged_rv) {
        BOOST_REQUIRE(mask_itr != NULL);
        BOOST_REQUIRE_EQUAL((int)itr->GetFrom(), (int)mask_itr->ssr->left);
        BOOST_REQUIRE_EQUAL((int)itr->GetTo(), (int)mask_itr->ssr->right);
        mask_itr = mask_itr->next;
    }
    BOOST_REQUIRE(mask_itr == NULL);

    mask = BlastSeqLocFree(mask);
}

extern "C" void BlastSeqLocListReverse(BlastSeqLoc** head);

BOOST_AUTO_TEST_CASE(TestBlastSeqLocListReverse) 
{
    TRangeVector rv;
    rv.push_back(TRangeVector::value_type(10, 77));
    rv.push_back(TRangeVector::value_type(0, 100));
    rv.push_back(TRangeVector::value_type(3, 50));

    BlastSeqLoc* mask = s_RangeVector2BlastSeqLoc(rv);
    BlastSeqLocListReverse(&mask);
    reverse(rv.begin(), rv.end());

    BlastSeqLoc* mask_itr = mask;
    ITERATE(TRangeVector, itr, rv) {
        BOOST_REQUIRE(mask_itr != NULL);
        BOOST_REQUIRE_EQUAL((int)itr->GetFrom(), (int)mask_itr->ssr->left);
        BOOST_REQUIRE_EQUAL((int)itr->GetTo(), (int)mask_itr->ssr->right);
        mask_itr = mask_itr->next;
    }
    BOOST_REQUIRE(mask_itr == NULL);

    mask = BlastSeqLocFree(mask);
}

BOOST_AUTO_TEST_CASE(TestGetTaxIdWithWindowMaskerSupport) 
{
    set<int> taxids;
    GetTaxIdWithWindowMaskerSupport(taxids);
    BOOST_REQUIRE(taxids.empty() == false);
    BOOST_REQUIRE(taxids.find(9606) != taxids.end());
}

BOOST_AUTO_TEST_SUITE_END()
