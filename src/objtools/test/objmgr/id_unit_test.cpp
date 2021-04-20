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
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/genbank/readers.hpp>
#include <objtools/data_loaders/genbank/id1/reader_id1.hpp>
#include <objtools/data_loaders/genbank/impl/psg_loader_impl.hpp>

#include <corelib/ncbi_system.hpp>
#include <corelib/ncbiapp.hpp>
#include <dbapi/driver/drivers.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>
#include <algorithm>

//#define RUN_MT_TESTS
#if defined(RUN_MT_TESTS) && defined(NCBI_THREADS)
# include <objtools/simple/simple_om.hpp>
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


bool s_HaveID2(void)
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
    return true;
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
                idh = CSeq_id_Handle::GetGiHandle(idh.GetGi() + gi_offset);
            }
            req_ids.push_back(idh);
        }
    }

    CScope::TIds ids = s_Normalize(scope->GetIds(idh));
    s_CompareIdSets("expected with get-ids",
                    req_ids, ids, true);

    CBioseq_Handle bh = scope->GetBioseqHandle(idh);
    BOOST_CHECK(bh);
    //s_Print("Actual ", bh.GetId());
    s_CompareIdSets("get-ids with Bioseq",
                    ids, s_Normalize(bh.GetId()), true);
}


void s_CheckSequence(const SBioseqInfo& info, CScope* scope)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(info.m_RequestId);
    CBioseq_Handle bh = scope->GetBioseqHandle(idh);
    BOOST_CHECK(bh);
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


void s_CheckFeat(const SAnnotSelector& sel,
                 const string& str_id,
                 CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
    s_CheckFeat(s_InitScope(), sel, str_id, range);
}


void s_CheckGraph(const SAnnotSelector& sel,
                  const string& str_id,
                  CRange<TSeqPos> range = CRange<TSeqPos>::GetWhole())
{
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
    BOOST_CHECK(CGraph_CI(*scope, *loc, sel));

    CBioseq_Handle bh = scope->GetBioseqHandle(*seq_id);
    BOOST_REQUIRE(bh);
    BOOST_CHECK(CGraph_CI(bh, range, sel));
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
    s_CheckFeat(sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
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
    s_CheckFeat(scope, sel, id);
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
    SAnnotSelector sel(CSeq_annot::C_Data::e_Graph);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.IncludeNamedAnnotAccession("SNP");
    sel.AddNamedAnnots("SNP");
    s_CheckGraph(sel, "NC_000001.10", CRange<TSeqPos>(249200000, 249210000));
}


static const char kNativeCDDSection[] = "GENBANK";
static const char kNativeCDDParam[] = "vdb_cdd";
static string s_SavedNativeCDD;

static string s_SetNativeCDDParam(const string& value)
{
    auto app = CNcbiApplication::InstanceGuard();
    string old_value = app->GetConfig().Get(kNativeCDDSection, kNativeCDDParam);
    if ( value.empty() ) {
        app->GetConfig().Unset(kNativeCDDSection, kNativeCDDParam);
    }
    else {
        app->GetConfig().Set(kNativeCDDSection, kNativeCDDParam, value);
    }
    return old_value;
}

static void s_SetNativeCDD(bool native = true)
{
    s_SavedNativeCDD = s_SetNativeCDDParam(native? "0": "1");
}

static void s_RestoreCDDType()
{
    s_SetNativeCDDParam(s_SavedNativeCDD);
}

BOOST_AUTO_TEST_CASE(CheckExtCDD)
{
    LOG_POST("Checking ExtAnnot ID CDD");
    s_SetNativeCDD();
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "AAC73113.1");
    s_RestoreCDDType();
}


BOOST_AUTO_TEST_CASE(CheckExtCDD2)
{
    LOG_POST("Checking ExtAnnot VDB CDD");
    s_SetNativeCDD(false);
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "AAC73113.1");
    s_RestoreCDDType();
}


