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
*   Sample unit tests file for basic cleanup.
*
* ===========================================================================
*/

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


// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>
#include <objtools/format/flat_file_config.hpp>
#include <objtools/format/gather_items.hpp>
#include <objtools/format/item_ostream.hpp>
#include <objtools/format/ostream_text_ostream.hpp>
#include <objtools/format/format_item_ostream.hpp>
#include <objtools/format/item_formatter.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_entry_handle.hpp>

#include <objtools/unit_test_util/unit_test_util.hpp>

/*
#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objects/seqfeat/Gb_qual.hpp>

#include <serial/objistrasn.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/stream.hpp>
#include <objmgr/util/objutil.hpp>
*/

USING_NCBI_SCOPE;
USING_SCOPE(objects);

using namespace unit_test_util;

// Needed under windows for some reason.
#ifdef BOOST_NO_EXCEPTIONS

namespace boost
{
void throw_exception( std::exception const & e ) {
    throw e;
}
}

#endif

BOOST_AUTO_TEST_CASE(Test_FlatFileGatherer)
{
    ostringstream ostr;
    CRef<COStreamTextOStream> pTextOs(new COStreamTextOStream(ostr)); 
    CRef<CFlatItemOStream> pItemOs(new CFormatItemOStream(pTextOs));
    CRef<CFlatItemFormatter> pFormatter(CFlatItemFormatter::New(CFlatFileConfig::eFormat_GenBank));

    CRef<CFlatGatherer> pGatherer(CFlatGatherer::New(CFlatFileConfig::eFormat_GenBank));

    CFlatFileConfig config;
    auto pContext = Ref(new CFlatFileContext(config));

    pFormatter->SetContext(*pContext);
    pItemOs->SetFormatter(pFormatter);
    auto pEntry = BuildGoodNucProtSet();
    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));
    auto seh = pScope->AddTopLevelSeqEntry(*pEntry);
   
    pContext->SetEntry(seh);
    pGatherer->Gather(*pContext, *pItemOs);
    BOOST_CHECK(NStr::Find(ostr.str(), "source") != NPOS);
    BOOST_CHECK(NStr::Find(ostr.str(), "CDS") != NPOS);
}


