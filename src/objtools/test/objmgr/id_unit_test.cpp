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
* Authors:  Eugene Vasilchenko
*
* File Description:
*   Unit test for data loading from ID.
*/

#define NCBI_TEST_APPLICATION

#include <ncbi_pch.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <objmgr/seq_table_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>
#include <objtools/data_loaders/genbank/id2/reader_id2.hpp>
#include <objtools/data_loaders/genbank/impl/psg_loader_impl.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <dbapi/driver/drivers.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>
#include <algorithm>

//#define RUN_SLOW_MT_TESTS
#include <objtools/simple/simple_om.hpp>
#if defined(NCBI_THREADS)
# include <future>
#endif

#include <objects/general/general__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objects/seq/seq__.hpp>
#include <objmgr/util/sequence.hpp>
#include <serial/iterator.hpp>

#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

NCBI_PARAM_DECL(Int8, GENBANK, GI_OFFSET);
NCBI_PARAM_DEF_EX(Int8, GENBANK, GI_OFFSET, 0,
                  eParam_NoThread, GENBANK_GI_OFFSET);

NCBI_PARAM_DECL(bool, ID_UNIT_TEST, PSG2_LOADER);
NCBI_PARAM_DEF(bool, ID_UNIT_TEST, PSG2_LOADER, false);

static
TIntId GetGiOffset(void)
{
    static volatile TIntId gi_offset;
    static volatile bool initialized;
    if ( !initialized ) {
        gi_offset = NCBI_PARAM_TYPE(GENBANK, GI_OFFSET)::GetDefault();
        initialized = true;
    }
    return gi_offset;
}

static
bool IsUsingPSG2Loader(void)
{
    return NCBI_PARAM_TYPE(ID_UNIT_TEST, PSG2_LOADER)::GetDefault();
}

static
bool NeedsOtherLoaders()
{
    return CGBDataLoader::IsUsingPSGLoader() && !IsUsingPSG2Loader();
}


static CRef<CScope> s_InitScope(bool reset_loader = true)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    if ( reset_loader ) {
        CDataLoader* loader =
            om->FindDataLoader(CGBDataLoader::GetLoaderNameFromArgs());
        if ( loader ) {
            if ( NeedsOtherLoaders() ) {
                om->RevokeAllDataLoaders();
            }
            else {
                BOOST_CHECK(om->RevokeDataLoader(*loader));
            }
        }
    }
#ifdef HAVE_PUBSEQ_OS
    DBAPI_RegisterDriver_FTDS();
    GenBankReaders_Register_Pubseq();
    GenBankReaders_Register_Pubseq2();
#endif
    CGBDataLoader::RegisterInObjectManager(*om);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    if ( NeedsOtherLoaders() ) {
        // Add SNP/CDD/WGS loaders.
        scope->AddDataLoader(om->RegisterDataLoader(0, "cdd")->GetName());
        scope->AddDataLoader(om->RegisterDataLoader(0, "snp")->GetName());
        scope->AddDataLoader(om->RegisterDataLoader(0, "wgs")->GetName());
    }
    return scope;
}


string s_AsString(const vector<CSeq_id_Handle>& ids)
{
    CNcbiOstrstream out;
    out << '{';
    ITERATE ( vector<CSeq_id_Handle>, it, ids ) {
        out << ' ' <<  *it;
    }
    out << " }";
    return CNcbiOstrstreamToString(out);
}

enum EIncludePubseqos2 {
    eIncludePubseqos2,
    eExcludePubseqos2
};

bool s_HaveID2(EIncludePubseqos2 include_pubseqos2 = eIncludePubseqos2)
{
    const char* env = getenv("GENBANK_LOADER_METHOD_BASE");
    if ( !env ) {
        env = getenv("GENBANK_LOADER_METHOD");
    }
    if ( !env ) {
        // assume default ID2
        return true;
    }
    if ( NStr::EndsWith(env, "id1", NStr::eNocase) ||
         NStr::EndsWith(env, "pubseqos", NStr::eNocase) ) {
        // non-ID2 based readers
        return false;
    }
    if ( NStr::EndsWith(env, "pubseqos2", NStr::eNocase) ) {
        // only if pubseqos2 reader is allowed
        return include_pubseqos2 == eIncludePubseqos2;
    }
    if ( NStr::EndsWith(env, "id2", NStr::eNocase) ) {
        // real id2 reader
        return true;
    }
    return false;
}


bool s_HaveID1(void)
{
    const char* env = getenv("GENBANK_LOADER_METHOD_BASE");
    if ( !env ) {
        env = getenv("GENBANK_LOADER_METHOD");
    }
    if ( !env ) {
        // assume default ID2
        return false;
    }
    return NStr::EndsWith(env, "id1", NStr::eNocase);
}


bool s_HavePubSeqOS(void)
{
    const char* env = getenv("GENBANK_LOADER_METHOD_BASE");
    if ( !env ) {
        env = getenv("GENBANK_LOADER_METHOD");
    }
    if ( !env ) {
        // assume default ID2
        return false;
    }
    return NStr::EndsWith(env, "pubseqos", NStr::eNocase);
}


bool s_HaveCache(void)
{
    const char* env = getenv("GENBANK_LOADER_METHOD");
    if ( !env ) {
        // assume default ID2
        return false;
    }
    return NStr::StartsWith(env, "cache", NStr::eNocase);
}


bool s_HaveOnlyCache(void)
{
    const char* env = getenv("GENBANK_LOADER_METHOD");
    if ( !env ) {
        // assume default ID2
        return false;
    }
    return NStr::Equal(env, "cache", NStr::eNocase);
}


bool s_HaveNA()
{
    // NA are available in PSG and ID2
    return CGBDataLoader::IsUsingPSGLoader() || s_HaveID2() || s_HavePubSeqOS();
}


bool s_HaveSplit()
{
    // split data are available in PSG and ID2
    return CGBDataLoader::IsUsingPSGLoader() || s_HaveID2();
}


bool s_HaveMongoDBCDD()
{
    // MongoDB plugin is available in PSG and OSG
    return CGBDataLoader::IsUsingPSGLoader() || (s_HaveID2(eExcludePubseqos2) && CId2Reader::GetVDB_CDD_Enabled());
}


bool s_HaveVDBWGS()
{
    // WGS VDB plugin is available in PSG
    if ( CGBDataLoader::IsUsingPSGLoader() ) {
        return true;
    }
    // WGS VDB plugin is available in OSG
    if ( s_HaveID2(eExcludePubseqos2) ) {
        return true;
    }
    // WGS VDB plugin may be set in GB loader on client side
    if ( s_HaveID2() ) {
        const char* env = getenv("GENBANK_ID2_PROCESSOR");
        if ( env && NStr::Equal(env, "wgs") ) {
            return true;
        }
    }
    return false;
}


bool s_HaveVDBSNP()
{
    // VDB SNP plugin is available in PSG and OSG
    return CGBDataLoader::IsUsingPSGLoader() || s_HaveID2(eExcludePubseqos2);
}



static string s_HUPToken;

bool s_HaveHUPToken()
{
    // check if HUP token is defined
    if ( !s_HUPToken.empty() ) {
        return true;
    }
    return false;
}


bool s_RelaxGpipeCheck(void)
{
    return true;
}


bool s_ContainsId(const CSeq_id_Handle& id, const vector<CSeq_id_Handle>& ids)
{
    if ( find(ids.begin(), ids.end(), id) != ids.end() ) {
        return true;
    }
    if ( id.Which() == CSeq_id::e_Gpipe && s_RelaxGpipeCheck() ) {
        return true;
    }
    return false;
}


void s_CompareIdSets(const char* type,
                     const vector<CSeq_id_Handle>& req_ids,
                     const vector<CSeq_id_Handle>& ids,
                     bool exact)
{
    LOG_POST("CompareIdSets: "<<type);
    LOG_POST("req_ids: "<<s_AsString(req_ids));
    LOG_POST("    ids: "<<s_AsString(ids));
    if ( exact ) {
        BOOST_CHECK_EQUAL(ids.size(), req_ids.size());
    }
    else {
        BOOST_CHECK(ids.size() >= req_ids.size());
    }
    ITERATE ( vector<CSeq_id_Handle>, it, req_ids ) {
        LOG_POST("checking "<<*it);
        BOOST_CHECK(s_ContainsId(*it, ids));
    }
}


CSeq_id_Handle s_Normalize(const CSeq_id_Handle& id)
{
    return CSeq_id_Handle::GetHandle(id.AsString());
}


vector<CSeq_id_Handle> s_Normalize(const vector<CSeq_id_Handle>& ids)
{
    vector<CSeq_id_Handle> ret;
    ITERATE ( vector<CSeq_id_Handle>, it, ids ) {
        ret.push_back(s_Normalize(*it));
    }
    return ret;
}


struct SBioseqInfo
{
    string m_RequestId;
    string m_RequiredIds;
    TSeqPos m_MinLength, m_MaxLength;
};


static const SBioseqInfo s_BioseqInfos[] = {
    {
        "NC_000001.10",
        "gi|224589800;ref|NC_000001.10|;gpp|GPC_000001293.1|;gnl|NCBI_GENOMES|1;gnl|ASM:GCF_000001305|1",
        249250621,
        249250621
    }
};


