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
*   Unit test module to test the remote BLAST API
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>
#include <objtools/blast/services/blast_services.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include "test_objmgr.hpp"
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <algo/blast/api/blast_options.hpp>
#include <algo/blast/api/blast_rps_options.hpp>
#include <algo/blast/api/blast_nucl_options.hpp>
#include <algo/blast/api/disc_nucl_options.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/blastinput/blast_fasta_input.hpp>
#include <algo/blast/blastinput/blast_input_aux.hpp>
#include <serial/serial.hpp>
#include <serial/objostr.hpp>
#include <serial/exception.hpp>
#include <util/range.hpp>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;


static int 
x_CountHits(const string & rid)
{
    // Another preserved query, with mixed ID types.
    
    CRemoteBlast rb(rid);
    
    TSeqAlignVector sav(rb.GetSeqAlignSets());
    
    int total = 0;
    
    for(unsigned i = 0; i < sav.size(); i++) {
        total += sav[i]->Get().size();
    }
    
    return total;
}

static void 
x_PushPairVec(vector< pair<string,string> > & rids, string a, string b)
{
    pair<string,string> ab;
    ab.first = a;
    ab.second = b;
    
    rids.push_back(ab);
}

template<class TOBJ>
string x_Stringify(TOBJ & obj)
{
    CNcbiOstrstream oss;
    
    auto_ptr<CObjectOStream>
        outpstr(CObjectOStream::Open(eSerial_AsnText, oss));
    
    *outpstr << obj;
    
    return CNcbiOstrstreamToString(oss);
}

BOOST_AUTO_TEST_SUITE(remote_blast)

BOOST_AUTO_TEST_CASE(MaskedQueryRegions) {
    const EBlastProgramType prog = eBlastTypeBlastn;
    CRef<CBlastOptionsHandle> oh(
        CBlastOptionsFactory::Create(eBlastn, CBlastOptions::eRemote));
    oh->SetDbLength(5000000);
    // only our masking specified below should be used
    oh->SetFilterString("F");/* NCBI_FAKE_WARNING */

    CRemoteBlast rmt_blast(oh);
    //rmt_blast.SetVerbose();
    rmt_blast.SetDatabase("UniVec");

    const size_t kNumQueries(2);
    CRemoteBlast::TSeqLocList query_seqlocs(kNumQueries);
    TSeqLocInfoVector query_masks(kNumQueries);

    // Setup the first query
    {
        CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 555));
        query_seqlocs.front().Reset(new CSeq_loc);
        query_seqlocs.front()->SetWhole(*id);

        CRef<CSeq_interval> si(new CSeq_interval(*id, 50, 100));
        CRef<CSeqLocInfo> sli(new CSeqLocInfo(si, 
                                              CSeqLocInfo::eFramePlus1));
        query_masks.front().push_back(sli);
    }

    // Setup the second query
    {
        CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 556));
        query_seqlocs.back().Reset(new CSeq_loc);
        query_seqlocs.back()->SetWhole(*id);

        // this mask should be ignored... and a warning issued
        CRef<CSeq_interval> si(new CSeq_interval(*id, 200, 100));
        CRef<CSeqLocInfo> sli(new CSeqLocInfo(si,
                                              CSeqLocInfo::eFrameMinus1));
        query_masks.back().push_back(sli);

        si.Reset(new CSeq_interval(*id, 200, 300));
        sli.Reset(new CSeqLocInfo(si, CSeqLocInfo::eFramePlus1));
        query_masks.back().push_back(sli);
    }
    const string kClientId("remote_blast_unit_test.cpp");
    rmt_blast.SetClientId(kClientId);

    rmt_blast.SetQueries(query_seqlocs,
                         query_masks);
    BOOST_REQUIRE_EQUAL(true, rmt_blast.Submit());
    BOOST_REQUIRE_EQUAL(CRemoteBlast::eStatus_Pending, rmt_blast.CheckStatus());
    BOOST_REQUIRE_EQUAL(false, rmt_blast.GetRID().empty());
    BOOST_REQUIRE_EQUAL(kClientId, rmt_blast.GetClientId());

    BOOST_REQUIRE(rmt_blast.GetErrors().empty());

    vector<string> warnings;
    const CBlast4_get_search_results_reply::TMasks& network_masks =
        CRemoteBlast::ConvertToRemoteMasks(query_masks, prog, &warnings);
    BOOST_REQUIRE_EQUAL(kNumQueries, network_masks.size());

    CRef<CBlast4_mask> mask = network_masks.front();
    BOOST_REQUIRE_EQUAL((size_t)1, mask->GetLocations().size());
    CRef<CSeq_loc> sl = mask->GetLocations().front();
    BOOST_REQUIRE(sl->IsPacked_int());
    BOOST_REQUIRE(sl->GetPacked_int().Get().size() == 1);

    CRef<CSeq_interval> si = sl->GetPacked_int().Get().front();
    BOOST_REQUIRE_EQUAL((TSeqPos) 50, si->GetFrom());
    BOOST_REQUIRE_EQUAL((TSeqPos) 100, si->GetTo());

    mask  = network_masks.back();
    BOOST_REQUIRE_EQUAL((size_t)1, mask->GetLocations().size());
    sl = mask->GetLocations().front();
    BOOST_REQUIRE(sl->IsPacked_int());
    BOOST_REQUIRE(sl->GetPacked_int().Get().size() == 2);

    si = sl->GetPacked_int().Get().front();
    BOOST_REQUIRE_EQUAL((TSeqPos) 200, si->GetFrom());
    BOOST_REQUIRE_EQUAL((TSeqPos) 100, si->GetTo());
}

// Note that no CRemoteBlast constructor takes a CBlastRPSOptionsHandle, so
// the constructor which takes a CBlastOptionsHandle will be invoked
BOOST_AUTO_TEST_CASE(CheckRemoteRPSBlastOptionsHandle) {
    CBlastRPSOptionsHandle rps_opts(CBlastOptions::eRemote);

    CRemoteBlast rmt_blaster(&rps_opts);
    rmt_blaster.SetDatabase("cdd");
    CRemoteBlast::TSeqLocList query_seqlocs;
    CRef<CSeq_loc> sl(new CSeq_loc);
    sl->SetWhole().SetGi(GI_FROM(TIntId, 129295));
    query_seqlocs.push_back(sl);
    rmt_blaster.SetQueries(query_seqlocs);

    BOOST_REQUIRE_EQUAL(true, rmt_blaster.Submit());
}