BOOST_AUTO_TEST_CASE(CheckExtCDDonWGS)
{
    LOG_POST("Checking ExtAnnot ID CDD on WGS sequence");
    s_SetNativeCDD();
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "RLL67630.1");
    s_RestoreCDDType();
}


BOOST_AUTO_TEST_CASE(CheckExtCDD2onWGS)
{
    LOG_POST("Checking ExtAnnot VDB CDD on WGS sequence");
    s_SetNativeCDD(false);
    SAnnotSelector sel(CSeqFeatData::eSubtype_region);
    sel.SetResolveAll().SetAdaptiveDepth();
    sel.AddNamedAnnots("CDD");
    s_CheckFeat(sel, "RLL67630.1");
    s_RestoreCDDType();
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
    if (CGBDataLoader::IsUsingPSGLoader()) {
        LOG_POST("Skipping ExtAnnot STS test with PSG data loader");
        return;
    }
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
    bool have_na = s_HaveID2();
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
    if ( !s_HaveID2() ) {
        LOG_POST("Skipping NA Graph Track test without ID2");
        return;
    }
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
        s_CheckGraph(sel, id);
    }
}


BOOST_AUTO_TEST_CASE(CheckSuppressedGene)
{
    LOG_POST("Checking features with suppressed gene");
    SAnnotSelector sel;
    sel.SetResolveAll().SetAdaptiveDepth();
    auto scope = s_InitScope();
    BOOST_CHECK_EQUAL(s_CheckFeat(scope, sel, "33347893", CRange<TSeqPos>(49354, 49354)), 8u);
    sel.SetExcludeIfGeneIsSuppressed();
    BOOST_CHECK_EQUAL(s_CheckFeat(scope, sel, "33347893", CRange<TSeqPos>(49354, 49354)), 6u);
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
 
}