void s_CheckIds(const SBioseqInfo& info, CScope* scope)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(info.m_RequestId);
    vector<CSeq_id_Handle> req_ids;
    {
        vector<string> req_ids_str;
        NStr::Split(info.m_RequiredIds, ";", req_ids_str);
        TIntId gi_offset = GetGiOffset();
        ITERATE ( vector<string>, it, req_ids_str ) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*it);
            if ( gi_offset && idh.IsGi() ) {
                idh = CSeq_id_Handle::GetGiHandle(idh.GetGi() + GI_FROM(TIntId, gi_offset));
            }
            req_ids.push_back(idh);
        }
    }

    CScope::TIds ids = s_Normalize(scope->GetIds(idh));
    s_CompareIdSets("expected with get-ids",
                    req_ids, ids, true);

    CBioseq_Handle bh = scope->GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    //s_Print("Actual ", bh.GetId());
    s_CompareIdSets("get-ids with Bioseq",
                    ids, s_Normalize(bh.GetId()), true);
}


void s_CheckSequence(const SBioseqInfo& info, CScope* scope)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(info.m_RequestId);
    CBioseq_Handle bh = scope->GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    TSeqPos len = bh.GetBioseqLength();
    BOOST_CHECK(len >= info.m_MinLength);
    BOOST_CHECK(len <= info.m_MaxLength);
    if ( 0 ) {
        BOOST_CHECK(bh.GetSeqVector().CanGetRange(len/2, len));
        BOOST_CHECK(bh.GetSeqVector().CanGetRange(0, len/2));
        BOOST_CHECK(bh.GetSeqVector().CanGetRange(0, len));
    }
}


void s_CheckAll(const SBioseqInfo& info, CScope* scope)
{
    s_CheckIds(info, scope);
    s_CheckSequence(info, scope);
}


SAnnotSelector s_GetSelector(CSeqFeatData::ESubtype subtype,
                             const char* name = 0)
{
    SAnnotSelector sel(subtype);
    sel.SetResolveAll().SetAdaptiveDepth();
    if ( name ) {
        sel.AddNamedAnnots(name);
    }
    return sel;
}


pair<size_t, size_t>
s_CheckFeat(CRef<CScope> scope,
            const SAnnotSelector& sel,
            const string& str_id,
            CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    pair<size_t, size_t> ret;
    set<CTSE_Handle> tses;
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
        ret.second += it.GetSize();
        BOOST_CHECK(it);
        for ( ; it; ++it ) {
            tses.insert(it->GetAnnot().GetTSE_Handle());
        }
    }

    CBioseq_Handle bh = scope->GetBioseqHandle(*seq_id);
    BOOST_REQUIRE(bh);
    {
        CFeat_CI it(bh, range, sel);
        ret.second += it.GetSize();
        BOOST_CHECK(it);
        for ( ; it; ++it ) {
            tses.insert(it->GetAnnot().GetTSE_Handle());
        }
    }
    ret.first = tses.size();
    return ret;
}


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


pair<size_t, size_t>
s_CheckFeat(const SAnnotSelector& sel,
            const string& str_id,
            CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    return s_CheckFeat(s_InitScope(), sel, str_id, range);
}


// number of TSEs, number of graphs
pair<size_t, size_t>
s_CheckGraph(CRef<CScope> scope,
             const SAnnotSelector& sel,
             const string& str_id,
             CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    pair<size_t, size_t> ret;
    set<CTSE_Handle> tses;
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
        CGraph_CI it(*scope, *loc, sel);
        ret.second += it.GetSize();
        BOOST_CHECK(it);
        for ( ; it; ++it ) {
            tses.insert(it->GetAnnot().GetTSE_Handle());
        }
    }

    CBioseq_Handle bh = scope->GetBioseqHandle(*seq_id);
    BOOST_REQUIRE(bh);
    {
        CGraph_CI it(bh, range, sel);
        BOOST_CHECK(it);
        ret.second += it.GetSize();
        for ( ; it; ++it ) {
            tses.insert(it->GetAnnot().GetTSE_Handle());
        }
    }
    ret.first = tses.size();
    return ret;
}


pair<size_t, size_t>
s_CheckGraph(const SAnnotSelector& sel,
             const string& str_id,
             CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    return s_CheckGraph(s_InitScope(), sel, str_id, range);
}


pair<size_t, size_t>
s_CheckTable(const SAnnotSelector& sel,
             const string& str_id,
             CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    pair<size_t, size_t> ret;
    set<CTSE_Handle> tses;
    CRef<CScope> scope = s_InitScope();
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
        CSeq_table_CI it(*scope, *loc, sel);
        BOOST_CHECK(it);
        ret.second += it.GetSize();
        for ( ; it; ++it ) {
            tses.insert(it->GetTSE_Handle());
        }
    }

    CBioseq_Handle bh = scope->GetBioseqHandle(*seq_id);
    BOOST_REQUIRE(bh);
    {
        CSeq_table_CI it(bh, range, sel);
        BOOST_CHECK(it);
        ret.second += it.GetSize();
        for ( ; it; ++it ) {
            tses.insert(it->GetTSE_Handle());
        }
    }
    ret.first = tses.size();
    return ret;
}


BOOST_AUTO_TEST_CASE(CheckIds)
{
    for ( size_t i = 0; i < ArraySize(s_BioseqInfos); ++i ) {
        CRef<CScope> scope = s_InitScope();
        s_CheckIds(s_BioseqInfos[i], scope);
    }
}


BOOST_AUTO_TEST_CASE(CheckSequence)
{
    for ( size_t i = 0; i < ArraySize(s_BioseqInfos); ++i ) {
        CRef<CScope> scope = s_InitScope();
        s_CheckSequence(s_BioseqInfos[i], scope);
    }
}


BOOST_AUTO_TEST_CASE(CheckAll)
{
    for ( size_t i = 0; i < ArraySize(s_BioseqInfos); ++i ) {
        CRef<CScope> scope = s_InitScope();
        s_CheckAll(s_BioseqInfos[i], scope);
    }
}


BOOST_AUTO_TEST_CASE(CheckExtSNP)
{
    LOG_POST("Checking ExtAnnot SNP");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");
    auto counts = s_CheckFeat(sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
    BOOST_CHECK_EQUAL(counts.first, 1u);
}


BOOST_AUTO_TEST_CASE(CheckExtSNPEdit)
{
    LOG_POST("Checking ExtAnnot SNP for edited sequence");
    string id = "NM_004006.2";
    
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");
    CRef<CScope> scope = s_InitScope();
    scope->SetKeepExternalAnnotsForEdit();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(id));
    BOOST_REQUIRE(bh);
    CBioseq_EditHandle bhe = bh.GetEditHandle();
    BOOST_REQUIRE(bh);
    BOOST_REQUIRE(bhe);
    auto counts = s_CheckFeat(scope, sel, id);
    BOOST_CHECK_EQUAL(counts.first, 1u);
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
    CRef<CScope> scope = s_InitScope();
    scope->SetKeepExternalAnnotsForEdit();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle(id));
    BOOST_REQUIRE(bh);
    CBioseq_EditHandle bhe = bh.GetEditHandle();
    BOOST_REQUIRE(bh);
    BOOST_REQUIRE(bhe);
    auto counts1 = s_CheckFeat(scope, sel, id);
    BOOST_CHECK_EQUAL(counts1.first, 1u);
    vector<CSeq_id_Handle> ids = bhe.GetId();
    bhe.ResetId();
    bhe.AddId(CSeq_id_Handle::GetHandle(dummy_id));
    s_CheckNoFeat(scope, sel, dummy_id);
    for ( auto idh : ids ) {
        bhe.AddId(idh);
    }
    auto counts2 = s_CheckFeat(scope, sel, dummy_id);
    BOOST_CHECK_EQUAL(counts2.first, 1u);
    BOOST_CHECK_EQUAL(counts2.second, counts1.second);
}