// Search of GIs 555 and 3090 against ecoli
BOOST_AUTO_TEST_CASE(CheckBlastnMasks) {
    const string rid("BW3U058R01R");
    CRemoteBlast rmt_blaster(rid);

    BOOST_REQUIRE_EQUAL(rid, rmt_blaster.GetRID());
    BOOST_REQUIRE_EQUAL(true, rmt_blaster.CheckDone());
    BOOST_REQUIRE_EQUAL(kEmptyStr, rmt_blaster.GetErrors());
    BOOST_REQUIRE(rmt_blaster.GetDbFilteringAlgorithmId() == -1);
    BOOST_REQUIRE(rmt_blaster.GetDbFilteringAlgorithmKey() == kEmptyStr);

    const EBlastProgramType prog = 
        NetworkProgram2BlastProgramType(rmt_blaster.GetProgram(),
                                        rmt_blaster.GetService());

    TSeqLocInfoVector masks = rmt_blaster.GetMasks();
    vector<string> warnings;
    const CBlast4_get_search_results_reply::TMasks& network_masks =
        CRemoteBlast::ConvertToRemoteMasks(masks, prog, &warnings);
    BOOST_REQUIRE(!masks.empty());
    BOOST_REQUIRE(!network_masks.empty());
    BOOST_REQUIRE(warnings.empty());
    const size_t kNumQueries = 2;
    BOOST_REQUIRE_EQUAL(kNumQueries, masks.size());
    BOOST_REQUIRE_EQUAL(kNumQueries, network_masks.size());

    size_t index = 0;
    vector<TSeqRange> expected_masks;
    expected_masks.push_back(TSeqRange(78, 89));
    BOOST_REQUIRE_EQUAL(expected_masks.size(), masks.front().size());

    ITERATE(TMaskedQueryRegions, seqlocinfo, masks.front()) {
        BOOST_REQUIRE(seqlocinfo->NotEmpty());
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetFrom(), 
                             (*seqlocinfo)->GetInterval().GetFrom());
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetTo(),
                             (*seqlocinfo)->GetInterval().GetTo());
        BOOST_REQUIRE_EQUAL((int)CSeqLocInfo::eFrameNotSet,
                             (*seqlocinfo)->GetFrame());
        index++;
    }
    index = 0;
    BOOST_REQUIRE_EQUAL(eBlast4_frame_type_plus1,
                        network_masks.front()->GetFrame());
    CBlast4_mask::TLocations const* net_masks =
        &network_masks.front()->GetLocations();
    BOOST_REQUIRE_EQUAL((size_t)1, net_masks->size());
    ITERATE(CPacked_seqint::Tdata, seqint, 
            net_masks->front()->GetPacked_int().Get()) {
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetFrom(),
                            (*seqint)->GetFrom());
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetTo(),
                             (*seqint)->GetTo());
        index++;
    }

    index = 0;
    expected_masks.clear();
    expected_masks.push_back(TSeqRange(25, 31));
    expected_masks.push_back(TSeqRange(35, 101));
    expected_masks.push_back(TSeqRange(116, 123));
    expected_masks.push_back(TSeqRange(131, 195));
    expected_masks.push_back(TSeqRange(2022, 2337));
    BOOST_REQUIRE_EQUAL(expected_masks.size(), masks.back().size());
    ITERATE(TMaskedQueryRegions, seqlocinfo, masks.back()) {
        BOOST_REQUIRE(seqlocinfo->NotEmpty());
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetFrom(), 
                             (*seqlocinfo)->GetInterval().GetFrom());
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetTo(),
                             (*seqlocinfo)->GetInterval().GetTo());
        BOOST_REQUIRE_EQUAL((int)CSeqLocInfo::eFrameNotSet,
                             (*seqlocinfo)->GetFrame());
        index++;
    }

    index = 0;
    BOOST_REQUIRE_EQUAL(eBlast4_frame_type_plus1,
                        network_masks.back()->GetFrame());
    net_masks = &network_masks.back()->GetLocations();
    BOOST_REQUIRE_EQUAL((size_t)1, net_masks->size());
    ITERATE(CPacked_seqint::Tdata, seqint, 
            net_masks->front()->GetPacked_int().Get()) {
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetFrom(),
                            (*seqint)->GetFrom());
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetTo(),
                             (*seqint)->GetTo());
        index++;
    }
}

BOOST_AUTO_TEST_CASE(CheckBlastpMasks) {
    const string rid("BD4H4FMN014");
    CRemoteBlast rmt_blaster(rid);

    BOOST_REQUIRE_MESSAGE(rid == rmt_blaster.GetRID(), "RID=" << rid);
    BOOST_REQUIRE_MESSAGE(rmt_blaster.CheckDone(), "RID=" << rid);
    BOOST_REQUIRE_MESSAGE(kEmptyStr == rmt_blaster.GetErrors(), "RID=" << rid);

    const EBlastProgramType prog = 
        NetworkProgram2BlastProgramType(rmt_blaster.GetProgram(),
                                        rmt_blaster.GetService());

    TSeqLocInfoVector masks = rmt_blaster.GetMasks();
    vector<string> warnings;
    const CBlast4_get_search_results_reply::TMasks& network_masks =
        CRemoteBlast::ConvertToRemoteMasks(masks, prog, &warnings);
    BOOST_REQUIRE(!masks.empty());
    BOOST_REQUIRE(!network_masks.empty());
    BOOST_REQUIRE(warnings.empty());
    const size_t kNumQueries = 2;
    BOOST_REQUIRE_EQUAL(kNumQueries, masks.size());
    BOOST_REQUIRE_EQUAL(kNumQueries, network_masks.size());

    size_t index = 0;
    vector<TSeqRange> expected_masks;
    expected_masks.push_back(TSeqRange(95, 119));
    expected_masks.push_back(TSeqRange(196, 207));
    BOOST_REQUIRE_EQUAL(expected_masks.size(), masks.front().size());

    ITERATE(TMaskedQueryRegions, seqlocinfo, masks.front()) {
        BOOST_REQUIRE(seqlocinfo->NotEmpty());
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetFrom(), 
                             (*seqlocinfo)->GetInterval().GetFrom());
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetTo(),
                             (*seqlocinfo)->GetInterval().GetTo());
        BOOST_REQUIRE_EQUAL((int)CSeqLocInfo::eFrameNotSet,
                             (*seqlocinfo)->GetFrame());
        index++;
    }
    index = 0;
    BOOST_REQUIRE_EQUAL(eBlast4_frame_type_notset,
                        network_masks.front()->GetFrame());
    CBlast4_mask::TLocations const* net_masks =
        &network_masks.front()->GetLocations();
    BOOST_REQUIRE_EQUAL((size_t)1, net_masks->size());
    ITERATE(CPacked_seqint::Tdata, seqint, 
            net_masks->front()->GetPacked_int().Get()) {
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetFrom(),
                            (*seqint)->GetFrom());
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetTo(),
                             (*seqint)->GetTo());
        index++;
    }

    index = 0;
    expected_masks.clear();
    expected_masks.push_back(TSeqRange(91, 103));
    expected_masks.push_back(TSeqRange(270, 289));
    BOOST_REQUIRE_EQUAL(expected_masks.size(), masks.back().size());
    ITERATE(TMaskedQueryRegions, seqlocinfo, masks.back()) {
        BOOST_REQUIRE(seqlocinfo->NotEmpty());
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetFrom(), 
                             (*seqlocinfo)->GetInterval().GetFrom());
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetTo(),
                             (*seqlocinfo)->GetInterval().GetTo());
        BOOST_REQUIRE_EQUAL((int)CSeqLocInfo::eFrameNotSet,
                             (*seqlocinfo)->GetFrame());
        index++;
    }
    index = 0;
    BOOST_REQUIRE_EQUAL(eBlast4_frame_type_notset,
                        network_masks.back()->GetFrame());
    net_masks = &network_masks.back()->GetLocations();
    BOOST_REQUIRE_EQUAL((size_t)1, net_masks->size());
    ITERATE(CPacked_seqint::Tdata, seqint, 
            net_masks->front()->GetPacked_int().Get()) {
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetFrom(),
                            (*seqint)->GetFrom());
        BOOST_REQUIRE_EQUAL(expected_masks[index].GetTo(),
                             (*seqint)->GetTo());
        index++;
    }
}

