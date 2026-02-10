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
* Author:  Justin Foley, NCBI
*
* File Description:
*   tests for CHugeFileProcess
*
* ===========================================================================
*/
#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>

#include <corelib/ncbi_system.hpp>

// This macro should be defined before inclusion of test_boost.hpp in all
// "*.cpp" files inside executable except one. It is like function main() for
// non-Boost.Test executables is defined only in one *.cpp file - other files
// should not include it. If NCBI_BOOST_NO_AUTO_TEST_MAIN will not be defined
// then test_boost.hpp will define such "main()" function for tests.
//
// Usually if your unit tests contain only one *.cpp file you should not
// care about this macro at all.
//
//#define NCBI_BOOST_NO_AUTO_TEST_MAIN

#include <corelib/ncbistre.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objtools/huge_asn/huge_file.hpp>
#include <objtools/huge_asn/huge_asn_reader.hpp>
#include <objtools/huge_asn/huge_asn_loader.hpp>
#include <objtools/huge_asn/huge_file_process.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objmgr/feat_ci.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(edit);

// Needed under windows for some reason.
#ifdef BOOST_NO_EXCEPTIONS

namespace boost
{
void throw_exception( std::exception const & e ) {
    throw e;
}
}

#endif

BOOST_AUTO_TEST_CASE(Test_HugeFileProcess)
{
    string filename = "./huge_asn_test_files/rw-1974.asn";
    CRef<CHugeAsnReader> pReader;
    {
        CHugeFileProcess process;
        process.Open(filename);
        pReader.Reset(&process.GetReader());
    } // process is out of scope, but reader remains
    pReader->GetNextBlob();
    pReader->FlattenGenbankSet();
    BOOST_CHECK_EQUAL(pReader->GetTopIds().size(),3);
}


BOOST_AUTO_TEST_CASE(Test_RemoteSequences)
{   // RW-2308 - This test demonstrates why huge mode differs from traditional mode
    // in an usual validator corner case
    string filename = "./huge_asn_test_files/AL592437.sqn";
    CRef<CHugeAsnReader> pReader;
    {
        CHugeFileProcess process;
        process.Open(filename);
        pReader.Reset(&process.GetReader());
    }
    pReader->GetNextBlob();
    pReader->FlattenGenbankSet();

    { // Register Genbank data loader
        CGBLoaderParams params;
        CGBDataLoader::RegisterInObjectManager(
                *(CObjectManager::GetInstance()), 
                params,
                CObjectManager::eDefault,
                16000);
    }

    // Feature locations in the input file reference this Genbank record
    // However, this record doesn't have any features.
    auto pRemoteId = Ref(new CSeq_id());
    pRemoteId->SetGi(GI_CONST(17017835));

    {   
        // Register huge file data loader
        edit::CHugeAsnDataLoader::RegisterInObjectManager(
            *(CObjectManager::GetInstance()), filename, pReader.GetPointer(), CObjectManager::eNonDefault, 1); 


        auto pScope = Ref(new CScope(*(CObjectManager::GetInstance())));
        pScope->AddDefaults();
        pScope->AddDataLoader(filename);

        auto bsh = pScope->GetBioseqHandle(*pRemoteId);
        CFeat_CI cit(bsh);
        BOOST_CHECK(!cit); // No feature found
    }

    // In this case, CFeat_CI(bsh) returns a valid iterator.
    {
        auto pEntry = Ref(new CSeq_entry());
        CNcbiIfstream ifstr(filename);
        ifstr >> MSerial_AsnText >> *pEntry;

        auto pScope = Ref(new CScope(*(CObjectManager::GetInstance())));
        pScope->AddDefaults();
        pScope->AddTopLevelSeqEntry(*pEntry);

        // retrieve the bioseq handle for 17017835
        auto bsh = pScope->GetBioseqHandle(*pRemoteId);
        CFeat_CI cit(bsh);
        BOOST_CHECK(cit); // Feature found
    }
}

BOOST_AUTO_TEST_CASE(Test_NullPointerException)
{
    string filename = "./huge_asn_test_files/congo.asn";
    CRef<CHugeAsnReader> pReader;
    {   
        CHugeFileProcess process;
        process.Open(filename);
        pReader.Reset(&process.GetReader());
    }   
    pReader->GetNextBlob();
    pReader->FlattenGenbankSet();
    // Register huge file data loader
    edit::CHugeAsnDataLoader::RegisterInObjectManager(
            *(CObjectManager::GetInstance()), filename, pReader.GetPointer(), CObjectManager::eNonDefault, 1); 
    
    auto pScope = Ref(new CScope(*(CObjectManager::GetInstance())));
    pScope->AddDataLoader(filename);
   
    auto pId = Ref(new CSeq_id());
    pId->SetLocal().SetStr("ex1");

    auto bsh = pScope->GetBioseqHandle(*pId);
    BOOST_REQUIRE(bsh);
    auto pEntry = bsh.GetTopLevelEntry().GetCompleteSeq_entry();
    BOOST_CHECK(pEntry);

    BOOST_CHECK(pScope->GetSeq_entryHandle(*pEntry));

    BOOST_CHECK(pScope->GetSeq_entryHandle(*pEntry).GetEditHandle());

    BOOST_CHECK(!pScope->GetSeq_entryHandle(*pEntry, CScope::eMissing_Null));
    BOOST_CHECK_THROW(pScope->GetSeq_entryHandle(*pEntry), CObjMgrException);
}