template<class TObject>
void s_CheckFeatData(const SAnnotSelector& sel,
                     const string& str_id,
                     CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    CRef<CScope> scope = s_InitScope(false);
    CRef<CSeq_id> seq_id(new CSeq_id("NC_000001.10"));
    CBioseq_Handle bh = scope->GetBioseqHandle(*seq_id);
    BOOST_REQUIRE(bh);

    vector< CConstRef<TObject> > objs, objs_copy;
    CFeat_CI fit(bh, range, sel);
    BOOST_REQUIRE(fit);
    for ( ; fit; ++fit ) {
        CConstRef<CSeq_feat> feat = fit->GetSeq_feat();
        bool old_data = true;
        for ( size_t i = 0; i < objs.size(); ++i ) {
            BOOST_REQUIRE(objs[i]->Equals(*objs_copy[i]));
        }
        for ( CTypeConstIterator<TObject> it(Begin(*feat)); it; ++it ) {
            if ( old_data ) {
                objs.clear();
                objs_copy.clear();
                old_data = false;
            }
            CConstRef<TObject> obj(&*it);
            objs.push_back(obj);
            objs_copy.push_back(Ref(SerialClone(*obj)));
        }
    }
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Seq_feat)
{
    LOG_POST("Checking ExtAnnot SNP Seq-feat generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CSeq_feat>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_SeqFeatData)
{
    LOG_POST("Checking ExtAnnot SNP SeqFeatData generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CSeqFeatData>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Imp_feat)
{
    LOG_POST("Checking ExtAnnot SNP Imp-feat generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CImp_feat>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Seq_loc)
{
    LOG_POST("Checking ExtAnnot SNP Seq-loc generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CSeq_loc>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Seq_id)
{
    LOG_POST("Checking ExtAnnot SNP Seq-id generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CSeq_id>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Seq_point)
{
    LOG_POST("Checking ExtAnnot SNP Seq-point generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CSeq_point>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Seq_interval)
{
    LOG_POST("Checking ExtAnnot SNP Seq-interval generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CSeq_interval>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Gb_qual)
{
    LOG_POST("Checking ExtAnnot SNP Gb-qual generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CGb_qual>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Dbtag)
{
    LOG_POST("Checking ExtAnnot SNP Dbtag generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CDbtag>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_Object_id)
{
    LOG_POST("Checking ExtAnnot SNP Object-id generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CObject_id>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_User_object)
{
    LOG_POST("Checking ExtAnnot SNP User-object generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CUser_object>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNP_User_field)
{
    LOG_POST("Checking ExtAnnot SNP User-field generation");
    SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");

    s_CheckFeatData<CUser_field>
        (sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


BOOST_AUTO_TEST_CASE(CheckExtSNPGraph)
{
    LOG_POST("Checking ExtAnnot SNP graph");
    string id = "NC_000001.10";
    CRange<TSeqPos> range(249200000, 249210000);

    auto scope = s_InitScope();
    const int kCoverageZoom = 100;
    for ( int t = 0; t < 2; ++t ) {
        {
            SAnnotSelector sel(CSeq_annot::C_Data::e_Graph);
            sel.SetResolveAll().SetAdaptiveDepth();
            sel.IncludeNamedAnnotAccession("SNP");
            sel.AddNamedAnnots("SNP");
            auto counts = s_CheckGraph(scope, sel, id, range);
            BOOST_CHECK_EQUAL(counts.first, 1u);
        }
        if ( s_HaveVDBSNP() ) {
            SAnnotSelector sel(CSeq_annot::C_Data::e_Graph);
            sel.SetResolveAll().SetAdaptiveDepth();
            sel.IncludeNamedAnnotAccession("SNP", kCoverageZoom);
            sel.AddNamedAnnots(CombineWithZoomLevel("SNP", kCoverageZoom));
            auto counts = s_CheckGraph(scope, sel, id, range);
            BOOST_CHECK_EQUAL(counts.first, 1u);
        }
        {
            SAnnotSelector sel(CSeqFeatData::eSubtype_variation);
            sel.SetResolveAll().SetAdaptiveDepth();
            sel.IncludeNamedAnnotAccession("SNP");
            sel.AddNamedAnnots("SNP");
            auto counts = s_CheckFeat(scope, sel, id, range);
            BOOST_CHECK_EQUAL(counts.first, 1u);
        }
    }
}


static string s_GetVDB_CDD_Source()
{
    return CId2Reader::GetVDB_CDD_Enabled()? "CGI": "ID";
}


struct SInvertVDB_CDD {
    void s_Invert()
        {
            CId2Reader::SetVDB_CDD_Enabled(!CId2Reader::GetVDB_CDD_Enabled());
        }
    SInvertVDB_CDD()
        {
            s_Invert();
        }
    ~SInvertVDB_CDD()
        {
            s_Invert();
        }
    SInvertVDB_CDD(const SInvertVDB_CDD&) = delete;
    void operator=(const SInvertVDB_CDD&) = delete;


    static bool IsPossible()
        {
            // Only OSG can switch CDD source
            return !CGBDataLoader::IsUsingPSGLoader() && s_HaveID2(eExcludePubseqos2);
        }
};

BOOST_AUTO_TEST_CASE(CheckExtCDD)
{
    if (!s_HaveMongoDBCDD()) return;
    LOG_POST("Checking ExtAnnot "<<s_GetVDB_CDD_Source()<<" CDD");
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "AAC73113.1");
}


BOOST_AUTO_TEST_CASE(CheckExtCDD2)
{
    SInvertVDB_CDD invert;
    if (!s_HaveMongoDBCDD()) return;
    LOG_POST("Checking ExtAnnot "<<s_GetVDB_CDD_Source()<<" CDD");
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "AAC73113.1");
}


BOOST_AUTO_TEST_CASE(CheckExtCDDonWGS)
{
    if (!s_HaveMongoDBCDD()) return;
    LOG_POST("Checking ExtAnnot "<<s_GetVDB_CDD_Source()<<" CDD on WGS sequence");
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "RLL67630.1");
}


BOOST_AUTO_TEST_CASE(CheckExtCDD2onWGS)
{
    SInvertVDB_CDD invert;
    if (!s_HaveMongoDBCDD()) return;
    LOG_POST("Checking ExtAnnot "<<s_GetVDB_CDD_Source()<<" CDD on WGS sequence");
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "RLL67630.1");
}


BOOST_AUTO_TEST_CASE(CheckExtCDDonPDB)
{
    LOG_POST("Checking ExtAnnot CDD on PDB sequence");
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "pdb|4XNUA");
}


BOOST_AUTO_TEST_CASE(CheckExtMGC)
{
    LOG_POST("Checking ExtAnnot MGC");
    SAnnotSelector sel(CSeqFeatData::eSubtype_misc_difference);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("MGC");
    s_CheckFeat(sel, "BC103747.1");
}


BOOST_AUTO_TEST_CASE(CheckExtHPRD)
{
    LOG_POST("Checking ExtAnnot HPRD");
    SAnnotSelector sel(CSeqFeatData::eSubtype_site);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("HPRD");
    s_CheckFeat(sel, "NP_003933.1");
}


BOOST_AUTO_TEST_CASE(CheckExtSTS)
{
    LOG_POST("Checking ExtAnnot STS");
    SAnnotSelector sel(CSeqFeatData::eSubtype_STS);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("STS");
    s_CheckFeat(sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249220000));
}


BOOST_AUTO_TEST_CASE(CheckExtTRNA)
{
    LOG_POST("Checking ExtAnnot tRNA");
    SAnnotSelector sel(CSeqFeatData::eSubtype_tRNA);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("tRNA");
    s_CheckFeat(sel, "NT_026437.11");
}


BOOST_AUTO_TEST_CASE(CheckExtTRNAEdit)
{
    LOG_POST("Checking ExtAnnot tRNA edited");
    SAnnotSelector sel(CSeqFeatData::eSubtype_tRNA);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("tRNA");
    CRef<CScope> scope = s_InitScope();
    scope->SetKeepExternalAnnotsForEdit();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("NT_026437.11"));
    BOOST_REQUIRE(bh);
    CBioseq_EditHandle bhe = bh.GetEditHandle();
    BOOST_REQUIRE(bh);
    BOOST_REQUIRE(bhe);
    s_CheckFeat(scope, sel, "NT_026437.11");
}


BOOST_AUTO_TEST_CASE(CheckExtMicroRNA)
{
    LOG_POST("Checking ExtAnnot microRNA");
    SAnnotSelector sel(CSeqFeatData::eSubtype_otherRNA);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("other");
    s_CheckFeat(sel, "NT_026437.11");
}


BOOST_AUTO_TEST_CASE(CheckExtExon)
{
    LOG_POST("Checking ExtAnnot Exon");
    SAnnotSelector sel(CSeqFeatData::eSubtype_exon);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("Exon");
    s_CheckFeat(sel, "NR_003287.2");
}


BOOST_AUTO_TEST_CASE(CheckNAZoom)
{
    bool have_na = s_HaveNA();
    LOG_POST("Checking NA Tracks");
    string id = "NC_000024.10";
    string na_acc = "NA000000271.4";

    for ( int t = 0; t < 4; ++t ) {
        SAnnotSelector sel;
        sel.IncludeNamedAnnotAccession(na_acc, -1);
        if ( t&1 ) {
            sel.AddNamedAnnots(CombineWithZoomLevel(na_acc, -1));
        }
        if ( t&2 ) {
            sel.AddNamedAnnots(na_acc);
            sel.AddNamedAnnots(CombineWithZoomLevel(na_acc, 100));
        }
        sel.SetCollectNames();

        CRef<CSeq_loc> loc(new CSeq_loc);
        loc->SetWhole().Set(id);
        set<int> tracks;
        CRef<CScope> scope = s_InitScope();
        CGraph_CI it(*scope, *loc, sel);
        if ( !have_na ) {
            BOOST_CHECK(!it);
            continue;
        }
        ITERATE ( CGraph_CI::TAnnotNames, i, it.GetAnnotNames() ) {
            if ( !i->IsNamed() ) {
                continue;
            }
            int zoom;
            string acc;
            if ( !ExtractZoomLevel(i->GetName(), &acc, &zoom) ) {
                continue;
            }
            if ( acc != na_acc ) {
                continue;
            }
            tracks.insert(zoom);
        }
        BOOST_CHECK(tracks.count(100));
    }
}


BOOST_AUTO_TEST_CASE(CheckNAZoom10)
{
    LOG_POST("Checking NA Graph Track");
    string id = "NC_000022.11";
    string na_acc = "NA000000270.4";

    for ( int t = 0; t < 2; ++t ) {
        SAnnotSelector sel;
        sel.IncludeNamedAnnotAccession(na_acc, 10);
        if ( t&1 ) {
            sel.AddNamedAnnots(CombineWithZoomLevel(na_acc, -1));
        }
        else {
            sel.AddNamedAnnots(CombineWithZoomLevel(na_acc, 10));
        }
        auto counts = s_CheckGraph(sel, id);
        BOOST_CHECK_EQUAL(counts.first, 1u);
    }
}


BOOST_AUTO_TEST_CASE(CheckNATable)
{
    LOG_POST("Checking NA Seq-table Track");
    string id = "NC_000001.11";
    string na_acc = "NA000355453.1"; // NA000344170.1 is not live

    SAnnotSelector sel;
    sel.IncludeNamedAnnotAccession(na_acc);
    auto counts = s_CheckTable(sel, id);
    BOOST_CHECK_EQUAL(counts.first, 1u);
}


BOOST_AUTO_TEST_CASE(CheckSuppressedGene)
{
    LOG_POST("Checking features with suppressed gene");
    SAnnotSelector sel;
    sel.SetResolveAll().SetAdaptiveDepth();
    auto scope = s_InitScope();
    {
        auto counts = s_CheckFeat(scope, sel, "33347893", CRange<TSeqPos>(49354, 49354));
        BOOST_CHECK_EQUAL(counts.first, 1u);
        BOOST_CHECK_EQUAL(counts.second, 8u);
    }
    sel.SetExcludeIfGeneIsSuppressed();
    {
        auto counts = s_CheckFeat(scope, sel, "33347893", CRange<TSeqPos>(49354, 49354));
        BOOST_CHECK_EQUAL(counts.first, 1u);
        BOOST_CHECK_EQUAL(counts.second, 6u);
    }
}


static CRef<CBioseq> s_MakeTemporaryDelta(const CSeq_loc& loc, CScope& scope)

{
    CBioseq_Handle bsh = scope.GetBioseqHandle(loc);
    CRef<CBioseq> seq(new CBioseq());
    // TODO: Need to make thread-safe unique seq-id
    seq->SetId().push_back(CRef<CSeq_id>(new CSeq_id("lcl|temporary_delta")));
    seq->SetInst().Assign(bsh.GetInst());
    seq->SetInst().ResetSeq_data();
    seq->SetInst().SetRepr(CSeq_inst::eRepr_delta);
    CRef<CDelta_seq> element(new CDelta_seq());
    element->SetLoc().Assign(loc);
    seq->SetInst().ResetExt();
    seq->SetInst().SetExt().SetDelta().Set().push_back(element);
    seq->SetInst().ResetLength();
    return seq;
}
 
 
BOOST_AUTO_TEST_CASE(Test_DeltaSAnnot)
{
    LOG_POST("Checking DeltaSAnnot");
    // set up objmgr with fetching
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*objmgr);
    CScope scope(*objmgr);
    scope.AddDefaults();
 
    CRef<CSeq_loc> loc(new CSeq_loc());
    loc->SetInt().SetId().SetOther().SetAccession("NT_077402");
    loc->SetInt().SetId().SetOther().SetVersion(3);
    loc->SetInt().SetFrom(50000);
    loc->SetInt().SetTo(100000);
 
    CRef<CBioseq> delta = s_MakeTemporaryDelta(*loc, scope);
 
    CBioseq_Handle delta_bsh = scope.AddBioseq(*delta);
    //LOG_POST("Bioseq: "<<MSerial_AsnText<<*delta_bsh.GetCompleteBioseq());
 
    SAnnotSelector sel_cpy;
    sel_cpy.SetResolveAll();
    sel_cpy.SetAdaptiveDepth(true);
    sel_cpy.ExcludeFeatSubtype(CSeqFeatData::eSubtype_variation);
    sel_cpy.ExcludeFeatSubtype(CSeqFeatData::eSubtype_STS);
    CFeat_CI it(delta_bsh, sel_cpy);
    size_t num_feat = 0;
    while (it) {
        //LOG_POST("Feature: "<<MSerial_AsnText<<*it->GetSeq_feat());
        ++num_feat;
        CSeqFeatData::ESubtype subtype = it->GetData().GetSubtype();
        BOOST_CHECK(subtype != CSeqFeatData::eSubtype_bad);
        if (it->GetSeq_feat()->GetLocation().IsInt()) {
            const CSeq_id& id = it->GetOriginalSeq_feat()->GetLocation().GetInt().GetId();
            CBioseq_Handle local_bsh = scope.GetBioseqHandle(id);
            ITERATE(CBioseq_Handle::TId, id_it, local_bsh.GetId()) {
                string label;
                id_it->GetSeqId()->GetLabel(&label);
                TSeqPos start = it->GetLocation().GetStart(eExtreme_Biological);
                TSeqPos stop = it->GetLocation().GetStop(eExtreme_Biological);
                BOOST_CHECK(start != stop);
            }
            TSeqPos start = it->GetLocation().GetStart(eExtreme_Biological);
            TSeqPos stop = it->GetLocation().GetStop(eExtreme_Biological);
            BOOST_CHECK(start != stop);
        }
        ++it;
    }
    BOOST_CHECK_EQUAL(num_feat, 0u);
}


BOOST_AUTO_TEST_CASE(Test_HUP)
{
    bool authorized;
    string user_name = CSystemInfo::GetUserName();
    if ( CGBDataLoader::IsUsingPSGLoader() ) {
        authorized = s_HaveHUPToken();
    }
    else if ( user_name == "vasilche" ) {
        authorized = true;
    }
    else if ( user_name == "coremake" ) {
        authorized = false;
    }
    else {
        LOG_POST("Skipping HUP access for unknown user");
        return;
    }
    bool only_cache = s_HaveOnlyCache();
    if ( authorized ) {
        LOG_POST("Checking HUP access for authorized user");
    }
    else {
        LOG_POST("Checking HUP access for unauthorized user");
    }
    
    // set up objmgr with fetching
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    string gb_main = CGBDataLoader::RegisterInObjectManager(*objmgr).GetLoader()->GetName();
    BOOST_REQUIRE_EQUAL(gb_main, "GBLOADER");
    string gb_main2 = CGBDataLoader::RegisterInObjectManager(*objmgr, "pubseqos").GetLoader()->GetName();
    BOOST_REQUIRE_EQUAL(gb_main2, "GBLOADER");
    string gb_main3 = CGBDataLoader::RegisterInObjectManager(*objmgr, "psg").GetLoader()->GetName();
    BOOST_REQUIRE_EQUAL(gb_main3, "GBLOADER");
    string gb_hup = CGBDataLoader::RegisterInObjectManager(*objmgr, CGBDataLoader::eIncludeHUP, s_HUPToken).GetLoader()->GetName();
#define EXPECTED_LOADER_NAME "GBLOADER-HUP"
    BOOST_CHECK_EQUAL(gb_hup.substr(0, strlen(EXPECTED_LOADER_NAME)), EXPECTED_LOADER_NAME);

    CSeq_id_Handle id_main = CSeq_id_Handle::GetHandle("NT_077402");
    //CSeq_id_Handle id_hup = CSeq_id_Handle::GetHandle("AY263392");
    CSeq_id_Handle id_hup = CSeq_id_Handle::GetHandle("PP445004");
    {{
        CScope scope(*objmgr);
        scope.AddDataLoader(gb_hup);
        {{
            auto bh_hup = scope.GetBioseqHandle(id_hup);
            if ( authorized ) {
                BOOST_CHECK(bh_hup);
                BOOST_CHECK(bh_hup.GetState() == CBioseq_Handle::fState_none);
            }
            else {
                BOOST_CHECK(!bh_hup);
                BOOST_CHECK(bh_hup.GetState() & CBioseq_Handle::fState_no_data);
                if ( !only_cache ) {
                    BOOST_CHECK(bh_hup.GetState() & CBioseq_Handle::fState_confidential);
                }
            }
        }}
        {{
            auto bh_main = scope.GetBioseqHandle(id_main);
            BOOST_CHECK(bh_main);
            BOOST_CHECK_EQUAL(bh_main.GetState(), CBioseq_Handle::fState_none);
        }}
    }}

    {{
        CScope scope(*objmgr);
        scope.AddDefaults();
        
        BOOST_CHECK(!scope.GetBioseqHandle(id_hup));
        scope.AddDataLoader(gb_hup);
        {{
            auto bh_hup = scope.GetBioseqHandle(id_hup);
            if ( authorized ) {
                BOOST_CHECK(bh_hup);
                BOOST_CHECK(bh_hup.GetState() == CBioseq_Handle::fState_none);
            }
            else {
                BOOST_CHECK(!bh_hup);
                BOOST_CHECK(bh_hup.GetState() & CBioseq_Handle::fState_no_data);
                if ( !only_cache ) {
                    BOOST_CHECK(bh_hup.GetState() & CBioseq_Handle::fState_confidential);
                }
            }
        }}
        {{
            auto bh_main = scope.GetBioseqHandle(id_main);
            BOOST_CHECK(bh_main);
            BOOST_CHECK_EQUAL(bh_main.GetState(), CBioseq_Handle::fState_none);
        }}
        scope.RemoveDataLoader(gb_hup);
        {{
            // without HUP GB loader it's simply confidential
            auto bh_hup = scope.GetBioseqHandle(id_hup);
            BOOST_CHECK(!bh_hup);
            BOOST_CHECK(bh_hup.GetState() & CBioseq_Handle::fState_no_data);
            if ( !only_cache ) {
                BOOST_CHECK(bh_hup.GetState() & CBioseq_Handle::fState_confidential);
            }
        }}
    }}
    
    objmgr->RevokeDataLoader(gb_hup);
}


BOOST_AUTO_TEST_CASE(TestHistory)
{
    LOG_POST("Checking CScope::ResetHistory()");
    // set up objmgr with fetching
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*objmgr);
    CScope scope(*objmgr);
    scope.AddDefaults();
    SAnnotSelector sel;
    sel.ExcludeFeatSubtype(CSeqFeatData::eSubtype_variation);
    for ( int t = 0; t < 2; ++t ) {
        vector <CSeq_feat_Handle> h1;
        {
            auto seq_id = Ref(new CSeq_id("NM_002020"));
            auto handle = scope.GetBioseqHandle(*seq_id);
            CFeat_CI feat_it(handle, TSeqRange(1, 500), sel);
            while (feat_it) {
                h1.push_back(feat_it->GetSeq_feat_Handle());
                ++feat_it;
            }
        }
        for (auto&& h : h1) {
            auto& annot = h.GetAnnot();
            BOOST_CHECK(annot);
            if ( annot ) {
                CDataLoader::TBlobId blob_id = annot.GetTSE_Handle().GetBlobId();
                cout << "BlobId: " << blob_id.ToString() << endl;
            }
        }
        cout << "ResetHistory()" << endl;
        scope.ResetHistory();
        for (auto&& h : h1) {
            auto& annot = h.GetAnnot();
            BOOST_CHECK(annot);
        }
        cout << "ResetHistory(eRemoveIfLocked)" << endl;
        scope.ResetHistory(scope.eRemoveIfLocked);
        for (auto&& h : h1) {
            auto& annot = h.GetAnnot();
            BOOST_CHECK(!annot);
        }
    }
}

BOOST_AUTO_TEST_CASE(TestGBLoaderName)
{
    LOG_POST("Checking CGBDataLoader user-defined name");
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
    CGBLoaderParams params;
    params.SetLoaderName("GBLOADER-user");
    string name0 = CGBDataLoader::RegisterInObjectManager(*objmgr).GetLoader()->GetName();
    BOOST_CHECK_EQUAL(name0, "GBLOADER");
    string name1 = CGBDataLoader::RegisterInObjectManager(*objmgr, params).GetLoader()->GetName();
    BOOST_CHECK_EQUAL(name1, "GBLOADER-user");
    objmgr->RevokeDataLoader("GBLOADER");
    objmgr->RevokeDataLoader("GBLOADER-user");
}

BOOST_AUTO_TEST_CASE(CheckExtGetAllTSEs)
{
    LOG_POST("Checking CScope::GetAllTSEs() with external annotations");
    SAnnotSelector sel(CSeqFeatData::eSubtype_cdregion);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("AAC73113.1"));
    BOOST_CHECK(bh);
    BOOST_CHECK(!CFeat_CI(bh, sel));
    CScope::TTSE_Handles tses;
    scope->GetAllTSEs(tses, CScope::eAllTSEs);
    BOOST_CHECK(tses.size() > (s_HaveMongoDBCDD() ? 1 : 0));
    size_t size = 0;
    for ( auto& tse : tses ) {
        size += CFeat_CI(tse.GetTopLevelEntry(), sel).GetSize();
    }
    BOOST_CHECK(size == 0);
    if (s_HaveMongoDBCDD()) {
        sel.SetFeatSubtype(CSeqFeatData::eSubtype_region);
        for ( auto& tse : tses ) {
            size += CFeat_CI(tse.GetTopLevelEntry(), sel).GetSize();
        }
        BOOST_CHECK(size > 0);
    }
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr)
{
    LOG_POST("Checking WGS master sequence descriptors");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("BASL01000795.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Title));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(!(desc_mask & (1<<CSeqdesc::e_Comment)));
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Genbank));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(total_count, 10);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr2)
{
    LOG_POST("Checking WGS master sequence descriptors with duplicates");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("NZ_JJPX01000174.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Comment));
    BOOST_CHECK_EQUAL(comment_count, 1);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 4u);
    int extra = 0;
    if ( !s_HaveID1() ) { // TODO: ID1 returns 1 StructuredComment
        BOOST_CHECK_EQUAL(user_count["StructuredComment"], 2);
    }
    else {
        BOOST_CHECK_GE(user_count["StructuredComment"], 1);
        extra = user_count["StructuredComment"]-2;
    }
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["RefGeneTracking"], 1);
    BOOST_CHECK_EQUAL(user_count["FeatureFetchPolicy"], 1);
    BOOST_CHECK_EQUAL(total_count, 12+extra);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr3)
{
    LOG_POST("Checking WGS master sequence descriptors with duplicates");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("NZ_JJPX01000091.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Comment));
    BOOST_CHECK_EQUAL(comment_count, 1);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 4u);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 2);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["RefGeneTracking"], 1);
    int extra = 0;
    if ( !s_HaveID1() ) { // TODO: ID1 returns 2 FeatureFetchPolicy
        BOOST_CHECK_EQUAL(user_count["FeatureFetchPolicy"], 1);
    }
    else {
        BOOST_CHECK_GE(user_count["FeatureFetchPolicy"], 1);
        extra = user_count["FeatureFetchPolicy"]-1;
    }
    BOOST_CHECK_EQUAL(total_count, 12+extra);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr4)
{
    LOG_POST("Checking WGS master sequence descriptors 4+9");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("CAAD010000001.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Title));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Embl));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(!(desc_mask & (1<<CSeqdesc::e_Comment)));
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 1u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(total_count, 9);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr5)
{
    LOG_POST("Checking WGS master sequence descriptors NZ_4+9");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("NZ_CAVJ010000001.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 3);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Comment));
    BOOST_CHECK_EQUAL(comment_count, 1);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Embl));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 4u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 2);
    BOOST_CHECK_EQUAL(user_count["RefGeneTracking"], 1);
    BOOST_CHECK_EQUAL(user_count["FeatureFetchPolicy"], 1);
    BOOST_CHECK_EQUAL(total_count, 14);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr6)
{
    LOG_POST("Checking WGS master sequence descriptors NZ_6+9");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("NZ_CAACXP010000001.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 1);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Comment));
    BOOST_CHECK_EQUAL(comment_count, 1);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Embl));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 4u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(user_count["RefGeneTracking"], 1);
    BOOST_CHECK_EQUAL(user_count["FeatureFetchPolicy"], 1);
    BOOST_CHECK_EQUAL(total_count, 11);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr7)
{
    LOG_POST("Checking WGS master sequence descriptors: Unverified");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("AVKQ01000001"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 1);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Comment));
    BOOST_CHECK_EQUAL(comment_count, 1);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 3u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(user_count["Unverified"], 1);
    BOOST_CHECK_EQUAL(total_count, 9);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr8)
{
    if ( !s_HaveVDBWGS() ) {
        LOG_POST("Skipping test for WGS descriptors of detached protein without VDB");
        return;
    }
    LOG_POST("Checking VDB WGS master sequence descriptors on a detached protein");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("MBA2057862.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Title));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Comment));
    BOOST_CHECK_EQUAL(comment_count, 2);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 2);
    BOOST_CHECK_EQUAL(total_count, 12);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr9)
{
    if ( !s_HaveVDBWGS() ) {
        LOG_POST("Skipping test for WGS descriptors of VDB scaffolds without VDB");
        return;
    }
    LOG_POST("Checking VDB WGS master sequence descriptors on scaffolds");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("ALWZ04S0000001"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(!(desc_mask & (1<<CSeqdesc::e_Title)));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 4);
    BOOST_CHECK(!(desc_mask & (1<<CSeqdesc::e_Comment)));
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(total_count, 10);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr10)
{
    LOG_POST("Checking WGS master sequence descriptors with split main seqence");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("KGB65708"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Title));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(!(desc_mask & (1<<CSeqdesc::e_Comment)));
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(total_count, 9);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr11)
{
    LOG_POST("Checking WGS master sequence descriptors with split main seqence");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("JRAV01000001"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 2);
    BOOST_CHECK(!(desc_mask & (1<<CSeqdesc::e_Comment)));
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(total_count, 8);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr12)
{
    if ( !s_HaveVDBWGS() ) {
        LOG_POST("Skipping test for WGS descriptors of VDB contig without VDB");
        return;
    }
    LOG_POST("Checking VDB WGS master sequence descriptors with split main seqence");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("AAHIBN010000001"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Genbank));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 1);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Comment));
    BOOST_CHECK_EQUAL(comment_count, 2);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 2);
    BOOST_CHECK_EQUAL(total_count, 11);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr13)
{
    if ( !s_HaveVDBWGS() ) {
        LOG_POST("Skipping test for WGS descriptors of VDB contig without VDB");
        return;
    }
    LOG_POST("Checking VDB WGS master sequence descriptors with partial optional desc");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("ALWZ040100108.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Title));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 4);
    BOOST_CHECK(!(desc_mask & (1<<CSeqdesc::e_Comment)));
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(total_count, 11);
}


BOOST_AUTO_TEST_CASE(CheckWGSMasterDescr14)
{
    if ( !s_HaveVDBWGS() ) {
        LOG_POST("Skipping test for WGS descriptors of VDB contig without VDB");
        return;
    }
    LOG_POST("Checking VDB WGS master sequence descriptors with partial optional desc");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("ALWZ050100108.1"));
    BOOST_REQUIRE(bh);
    int desc_mask = 0;
    map<string, int> user_count;
    int comment_count = 0;
    int pub_count = 0;
    int total_count = 0;
    for ( CSeqdesc_CI it(bh); it; ++it ) {
        if ( it->IsUser() && it->GetUser().GetType().GetStr() == "WithMasterDescr" ) {
            LOG_POST("Got WithMasterDescr");
            continue;
        }
        ++total_count;
        desc_mask |= 1<<it->Which();
        switch ( it->Which() ) {
        case CSeqdesc::e_Comment:
            ++comment_count;
            break;
        case CSeqdesc::e_Pub:
            ++pub_count;
            break;
        case CSeqdesc::e_User:
            ++user_count[it->GetUser().GetType().GetStr()];
            break;
        default:
            break;
        }
    }
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Source));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Molinfo));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Pub));
    BOOST_CHECK_EQUAL(pub_count, 4);
    BOOST_CHECK(!(desc_mask & (1<<CSeqdesc::e_Comment)));
    BOOST_CHECK_EQUAL(comment_count, 0);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(total_count, 10);
}


