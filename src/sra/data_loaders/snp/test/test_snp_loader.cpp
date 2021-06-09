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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Unit tests for SNP data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <sra/data_loaders/snp/snploader.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_table_ci.hpp>
#include <objects/seqtable/seqtable__.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <sra/readers/ncbi_traces_path.hpp>

#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;
USING_SCOPE(objects);

static string s_LoaderName;

enum ESNPSource {
    eFromGB,
    eFromSNP
};

static
CRef<CScope> s_MakeScope0()
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    return scope;
}


static
CRef<CScope> s_MakeScope(const string& file_name = kEmptyStr)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    if ( !s_LoaderName.empty() ) {
        om->RevokeDataLoader(s_LoaderName);
        s_LoaderName.erase();
    }
    if ( file_name.empty() ) {
        s_LoaderName =
            CSNPDataLoader::RegisterInObjectManager(*om)
            .GetLoader()->GetName();
        BOOST_CHECK_EQUAL(s_LoaderName, CSNPDataLoader::GetLoaderNameFromArgs());
    }
    else {
        s_LoaderName =
            CSNPDataLoader::RegisterInObjectManager(*om, file_name)
            .GetLoader()->GetName();
        BOOST_CHECK_EQUAL(s_LoaderName, CSNPDataLoader::GetLoaderNameFromArgs(file_name));
    }
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    scope->AddDataLoader(s_LoaderName);
    return scope;
}


static
void s_CheckSource(const string& descr,
                   ESNPSource source,
                   const set<CTSE_Handle>& tse_set)
{
    BOOST_CHECK_EQUAL(tse_set.size(), 1u);
    for ( auto& tse : tse_set ) {
        LOG_POST(descr<<": blob id: "<<tse.GetBlobId());
        if ( source == eFromGB ) {
            BOOST_CHECK(dynamic_cast<CGBDataLoader*>(tse.GetDataLoader()));
        }
        else {
            BOOST_CHECK(dynamic_cast<CSNPDataLoader*>(tse.GetDataLoader()));
        }
    }
}


static
void s_CheckSource(const string& descr,
                   ESNPSource source,
                   CFeat_CI feat_it)
{
    set<CTSE_Handle> tse_set;
    for ( ; feat_it; ++feat_it ) {
        tse_set.insert(feat_it.GetAnnot().GetTSE_Handle());
    }
    s_CheckSource(descr, source, tse_set);
}


static
void s_CheckSource(const string& descr,
                   ESNPSource source,
                   CGraph_CI graph_it)
{
    set<CTSE_Handle> tse_set;
    for ( ; graph_it; ++graph_it ) {
        tse_set.insert(graph_it.GetAnnot().GetTSE_Handle());
    }
    s_CheckSource(descr, source, tse_set);
}


static
size_t s_CheckFeat(CRef<CScope> scope,
                   const SAnnotSelector& sel,
                   const string& str_id,
                   CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    size_t ret = 0;
    CRef<CSeq_id> seq_id(new CSeq_id(str_id));
    CRef<CSeq_loc> loc(new CSeq_loc);
    if ( range == CRange<TSeqPos>::GetWhole() ) {
        loc->SetWhole(*seq_id);
    }
    else {
        CSeq_interval& interval = loc->SetInt();
        interval.SetId(*seq_id);
        interval.SetFrom(range.GetFrom());
        interval.SetTo(range.GetTo());
    }
    {
        CFeat_CI it(*scope, *loc, sel);
        ret += it.GetSize();
        BOOST_CHECK(it);
    }

    CBioseq_Handle bh = scope->GetBioseqHandle(*seq_id);
    BOOST_REQUIRE(bh);
    {
        CFeat_CI it(bh, range, sel);
        ret += it.GetSize();
        BOOST_CHECK(it);
    }
    return ret;
}


static
void s_CheckNoFeat(CRef<CScope> scope,
                   const SAnnotSelector& sel,
                   const string& str_id,
                   CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    CRef<CSeq_id> seq_id(new CSeq_id(str_id));
    CRef<CSeq_loc> loc(new CSeq_loc);
    if ( range == CRange<TSeqPos>::GetWhole() ) {
        loc->SetWhole(*seq_id);
    }
    else {
        CSeq_interval& interval = loc->SetInt();
        interval.SetId(*seq_id);
        interval.SetFrom(range.GetFrom());
        interval.SetTo(range.GetTo());
    }
    {
        CFeat_CI it(*scope, *loc, sel);
        BOOST_CHECK(!it);
    }

    CBioseq_Handle bh = scope->GetBioseqHandle(*seq_id);
    BOOST_REQUIRE(bh);
    {
        CFeat_CI it(bh, range, sel);
        BOOST_CHECK(!it);
    }
}


