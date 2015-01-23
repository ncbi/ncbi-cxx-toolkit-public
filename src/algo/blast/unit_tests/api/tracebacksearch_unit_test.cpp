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
 * Author:  Kevin Bealer
 *
 * File Description:
 *   Unit test module for the traceback stage of the BLAST search.
 *
 * ===========================================================================
 */
#include <ncbi_pch.hpp>
#include <corelib/test_boost.hpp>

#include <algo/blast/api/traceback_stage.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <algo/blast/api/local_search.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>

#include <algo/blast/core/blast_hspstream.h>
#include <algo/blast/core/hspfilter_collector.h>

#include <objects/seqalign/seqalign__.hpp>

#include <algo/blast/api/seqsrc_seqdb.hpp>
#include <algo/blast/api/blast_seqinfosrc.hpp>
#include <algo/blast/api/seqinfosrc_seqdb.hpp>


// needed for objmgr dependent tests of query data interface
#include "test_objmgr.hpp"
#include "blast_test_util.hpp"

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::blast;


class CTracebackSearchTestFixture {
public:    

    CRef< CStructWrapper<BlastHSPStream> >
    x_GetSampleHspStream(CRef<CBlastOptions> opts, CSeqDB & db)
    {
        CSeq_id sid("gi|158292535");
        
        vector<int> OIDS;
        db.SeqidToOids(sid, OIDS);
        
        BOOST_REQUIRE(OIDS.size());
        
        const int k_num_hsps_start = 12;
        //const int k_num_hsps_end = 10;
        
        // Taken from traceback unit test.
        
        BlastHSPList* hsp_list = 
            (BlastHSPList*) calloc(1, sizeof(BlastHSPList)); 
        hsp_list->oid = OIDS[0];
        hsp_list->hspcnt = k_num_hsps_start;
        hsp_list->allocated = k_num_hsps_start;
        hsp_list->hsp_max = k_num_hsps_start;
        hsp_list->do_not_reallocate = FALSE;

        hsp_list->hsp_array = (BlastHSP**)
            malloc(hsp_list->allocated*sizeof(BlastHSP*));
        
	BlastHSPWriterInfo * writer_info = BlastHSPCollectorInfoNew(
	               BlastHSPCollectorParamsNew(
                                opts->GetHitSaveOpts(),
                                opts->GetExtnOpts()->compositionBasedStats,
                                opts->GetScoringOpts()->gapped_calculation));
        
        BlastHSPWriter* writer = BlastHSPWriterNew(&writer_info, NULL);
        BOOST_REQUIRE(writer_info == NULL);
        
        BlastHSPStream* hsp_stream = BlastHSPStreamNew(
                                       opts->GetProgramType(),
                                       opts->GetExtnOpts(),
                                       FALSE, 1, writer);

        const int query_offset[k_num_hsps_start] =
            { 0, 3864, 3254, 1828, 2189, 795,
              607, 1780, 1363, 2751, 3599, 242 };
        const int query_end[k_num_hsps_start] =
            { 307, 4287, 3556, 2058, 2269, 914,
              741, 1821, 1451, 2810, 3631, 285 };
        const int subject_offset[k_num_hsps_start] =
            { 1, 2723, 2267, 1028, 1292, 634,
              501, 925, 1195, 1795, 477, 1233 };
        const int subject_end[k_num_hsps_start] =
            { 321, 3171, 2537, 1243, 1371, 749,
              618, 966, 1286, 1869, 509, 1276 };
        const int score[k_num_hsps_start] =
            { 370, 319, 139, 120, 89, 84,
              75, 70, 69, 60, 47, 43 };
        const int query_gapped_start[k_num_hsps_start] =
            { 47, 4181, 3286, 2034, 2228, 871,
              632, 1798, 1383, 2759, 3606, 259 };
        const int subject_gapped_start[k_num_hsps_start] =
            { 48, 3073, 2299, 1219, 1330, 709,
              526, 943, 1215, 1803, 484, 1250 };
        
        for (int index=0; index<k_num_hsps_start; index++) {
            BlastHSP * h1
                = hsp_list->hsp_array[index]
                = (BlastHSP*) calloc(1, sizeof(BlastHSP));
            
            h1->query.offset = query_offset[index];
            h1->query.end = query_end[index];
            h1->subject.offset = subject_offset[index];
            h1->subject.end = subject_end[index];
            h1->score = score[index];
            h1->query.gapped_start = query_gapped_start[index];
            h1->subject.gapped_start = subject_gapped_start[index];
        }
        
        // needed after tie-breaking algorithm for scores was changed
        // in ScoreCompareHSP (blast_hits.c, revision 1.139)
        Blast_HSPListSortByScore(hsp_list);
        BlastHSPStreamWrite(hsp_stream, &hsp_list);
        
        CRef< CStructWrapper<BlastHSPStream> >
            hsps(WrapStruct(hsp_stream, BlastHSPStreamFree));
        
        return hsps;
    }
    