BOOST_AUTO_TEST_CASE(CheckWGSFeat1)
{
    if ( !s_HaveVDBWGS() ) {
        LOG_POST("Skipping test for WGS VDB split features without VDB");
        return;
    }
    LOG_POST("Checking WGS VDB split features");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("AAAAAA010000001"));
    BOOST_REQUIRE(bh);
    CFeat_CI feat_it(bh, CRange<TSeqPos>(100000, 500000));
    BOOST_CHECK_EQUAL(feat_it.GetSize(), 800u);
}


enum EInstType {
    eInst_ext, // inst.ext is set, inst.seq-data is not set
    eInst_data, // inst.ext is not set, inst.seq-data is set
    eInst_split_data // inst.ext is not set, inst.seq-data is split
};

static void s_CheckSplitSeqData(CScope& scope, const string& seq_id, EInstType type)
{
    CBioseq_Handle bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle(seq_id));
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetSeq_data(), type == eInst_data);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetExt(), type == eInst_ext);
    BOOST_CHECK_EQUAL(bh.IsSetInst_Seq_data(), type != eInst_ext);
    BOOST_CHECK_EQUAL(bh.IsSetInst_Ext(), type == eInst_ext);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetSeq_data(), type == eInst_data);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetExt(), type == eInst_ext);
    bh.GetInst();
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetSeq_data(), type != eInst_ext);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetExt(), type == eInst_ext);
    BOOST_CHECK_EQUAL(bh.IsSetInst_Seq_data(), type != eInst_ext);
    BOOST_CHECK_EQUAL(bh.IsSetInst_Ext(), type == eInst_ext);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetSeq_data(), type != eInst_ext);
    BOOST_CHECK_EQUAL(bh.GetBioseqCore()->GetInst().IsSetExt(), type == eInst_ext);
}