// Search of GIs 555 and 3090 against ecoli
BOOST_AUTO_TEST_CASE(CheckBlastxMasks) {
    const string rid("BW41NPVB014");
    CRemoteBlast rmt_blaster(rid);

    BOOST_REQUIRE_EQUAL(rid, rmt_blaster.GetRID());
    BOOST_REQUIRE_EQUAL(true, rmt_blaster.CheckDone());
    BOOST_REQUIRE_EQUAL(kEmptyStr, rmt_blaster.GetErrors());

    const EBlastProgramType prog = 
        NetworkProgram2BlastProgramType(rmt_blaster.GetProgram(),
                                        rmt_blaster.GetService());

    TSeqLocInfoVector masks = rmt_blaster.GetMasks();
    vector<string> warnings;
    const CBlast4_get_search_results_reply::TMasks& network_masks =
        CRemoteBlast::ConvertToRemoteMasks(masks, prog, &warnings);
    BOOST_REQUIRE(!masks.empty());
    BOOST_REQUIRE(!network_masks.empty());
    const size_t kNumQueries = 2;
    const size_t kNumNetMasks = 7;
    BOOST_REQUIRE_EQUAL(kNumQueries, masks.size());
    BOOST_REQUIRE_EQUAL(kNumNetMasks, network_masks.size());

    TMaskedQueryRegions query1_masks = masks.front();
    size_t index = 0;
    typedef pair<TSeqRange, CSeqLocInfo::ETranslationFrame> TMask;
    typedef vector<TMask> TQueryMasks;
    TQueryMasks expected_masks;
    expected_masks.push_back(make_pair(TSeqRange(114, 155),
                                       CSeqLocInfo::eFrameMinus1));
    BOOST_REQUIRE_EQUAL(expected_masks.size(), query1_masks.size());

    ITERATE(TMaskedQueryRegions, seqlocinfo, query1_masks) {
        BOOST_REQUIRE(seqlocinfo->NotEmpty());
        const TMask& mask = expected_masks[index++];
        BOOST_REQUIRE_EQUAL(mask.first.GetFrom(), 
                             (*seqlocinfo)->GetInterval().GetFrom());
        BOOST_REQUIRE_EQUAL(mask.first.GetTo(),
                             (*seqlocinfo)->GetInterval().GetTo());
        BOOST_REQUIRE_EQUAL((int)mask.second, (*seqlocinfo)->GetFrame());
    }
    index = 0;
    BOOST_REQUIRE_EQUAL(eBlast4_frame_type_minus1,
                        network_masks.front()->GetFrame());
    CBlast4_mask::TLocations const* net_masks =
        &network_masks.front()->GetLocations();
    BOOST_REQUIRE_EQUAL((size_t)1, net_masks->size());
    ITERATE(CPacked_seqint::Tdata, seqint, 
            net_masks->front()->GetPacked_int().Get()) {
        const TMask& mask = expected_masks[index++];
        BOOST_REQUIRE_EQUAL(mask.first.GetFrom(),
                            (*seqint)->GetFrom());
        BOOST_REQUIRE_EQUAL(mask.first.GetTo(),
                             (*seqint)->GetTo());
    }

    TMaskedQueryRegions query2_masks = masks.back();

    index = 0;
    expected_masks.clear();
    expected_masks.push_back(make_pair(TSeqRange(36, 66), CSeqLocInfo::eFramePlus1));
    expected_masks.push_back(make_pair(TSeqRange(129, 240), CSeqLocInfo::eFramePlus1));
    expected_masks.push_back(make_pair(TSeqRange(363, 393), CSeqLocInfo::eFramePlus1));
    expected_masks.push_back(make_pair(TSeqRange(423, 471), CSeqLocInfo::eFramePlus1));
    expected_masks.push_back(make_pair(TSeqRange(933, 972), CSeqLocInfo::eFramePlus1));
    expected_masks.push_back(make_pair(TSeqRange(1092, 1137), CSeqLocInfo::eFramePlus1));
    expected_masks.push_back(make_pair(TSeqRange(1158, 1206), CSeqLocInfo::eFramePlus1));
    expected_masks.push_back(make_pair(TSeqRange(1224, 1260), CSeqLocInfo::eFramePlus1));
    expected_masks.push_back(make_pair(TSeqRange(1665, 1734), CSeqLocInfo::eFramePlus1));
    expected_masks.push_back(make_pair(TSeqRange(1842, 1899), CSeqLocInfo::eFramePlus1));
    expected_masks.push_back(make_pair(TSeqRange(1971, 2010), CSeqLocInfo::eFramePlus1));
    expected_masks.push_back(make_pair(TSeqRange(2058, 2226), CSeqLocInfo::eFramePlus1));
    expected_masks.push_back(make_pair(TSeqRange(2256, 2334), CSeqLocInfo::eFramePlus1));
    expected_masks.push_back(make_pair(TSeqRange(37, 64), CSeqLocInfo::eFramePlus2));
    expected_masks.push_back(make_pair(TSeqRange(607, 652), CSeqLocInfo::eFramePlus2));
    expected_masks.push_back(make_pair(TSeqRange(1153, 1192), CSeqLocInfo::eFramePlus2));
    expected_masks.push_back(make_pair(TSeqRange(1702, 1744), CSeqLocInfo::eFramePlus2));
    expected_masks.push_back(make_pair(TSeqRange(2014, 2092), CSeqLocInfo::eFramePlus2));
    expected_masks.push_back(make_pair(TSeqRange(2104, 2194), CSeqLocInfo::eFramePlus2));
    expected_masks.push_back(make_pair(TSeqRange(2251, 2278), CSeqLocInfo::eFramePlus2));
    expected_masks.push_back(make_pair(TSeqRange(2305, 2335), CSeqLocInfo::eFramePlus2));
    expected_masks.push_back(make_pair(TSeqRange(35, 56), CSeqLocInfo::eFramePlus3));
    expected_masks.push_back(make_pair(TSeqRange(92, 173), CSeqLocInfo::eFramePlus3));
    expected_masks.push_back(make_pair(TSeqRange(239, 275), CSeqLocInfo::eFramePlus3));
    expected_masks.push_back(make_pair(TSeqRange(359, 398), CSeqLocInfo::eFramePlus3));
    expected_masks.push_back(make_pair(TSeqRange(1679, 1733), CSeqLocInfo::eFramePlus3));
    expected_masks.push_back(make_pair(TSeqRange(2072, 2135), CSeqLocInfo::eFramePlus3));
    expected_masks.push_back(make_pair(TSeqRange(2159, 2294), CSeqLocInfo::eFramePlus3));
    expected_masks.push_back(make_pair(TSeqRange(2309, 2333), CSeqLocInfo::eFramePlus3));
    expected_masks.push_back(make_pair(TSeqRange(2311, 2337), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(2221, 2280), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(2155, 2202), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(2035, 2148), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(1816, 1857), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(1684, 1761), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(1348, 1389), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(1249, 1287), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(982, 1014), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(613, 654), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(514, 552), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(256, 279), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(121, 174), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(22, 84), CSeqLocInfo::eFrameMinus1));
    expected_masks.push_back(make_pair(TSeqRange(2274, 2336), CSeqLocInfo::eFrameMinus2));
    expected_masks.push_back(make_pair(TSeqRange(2004, 2261), CSeqLocInfo::eFrameMinus2));
    expected_masks.push_back(make_pair(TSeqRange(222, 242), CSeqLocInfo::eFrameMinus2));
    expected_masks.push_back(make_pair(TSeqRange(183, 203), CSeqLocInfo::eFrameMinus2));
    expected_masks.push_back(make_pair(TSeqRange(132, 164), CSeqLocInfo::eFrameMinus2));
    expected_masks.push_back(make_pair(TSeqRange(30, 83), CSeqLocInfo::eFrameMinus2));
    expected_masks.push_back(make_pair(TSeqRange(2255, 2335), CSeqLocInfo::eFrameMinus3));
    expected_masks.push_back(make_pair(TSeqRange(2192, 2239), CSeqLocInfo::eFrameMinus3));
    expected_masks.push_back(make_pair(TSeqRange(2060, 2185), CSeqLocInfo::eFrameMinus3));
    expected_masks.push_back(make_pair(TSeqRange(1964, 2011), CSeqLocInfo::eFrameMinus3));
    expected_masks.push_back(make_pair(TSeqRange(1850, 1888), CSeqLocInfo::eFrameMinus3));
    expected_masks.push_back(make_pair(TSeqRange(1673, 1741), CSeqLocInfo::eFrameMinus3));
    expected_masks.push_back(make_pair(TSeqRange(1226, 1261), CSeqLocInfo::eFrameMinus3));
    expected_masks.push_back(make_pair(TSeqRange(1166, 1204), CSeqLocInfo::eFrameMinus3));
    expected_masks.push_back(make_pair(TSeqRange(1097, 1135), CSeqLocInfo::eFrameMinus3));
    expected_masks.push_back(make_pair(TSeqRange(431, 469), CSeqLocInfo::eFrameMinus3));
    expected_masks.push_back(make_pair(TSeqRange(365, 394), CSeqLocInfo::eFrameMinus3));
    expected_masks.push_back(make_pair(TSeqRange(242, 289), CSeqLocInfo::eFrameMinus3));
    expected_masks.push_back(make_pair(TSeqRange(131, 208), CSeqLocInfo::eFrameMinus3));
    expected_masks.push_back(make_pair(TSeqRange(38, 70), CSeqLocInfo::eFrameMinus3));

    BOOST_REQUIRE_EQUAL(expected_masks.size(), query2_masks.size());
    ITERATE(TMaskedQueryRegions, seqlocinfo, masks.back()) {
        BOOST_REQUIRE(seqlocinfo->NotEmpty());
        const TMask& mask = expected_masks[index++];
        BOOST_REQUIRE_EQUAL(mask.first.GetFrom(), 
                             (*seqlocinfo)->GetInterval().GetFrom());
        BOOST_REQUIRE_EQUAL(mask.first.GetTo(),
                             (*seqlocinfo)->GetInterval().GetTo());
        BOOST_REQUIRE_EQUAL((int)mask.second, (*seqlocinfo)->GetFrame());
    }
}