    CRef< CStructWrapper<BlastHSPStream> >
    x_GetSelfHitHspStream(CRef<CBlastOptions> opts, CSeqDB & db, CSeq_id& sid)
    {
        vector<int> OIDS;
        db.SeqidToOids(sid, OIDS);
        
        BOOST_REQUIRE(OIDS.size());
        
        const int k_num_hsps_start = 1;
        const int k_seq_length = db.GetSeqLength(OIDS.front());
        
        BlastHSPList* hsp_list = 
            (BlastHSPList*) calloc(1, sizeof(BlastHSPList)); 
        hsp_list->oid = OIDS.front();
        hsp_list->hspcnt = k_num_hsps_start;
        hsp_list->allocated = k_num_hsps_start;
        hsp_list->hsp_max = k_num_hsps_start;
        hsp_list->do_not_reallocate = FALSE;

        hsp_list->hsp_array = (BlastHSP**)
            malloc(hsp_list->allocated*sizeof(BlastHSP*));
        
        
        BlastHSPWriterInfo * writer_info = BlastHSPCollectorInfoNew(
                   BlastHSPCollectorParamsNew(
                                    opts->GetHitSaveOpts(),
                                    opts->GetExtnOpts()->compositionBasedStats,
                                    opts->GetScoringOpts()->gapped_calculation));

        BlastHSPWriter* writer = BlastHSPWriterNew(&writer_info, NULL);
        BOOST_REQUIRE(writer_info == NULL);
        
        BlastHSPStream* hsp_stream = BlastHSPStreamNew(
                                       opts->GetProgramType(),
                                       opts->GetExtnOpts(),
                                       FALSE, 1, writer);

        const int query_offset[k_num_hsps_start] = { 0 };
        const int query_end[k_num_hsps_start] = { k_seq_length - 1 };
        const int subject_offset[k_num_hsps_start] = { 0 };
        const int subject_end[k_num_hsps_start] = { k_seq_length - 1 };
        const int score[k_num_hsps_start] = { 0 };
        const int query_gapped_start[k_num_hsps_start] = { 0 };
        const int subject_gapped_start[k_num_hsps_start] = { 0 };
        
        for (int index=0; index<k_num_hsps_start; index++) {
            BlastHSP * h1
                = hsp_list->hsp_array[index]
                = (BlastHSP*) calloc(1, sizeof(BlastHSP));
            
            h1->query.offset = query_offset[index];
            h1->query.end = query_end[index];
            h1->subject.offset = subject_offset[index];
            h1->subject.end = subject_end[index];
            h1->score = score[index];
            h1->query.gapped_start = query_gapped_start[index];
            h1->subject.gapped_start = subject_gapped_start[index];
        }
        
        // needed after tie-breaking algorithm for scores was changed
        // in ScoreCompareHSP (blast_hits.c, revision 1.139)
        Blast_HSPListSortByScore(hsp_list);
        BlastHSPStreamWrite(hsp_stream, &hsp_list);
        
        CRef< CStructWrapper<BlastHSPStream> >
            hsps(WrapStruct(hsp_stream, BlastHSPStreamFree));
        
        return hsps;
    }

    void x_FindUsedGis(const CDense_seg & dseg, set<int> & used)
    {
        typedef vector< CRef< CScore > > TScoreList;
        
        if (dseg.CanGetScores()) {
            const TScoreList & scores = dseg.GetScores();
            
            ITERATE(TScoreList, sc, scores) {
                const CScore & sc1 = **sc;
                
                if (sc1.CanGetId() && sc1.GetId().IsStr()) {
                    string id_name = sc1.GetId().GetStr();
                    
                    if (id_name == "use_this_gi") {
                        BOOST_REQUIRE(sc1.CanGetValue());
                        BOOST_REQUIRE(sc1.GetValue().IsInt());
                        
                        used.insert(sc1.GetValue().GetInt());
                    }
                }
            }
        }
    }
    