BOOST_AUTO_TEST_CASE(CheckSplitSeqData)
{
    LOG_POST("Checking split Seq-data access");
    CRef<CScope> scope = s_InitScope();
    s_CheckSplitSeqData(*scope, "NC_000001.11", eInst_ext);
    s_CheckSplitSeqData(*scope, "568815361", eInst_ext);
    s_CheckSplitSeqData(*scope, "24210292", eInst_ext);
    s_CheckSplitSeqData(*scope, "499533515", eInst_data);
    s_CheckSplitSeqData(*scope, "10086043", eInst_split_data);
    s_CheckSplitSeqData(*scope, "8894296", eInst_split_data);
}


BOOST_AUTO_TEST_CASE(CheckSeqState)
{
    LOG_POST("Checking GetSequenceState()");
    CRef<CScope> scope = s_InitScope();
    CSeq_id_Handle id = CSeq_id_Handle::GetHandle("NM_001007539.1");
    CBioseq_Handle::TBioseqStateFlags state = CBioseq_Handle::fState_suppress_perm;
    LOG_POST("1st scope->GetSequenceState(id)");
    BOOST_CHECK_EQUAL(scope->GetSequenceState(id), state);
    LOG_POST("2nd scope->GetSequenceState(id)");
    BOOST_CHECK_EQUAL(scope->GetSequenceState(id), state);
    CBioseq_Handle bh = scope->GetBioseqHandle(id);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), state);
    LOG_POST("3rd scope->GetSequenceState(id)");
    BOOST_CHECK_EQUAL(scope->GetSequenceState(id), state);
    bh.Reset();
    LOG_POST("4th scope->GetSequenceState(id)");
    BOOST_CHECK_EQUAL(scope->GetSequenceState(id), state);
    scope = s_InitScope(false);
    LOG_POST("5th scope->GetSequenceState(id)");
    BOOST_CHECK_EQUAL(scope->GetSequenceState(id), state);
    scope.Reset();
    scope = s_InitScope(true);
    LOG_POST("6th scope->GetSequenceState(id)");
    BOOST_CHECK_EQUAL(scope->GetSequenceState(id), state);
}


