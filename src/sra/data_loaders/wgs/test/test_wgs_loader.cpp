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
*   Unit tests for WGS data loader
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <sra/data_loaders/wgs/wgsloader.hpp>
#include <sra/readers/sra/wgsread.hpp>
#include <sra/readers/ncbi_traces_path.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/align_ci.hpp>
#include <objmgr/graph_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objects/seqalign/seqalign__.hpp>
#include <objects/seq/seq__.hpp>
#include <objects/seqset/seqset__.hpp>
#include <corelib/ncbi_system.hpp>
#include <objtools/readers/idmapper.hpp>
#include <serial/iterator.hpp>
#include <objmgr/util/sequence.hpp>

#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

#define PILEUP_NAME_SUFFIX " pileup graphs"

CRef<CObjectManager> sx_GetOM(void)
{
    SetDiagPostLevel(eDiag_Info);
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CObjectManager::TRegisteredNames names;
    om->GetRegisteredNames(names);
    ITERATE ( CObjectManager::TRegisteredNames, it, names ) {
        om->RevokeDataLoader(*it);
    }
    return om;
}

void sx_CheckNames(CScope& scope, const CSeq_loc& loc,
                   const string& name)
{
    SAnnotSelector sel;
    sel.SetSearchUnresolved();
    sel.SetCollectNames();
    CAnnotTypes_CI it(CSeq_annot::C_Data::e_not_set, scope, loc, &sel);
    CAnnotTypes_CI::TAnnotNames names = it.GetAnnotNames();
    ITERATE ( CAnnotTypes_CI::TAnnotNames, i, names ) {
        if ( i->IsNamed() ) {
            NcbiCout << "Named annot: " << i->GetName()
                     << NcbiEndl;
        }
        else {
            NcbiCout << "Unnamed annot"
                     << NcbiEndl;
        }
    }
    //NcbiCout << "Checking for name: " << name << NcbiEndl;
    BOOST_CHECK_EQUAL(names.count(name), 1u);
    if ( names.size() == 2 ) {
        BOOST_CHECK_EQUAL(names.count(name+PILEUP_NAME_SUFFIX), 1u);
    }
    else {
        BOOST_CHECK_EQUAL(names.size(), 1u);
    }
}

void sx_CheckSeq(CScope& scope,
                 const CSeq_id_Handle& main_idh,
                 const CSeq_id& id)
{
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(id);
    if ( idh == main_idh ) {
        return;
    }
    const CBioseq_Handle::TId& ids = scope.GetIds(idh);
    BOOST_REQUIRE_EQUAL(ids.size(), 1u);
    BOOST_CHECK_EQUAL(ids[0], idh);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
}

BOOST_AUTO_TEST_CASE(FetchSeq1)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "AAAA01000102";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), CBioseq_Handle::fState_suppress_perm);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Molinfo, 1));
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}

BOOST_AUTO_TEST_CASE(FetchSeq2)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "AAAA02000102.1";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Molinfo, 1));
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}

BOOST_AUTO_TEST_CASE(FetchSeq3)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "ref|NZ_AAAA01000102";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), CBioseq_Handle::fState_dead);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Molinfo, 1));
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}

BOOST_AUTO_TEST_CASE(FetchSeq4)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "ref|NZ_AAAA010000001";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}

BOOST_AUTO_TEST_CASE(FetchSeq5)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "ref|NZ_AAAA0100000001";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}

BOOST_AUTO_TEST_CASE(FetchSeq6)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "ref|ZZZZ01000001";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FetchSeq7)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "AAAA01010001";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CGBDataLoader::RegisterInObjectManager(*om, 0, CObjectManager::eNonDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), CBioseq_Handle::fState_suppress_perm);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Molinfo, 1));
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}


BOOST_AUTO_TEST_CASE(FetchSeq8)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "ALWZ01S31652451";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CGBDataLoader::RegisterInObjectManager(*om, 0, CObjectManager::eNonDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Title, 1));
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}

BOOST_AUTO_TEST_CASE(FetchSeq9)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "ALWZ0100000001";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CGBDataLoader::RegisterInObjectManager(*om, 0, CObjectManager::eNonDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Title, 1));
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}