    typedef vector< CRef< CScore > > TScoreList;
    
    void x_FindUsedGis(const TScoreList & scores, set<int> & used)
    {
        ITERATE(TScoreList, sc, scores) {
            const CScore & sc1 = **sc;
            
            if (sc1.CanGetId() && sc1.GetId().IsStr()) {
                string id_name = sc1.GetId().GetStr();
                
                if (id_name == "use_this_gi") {
                    BOOST_REQUIRE(sc1.CanGetValue());
                    BOOST_REQUIRE(sc1.GetValue().IsInt());
                        
                    used.insert(sc1.GetValue().GetInt());
                }
            }
        }
    }
    
    void x_FindUsedGis(const CSeq_align_set & aset, set<int> & used)
    {
        ITERATE(CSeq_align_set::Tdata, align, aset.Get()) {
            CSeq_align::C_Segs::E_Choice ch = (**align).GetSegs().Which();
            
            if ((**align).CanGetScore()) {
                x_FindUsedGis((**align).GetScore(), used);
            }
            
            if (ch == CSeq_align::C_Segs::e_Disc) {
                x_FindUsedGis((**align).GetSegs().GetDisc(), used);
            } else if (ch == CSeq_align::C_Segs::e_Denseg) {
//                 x_FindUsedGis((**align).GetSegs().GetDenseg(), used);
            } else {
                BOOST_REQUIRE_EQUAL((int)ch, 0);
            }
        }
    }
    
    CSearchResultSet x_Traceback(CSeqDBGiList * gi_list)
    {
        // Build uniform search factory, get options
        CRef<ISearchFactory> sf(new CLocalSearchFactory);
        CRef<CBlastOptionsHandle> opth(&* sf->GetOptions(eBlastp));
        
        // Get a SeqDB and a query factory... This uses two SeqDB
        // objects in the gi_list case, so that it is possible to
        // fetch the query, which is not in the GI list.
        
        CRef<CSeqDB> query_seqdb(new CSeqDB("nr", CSeqDB::eProtein));
        CRef<CSeqDB> subject_seqdb;
        
        if (gi_list) {
            subject_seqdb.Reset(new CSeqDB("nr", CSeqDB::eProtein, gi_list));
        } else {
            subject_seqdb = query_seqdb;
        }
        
        CSeq_id query("gi|54607139");
        auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(query));
        TSeqLocVector qv;
        qv.push_back(*sl);
        CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(qv));
        
        // Note that the traceback code will (in some cases) modify
        // the options object.
        
        CRef<CBlastOptions>
            opts(& const_cast<CBlastOptions&>(opth->GetOptions()));
        
        // Construct false HSPs.
        CRef< CStructWrapper<BlastHSPStream> >
            hsps(x_GetSampleHspStream(opts, *subject_seqdb));
        
        // Now to do the traceback..
        
        // Build the object itself
        CRef<CBlastTracebackSearch> tbs;
        
        BOOST_REQUIRE(subject_seqdb.NotEmpty());
        CBlastSeqSrc seq_src = SeqDbBlastSeqSrcInit(subject_seqdb);
        CRef<blast::IBlastSeqInfoSrc> seq_info_src(new blast::CSeqDbSeqInfoSrc(subject_seqdb));
        tbs.Reset(new CBlastTracebackSearch(qf, opts, seq_src.Get(), seq_info_src, hsps));
        
        CSearchResultSet v = *tbs->Run();
        
        BOOST_REQUIRE_EQUAL(1, (int)v.GetNumResults());
        BOOST_REQUIRE_EQUAL(0, (int)v[0].GetErrors().size());
        
        return v;
    }
};

BOOST_FIXTURE_TEST_SUITE(tracebacksearch, CTracebackSearchTestFixture)

BOOST_AUTO_TEST_CASE(Traceback) {
    CSearchResultSet rset = x_Traceback(0);
    
    set<int> use_these;
    x_FindUsedGis(*rset[0].GetSeqAlign(), use_these);
    
    BOOST_REQUIRE(use_these.empty());
}

