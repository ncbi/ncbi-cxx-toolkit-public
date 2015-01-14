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
* Author:  Jian Ye
*
* File Description:
*   Unit test module to test CShowBlastDefline
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbistre.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <objects/blastdb/defline_extra.hpp>
#include <objtools/align_format/showdefline.hpp>

#include "blast_test_util.hpp"
#define NCBI_BOOST_NO_AUTO_TEST_MAIN
#include <corelib/test_boost.hpp>
#include <boost/test/auto_unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>

using namespace std;
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::align_format;
using namespace TestUtil;

BOOST_AUTO_TEST_SUITE(showdefline)

struct CShowBlastDeflineTest : public CShowBlastDefline {

    CShowBlastDeflineTest(const CSeq_align_set& seqalign,
                      CScope& scope,
                      size_t line_length = 65,
                      size_t deflines_to_show = align_format::kDfltArgNumDescriptions,
                      bool translated_nuc_alignment = false,
                      CRange<TSeqPos>* master_range = NULL)
        : CShowBlastDefline(seqalign, scope, line_length, 
                            deflines_to_show, translated_nuc_alignment,
                            master_range)
    {}

    static void GetDeflineInfo(CBlastOM::ELocation location)
    {
        CNcbiIfstream is("data/showdefline-cppunit.aln");
        auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, is));
        CRef<CSeq_annot> san(new CSeq_annot);
        *in >> *san;
        
        const CSeq_annot::TData& data = san->GetData();
        const CSeq_annot::TData::TAlign& align= data.GetAlign();
        
        CRef<CSeq_align_set> seqalign(new CSeq_align_set);
        seqalign->Set() = align;
        
        const string kDbName("nr");
        const CBlastDbDataLoader::EDbType kDbType(CBlastDbDataLoader::eProtein);
        
        TestUtil::CBlastOM tmp_data_loader(kDbName, kDbType, location);
        
        CRef<CScope> scope = tmp_data_loader.NewScope();
        
        CShowBlastDeflineTest sbd(*seqalign, *scope);
        int options = 0;
        options += CShowBlastDefline::eShowGi|
            CShowBlastDefline::eHtml|
            CShowBlastDefline::eLinkout;
        sbd.SetOption(options);
        
        int i = 0;
        ITERATE(CSeq_align_set::Tdata, iter, seqalign->Get()){
            auto_ptr<CShowBlastDefline::SScoreInfo>
                si(sbd.x_GetScoreInfo(**iter, 1));
            auto_ptr<CShowBlastDefline::SDeflineInfo>
                dl(sbd.x_GetDeflineInfo(si->id, si->use_this_gi, 1));
            CShowBlastDeflineTest::TestData(dl.get(), si.get(), i);
            i++;
            if (i > 1) {
                break;
            }
        }
    }

    static void TestData(CShowBlastDefline::SDeflineInfo* dl, 
                  CShowBlastDefline::SScoreInfo* si, 
                  int index)
    {
        TGi gi_list[] = {GI_FROM(TIntId, 18426812), GI_FROM(TIntId, 6680636)};
        string defline[] = {"adenosine deaminase [Rattus norvegicus]", 
                            "adenosine deaminase [Mus musculus]"};
        string evalue_string[] = {"2e-141", "3e-141"};
        string bit_string[] = {"503", "502"};
        int sum_n[] = {1, 1};
                
        string id_url[] = {"<a title=\"Show report for NP_569083.1\" href=\"http://www.ncbi.nlm.nih.gov/nucleotide/18426812?report=genbank&log$=nucltop&blast_rank=1&RID=\" >",
                           "<a title=\"Show report for NP_031424.1\" href=\"http://www.ncbi.nlm.nih.gov/nucleotide/6680636?report=genbank&log$=nucltop&blast_rank=1&RID=\" >"};
        string score_url[] = {"<a href=#18426812>", "<a href=#6680636>"};
        bool is_new[] = {false, false};
        bool was_checked[] = {false, false};
        
        BOOST_REQUIRE_EQUAL(dl->gi, gi_list[index]);
        BOOST_REQUIRE_EQUAL(dl->defline, defline[index]);
        BOOST_REQUIRE_EQUAL(dl->id_url, id_url[index]);
        BOOST_REQUIRE_EQUAL(dl->score_url, score_url[index]);
        BOOST_REQUIRE_EQUAL(dl->is_new, is_new[index]);
        BOOST_REQUIRE_EQUAL(dl->was_checked, was_checked[index]);

        BOOST_REQUIRE_EQUAL(si->evalue_string, evalue_string[index]);
        BOOST_REQUIRE_EQUAL(si->bit_string, bit_string[index]);
        BOOST_REQUIRE_EQUAL(si->sum_n, sum_n[index]); 
    }
};

BOOST_AUTO_TEST_CASE(LocalDeflineInfo)
{
    CShowBlastDeflineTest::GetDeflineInfo(CBlastOM::eLocal);
}

BOOST_AUTO_TEST_CASE(RemoteDeflineInfo)
{
    CShowBlastDeflineTest::GetDeflineInfo(CBlastOM::eRemote);
}

BOOST_AUTO_TEST_SUITE_END()