// This tests some of the functionality in get_filter_options.[hc]pp
BOOST_AUTO_TEST_CASE(SetFilteringOptions) {
    CBlastProteinOptionsHandle prot_opts(CBlastOptions::eRemote);
    prot_opts.SetSegFiltering(false);
    {
        TAutoCharPtr tmp = prot_opts.GetFilterString();/* NCBI_FAKE_WARNING */
        BOOST_REQUIRE_EQUAL(string("F"), string(tmp.get()));
    }

    CRemoteBlast rmt_blaster(&prot_opts);
    rmt_blaster.SetDatabase("alu");

    CRemoteBlast::TSeqLocList query_seqloc(1);
    // use gi with low complexity regions
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 115129102));
    query_seqloc.front().Reset(new CSeq_loc);
    query_seqloc.front()->SetWhole(*id);
    rmt_blaster.SetQueries(query_seqloc);

    BOOST_REQUIRE_EQUAL(true, rmt_blaster.Submit());

    TSeqLocInfoVector masks = rmt_blaster.GetMasks();
    BOOST_REQUIRE(masks.size() == 1);
    BOOST_REQUIRE(masks.front().empty());
}

BOOST_AUTO_TEST_CASE(SubmitNullDatabase) {
    CBlastProteinOptionsHandle prot_opts(CBlastOptions::eRemote);

    CRemoteBlast rmt_blaster(&prot_opts);
    BOOST_REQUIRE_THROW(rmt_blaster.SetDatabase(""), CBlastException);
}

BOOST_AUTO_TEST_CASE(SubmitNullQueries) {
    CBlastProteinOptionsHandle prot_opts(CBlastOptions::eRemote);

    CRemoteBlast rmt_blaster(&prot_opts);
    CRef<CBioseq_set> no_queries;
    BOOST_REQUIRE_THROW(rmt_blaster.SetQueries(no_queries),
                        CBlastException);
}

BOOST_AUTO_TEST_CASE_TIMEOUT(CheckPrimerBlastRID, 45);
BOOST_AUTO_TEST_CASE(CheckPrimerBlastRID) {
    // Permanent RID provided by Jian
    const string rid("TNNF2YHZ016"); 

    CRemoteBlast rmt_blaster(rid);
    
    BOOST_REQUIRE_EQUAL(rid, rmt_blaster.GetRID());
    BOOST_REQUIRE_EQUAL(true, rmt_blaster.CheckDone());
    BOOST_REQUIRE_EQUAL(kEmptyStr, rmt_blaster.GetErrors());
    BOOST_REQUIRE_EQUAL(CRemoteBlast::eStatus_Done, rmt_blaster.CheckStatus());
    
    CRef<CSeq_align_set> sas = rmt_blaster.GetAlignments();
    BOOST_REQUIRE(sas.GetPointer() != NULL);
}

BOOST_AUTO_TEST_CASE(CheckRID) {
    // Permanent RID provided by Yan
    const string rid("5VPB2NH1014");
    CRemoteBlast rmt_blaster(rid);
    
    BOOST_REQUIRE_EQUAL(rid, rmt_blaster.GetRID());
    BOOST_REQUIRE_EQUAL(true, rmt_blaster.CheckDone());
    BOOST_REQUIRE_EQUAL(kEmptyStr, rmt_blaster.GetErrors());
    BOOST_REQUIRE_EQUAL(CRemoteBlast::eStatus_Done, rmt_blaster.CheckStatus());
    
    CRef<CSeq_align_set> sas = rmt_blaster.GetAlignments();
    BOOST_REQUIRE(sas.GetPointer() != NULL);
}

BOOST_AUTO_TEST_CASE(CheckColoRID) {
    // Colo RID that is preserved permanently
    const string rid("953V6EF901N");
    CRemoteBlast rmt_blaster(rid);
    //rmt_blaster.SetVerbose();
    
    BOOST_REQUIRE_EQUAL(rid, rmt_blaster.GetRID());
    BOOST_REQUIRE_EQUAL(true, rmt_blaster.CheckDone());
    BOOST_REQUIRE_EQUAL(kEmptyStr, rmt_blaster.GetErrors());

    CRef<CSeq_align_set> sas = rmt_blaster.GetAlignments();
    BOOST_REQUIRE(sas.GetPointer() != NULL);
    
    TSeqAlignVector sav = rmt_blaster.GetSeqAlignSets();
    BOOST_REQUIRE(! sav.empty());
    BOOST_REQUIRE(sav[0].NotEmpty());
}