#if defined(NCBI_THREADS)
BOOST_AUTO_TEST_CASE(MTCrash1)
{
    CRef<CScope> scope = CSimpleOM::NewScope();
    SAnnotSelector sel;
    sel
        // consider overlaps by total range...
        .SetOverlapTotalRange()
        // resolve all segments...
        .SetResolveAll()
        .SetAdaptiveDepth(true)
        .SetResolveAll()
        ;
    sel.SetFeatType(CSeqFeatData::e_Gene)
        .IncludeFeatType(CSeqFeatData::e_Rna)
        .IncludeFeatType(CSeqFeatData::e_Cdregion);
    
    vector<string> accs;
    for (int i = 1; i < 6; ++i) {
        CNcbiOstrstream ss;
        ss << "NC_" << std::setw(6) << std::setfill('0') << i;
        accs.push_back(CNcbiOstrstreamToString(ss));
    }
    for (int c = 0; c < 5; ++c) {
        //                srand(time(0));
        //                random_shuffle(accs.begin(), accs.end());
        for (const auto& acc : accs) {
            auto seq_id = Ref(new CSeq_id(acc));
            int bioseq_len = 0;
            {
                auto bsh = scope->GetBioseqHandle(*seq_id);
                bioseq_len = bsh.GetBioseqLength();
            }
            int start = 0;
            int chunk_size = max<int>(100000, bioseq_len / 8);
            vector<future<bool>> res;
            while (start < bioseq_len - 1) {
                int stop = min(start + chunk_size, bioseq_len - 1);
                res.emplace_back(async(launch::async, [&]()->bool {
                    auto bsh = scope->GetBioseqHandle(*seq_id);
                    TSeqRange range(start, stop);
                    CFeat_CI feat_it(bsh, range, sel);
                    for (;  feat_it;  ++feat_it) {
                    }
                    return true;
                }));
                start = stop + 1;
            }
            bool all_is_good = all_of(res.begin(), res.end(), [](future<bool>& f) { return f.get(); });
            BOOST_CHECK(all_is_good);
            cout << acc << ": passed" << endl;
        }
        cout << "============================" << endl;
    }
}
#endif


BOOST_AUTO_TEST_CASE(TestGetBlobById)
{
    bool is_psg = CGBDataLoader::IsUsingPSGLoader();
    if ( is_psg ) {
        // test PSG
    }
    else {
        if ( !s_HaveID2(eExcludePubseqos2) ) {
            LOG_POST("Skipping CDataLoader::GetBlobById() test without OSG or PSG");
            return;
        }
        if ( s_HaveCache() ) {
            LOG_POST("Skipping CDataLoader::GetBlobById() with GenBank loader cache enabled");
            return;
        }
        // test ID2
    }    
    LOG_POST("Testing CDataLoader::GetBlobById()");
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    om->RevokeAllDataLoaders();
    CRef<CReader> reader;
    if ( is_psg ) {
        CGBDataLoader::RegisterInObjectManager(*om);
    }
    else {
        CGBLoaderParams params;
        reader = new CId2Reader();
        params.SetReaderPtr(reader);
        CGBDataLoader::RegisterInObjectManager(*om, params);
    }
    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();
    
    map<int, set<CConstRef<CSeq_entry>>> entries;
    map<int, CBlobIdKey> blob_ids;
    for ( int t = 0; t < 4; ++t ) {
        int gi_start = 2;
        int gi_end = 100;
        int gi_stop_check = gi_end / 2;
        bool no_connection = false;
        if ( t == 0 ) {
            LOG_POST("Collecting entries");
        }
        else if ( t == 1 ) {
            LOG_POST("Re-loading entries");
        }
        else if ( t == 2 ) {
            LOG_POST("Re-loading entries with errors");
            if ( is_psg ) {
#ifdef HAVE_PSG_LOADER
                CPSGDataLoader_Impl::SetGetBlobByIdShouldFail(true);
                no_connection = true;
#endif
            }
            else {
                reader->SetMaximumConnections(0);
                no_connection = true;
            }
        }
        else {
            LOG_POST("Re-loading entries without errors");
            if ( is_psg ) {
#ifdef HAVE_PSG_LOADER
                CPSGDataLoader_Impl::SetGetBlobByIdShouldFail(false);
#endif
            }
            else {
                reader->SetMaximumConnections(1);
            }
        }
        if ( no_connection ) {
            LOG_POST("Multiple exception messages may appear below.");
        }
        for ( int gi = gi_start; gi <= gi_end; ++gi ) {
            CSeq_id_Handle idh = CSeq_id_Handle::GetGiHandle(GI_FROM(int, gi));
            CBioseq_Handle bh;
            if ( no_connection ) {
                if ( gi < gi_stop_check ) {
                    BOOST_CHECK_THROW(scope->GetBioseqHandle(idh), CException);
                }
                continue;
            }
            else {
                bh = scope->GetBioseqHandle(idh);
            }
            BOOST_REQUIRE(bh);
            if ( t == 0 ) {
                blob_ids[gi] = bh.GetTSE_Handle().GetBlobId();
            }
            auto entry = bh.GetTSE_Handle().GetTSECore();
            if ( gi < gi_stop_check ) {
                BOOST_CHECK(entries[gi].insert(entry).second);
            }
        }
    }
    /*
    for ( auto& s : blob_ids ) {
    }
    */
}