BOOST_AUTO_TEST_CASE(Test_HUP)
{
    if (CGBDataLoader::IsUsingPSGLoader()) {
        LOG_POST("Skipping HUP test with PSG data loader");
        return;
    }
    bool authorized;
    string user_name = CSystemInfo::GetUserName();
    if ( user_name == "vasilche" ) {
        authorized = true;
    }
    else if ( user_name == "coremake" ) {
        authorized = false;
    }
    else {
        LOG_POST("Skipping HUP access for unknown user");
        return;
    }
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
    string gb_hup = CGBDataLoader::RegisterInObjectManager(*objmgr, CGBDataLoader::eIncludeHUP).GetLoader()->GetName();
    BOOST_REQUIRE_EQUAL(gb_hup, "GBLOADER-HUP");

    CSeq_id_Handle id_main = CSeq_id_Handle::GetHandle("NT_077402");
    CSeq_id_Handle id_hup = CSeq_id_Handle::GetHandle("AY263392");
    {{
        CScope scope(*objmgr);
        scope.AddDataLoader(gb_hup);
        if ( authorized ) {
            BOOST_CHECK(scope.GetBioseqHandle(id_hup));
        }
        else {
            BOOST_CHECK(!scope.GetBioseqHandle(id_hup));
        }
        BOOST_CHECK(scope.GetBioseqHandle(id_main));
    }}

    {{
        CScope scope(*objmgr);
        scope.AddDefaults();
        
        BOOST_CHECK(!scope.GetBioseqHandle(id_hup));
        scope.AddDataLoader(gb_hup);
        if ( authorized ) {
            BOOST_CHECK(scope.GetBioseqHandle(id_hup));
        }
        else {
            BOOST_CHECK(!scope.GetBioseqHandle(id_hup));
        }
        BOOST_CHECK(scope.GetBioseqHandle(id_main));
        scope.RemoveDataLoader(gb_hup);
        BOOST_CHECK(!scope.GetBioseqHandle(id_hup));
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
    BOOST_CHECK(tses.size() > 1);
    size_t size = 0;
    for ( auto& tse : tses ) {
        size += CFeat_CI(tse.GetTopLevelEntry(), sel).GetSize();
    }
    BOOST_CHECK(size == 0);
    sel.SetFeatSubtype(CSeqFeatData::eSubtype_region);
    for ( auto& tse : tses ) {
        size += CFeat_CI(tse.GetTopLevelEntry(), sel).GetSize();
    }
    BOOST_CHECK(size > 0);
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
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK_EQUAL(pub_count, 3);
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Genbank));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 2u);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
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
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    if ( !s_HaveID1() ) { // TODO: ID1 returns 1 StructuredComment
        BOOST_CHECK_EQUAL(user_count["StructuredComment"], 2);
    }
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["RefGeneTracking"], 1);
    BOOST_CHECK_EQUAL(user_count["FeatureFetchPolicy"], 1);
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
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    if ( !s_HaveID1() ) { // TODO: ID1 returns 2 FeatureFetchPolicy
        BOOST_CHECK_EQUAL(user_count["FeatureFetchPolicy"], 1);
    }
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
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 1u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
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
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Embl));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 4u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 2);
    BOOST_CHECK_EQUAL(user_count["RefGeneTracking"], 1);
    BOOST_CHECK_EQUAL(user_count["FeatureFetchPolicy"], 1);
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
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Embl));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 4u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(user_count["RefGeneTracking"], 1);
    BOOST_CHECK_EQUAL(user_count["FeatureFetchPolicy"], 1);
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
    for ( CSeqdesc_CI it(bh); it; ++it ) {
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
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Create_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_Update_date));
    BOOST_CHECK(desc_mask & (1<<CSeqdesc::e_User));
    BOOST_CHECK_EQUAL(user_count.size(), 3u);
    BOOST_CHECK_EQUAL(user_count["DBLink"], 1);
    BOOST_CHECK_EQUAL(user_count["StructuredComment"], 1);
    BOOST_CHECK_EQUAL(user_count["Unverified"], 1);
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
    if ( !s_HaveID2() ) {
        LOG_POST("Skipping check of split Seq-data access without ID2");
        return;
    }
    LOG_POST("Checking split Seq-data access");
    CRef<CScope> scope = s_InitScope();
    s_CheckSplitSeqData(*scope, "NC_000001.11", eInst_ext);
    s_CheckSplitSeqData(*scope, "568815361", eInst_ext);
    s_CheckSplitSeqData(*scope, "24210292", eInst_ext);
    s_CheckSplitSeqData(*scope, "499533515", eInst_data);
    s_CheckSplitSeqData(*scope, "10086043", eInst_split_data);
    s_CheckSplitSeqData(*scope, "8894296", eInst_split_data);
}


#if defined(RUN_MT_TESTS) && defined(NCBI_THREADS)
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
    LOG_POST("Testing CDataLoader::GetBlobById()");
    bool is_psg = CGBDataLoader::IsUsingPSGLoader();
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    om->RevokeAllDataLoaders();
    CRef<CReader> reader;
    if ( is_psg ) {
        CGBDataLoader::RegisterInObjectManager(*om);
    }
    else {
        CGBLoaderParams params;
        reader = new CId1Reader();
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


NCBITEST_INIT_TREE()
{
    NCBITEST_DISABLE(CheckAll);
    NCBITEST_DISABLE(CheckExtHPRD);
    NCBITEST_DISABLE(CheckExtMGC);
    NCBITEST_DISABLE(CheckExtTRNA);
    NCBITEST_DISABLE(CheckExtTRNAEdit);
    NCBITEST_DISABLE(CheckExtMicroRNA);
    NCBITEST_DISABLE(CheckExtExon);
#if !defined(HAVE_PUBSEQ_OS) || (defined(NCBI_THREADS) && !defined(HAVE_SYBASE_REENTRANT))
    // HUP test needs multiple PubSeqOS readers
    NCBITEST_DISABLE(Test_HUP);
    // GBLoader name test needs multiple PubSeqOS readers
    NCBITEST_DISABLE(TestGBLoaderName);
#endif
    NCBITEST_DISABLE(TestGetBlobById);
}