BOOST_AUTO_TEST_CASE(GetErrorsFromFailedRID) {
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable tmp_env("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    const string rid("HU0G064A013"); 
    CRemoteBlast rmt_blaster(rid);
    //rmt_blaster.SetVerbose();
    
    CRef<CSeq_align_set> sas = rmt_blaster.GetAlignments();
    BOOST_REQUIRE(sas.GetPointer() == NULL);

    BOOST_REQUIRE_EQUAL(rid, rmt_blaster.GetRID());
    BOOST_REQUIRE_EQUAL(true, rmt_blaster.CheckDone());
    BOOST_REQUIRE_EQUAL(kEmptyStr, rmt_blaster.GetWarnings());
    
    const string error("CPU usage limit was exceeded");
    BOOST_REQUIRE(rmt_blaster.GetErrors().empty() == false);
    BOOST_REQUIRE(NStr::FindNoCase(rmt_blaster.GetErrors(), error) != NPOS);
    BOOST_REQUIRE_EQUAL(CRemoteBlast::eStatus_Failed, 
                        rmt_blaster.CheckStatus());
}

// This tests an expired/invalid RID
BOOST_AUTO_TEST_CASE(RetrieveInvalidRID) {
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable tmp_env("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    const string non_existent_rid("1068741992-11111-263425.BLASTQ3");
    CRemoteBlast rmt_blaster(non_existent_rid);
    //rmt_blaster.SetVerbose();

    BOOST_REQUIRE_EQUAL(non_existent_rid, rmt_blaster.GetRID());
    // make sure error is something like: RID not found
    BOOST_REQUIRE_EQUAL(false, rmt_blaster.CheckDone());
    //cerr << "Errors: '" << rmt_blaster.GetErrors() << "'" << endl;
    BOOST_REQUIRE(rmt_blaster.GetErrors() != kEmptyStr);
    BOOST_REQUIRE_EQUAL(CRemoteBlast::eStatus_Unknown,
                        rmt_blaster.CheckStatus());
}

BOOST_AUTO_TEST_CASE(RetrieveRIDWithError) {
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable tmp_env("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    const string rid("HTXY8N45013"); 
    CRemoteBlast rmt_blaster(rid);
    //rmt_blaster.SetVerbose();

    BOOST_REQUIRE_EQUAL(rid, rmt_blaster.GetRID());
    BOOST_REQUIRE_EQUAL(true, rmt_blaster.CheckDone());
    BOOST_REQUIRE_MESSAGE(NStr::Find(rmt_blaster.GetErrors(), 
                                     "CPU usage limit was exceeded, resulting in SIGXCPU") != NPOS,
                          "RID=" << rid);
    BOOST_REQUIRE_EQUAL(CRemoteBlast::eStatus_Failed, rmt_blaster.CheckStatus());
}

BOOST_AUTO_TEST_CASE(RetrieveRIDWithSIGXCPU) {
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable tmp_env("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    const string rid("HTSZX47V013"); 
    CRemoteBlast rmt_blaster(rid);
    //rmt_blaster.SetVerbose();

    BOOST_REQUIRE_EQUAL(rid, rmt_blaster.GetRID());
    BOOST_REQUIRE_EQUAL(true, rmt_blaster.CheckDone());
    //cerr << "Errors: '" << rmt_blaster.GetErrors() << "'" << endl;
    BOOST_REQUIRE_MESSAGE(NStr::Find(rmt_blaster.GetErrors(), 
                             "Error: CPU usage limit was exceeded") != NPOS,
                          "RID=" << rid);
    BOOST_REQUIRE_EQUAL(CRemoteBlast::eStatus_Failed,
                        rmt_blaster.CheckStatus());
}


//     BOOST_AUTO_TEST_CASE(SubmitNonExistentDatabase) {
//         CBlastProteinOptionsHandle prot_opts(CBlastOptions::eRemote);
//
//         CRemoteBlast rmt_blaster(& prot_opts);
//         rmt_blaster.SetDatabase("non_existent_database");
//         BOOST_REQUIRE_EQUAL(true, rmt_blaster.Submit());
//     }

BOOST_AUTO_TEST_CASE(CheckRemoteNuclOptionsHandle) {
    CBlastNucleotideOptionsHandle nucl_opts(CBlastOptions::eRemote);
    // These should not produce errors, although some of them would not have the desired
    // effect either, because there are no remote name-value pairs corresponding to these
    // options.
    nucl_opts.SetWordSize(23);
    try {
        nucl_opts.GetWordSize();
    } catch (const CBlastException& exptn) {
        BOOST_REQUIRE(!strcmp("Error: GetWordSize() not available.", 
                               exptn.GetMsg().c_str()));
    }
}

BOOST_AUTO_TEST_CASE(CheckRemoteDiscNuclOptionsHandle) {
    CDiscNucleotideOptionsHandle nucl_opts(CBlastOptions::eRemote);
    CBlastOptions& opts = nucl_opts.SetOptions();
    const int kWordSize = 12;
    nucl_opts.SetWordSize(kWordSize);

    typedef ncbi::objects::CBlast4_parameters TBlast4Opts;
    TBlast4Opts* blast4_opts = opts.GetBlast4AlgoOpts();

    BOOST_REQUIRE_EQUAL(kWordSize, 
          blast4_opts->GetParamByName("WordSize")->GetValue().GetInteger());
    BOOST_REQUIRE_EQUAL(18, 
          blast4_opts->GetParamByName("MBTemplateLength")->GetValue().GetInteger());
    BOOST_REQUIRE_EQUAL(0, 
          blast4_opts->GetParamByName("MBTemplateType")->GetValue().GetInteger());
    BOOST_REQUIRE_EQUAL(BLAST_WINDOW_SIZE_DISC, 
          blast4_opts->GetParamByName("WindowSize")->GetValue().GetInteger());

/*
    ITERATE(ncbi::objects::CBlast4_parameters_Base::Tdata, 
           it, blast4_opts->Set())
    {
           cerr << '\n';
           cerr << (*it)->GetName() << '\n';
    }
*/
}

BOOST_AUTO_TEST_CASE(RetrieveMultipleQueryResults)
{
    // A preserved query of 129295, 129296, and 129297.
    
    string rid("1112991234-9646-26841459756.BLASTQ3");
    CRemoteBlast rb(rid);
    
    TSeqAlignVector sav(rb.GetSeqAlignSets());
    
    BOOST_REQUIRE_EQUAL(3, (int)sav.size());
    
    vector<string> ids;
    for(int i = 0; i < (int)sav.size(); i++) {
        string L;
        sav[i]->Get().front()->GetSeq_id(0).GetLabel(& L);
        ids.push_back(L);
    }
    
    BOOST_REQUIRE_EQUAL(string("gi|129295"), ids[0]);
    BOOST_REQUIRE_EQUAL(string("gi|129296"), ids[1]);
    BOOST_REQUIRE_EQUAL(string("gi|129297"), ids[2]);
}

BOOST_AUTO_TEST_CASE(RetrieveQuerySet)
{
    // Another preserved query, with mixed ID types.
    
    string rid("BWX50RMX016");  // GIs 104501, 129295, and FASTA for 400260645 vs swissprot   
    
    CRemoteBlast rb(rid);
    
    TSeqAlignVector sav(rb.GetSeqAlignSets());
    
    BOOST_REQUIRE_EQUAL(3, (int)sav.size());
    
    vector<string> ids;
    for(int i = 0; i < (int)sav.size(); i++) {
        string L;
        sav[i]->Get().front()->GetSeq_id(0).GetLabel(& L);
        ids.push_back(L);
    }
    
    BOOST_REQUIRE_EQUAL(string("gi|104501"), ids[0]);
    BOOST_REQUIRE_EQUAL(string("gi|129295"), ids[1]);
    BOOST_REQUIRE_EQUAL(string("lcl|47622"), ids[2]);
}

BOOST_AUTO_TEST_CASE(GetRequestInfo)
{
    string rid("1138040498-4204-115424753375.BLASTQ4");
    
    CRemoteBlast rb(rid);
    
    string db_name = "nr";
    
    CRef<CBlast4_database> dbs = rb.GetDatabases();
    
    BOOST_REQUIRE_EQUAL(dbs->GetName(), db_name);
    BOOST_REQUIRE_EQUAL(dbs->GetType(), eBlast4_residue_type_nucleotide);
    
    BOOST_REQUIRE_EQUAL(rb.GetProgram(), string("blastn"));
    BOOST_REQUIRE_EQUAL(rb.GetService(), string("megablast"));
    BOOST_REQUIRE_EQUAL(rb.GetCreatedBy(), string("newblast"));
    
    CRef<CBlast4_queries> queries = rb.GetQueries();
    
    BOOST_REQUIRE_EQUAL(queries->Which(), CBlast4_queries::e_Seq_loc_list);

}

BOOST_AUTO_TEST_CASE(FetchQuerySequence)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");

    // This RID refers to a search by Seq-loc - this tests the
    // ability of the CRemoteBlast class to fetch the query
    // sequence (data) associated with the described sequence,
    // and checks that the length is correct.
    
    string rid("BWY0XPAV014");  // P38398.2 and 129295 vs swissprot
    
    CRemoteBlast rb(rid);
    
    // Get queries - assume its a list o' Seq-loc.
    
    CRef<CBlast4_queries> queries = rb.GetQueries();
    
    // Get databases
    
    CRef<CBlast4_database> dbs = rb.GetDatabases();
    
    // And database type
    
    char db_type;
    string db_name;
    
    if (dbs->GetType() == eBlast4_residue_type_nucleotide) {
        db_type = 'n';
        db_name = "nucl_dbs";
    } else {
        db_type = 'p';
        db_name = "prot_dbs";
    }
    
    // Get first query Seq-loc.
    
    CRef<CSeq_loc> query1(queries->SetSeq_loc_list().front());
    
    // Assuming it is a "whole" Seq-loc, make a vector of Seq-ids.
    
    CRef<CSeq_id> seqid(&query1->SetWhole());
    
    CBlastServices::TSeqIdVector getseq_queries;
    getseq_queries.push_back(seqid);
    
    // Now fetch the sequence.
    
    string warnings, errors;
    CBlastServices::TBioseqVector results;
    
    CBlastServices::GetSequences(getseq_queries,
                               db_name,
                               db_type,
                               results,   // out
                               errors,    // out
                               warnings); // out
    
    BOOST_REQUIRE(results.size());
    BOOST_REQUIRE(results[0].NotEmpty());
    BOOST_REQUIRE(results[0]->CanGetInst());

    int length = results[0]->GetInst().GetLength();
    BOOST_REQUIRE_EQUAL(length, 1863);
    //length = results[1]->GetInst().GetLength();
    //BOOST_REQUIRE_EQUAL(length, 232);
}

BOOST_AUTO_TEST_CASE(FetchQuerySequence_NotFound)
{
    // Uncomment to redirect to test system
    //CAutoEnvironmentVariable autoenv("BLAST4_CONN_SERVICE_NAME", "blast4_test");
    const int kGi(129295);
    CRef<CSeq_id> seqid(new CSeq_id(CSeq_id::e_Gi, kGi));
    CBlastServices::TSeqIdVector getseq_queries;
    getseq_queries.push_back(seqid);
    
    string warnings, errors;
    CBlastServices::TBioseqVector results;
    
    CBlastServices::GetSequences(getseq_queries, "nr", 'n',
                               results,   // out
                               errors,    // out
                               warnings/*,  // out
                               true*/); // out
    
    BOOST_REQUIRE(results.empty());
    BOOST_REQUIRE( !errors.empty() );
    BOOST_REQUIRE( errors.find("Failed to fetch sequence") != NPOS );
    BOOST_REQUIRE( errors.find(NStr::IntToString(kGi)) != NPOS );
    BOOST_REQUIRE(warnings.empty());
}

BOOST_AUTO_TEST_CASE(SearchOptionsFromRID)
{
    {
        // Nucleotide
        
        string rid("5VPRD45W015");
        CRemoteBlast rmt(rid);
        
        CRef<CBlastOptionsHandle> cboh = rmt.GetSearchOptions();
        
        BOOST_REQUIRE(cboh.NotEmpty());
        
        BOOST_REQUIRE_EQUAL((Int8) 0, (Int8) cboh->GetDbLength());
        BOOST_REQUIRE_EQUAL((Int8) 0, (Int8) cboh->GetEffectiveSearchSpace());
        BOOST_REQUIRE_EQUAL(10.0, cboh->GetEvalueThreshold());
        {
            TAutoCharPtr tmp = cboh->GetFilterString();/* NCBI_FAKE_WARNING */
            BOOST_REQUIRE_EQUAL(string("L;R -d repeat/repeat_9606;m;"),
                                string(tmp.get()));
        }
        BOOST_REQUIRE_EQUAL(100, cboh->GetHitlistSize());
        BOOST_REQUIRE_EQUAL(0.0, cboh->GetPercentIdentity());
        BOOST_REQUIRE_EQUAL(true, cboh->GetGappedMode());
        BOOST_REQUIRE_EQUAL(0, cboh->GetWindowSize());
    }
    {
        // Protein
        
        string rid("BD4HUK3X014");
        
        CRemoteBlast rmt(rid);
        CRef<CBlastOptionsHandle> cboh = rmt.GetSearchOptions();
        
        BOOST_REQUIRE(cboh.NotEmpty());
        
        BOOST_REQUIRE_EQUAL((Int8) 0, (Int8) cboh->GetDbLength());
        BOOST_REQUIRE_EQUAL((Int8) 0, (Int8) cboh->GetEffectiveSearchSpace());
        BOOST_REQUIRE_EQUAL(13.0, cboh->GetEvalueThreshold());
        {
            TAutoCharPtr tmp = cboh->GetFilterString();/* NCBI_FAKE_WARNING */
            BOOST_REQUIRE_EQUAL(string("L;"), string(tmp.get()));
        }
        BOOST_REQUIRE_EQUAL(500, cboh->GetHitlistSize());
        BOOST_REQUIRE_EQUAL(0.0, cboh->GetPercentIdentity());
        BOOST_REQUIRE_EQUAL(true, cboh->GetGappedMode());
        BOOST_REQUIRE_EQUAL(40, cboh->GetWindowSize());
    }
    {
        // Some of everything
        vector< pair<string,string> > rids;
        
        x_PushPairVec(rids, "1126029035-8294-165438177459.BLASTQ3",  "blastp/plain");
        x_PushPairVec(rids, "1125682249-11093-192188840277.BLASTQ3", "blastn/plain");
        x_PushPairVec(rids, "1125679472-29663-68767107779.BLASTQ3",  "tblastn/plain");
        x_PushPairVec(rids, "1125682851-24545-80609495337.BLASTQ3",  "tblastx/plain");
        x_PushPairVec(rids, "1125682308-9604-184897235466.BLASTQ3",  "blastx/plain");
        x_PushPairVec(rids, "BD4GD2RE016",  "blastn/megablast");
        
        for(size_t i = 0; i < rids.size(); i++) {
            CRemoteBlast rmt(rids[i].first);
            CRef<CBlastOptionsHandle> cboh = rmt.GetSearchOptions();
            
            BOOST_REQUIRE(cboh.NotEmpty());
            
            string ps = rmt.GetProgram() + "/" + rmt.GetService();
            BOOST_REQUIRE_EQUAL(ps, rids[i].second);
        }
    }
}

BOOST_AUTO_TEST_CASE(CheckLongLifeHits)
{
    string has_hits = "1154969303-04718-55159010680.BLASTQ4";
    string no_hits = "1154969303-04728-192386478174.BLASTQ4";
    
    BOOST_REQUIRE_EQUAL(22, x_CountHits(has_hits));
    BOOST_REQUIRE_EQUAL(0, x_CountHits(no_hits));
}

BOOST_AUTO_TEST_CASE(CheckShortRIDs)
{
    BOOST_REQUIRE_EQUAL(102, x_CountHits("BD4HYZYB014"));
    BOOST_REQUIRE_EQUAL(102, x_CountHits("15ASW73R015"));
    BOOST_REQUIRE_EQUAL(102, x_CountHits("15AU5834013"));
    BOOST_REQUIRE_EQUAL(102, x_CountHits("BD4ENAS7014"));
}

BOOST_AUTO_TEST_CASE(CheckDuplicateOptions)
{
    CRef<CBlastProteinOptionsHandle> oh
        (new CBlastProteinOptionsHandle(CBlastOptions::eRemote));
    
    oh->SetWordSize(10);
    oh->SetWordSize(11);
    oh->SetWordSize(12);
    oh->SetWordSize(13);
    oh->SetWordSize(14);
    
    ncbi::objects::CBlast4_parameters * L =
        oh->SetOptions().GetBlast4AlgoOpts();
    typedef ncbi::objects::CBlast4_parameter TParam;
    typedef list< CRef<TParam> > TParamList;
    
    int count = 0;
    int value = 0;
    
    ITERATE(TParamList, iter, L->Set()) {
        const TParam & p = **iter;
        
        if (p.GetName() == "WordSize") {
            BOOST_REQUIRE(p.CanGetValue());
            BOOST_REQUIRE(p.GetValue().IsInteger());
            
            count ++;
            value = p.GetValue().GetInteger();
        }
    }
    
    BOOST_REQUIRE_EQUAL(1, count);
    BOOST_REQUIRE_EQUAL(14, value);
}

// Test that when a query with a range restriction is NOT provided, no
// RequiredEnd and RequiredStart fields are sent over the network
BOOST_AUTO_TEST_CASE(GetSearchStrategy_FullQuery) {
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 555));
    auto_ptr<blast::SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(*id));
    TSeqLocVector queries(1, *sl.get());
    CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(queries));
    const string kDbName("nt");
    const CSearchDatabase target_db(kDbName,
                                    CSearchDatabase::eBlastDbIsNucleotide);

    CRef<CBlastOptionsHandle> opts
        (CBlastOptionsFactory::Create(eBlastn, CBlastOptions::eRemote));

    CRemoteBlast rmt_blast(qf, opts, target_db);
    CRef<CBlast4_request> ss = rmt_blast.GetSearchStrategy();
    BOOST_REQUIRE(ss.NotEmpty());

    bool found_query_range = false;

    const CBlast4_request_body& body = ss->GetBody();
    BOOST_REQUIRE(body.IsQueue_search());
    const CBlast4_queue_search_request& qsr = body.GetQueue_search();

    // These are the parameters that we are looking for
    vector<string> param_names;
    param_names.push_back(CBlast4Field::GetName(eBlastOpt_RequiredStart));
    param_names.push_back(CBlast4Field::GetName(eBlastOpt_RequiredEnd));

    // Get the program options 
    if (qsr.CanGetProgram_options()) {
        const CBlast4_parameters& prog_options = qsr.GetProgram_options();
        ITERATE(vector<string>, pname, param_names) {
            CRef<CBlast4_parameter> p = prog_options.GetParamByName(*pname);
            if (p.NotEmpty()) {
                found_query_range = true;
                break;
            }
        }
    }
    BOOST_REQUIRE(found_query_range == false);

    // (check also the algorithm options, just in case they ever get misplaced)
    if (qsr.CanGetAlgorithm_options()) {
        const CBlast4_parameters& algo_options = qsr.GetAlgorithm_options();
        ITERATE(vector<string>, pname, param_names) {
            CRef<CBlast4_parameter> p = algo_options.GetParamByName(*pname);
            if (p.NotEmpty()) {
                found_query_range = true;
                break;
            }
        }
    }
    BOOST_REQUIRE(found_query_range == false);

    // just as a bonus, check the database
    BOOST_REQUIRE(qsr.CanGetSubject());
    BOOST_REQUIRE(qsr.GetSubject().GetDatabase() == kDbName);
}