// Traceback without Seq-align
BOOST_AUTO_TEST_CASE(SimpleTraceback)
{
        // Build uniform search factory, get options
        CRef<ISearchFactory> sf(new CLocalSearchFactory);
        CRef<CBlastOptionsHandle> opth(&* sf->GetOptions(eBlastp));
        
        // Get a SeqDB and a query factory... This uses two SeqDB
        // objects in the gi_list case, so that it is possible to
        // fetch the query, which is not in the GI list.
        
        CRef<CSeqDB> query_seqdb(new CSeqDB("nr", CSeqDB::eProtein));
        CRef<CSeqDB> subject_seqdb;
        
        subject_seqdb = query_seqdb;
        
        CSeq_id query("gi|54607139");
        auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(query));
        TSeqLocVector qv;
        qv.push_back(*sl);
        CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(qv));
        
        // Note that the traceback code will (in some cases) modify
        // the options object.
        
        CRef<CBlastOptions>
            opts(& const_cast<CBlastOptions&>(opth->GetOptions()));
        
        // Construct false HSPs.
        CRef< CStructWrapper<BlastHSPStream> >
            hsps(x_GetSampleHspStream(opts, *subject_seqdb));
        
        // Now to do the traceback..
        
        // Build the object itself
        CRef<CBlastTracebackSearch> tbs;
        
        BOOST_REQUIRE(subject_seqdb.NotEmpty());
        CBlastSeqSrc seq_src = SeqDbBlastSeqSrcInit(subject_seqdb);
        CRef<blast::IBlastSeqInfoSrc> seq_info_src(new blast::CSeqDbSeqInfoSrc(subject_seqdb));
        tbs.Reset(new CBlastTracebackSearch(qf, opts, seq_src.Get(), seq_info_src, hsps));
        
        BlastHSPResults* hsp = tbs->RunSimple();

        BOOST_REQUIRE_EQUAL(1, (int)hsp->num_queries);

	BlastHitList* hitlist = hsp->hitlist_array[0];

        BOOST_REQUIRE_EQUAL(1, (int)hitlist->hsplist_count);
}

BOOST_AUTO_TEST_CASE(TracebackWithPssm_AndWarning) {
    // read the pssm
    const string kPssmFile("data/pssm_zero_fratios.asn");
    CRef<CPssmWithParameters> pssm = 
        TestUtil::ReadObject<CPssmWithParameters>(kPssmFile);

    // set up the database
    CRef<CSeqDB> subject_seqdb(new CSeqDB("nr", CSeqDB::eProtein));

    CSeq_id query("gi|129295");
    auto_ptr<SSeqLoc> sl(CTestObjMgr::Instance().CreateSSeqLoc(query));
    TSeqLocVector qv;
    qv.push_back(*sl);
    CRef<IQueryFactory> qf(new CObjMgr_QueryFactory(qv));

    // get the options
    CRef<CBlastOptionsHandle> opth(CBlastOptionsFactory::Create(ePSIBlast));
    CRef<CBlastOptions>
        opts(& const_cast<CBlastOptions&>(opth->GetOptions()));
    
    // Construct false HSPs.
    CRef< CStructWrapper<BlastHSPStream> >
        hsps(x_GetSelfHitHspStream(opts, *subject_seqdb, query));

    // Build the object itself
    CBlastSeqSrc seq_src = SeqDbBlastSeqSrcInit(subject_seqdb);
    CRef<blast::IBlastSeqInfoSrc> seq_info_src(new blast::CSeqDbSeqInfoSrc(subject_seqdb));
    CRef<CBlastTracebackSearch> tbs
        (new CBlastTracebackSearch(qf, opts, seq_src.Get(), seq_info_src, hsps, pssm));

    CSearchResultSet v = *tbs->Run();
    
    BOOST_REQUIRE_EQUAL(1, (int)v.GetNumResults());
    BOOST_REQUIRE_EQUAL(0, (int)v[0].GetErrors().size());

    TQueryMessages qm = v[0].GetErrors(eBlastSevWarning);
    BOOST_REQUIRE_EQUAL(2, (int)qm.size());

    const char* msg = "Frequency ratios for PSSM are all zeros";
    BOOST_REQUIRE(qm.front()->GetMessage().find(msg) != string::npos);

}

BOOST_AUTO_TEST_CASE(TracebackEntrez) {
    CRef<CSeqDBGiList> gi_list
        (new CSeqDBFileGiList("data/Sample_gilist.p.gil"));
    
    CSearchResultSet rset = x_Traceback(gi_list.GetPointerOrNull());
    
    set<int> use_these;
    x_FindUsedGis(*rset[0].GetSeqAlign(), use_these);
    
    BOOST_REQUIRE_EQUAL((int)use_these.size(), 1);
    BOOST_REQUIRE_EQUAL(*use_these.begin(), 158292535);
}

BOOST_AUTO_TEST_SUITE_END()