static
void s_TestNoNA(const string& descr,
                const string& na_acc,
                CBioseq_Handle bh, TSeqPos range_from, TSeqPos range_to)
{
    if ( 1 ) {
        SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
        sel.AddNamedAnnots(na_acc);
        sel.IncludeNamedAnnotAccession(na_acc);
        CFeat_CI feat_it(bh, CRange<TSeqPos>(range_from, range_to), sel);
        if ( feat_it ) {
            s_CheckSource(descr, eFromGB, feat_it);
        }
    }
    if ( 1 ) {
        SAnnotSelector sel;
        sel.AddNamedAnnots(na_acc);
        sel.IncludeNamedAnnotAccession(na_acc);
        CGraph_CI graph_it(bh, CRange<TSeqPos>(range_from, range_to), sel);
        if ( graph_it ) {
            s_CheckSource(descr, eFromGB, graph_it);
        }
    }
}


static
void s_TestNA(const string& descr,
              ESNPSource source,
              const string& na_acc,
              CBioseq_Handle bh, TSeqPos range_from, TSeqPos range_to,
              size_t snp_count)
{
    if ( 1 ) { // check features
        SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
        sel.AddNamedAnnots(na_acc);
        sel.IncludeNamedAnnotAccession(na_acc);
        CFeat_CI feat_it(bh, CRange<TSeqPos>(range_from, range_to), sel);
        BOOST_CHECK(feat_it);
        BOOST_CHECK_EQUAL(feat_it.GetSize(), snp_count);
        s_CheckSource(descr, source, feat_it);
    }
    if ( 1 ) { // check coverage graphs
        SAnnotSelector sel;
        string graph_na_acc = CombineWithZoomLevel(na_acc, 100);
        sel.AddNamedAnnots(graph_na_acc);
        sel.IncludeNamedAnnotAccession(graph_na_acc);
        CGraph_CI graph_it(bh, CRange<TSeqPos>(range_from, range_to), sel);
        BOOST_CHECK(graph_it);
        s_CheckSource(descr, source, graph_it);
    }
    if ( 1 ) { // check overview graphs
        SAnnotSelector sel;
        string graph_na_acc = CombineWithZoomLevel(na_acc, 5000);
        sel.AddNamedAnnots(graph_na_acc);
        sel.IncludeNamedAnnotAccession(graph_na_acc);
        CGraph_CI graph_it(bh, CRange<TSeqPos>(range_from, range_to), sel);
        BOOST_CHECK(graph_it);
        s_CheckSource(descr, source, graph_it);
    }
}


static
void s_TestPTIS(const string& descr,
                ESNPSource source,
                CBioseq_Handle bh, TSeqPos range_from, TSeqPos range_to,
                size_t snp_count)
{
    if ( 1 ) { // check features
        SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
        sel.IncludeNamedAnnotAccession("SNP");
        sel.AddNamedAnnots("SNP");
        CFeat_CI feat_it(bh, CRange<TSeqPos>(range_from, range_to), sel);
        BOOST_CHECK(feat_it);
        BOOST_CHECK_GE(feat_it.GetSize(), snp_count);
        BOOST_CHECK_LT(feat_it.GetSize(), snp_count*2);
        s_CheckSource(descr, source, feat_it);
    }
    if ( 1 ) { // check coverage graphs
        SAnnotSelector sel;
        string graph_na_acc = CombineWithZoomLevel("SNP", 100);
        sel.AddNamedAnnots(graph_na_acc);
        sel.IncludeNamedAnnotAccession(graph_na_acc);
        CGraph_CI graph_it(bh, CRange<TSeqPos>(range_from, range_to), sel);
        BOOST_CHECK(graph_it);
        s_CheckSource(descr, source, graph_it);
    }
    if ( 1 ) { // check overview graphs
        SAnnotSelector sel;
        sel.AddNamedAnnots("SNP");
        sel.IncludeNamedAnnotAccession("SNP");
        CGraph_CI graph_it(bh, CRange<TSeqPos>(range_from, range_to), sel);
        BOOST_CHECK(graph_it);
        s_CheckSource(descr, source, graph_it);
    }
}