// Test that when a query with a range restriction is provided, the appropriate
// RequiredEnd and RequiredStart fields are sent over the network
BOOST_AUTO_TEST_CASE(GetSearchStrategy_QueryWithRange) {
    CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, 555));
    TSeqRange query_range(1,200);
    auto_ptr<blast::SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(*id,
                                query_range));
    TSeqLocVector queries(1, *sl.get());
    CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(queries));
    const string kDbName("nt");
    const CSearchDatabase target_db(kDbName,
                                    CSearchDatabase::eBlastDbIsNucleotide);

    CRef<CBlastOptionsHandle> opts
        (CBlastOptionsFactory::Create(eBlastn, CBlastOptions::eRemote));

    CRemoteBlast rmt_blast(qf, opts, target_db);
    CRef<CBlast4_request> ss = rmt_blast.GetSearchStrategy();
    BOOST_REQUIRE(ss.NotEmpty());

    bool found_query_range = false;

    const CBlast4_request_body& body = ss->GetBody();
    BOOST_REQUIRE(body.IsQueue_search());
    const CBlast4_queue_search_request& qsr = body.GetQueue_search();

    // These are the parameters that we are looking for
    vector<string> param_names;
    param_names.push_back(CBlast4Field::GetName(eBlastOpt_RequiredStart));
    param_names.push_back(CBlast4Field::GetName(eBlastOpt_RequiredEnd));

    // Get the program options 
    if (qsr.CanGetProgram_options()) {
        const CBlast4_parameters& prog_options = qsr.GetProgram_options();
        ITERATE(vector<string>, pname, param_names) {
            CRef<CBlast4_parameter> p = prog_options.GetParamByName(*pname);
            if (p.NotEmpty()) {
                BOOST_REQUIRE(p->CanGetValue());
                found_query_range = true;
                if (*pname == CBlast4Field::GetName(eBlastOpt_RequiredStart)) {
                    BOOST_REQUIRE_EQUAL((int)query_range.GetFrom(), 
                                        (int)p->GetValue().GetInteger());
                }
                if (*pname == CBlast4Field::GetName(eBlastOpt_RequiredEnd)) {
                    BOOST_REQUIRE_EQUAL((int)query_range.GetTo(), 
                                        (int)p->GetValue().GetInteger());
                }
            }
        }
    }
    BOOST_REQUIRE(found_query_range == true);

    found_query_range = false;
    // Check that this option is NOT specified in the algorithm options
    if (qsr.CanGetAlgorithm_options()) {
        const CBlast4_parameters& algo_options = qsr.GetAlgorithm_options();
        ITERATE(vector<string>, pname, param_names) {
            CRef<CBlast4_parameter> p = algo_options.GetParamByName(*pname);
            if (p.NotEmpty()) {
                found_query_range = true;
                break;
            }
        }
    }
    BOOST_REQUIRE(found_query_range == false);

    // just as a bonus, check the database
    BOOST_REQUIRE(qsr.CanGetSubject());
    BOOST_REQUIRE(qsr.GetSubject().GetDatabase() == kDbName);
}