BOOST_AUTO_TEST_CASE(FetchSeq10)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "ALWZ010000001";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CGBDataLoader::RegisterInObjectManager(*om, 0, CObjectManager::eNonDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FetchSeq11)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "NZ_ACUJ01000001";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Title, 1));
    BOOST_CHECK(!CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}


BOOST_AUTO_TEST_CASE(FetchSeq12)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "NZ_ACUJ01000001";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CGBDataLoader::RegisterInObjectManager(*om, 0, CObjectManager::eNonDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Title, 1));
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}


BOOST_AUTO_TEST_CASE(FetchSeq13)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "NZ_ACUJ01000001.3";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CGBDataLoader::RegisterInObjectManager(*om, 0, CObjectManager::eNonDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_REQUIRE(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);
    if ( 0 ) {
        NcbiCout << MSerial_AsnText << *bh.GetCompleteObject();
    }
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Title, 1));
    BOOST_CHECK(CSeqdesc_CI(bh, CSeqdesc::e_Pub, 1));
}


BOOST_AUTO_TEST_CASE(FetchSeq14)
{
    CRef<CObjectManager> om = sx_GetOM();

    string id = "NZ_ACUJ01000001.1";
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    CGBDataLoader::RegisterInObjectManager(*om, 0, CObjectManager::eNonDefault);
    CScope scope(*om);
    scope.AddDefaults();

    CRef<CSeq_id> seqid(new CSeq_id(id));
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*seqid);
    CBioseq_Handle bh = scope.GetBioseqHandle(idh);
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FetchSeq15)
{
    CRef<CObjectManager> om = sx_GetOM();
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault, 0);
    CGBDataLoader::RegisterInObjectManager(*om);

    CRef<CScope> scope(new CScope(*om));
    scope->AddDefaults();

    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle("gb|ABBA01000001.1|");
    CBioseq_Handle bsh = scope->GetBioseqHandle(idh);
    if ( !bsh ) {
        cerr << "failed to find sequence: " << idh << endl;
        CBioseq_Handle::TBioseqStateFlags flags = bsh.GetState();
        if (flags & CBioseq_Handle::fState_suppress_temp) {
            cerr << "  suppressed temporarily" << endl;
        }
        if (flags & CBioseq_Handle::fState_suppress_perm) {
            cerr << "  suppressed permanently" << endl;
        }
        if (flags & CBioseq_Handle::fState_suppress) {
            cerr << "  suppressed" << endl;
        }
        if (flags & CBioseq_Handle::fState_dead) {
            cerr << "  dead" << endl;
        }
        if (flags & CBioseq_Handle::fState_confidential) {
            cerr << "  confidential" << endl;
        }
        if (flags & CBioseq_Handle::fState_withdrawn) {
            cerr << "  withdrawn" << endl;
        }
        if (flags & CBioseq_Handle::fState_no_data) {
            cerr << "  no data" << endl;
        }
        if (flags & CBioseq_Handle::fState_conflict) {
            cerr << "  conflict" << endl;
        }
        if (flags & CBioseq_Handle::fState_not_found) {
            cerr << "  not found" << endl;
        }
        if (flags & CBioseq_Handle::fState_other_error) {
            cerr << "  other error" << endl;
        }
    }
    else {
        cerr << "found sequence: " << idh << endl;
        //cerr << MSerial_AsnText << *bsh.GetCompleteBioseq();
    }
    BOOST_CHECK(bsh);
}


static const bool s_MakeFasta = 0;


BOOST_AUTO_TEST_CASE(Scaffold2Fasta)
{
    CStopWatch sw(CStopWatch::eStart);
    CVDBMgr mgr;
    CWGSDb wgs_db(mgr, "ALWZ01");
    size_t limit_count = 30000, start_row = 1, count = 0;

    CRef<CObjectManager> om = sx_GetOM();
    CScope scope(*om);
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault, 0);
    scope.AddDefaults();

    auto_ptr<CNcbiOstream> out;
    auto_ptr<CFastaOstream> fasta;

    if ( s_MakeFasta ) {
        string outfile_name = "out.fsa";
        out.reset(new CNcbiOfstream(outfile_name.c_str()));
        fasta.reset(new CFastaOstream(*out));
    }

    for ( CWGSScaffoldIterator it(wgs_db, start_row); it; ++it ) {
        ++count;
        if (limit_count > 0 && count > limit_count)
            break;

        //scope.ResetHistory();
        CBioseq_Handle scaffold = scope.GetBioseqHandle(*it.GetAccSeq_id());
        if ( fasta.get() ) {
            fasta->Write(scaffold);
        }
    }
    NcbiCout << "Scanned in "<<sw.Elapsed() << NcbiEndl;
}