BOOST_AUTO_TEST_CASE(GBImplicitNA)
{
    CRef<CScope> scope = s_MakeScope0();
    string na_acc = "NA000124713.8#1";
    string na_acc2 = "NA000193272.4#17";
    string seq_id = "NC_000001.11";
    TSeqPos range_from = 0;
    TSeqPos range_to = 100000;
    size_t snp_count = 8894;
    size_t snp_count2 = 16587;
    
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id));
    BOOST_REQUIRE(bh);

    s_TestNA("GBImplicitNA 1", eFromGB, na_acc, bh, range_from, range_to, snp_count);
    s_TestNA("GBImplicitNA 2", eFromGB, na_acc2, bh, range_from, range_to, snp_count2);
}


BOOST_AUTO_TEST_CASE(GBImplicitSNP)
{
    CRef<CScope> scope = s_MakeScope0();
    string seq_id = "NC_000001.11";
    TSeqPos range_from = 0;
    TSeqPos range_to = 100000;
    size_t snp_count = 17889;
    
    string seq_id2 = "NC_000007";
    TSeqPos range_from2 = 1000000;
    TSeqPos range_to2 = 1100000;
    size_t snp_count2 = 34431;
    
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id));
    BOOST_REQUIRE(bh);
    CBioseq_Handle bh2 = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id2));
    BOOST_REQUIRE(bh2);

    s_TestPTIS("GBImplicitSNP 1", eFromGB, bh, range_from, range_to, snp_count);
    s_TestPTIS("GBImplicitSNP 2", eFromGB, bh2, range_from2, range_to2, snp_count2);
}


BOOST_AUTO_TEST_CASE(SNPExplicitNA)
{
    string na_acc = "NA000124713.8#1";
    string na_acc2 = "NA000193272.4#17";
    CRef<CScope> scope = s_MakeScope(na_acc);
    string seq_id = "NC_000001.11";
    TSeqPos range_from = 0;
    TSeqPos range_to = 100000;
    size_t snp_count = 8894;
    
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id));
    BOOST_REQUIRE(bh);

    s_TestNA("SNPExplicitNA 1", eFromSNP, na_acc, bh, range_from, range_to, snp_count);
    s_TestNoNA("SNPExplicitNA 2", na_acc2, bh, range_from, range_to);
}


BOOST_AUTO_TEST_CASE(SNPImplicitNA)
{
    CRef<CScope> scope = s_MakeScope();
    string na_acc = "NA000124713.8#1";
    string na_acc2 = "NA000193272.4#17";
    string seq_id = "NC_000001.11";
    TSeqPos range_from = 0;
    TSeqPos range_to = 100000;
    size_t snp_count = 8894;
    size_t snp_count2 = 16587;
    
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id));
    BOOST_REQUIRE(bh);

    s_TestNA("SNPImplicitNA 1", eFromSNP, na_acc, bh, range_from, range_to, snp_count);
    s_TestNA("SNPImplicitNA 2", eFromSNP, na_acc2, bh, range_from, range_to, snp_count2);
}


BOOST_AUTO_TEST_CASE(SNPImplicitSNP)
{
    CRef<CScope> scope = s_MakeScope();
    string seq_id = "NC_000001.11";
    TSeqPos range_from = 0;
    TSeqPos range_to = 100000;
    size_t snp_count = 29766; // new PTIS version - NA000305581.1#17
    // old was NA000193272.8#17 and had 23474 SNPs in this case
    // old was NA000193272.7#17 and had 17889 SNPs in this case
    
    string seq_id2 = "NC_000007";
    TSeqPos range_from2 = 1000000;
    TSeqPos range_to2 = 1100000;
    size_t snp_count2 = 49002; // new PTIS version - NA000305581.1#17
    // old was NA000193272.8#17 and had 44657 SNPs in this case
    // old was NA000193272.7#17 and had 34431 SNPs in this case
    
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id));
    BOOST_REQUIRE(bh);
    CBioseq_Handle bh2 = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id2));
    BOOST_REQUIRE(bh2);

    s_TestPTIS("SNPImplicitSNP 1", eFromSNP, bh, range_from, range_to, snp_count);
    s_TestPTIS("SNPImplicitSNP 2", eFromSNP, bh2, range_from2, range_to2, snp_count2);
}