// Test that when no identifier is provided for the sequence data, a Bioseq
// should be submitted
BOOST_AUTO_TEST_CASE(GetSearchStrategy_QueryWithLocalIds) {

    CSeq_entry seq_entry;
    ifstream in("data/seq_entry_lcl_id.asn");
    in >> MSerial_AsnText >> seq_entry;
    CSeq_id& id = const_cast<CSeq_id&>(*seq_entry.GetSeq().GetFirstId());
    in.close();

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    scope->AddTopLevelSeqEntry(seq_entry);
    CRef<CSeq_loc> sl(new CSeq_loc(id, (TSeqPos)0, (TSeqPos)11));
    TSeqLocVector query_loc(1, SSeqLoc(sl, scope));
    CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(query_loc));
    const string kDbName("nt");
    const CSearchDatabase target_db(kDbName,
                                    CSearchDatabase::eBlastDbIsNucleotide);

    CRef<CBlastOptionsHandle> opts
        (CBlastOptionsFactory::Create(eBlastn, CBlastOptions::eRemote));

    CRemoteBlast rmt_blast(qf, opts, target_db);
    CRef<CBlast4_request> ss = rmt_blast.GetSearchStrategy();
    BOOST_REQUIRE(ss.NotEmpty());


    const CBlast4_request_body& body = ss->GetBody();
    BOOST_REQUIRE(body.IsQueue_search());
    const CBlast4_queue_search_request& qsr = body.GetQueue_search();
    BOOST_REQUIRE(qsr.CanGetQueries());
    const CBlast4_queries& b4_queries = qsr.GetQueries();
    BOOST_REQUIRE_EQUAL(query_loc.size(), b4_queries.GetNumQueries());
    BOOST_REQUIRE(b4_queries.IsBioseq_set());
    BOOST_REQUIRE( !b4_queries.IsPssm() );
    BOOST_REQUIRE( !b4_queries.IsSeq_loc_list() );

    // just as a bonus, check the database
    BOOST_REQUIRE(qsr.CanGetSubject());
    BOOST_REQUIRE(qsr.GetSubject().GetDatabase() == kDbName);
}

