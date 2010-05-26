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
 *   LDS data loader and indexer unit test.
 *
 */

#include <ncbi_pch.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objtools/lds/lds_manager.hpp>
#include <objtools/data_loaders/lds/lds_dataloader.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>

#include <corelib/test_boost.hpp>
#include <boost/current_function.hpp>

#include <common/test_assert.h>  /* This header must go last */


#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CNonException : public exception {
};

#define BOOST_REQUIRE_CUTPOINT(X) if (cutpoint == X) throw CNonException()

#define DATA_DIR "data"
#define LDS_DIR "_LDS_"
#define LDS_DB_DIR "_LDS_/LDS"

static void s_InitLDSDir()
{
    CDir dir(LDS_DIR);
    dir.Remove();
    BOOST_REQUIRE(dir.Create());
}

static void s_RemoveFile(const string& name)
{
    CDirEntry file(CDirEntry::MakePath(LDS_DIR, name));
    BOOST_REQUIRE(file.Exists());
    BOOST_REQUIRE(file.Remove());
}

static void s_CopyFile(const string& name)
{
    CDirEntry file(CDirEntry::MakePath(DATA_DIR, name));
    BOOST_REQUIRE(file.Exists());
    BOOST_REQUIRE(file.CopyToDir(LDS_DIR));
}

static void s_TouchFile(const string& name)
{
    CDirEntry file(CDirEntry::MakePath(LDS_DIR, name));
    BOOST_REQUIRE(file.Exists());
    BOOST_REQUIRE(file.Remove());
    s_CopyFile(name);
}

static void s_MakeLDS()
{
    CLDS_Manager manager(LDS_DIR, LDS_DB_DIR);
    manager.Index();
}

static int s_RetrieveFeat(int gi, CObjectManager& om, const string& lds_name)
{
    CScope scope(om);
    scope.AddDefaults();
    scope.AddDataLoader(lds_name, 88);
    CBioseq_Handle bsh =
        scope.GetBioseqHandle(CSeq_id_Handle::GetGiHandle(gi));
    BOOST_REQUIRE(bsh);
    int count = 0;
    for ( CFeat_CI it(bsh); it; ++it ) {
        count += 1;
    }
    return count;
}

static int s_RetrieveFeat(int gi)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    
    CLDS_Manager manager(LDS_DB_DIR);
    auto_ptr<CLDS_Database> lds_db(manager.ReleaseDB());
    
    string lds_name = CLDS_DataLoader::RegisterInObjectManager(*om, *lds_db)
        .GetLoader()->GetName();
    
    try {
        int count = s_RetrieveFeat(gi, *om, lds_name);
        om->RevokeDataLoader(lds_name);
        return count;
    }
    catch (...) {
        om->RevokeDataLoader(lds_name);
        throw;
    }
}

static int s_RetrieveAlign(int gi, CObjectManager& om, const string& lds_name)
{
    CScope scope(om);
    scope.AddDefaults();
    scope.AddDataLoader(lds_name, 88);
    CBioseq_Handle bsh =
        scope.GetBioseqHandle(CSeq_id_Handle::GetGiHandle(gi));
    BOOST_REQUIRE(bsh);
    int count = 0;
    for ( CAlign_CI it(bsh); it; ++it ) {
        count += 1;
    }
    return count;
}

static int s_RetrieveAlign(int gi)
{
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*om);
    
    CLDS_Manager manager(LDS_DB_DIR);
    auto_ptr<CLDS_Database> lds_db(manager.ReleaseDB());
    
    string lds_name = CLDS_DataLoader::RegisterInObjectManager(*om, *lds_db)
        .GetLoader()->GetName();
    
    try {
        int count = s_RetrieveAlign(gi, *om, lds_name);
        om->RevokeDataLoader(lds_name);
        return count;
    }
    catch (...) {
        om->RevokeDataLoader(lds_name);
        throw;
    }
}

BOOST_AUTO_TEST_CASE(LDS_Test_align)
{
    s_InitLDSDir();
    s_CopyFile("lds_align.asn");
    s_MakeLDS();
    BOOST_REQUIRE(s_RetrieveAlign(2) == 3);
    s_TouchFile("lds_align.asn");
    s_MakeLDS();
    BOOST_REQUIRE(s_RetrieveAlign(2) == 3);
}

BOOST_AUTO_TEST_CASE(LDS_Test_annot)
{
    s_InitLDSDir();
    s_CopyFile("lds_annot.asn");
    s_MakeLDS();
    BOOST_REQUIRE(s_RetrieveAlign(2) == 1);
    s_TouchFile("lds_annot.asn");
    s_MakeLDS();
    BOOST_REQUIRE(s_RetrieveAlign(2) == 1);
}

BOOST_AUTO_TEST_CASE(LDS_Test_entry)
{
    s_InitLDSDir();
    s_CopyFile("lds_entry.asn");
    s_MakeLDS();
    BOOST_REQUIRE(s_RetrieveAlign(2) == 0);
    s_TouchFile("lds_entry.asn");
    s_MakeLDS();
    BOOST_REQUIRE(s_RetrieveAlign(2) == 0);
}

BOOST_AUTO_TEST_CASE(LDS_Test_conflict)
{
    // Temporary disabled tests - the existing LDS implementation
    // is not aware of external/orphan annotations and loads
    // all of them even from conflicting TSEs.
    s_InitLDSDir();
    s_CopyFile("lds_conflict1.asn");
    s_MakeLDS();
    s_CopyFile("lds_conflict2.asn");
    s_MakeLDS();
    BOOST_REQUIRE(s_RetrieveFeat(5) == 3);
    BOOST_REQUIRE(s_RetrieveFeat(6) == 1);
    s_RemoveFile("lds_conflict1.asn");
    s_MakeLDS();
    BOOST_REQUIRE(s_RetrieveFeat(5) == 3);
    BOOST_REQUIRE(s_RetrieveFeat(6) == 1);
    s_CopyFile("lds_conflict1.asn");
    s_MakeLDS();
    BOOST_REQUIRE(s_RetrieveFeat(5) == 4);
    BOOST_REQUIRE(s_RetrieveFeat(6) == 2);
}

#endif /* SKIP_DOXYGEN_PROCESSING */