static CScope::TBlobId s_MakeBlobId(int sat, int sat_key)
{
    CScope::TBlobId ret;
    if (CGBDataLoader::IsUsingPSGLoader()) {
#if defined(HAVE_PSG_LOADER)
        string blobid_str = NStr::NumericToString(sat)+'.'+NStr::NumericToString(sat_key);
        CRef<CPsgBlobId> real_blob_id;
        real_blob_id = new CPsgBlobId(blobid_str);
        ret = CScope::TBlobId(real_blob_id);
#endif
    }
    else {
        CRef<CBlob_id> real_blob_id(new CBlob_id);
        real_blob_id->SetSat(sat);
        real_blob_id->SetSatKey(sat_key);
        ret = CScope::TBlobId(real_blob_id);
    }
    return ret;
}


BOOST_AUTO_TEST_CASE(TestGetBlobByIdSat)
{
    const int sat = 4;
    const int sat_key = 207110312;
    
    CRef<CScope> scope = s_InitScope();
    CDataLoader* loader =
        CObjectManager::GetInstance()->FindDataLoader(CGBDataLoader::GetLoaderNameFromArgs());
    BOOST_REQUIRE(loader);
    CSeq_entry_Handle seh = scope->GetSeq_entryHandle(loader, s_MakeBlobId(sat, sat_key));
    BOOST_REQUIRE(seh);
}


#if defined(NCBI_THREADS)
BOOST_AUTO_TEST_CASE(TestGetBlobByIdSatMT)
{
    const int sat = 4;
    const int sat_key_0 = 207110312;
    
    CRef<CScope> scope = s_InitScope();
    CDataLoader* loader =
        CObjectManager::GetInstance()->FindDataLoader(CGBDataLoader::GetLoaderNameFromArgs());
    BOOST_REQUIRE(loader);

    const int NQ = 20;
    vector<future<bool>> res;
    for ( int i = 0; i < NQ; ++i ) {
        res.emplace_back(async(launch::async, [&](int add_sat_key)->bool {
            int sat_key = sat_key_0 + add_sat_key;
            CSeq_entry_Handle seh = scope->GetSeq_entryHandle(loader, s_MakeBlobId(sat, sat_key));
            return seh;
        }, i/2));
    }
    bool all_is_good = all_of(res.begin(), res.end(), [](future<bool>& f) {
        return f.get();
    });
    BOOST_CHECK(all_is_good);
}
#endif


BOOST_AUTO_TEST_CASE(TestStateDead)
{
    LOG_POST("Checking WGS state dead");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP01000011.1"));
    BOOST_CHECK(bh.GetState() & bh.fState_dead);
    BOOST_CHECK(!(bh.GetState() & bh.fState_withdrawn));
    BOOST_CHECK(bh);
}


BOOST_AUTO_TEST_CASE(TestStateWithdrawnDead)
{
    if ( s_HaveCache() ) {
        LOG_POST("Skipping test for withdrawn blob with GenBank loader cache enabled");
        return;
    }
    LOG_POST("Checking WGS state withdrawn");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP01000012.1"));
    BOOST_CHECK(bh.GetState() & bh.fState_withdrawn);
    if ( !s_HaveID1() ) { // ID1 cannot report 'dead' together with 'withdrawn'
        BOOST_CHECK(bh.GetState() & bh.fState_dead);
    }
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(TestStateWithdrawn)
{
    if ( s_HaveCache() ) {
        LOG_POST("Skipping test for withdrawn blob with GenBank loader cache enabled");
        return;
    }
    LOG_POST("Checking WGS state withdrawn");
    CRef<CScope> scope = s_InitScope();
    CBioseq_Handle bh = scope->GetBioseqHandle(CSeq_id_Handle::GetHandle("JAPEOG000000000.1"));
    BOOST_CHECK(bh.GetState() & bh.fState_withdrawn);
    BOOST_CHECK(!(bh.GetState() & bh.fState_dead));
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(TestStateDeadConflict)
{
    LOG_POST("Checking live state after loading dead blob");
    CRef<CScope> scope1 = s_InitScope();
    CRef<CScope> scope2 = s_InitScope(false);
    auto gbloader = dynamic_cast<CGBDataLoader*>(CObjectManager::GetInstance()->FindDataLoader("GBLOADER"));
    BOOST_REQUIRE(gbloader);
    auto blob_id = gbloader->GetBlobIdFromSatSatKey(17, 111408637);
    BOOST_REQUIRE(blob_id);
    auto eh1 = scope1->GetSeq_entryHandle(gbloader, blob_id);
    BOOST_REQUIRE(eh1);
    BOOST_CHECK(eh1.GetTSE_Handle().Blob_IsDead());
    CBioseq_Handle bh2 = scope2->GetBioseqHandle(CSeq_id_Handle::GetHandle("A00002"));
    BOOST_REQUIRE(bh2);
    BOOST_CHECK(!(bh2.GetState() & bh2.fState_dead));
    BOOST_CHECK(!bh2.GetTSE_Handle().Blob_IsDead());
    BOOST_CHECK_EQUAL(bh2.GetState(), bh2.fState_none);
    auto bh1 = scope1->GetBioseqHandleFromTSE(CSeq_id_Handle::GetHandle("A00002"), eh1.GetTSE_Handle());
    BOOST_REQUIRE(bh1);
    BOOST_CHECK(bh1.GetTSE_Handle().Blob_IsDead());
    auto id0 = blob_id.ToString();
    auto id1 = eh1.GetTSE_Handle().GetBlobId().ToString();
    auto id2 = bh2.GetTSE_Handle().GetBlobId().ToString();
    BOOST_CHECK_EQUAL(id0, id1);
    BOOST_CHECK_NE(id0, id2);
    BOOST_CHECK_NE(id1, id2);
}


enum EFallbackFailType {
    eFallbackFail_none,
    eFallbackFail_seq,
    eFallbackFail_na
};

static
void s_TestFallback(const string& id_str,
                    const string& na_str,
                    EFallbackFailType fail_type = eFallbackFail_none)
{
    if ( !CGBDataLoader::IsUsingPSGLoader() ) {
        LOG_POST("Skipping test for PSG fallback without PSG loader enabled");
        return;
    }
    if ( NeedsOtherLoaders() ) {
        LOG_POST("Skipping test for PSG fallback without PSG2 loader enabled");
        return;
    }
    LOG_POST("Checking PSG fallback for "<<id_str<<" + "<<na_str);
    CRef<CScope> scope = s_InitScope();
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id_str);
    CBioseq_Handle bh;
    SAnnotSelector base_sel;
    CRef<CSeq_loc> search_loc(new CSeq_loc());
    CRef<CSeq_id> seq_id(new CSeq_id(id_str));
    search_loc->SetWhole(*seq_id);
    if ( fail_type == eFallbackFail_seq ) {
        LOG_POST("Expecting to fail loading sequence "<<id_str);
        BOOST_CHECK_THROW(scope->GetBioseqHandle(idh), CLoaderException);
        return;
        LOG_POST("Creating a dummy sequence "<<id_str<<" for NA search.");
        auto ids = scope->GetIds(idh);
        BOOST_CHECK(!ids.empty());
        auto seq_len = scope->GetSequenceLength(idh);
        BOOST_CHECK(seq_len != kInvalidSeqPos);
        auto seq_type = scope->GetSequenceType(idh);
        BOOST_CHECK(seq_type != CSeq_inst::eMol_not_set);
        CRef<CBioseq> seq(new CBioseq);
        for ( const auto& id : ids ) {
            seq->SetId().push_back(Ref(SerialClone(*id.GetSeqId())));
        }
        seq->SetInst().SetRepr(CSeq_inst::eRepr_virtual);
        seq->SetInst().SetMol(seq_type);
        seq->SetInst().SetLength(seq_len);
        bh = scope->AddBioseq(*seq);
        BOOST_REQUIRE(bh);
    }
    else {
        bh = scope->GetBioseqHandle(idh);
        BOOST_REQUIRE(bh);
    }
    size_t annot_count_0 = 0;
    annot_count_0 += CFeat_CI(*scope, *search_loc, base_sel).GetSize();
    annot_count_0 += CGraph_CI(*scope, *search_loc, base_sel).GetSize();
    size_t all_annot_count = annot_count_0;
    vector<string> nas;
    NStr::Split(na_str, ",", nas);
    for ( auto& na : nas ) {
        SAnnotSelector sel = base_sel;
        sel.IncludeNamedAnnotAccession(na);
        if ( fail_type == eFallbackFail_na ) {
            size_t annot_count = 0;
            LOG_POST("Expecting to fail loading "<<na);
            BOOST_CHECK_THROW(annot_count += (CFeat_CI(*scope, *search_loc, sel).GetSize()+
                                              CGraph_CI(*scope, *search_loc, sel).GetSize()),
                              CLoaderException);
            BOOST_CHECK_GE(annot_count, annot_count_0);
        }
        else {
            size_t annot_count = 0;
            annot_count += CFeat_CI(*scope, *search_loc, sel).GetSize();
            annot_count += CGraph_CI(*scope, *search_loc, sel).GetSize();
            BOOST_CHECK_GT(annot_count, annot_count_0);
            all_annot_count += annot_count - annot_count_0;
        }
    }
    if ( nas.size() > 1 ) {
        SAnnotSelector sel = base_sel;
        for ( auto& na : nas ) {
            sel.IncludeNamedAnnotAccession(na);
        }
        if ( fail_type == eFallbackFail_na ) {
            size_t annot_count = 0;
            LOG_POST("Expecting to fail loading NAs");
            BOOST_CHECK_THROW(annot_count += (CFeat_CI(*scope, *search_loc, sel).GetSize()+
                                              CGraph_CI(*scope, *search_loc, sel).GetSize()),
                              CLoaderException);
            BOOST_CHECK_GE(annot_count, all_annot_count);
        }
        else {
            size_t annot_count = 0;
            annot_count += CFeat_CI(*scope, *search_loc, sel).GetSize();
            annot_count += CGraph_CI(*scope, *search_loc, sel).GetSize();
            BOOST_CHECK_EQUAL(annot_count, all_annot_count);
        }
    }
}


BOOST_AUTO_TEST_CASE(TestFallback1)
{
    s_TestFallback("NW_004545872.1", "NA000393821.1,NA000393800.1");
}


BOOST_AUTO_TEST_CASE(TestFallback2)
{
    s_TestFallback("QEIB01002456.1", "NA000204978.1");
}


BOOST_AUTO_TEST_CASE(TestFallback3)
{
    s_TestFallback("LR536440.1", "NA000210082.1,NA000210263.1");
}


BOOST_AUTO_TEST_CASE(TestFallback3g)
{
    s_TestFallback("LR536440.1", "NA000210082.1@@1000000");
}


BOOST_AUTO_TEST_CASE(TestFallback3f)
{
    s_TestFallback("LR536440.1", "NA000210263.1@@1000000", eFallbackFail_na);
}


BOOST_AUTO_TEST_CASE(TestFallback4)
{
    s_TestFallback("NC_026001.1", "NA000214470.1,NA000214465.1");
}


BOOST_AUTO_TEST_CASE(TestFallback5)
{
    s_TestFallback("NC_081782.1", "NA000417668.1,NA000417649.1", eFallbackFail_seq);
}


BOOST_AUTO_TEST_CASE(TestFallback7)
{
    s_TestFallback("2699449663", "");
}


BOOST_AUTO_TEST_CASE(TestFallback8)
{
    s_TestFallback("2699449523", "");
}


BOOST_AUTO_TEST_CASE(TestPirMatching)
{
    LOG_POST("Checking PIR/PRF accession to name matching");
    CRef<CScope> scope = s_InitScope();
    if ( 1 ) {
        string id_str = "pir|C24693";
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id_str);
        BOOST_CHECK_EQUAL(idh.Which(), CSeq_id::e_Pir);
        auto bsh = scope->GetBioseqHandle(idh);
        BOOST_CHECK(bsh);
    }
    if ( 0 ) { // prf|accession aren't resolved by servers
        string id_str = "prf|2210278A";
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id_str);
        BOOST_CHECK_EQUAL(idh.Which(), CSeq_id::e_Pir);
        auto bsh = scope->GetBioseqHandle(idh);
        BOOST_CHECK(bsh);
    }
    if ( 0 ) { // prf|accession aren't resolved by servers
        string id_str = "prf|2211422D";
        CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id_str);
        BOOST_CHECK_EQUAL(idh.Which(), CSeq_id::e_Pir);
        auto bsh = scope->GetBioseqHandle(idh);
        BOOST_CHECK(bsh);
    }
}