// Test that when GIs are provided as the queries, no bioseq
// should be submitted, instead a list of seqlocs should be sent
BOOST_AUTO_TEST_CASE(GetSearchStrategy_QueryWithGIs) {

    CRef<CScope> scope(new CScope(*CObjectManager::GetInstance()));
    typedef pair<int, int> TGiLength;
    vector<TGiLength> gis;
    gis.push_back(TGiLength(555, 624));
    gis.push_back(TGiLength(556, 310));
    ifstream in("data/seq_entry_gis.asn");
    TSeqLocVector query_loc;

    ITERATE(vector<TGiLength>, gi, gis) {
        CRef<CSeq_entry> seq_entry(new CSeq_entry);
        in >> MSerial_AsnText >> *seq_entry;
        scope->AddTopLevelSeqEntry(*seq_entry);
        CRef<CSeq_id> id(new CSeq_id(CSeq_id::e_Gi, gi->first));
        CRef<CSeq_loc> sl(new CSeq_loc(*id, 0, gi->second));
        query_loc.push_back(SSeqLoc(sl, scope));
    }
    in.close();

    CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(query_loc));
    const string kDbName("nt");
    const CSearchDatabase target_db(kDbName,
                                    CSearchDatabase::eBlastDbIsNucleotide);

    CRef<CBlastOptionsHandle> opts
        (CBlastOptionsFactory::Create(eBlastn, CBlastOptions::eRemote));

    CRemoteBlast rmt_blast(qf, opts, target_db);
    CRef<CBlast4_request> ss = rmt_blast.GetSearchStrategy();
    BOOST_REQUIRE(ss.NotEmpty());


    const CBlast4_request_body& body = ss->GetBody();
    BOOST_REQUIRE(body.IsQueue_search());
    const CBlast4_queue_search_request& qsr = body.GetQueue_search();
    BOOST_REQUIRE(qsr.CanGetQueries());
    const CBlast4_queries& b4_queries = qsr.GetQueries();
    BOOST_REQUIRE_EQUAL(query_loc.size(), b4_queries.GetNumQueries());
    BOOST_REQUIRE( !b4_queries.IsBioseq_set() );
    BOOST_REQUIRE( !b4_queries.IsPssm() );
    BOOST_REQUIRE( b4_queries.IsSeq_loc_list() );

    // just as a bonus, check the database
    BOOST_REQUIRE(qsr.CanGetSubject());
    BOOST_REQUIRE(qsr.GetSubject().GetDatabase() == kDbName);
}

BOOST_AUTO_TEST_CASE(ReadSearchStrategy_TextAsn1) 
{
    const char* fname = "data/ss.asn";
    ifstream in(fname);
    BOOST_REQUIRE(in);
    CRef<CBlast4_request> search_strategy = ExtractBlast4Request(in);
    BOOST_REQUIRE(search_strategy.NotEmpty());
    BOOST_REQUIRE(search_strategy->GetBody().GetQueue_search().GetProgram() 
                  == "blastn");
    BOOST_REQUIRE(search_strategy->GetBody().GetQueue_search().GetService() 
                  == "megablast");
}

BOOST_AUTO_TEST_CASE(ReadSearchStrategy_Xml) 
{
    const char* fname = "data/ss.xml";
    ifstream in(fname);
    BOOST_REQUIRE(in);
    CRef<CBlast4_request> search_strategy = ExtractBlast4Request(in);
    BOOST_REQUIRE(search_strategy.NotEmpty());
    BOOST_REQUIRE(search_strategy->GetBody().GetQueue_search().GetProgram() 
                  == "blastn");
    BOOST_REQUIRE(search_strategy->GetBody().GetQueue_search().GetService() 
                  == "plain");
}

BOOST_AUTO_TEST_CASE(ReadSearchStrategy_Invalid) 
{
    const char* fname = "data/seq_entry_gis.asn";
    ifstream in(fname);
    BOOST_REQUIRE(in);
    CRef<CBlast4_request> search_strategy;
    BOOST_REQUIRE_THROW(search_strategy = ExtractBlast4Request(in),
                        CSerialException);
}

BOOST_AUTO_TEST_CASE(ReadArchiveFormat)
{
    const char* fname = "data/archive.asn";
    ifstream in(fname);
    CRemoteBlast rb(in);
    rb.LoadFromArchive();
    BOOST_REQUIRE(rb.GetProgram() == "blastn");
    BOOST_REQUIRE(rb.GetService() == "megablast");
    BOOST_REQUIRE(rb.GetCreatedBy() == "tom");
    CRef<CBlast4_database> blast_db = rb.GetDatabases();
    BOOST_REQUIRE(blast_db->GetName() == "refseq_rna");
    BOOST_REQUIRE(rb.GetDbFilteringAlgorithmId() == -1);
    BOOST_REQUIRE(rb.GetDbFilteringAlgorithmKey() == kEmptyStr);
    CBlastNucleotideOptionsHandle* opts_handle = 
       dynamic_cast<CBlastNucleotideOptionsHandle*> (&*(rb.GetSearchOptions()));
    BOOST_REQUIRE(string(opts_handle->GetRepeatFilteringDB()) == "repeat/repeat_9606");
}

BOOST_AUTO_TEST_CASE(ReadBadArchiveFormat)
{
    const char* fname = "data/selenocysteines.fsa";
    ifstream in(fname);
    BOOST_REQUIRE_THROW(CRemoteBlast rb(in), CBlastException);

}

BOOST_AUTO_TEST_CASE(ReadBl2seqArchiveFormat)
{
    const char* fname = "data/archive.bl2seq.asn";
    ifstream in(fname);
    CRemoteBlast rb(in);
    rb.LoadFromArchive();
    BOOST_REQUIRE(rb.GetProgram() == "blastn");
    BOOST_REQUIRE(rb.GetService() == "megablast");
    BOOST_REQUIRE(rb.GetCreatedBy() == "tom");
}

BOOST_AUTO_TEST_CASE(ReadArchiveFormatMultipleQueries)
{
    const char* fname = "data/archive.multiple_queries.asn";
    ifstream in(fname);
    CRemoteBlast rb(in);
    rb.LoadFromArchive();
    BOOST_REQUIRE(rb.GetProgram() == "blastn");
    BOOST_REQUIRE(rb.GetService() == "plain");
    BOOST_REQUIRE(rb.GetCreatedBy() == "tom");
    CRef<CBlast4_database> blast_db = rb.GetDatabases();
    BOOST_REQUIRE(blast_db->GetName() == "nt");
    CRef<CSearchResultSet> result_set = rb.GetResultSet();
    BOOST_REQUIRE(result_set->GetNumQueries() == 3);
    BOOST_REQUIRE(result_set->GetNumResults() == 3);
}

class CDiagLevelGuard {
public:
    CDiagLevelGuard(EDiagSev target)    { m_Orig = SetDiagPostLevel(target); }
    ~CDiagLevelGuard()                  { SetDiagPostLevel(m_Orig); }
private:
    EDiagSev m_Orig;
};

BOOST_AUTO_TEST_CASE(GetBlast4Parameters)
{
    const string kUnknown("-");
    CBlast4Field p = CBlast4Field::Get(eBlastOpt_Web_ExclModels);
    BOOST_REQUIRE(p.GetName() != kUnknown);
    p = CBlast4Field::Get(eBlastOpt_MbIndexName);
    BOOST_REQUIRE(p.GetName() != kUnknown);

    // These shouldn't be found, supress diagnostics
    CDiagLevelGuard g(eDiag_Error);
    p = CBlast4Field::Get(eBlastOpt_MaxValue);
    BOOST_REQUIRE(p.GetName() == kUnknown);
    p = CBlast4Field::Get(eBlastOpt_Program);
    BOOST_REQUIRE(p.GetName() == kUnknown);

    p = CBlast4Field::Get(eBlastOpt_QueryCovHspPerc);
    BOOST_REQUIRE(p.GetName() != kUnknown);
}
BOOST_AUTO_TEST_SUITE_END()