BOOST_AUTO_TEST_CASE(Scaffold2Fasta2)
{
    CStopWatch sw(CStopWatch::eStart);
    CVDBMgr mgr;
    CWGSDb wgs_db(mgr, "ALWZ01");
    size_t limit_count = 30000, start_row = 1, count = 0;

    CRef<CObjectManager> om = sx_GetOM();
    CScope scope(*om);
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault, 0);
    scope.AddDefaults();

    auto_ptr<CNcbiOstream> out;
    auto_ptr<CFastaOstream> fasta;

    if ( s_MakeFasta ) {
        string outfile_name = "out.fsa";
        out.reset(new CNcbiOfstream(outfile_name.c_str()));
        fasta.reset(new CFastaOstream(*out));
    }

    CScope::TIds ids;
    for ( CWGSScaffoldIterator it(wgs_db, start_row); it; ++it ) {
        ++count;
        if (limit_count > 0 && count > limit_count)
            break;

        ids.push_back(CSeq_id_Handle::GetHandle(*it.GetAccSeq_id()));
    }

    CScope::TBioseqHandles handles = scope.GetBioseqHandles(ids);
    ITERATE ( CScope::TBioseqHandles, it, handles ) {
        CBioseq_Handle scaffold = *it;
        if ( fasta.get() ) {
            fasta->Write(scaffold);
        }
    }

    NcbiCout << "Scanned in "<<sw.Elapsed() << NcbiEndl;
}


BOOST_AUTO_TEST_CASE(WithdrawnCheck)
{
    CRef<CObjectManager> om = sx_GetOM();
    CScope scope(*om);
    CWGSDataLoader::RegisterInObjectManager(*om, CObjectManager::eDefault);
    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP02000011.1"));
    BOOST_CHECK(bh);
    BOOST_CHECK_EQUAL(bh.GetState(), 0);

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP01000011.1"));
    BOOST_CHECK(bh);
    BOOST_CHECK_EQUAL(bh.GetState(),
                      CBioseq_Handle::fState_suppress_perm);

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AFFP01000012.1"));
    BOOST_CHECK(!bh);
    BOOST_CHECK_EQUAL(bh.GetState(),
                      CBioseq_Handle::fState_no_data |
                      CBioseq_Handle::fState_withdrawn);
}


BOOST_AUTO_TEST_CASE(TPGTest)
{
    CRef<CObjectManager> om = sx_GetOM();
    CScope scope(*om);

#ifdef NCBI_OS_DARWIN
    string wgs_root = "/net/pan1/id_dumps/WGS/tmp";
#else
    string wgs_root = "//panfs/pan1/id_dumps/WGS/tmp";
#endif
    CWGSDataLoader::RegisterInObjectManager(*om,  wgs_root, vector<string>(), CObjectManager::eDefault);
    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("DAAH01000001.1"));
    BOOST_CHECK(bh);
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AGKB01000001.1"));
    BOOST_CHECK(bh);
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AAAA01000001.1"));
    BOOST_CHECK(!bh);
}


BOOST_AUTO_TEST_CASE(FixedFileTest)
{
    CRef<CObjectManager> om = sx_GetOM();
    CScope scope(*om);

    vector<string> files;
#ifdef NCBI_OS_DARWIN
    string wgs_root = "/net/pan1/id_dumps/WGS/tmp";
#else
    string wgs_root = "//panfs/pan1/id_dumps/WGS/tmp";
#endif
    files.push_back(wgs_root+"/DAAH01");
    CWGSDataLoader::RegisterInObjectManager(*om,  "", files, CObjectManager::eDefault);
    scope.AddDefaults();

    CBioseq_Handle bh;

    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("DAAH01000001.1"));
    BOOST_CHECK(bh);
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AGKB01000001.1"));
    BOOST_CHECK(!bh);
    bh = scope.GetBioseqHandle(CSeq_id_Handle::GetHandle("AAAA01000001.1"));
    BOOST_CHECK(!bh);
}