NCBITEST_INIT_CMDLINE(arg_descrs)
{
    arg_descrs->AddFlag("psg",
        "Update all test cases to current reader code (dangerous).",
        true );
    arg_descrs->AddOptionalKey("id2-service", "ID2Service",
                               "Service name for ID2 connection.",
                               CArgDescriptions::eString);
    arg_descrs->AddOptionalKey("HUP-token", "HUPToken",
                               "Authorizatoin token for HUP access.",
                               CArgDescriptions::eString);
}


NCBITEST_INIT_TREE()
{
    auto app = CNcbiApplication::Instance();
    const CArgs& args = app->GetArgs();
    if ( args["psg"] ) {
#if !defined(HAVE_PSG_LOADER)
        LOG_POST("Skipping -psg tests without PSG loader");
        NcbiTestSetGlobalDisabled();
        return;
#endif
        app->GetConfig().Set("genbank", "loader_psg", "1");
    }
    if ( args["id2-service"] ) {
        app->GetConfig().Set("genbank/id2", "service", args["id2-service"].AsString());
    }
    if ( args["HUP-token"] ) {
        s_HUPToken = args["HUP-token"].AsString();
    }
    
    NCBITEST_DISABLE(CheckAll);
    NCBITEST_DISABLE(CheckExtHPRD);
    NCBITEST_DISABLE(CheckExtMGC);
    NCBITEST_DISABLE(CheckExtTRNA);
    NCBITEST_DISABLE(CheckExtTRNAEdit);
    NCBITEST_DISABLE(CheckExtMicroRNA);
    NCBITEST_DISABLE(CheckExtExon);
    NCBITEST_DISABLE(TestFallback1);
    NCBITEST_DISABLE(TestFallback2);
    NCBITEST_DISABLE(TestFallback3);
    NCBITEST_DISABLE(TestFallback3g);
    NCBITEST_DISABLE(TestFallback3f);
    NCBITEST_DISABLE(TestFallback4);
    NCBITEST_DISABLE(TestFallback5);
    NCBITEST_DISABLE(TestFallback7);
    NCBITEST_DISABLE(TestFallback8);

    if ( !SInvertVDB_CDD::IsPossible() ) {
        NCBITEST_DISABLE(CheckExtCDD2);
        NCBITEST_DISABLE(CheckExtCDD2onWGS);
    }
    if ( !s_HaveMongoDBCDD() ) {
        NCBITEST_DISABLE(CheckExtCDDonPDB);
    }
    if (CGBDataLoader::IsUsingPSGLoader()) {
        NCBITEST_DISABLE(CheckExtSTS);
    }
    if ( !s_HaveNA() ) {
        NCBITEST_DISABLE(CheckNAZoom10);
        NCBITEST_DISABLE(CheckNATable);
    }
    if ( !s_HaveSplit() ) {
        NCBITEST_DISABLE(CheckSplitSeqData);
    }
    if ( !CGBDataLoader::IsUsingPSGLoader() &&
         (!s_HaveID2(eExcludePubseqos2) || s_HaveCache()) ) {
        NCBITEST_DISABLE(TestGetBlobById);
    }
    if ( !s_HaveVDBSNP() ) {
        NCBITEST_DISABLE(CheckExtSNP);
        NCBITEST_DISABLE(CheckExtSNPEdit);
        NCBITEST_DISABLE(CheckExtSNPEditChangeId);
        NCBITEST_DISABLE(CheckExtSNP_Seq_feat);
        NCBITEST_DISABLE(CheckExtSNP_SeqFeatData);
        NCBITEST_DISABLE(CheckExtSNP_Imp_feat);
        NCBITEST_DISABLE(CheckExtSNP_Seq_loc);
        NCBITEST_DISABLE(CheckExtSNP_Seq_id);
        NCBITEST_DISABLE(CheckExtSNP_Seq_interval);
        NCBITEST_DISABLE(CheckExtSNP_Seq_point);
        NCBITEST_DISABLE(CheckExtSNP_Seq_loc);
        NCBITEST_DISABLE(CheckExtSNP_Gb_qual);
        NCBITEST_DISABLE(CheckExtSNP_Dbtag);
        NCBITEST_DISABLE(CheckExtSNP_Object_id);
        NCBITEST_DISABLE(CheckExtSNP_User_object);
        NCBITEST_DISABLE(CheckExtSNP_User_field);
        NCBITEST_DISABLE(CheckExtSNPGraph);
        //NCBITEST_DISABLE(CheckExtSNPNAGraph5000);
        //NCBITEST_DISABLE(CheckExtSNPNAGraph100);
        //NCBITEST_DISABLE(CheckExtSNPNA);
    }
    if ( !s_HaveVDBWGS() ) {
        NCBITEST_DISABLE(CheckWGSMasterDescr8);
        NCBITEST_DISABLE(CheckWGSMasterDescr9);
        NCBITEST_DISABLE(CheckWGSMasterDescr12);
        NCBITEST_DISABLE(CheckWGSMasterDescr13);
        NCBITEST_DISABLE(CheckWGSMasterDescr14);
        NCBITEST_DISABLE(CheckWGSFeat1);
    }
#if defined(NCBI_THREADS) && !defined(RUN_SLOW_MT_TESTS)
    NCBITEST_DISABLE(MTCrash1);
#endif
#if !defined(HAVE_PUBSEQ_OS) || (defined(NCBI_THREADS) && !defined(HAVE_SYBASE_REENTRANT))
    // HUP test needs multiple PubSeqOS readers
    NCBITEST_DISABLE(Test_HUP);
    // GBLoader name test needs multiple PubSeqOS readers
    NCBITEST_DISABLE(TestGBLoaderName);
#else
    if (CGBDataLoader::IsUsingPSGLoader()) {
        NCBITEST_DISABLE(Test_HUP);
    }
    else {
        string user_name = CSystemInfo::GetUserName();
        if ( user_name != "vasilche" && user_name != "coremake" ) {
            NCBITEST_DISABLE(Test_HUP);
        }
    }
#endif
}