BOOST_AUTO_TEST_CASE(CheckExtSNPEditChangeId)
{
    LOG_POST("Checking ExtAnnot SNP for sequence with changed ids");
    string id = "NM_004006.2";
    string dummy_id = "lcl|dummy";
    
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");
    CRef<CScope> scope = s_MakeScope();
    scope->SetKeepExternalAnnotsForEdit();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(id));
    BOOST_REQUIRE(bh);
    CBioseq_EditHandle bhe = bh.GetEditHandle();
    BOOST_REQUIRE(bh);
    BOOST_REQUIRE(bhe);
    size_t count = s_CheckFeat(scope, sel, id);
    vector<CSeq_id_Handle> ids = bhe.GetId();
    bhe.ResetId();
    bhe.AddId(CSeq_id_Handle::GetHandle(dummy_id));
    s_CheckNoFeat(scope, sel, dummy_id);
    for ( auto idh : ids ) {
        bhe.AddId(idh);
    }
    BOOST_CHECK_EQUAL(s_CheckFeat(scope, sel, dummy_id), count);
}


BOOST_AUTO_TEST_CASE(GBImplicitNA64)
{
    CRef<CScope> scope = s_MakeScope0();
    string na_acc = "NA000306983.1#1";
    string seq_id = "2500000194";
    TSeqPos range_from = 0;
    TSeqPos range_to = 100000;
    size_t snp_count = 1;
    
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id));
    BOOST_REQUIRE(bh);

    s_TestNA("GBImplicitNA 1", eFromGB, na_acc, bh, range_from, range_to, snp_count);
}


BOOST_AUTO_TEST_CASE(GBImplicitSNP64)
{
    CRef<CScope> scope = s_MakeScope0();
    string seq_id = "2500000194";
    TSeqPos range_from = 0;
    TSeqPos range_to = 100000;
    size_t snp_count = 1;
    
    string seq_id2 = "CM029356.1";
    TSeqPos range_from2 = 0;
    TSeqPos range_to2 = 100000;
    size_t snp_count2 = 1;
    
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id));
    BOOST_REQUIRE(bh);
    CBioseq_Handle bh2 = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id2));
    BOOST_REQUIRE(bh2);

    s_TestPTIS("GBImplicitSNP 1", eFromGB, bh, range_from, range_to, snp_count);
    s_TestPTIS("GBImplicitSNP 2", eFromGB, bh2, range_from2, range_to2, snp_count2);
}


BOOST_AUTO_TEST_CASE(SNPExplicitNA64)
{
    string na_acc = "NA000306983.1#1";
    string na_acc2 = "NA000306983.1#3";
    CRef<CScope> scope = s_MakeScope(na_acc);
    string seq_id = "2500000194";
    TSeqPos range_from = 0;
    TSeqPos range_to = 100000;
    size_t snp_count = 1;
    
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id));
    BOOST_REQUIRE(bh);

    s_TestNA("SNPExplicitNA 1", eFromSNP, na_acc, bh, range_from, range_to, snp_count);
    s_TestNoNA("SNPExplicitNA 2", na_acc2, bh, range_from, range_to);
}


BOOST_AUTO_TEST_CASE(SNPImplicitNA64)
{
    CRef<CScope> scope = s_MakeScope();
    string na_acc = "NA000306983.1#1";
    string na_acc2 = "NA000306983.1#3";
    string seq_id = "2500000194";
    TSeqPos range_from = 0;
    TSeqPos range_to = 100000;
    size_t snp_count = 1;
    
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id));
    BOOST_REQUIRE(bh);

    s_TestNA("SNPImplicitNA 1", eFromSNP, na_acc, bh, range_from, range_to, snp_count);
    s_TestNoNA("SNPImplicitNA 2", na_acc2, bh, range_from, range_to);
}


BOOST_AUTO_TEST_CASE(SNPImplicitSNP64)
{
    CRef<CScope> scope = s_MakeScope();
    string seq_id = "2500000194";
    TSeqPos range_from = 0;
    TSeqPos range_to = 100000;
    size_t snp_count = 1;
    
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id));
    BOOST_REQUIRE(bh);

    s_TestPTIS("SNPImplicitSNP 1", eFromSNP, bh, range_from, range_to, snp_count);
}


NCBITEST_INIT_TREE()
{
    if ( !CSNPDataLoader::IsUsingPTIS() ) {
        NCBITEST_DISABLE(SNPImplicitSNP);
        NCBITEST_DISABLE(SNPImplicitSNP64);
    }
#ifdef NCBI_INT4_GI
    NCBITEST_DISABLE(GBImplicitNA64)
    NCBITEST_DISABLE(GBImplicitSNP64)
    NCBITEST_DISABLE(SNPExplicitNA64)
    NCBITEST_DISABLE(SNPImplicitNA64)
    NCBITEST_DISABLE(SNPImplicitSNP64)
#endif
}
