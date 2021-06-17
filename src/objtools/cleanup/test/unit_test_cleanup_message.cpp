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
 * Author:  Justin Foley
 *
 * File Description:
 *   Unit tests for cleanup messages
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/test_boost.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objtools/cleanup/cleanup_message.hpp>
#include <objtools/cleanup/cleanup.hpp>
#include <objtools/logging/listener.hpp>
#include <objtools/unit_test_util/unit_test_util.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);

BOOST_AUTO_TEST_CASE(Test_ParseCodeBreakMessages)
{
    auto pEntry = unit_test_util::BuildGoodNucProtSet();
    auto pCds = unit_test_util::GetCDSFromGoodNucProtSet(pEntry);

    auto pScope = Ref(new CScope(*CObjectManager::GetInstance()));
    auto seh = pScope->AddTopLevelSeqEntry(*pEntry);


    CObjtoolsListener listener;
    string badString = ":3..5,aa:Met";
    BOOST_CHECK_EQUAL(CCleanup::ParseCodeBreak(*pCds, pCds->SetData().SetCdregion(), badString, *pScope, &listener), false);

    BOOST_CHECK_EQUAL(CCleanup::ParseCodeBreak(*pCds, pCds->SetData().SetCdregion(), "(pos:3..6,aa:Met)", *pScope, &listener), false);

    BOOST_CHECK_EQUAL(CCleanup::ParseCodeBreak(*pCds, pCds->SetData().SetCdregion(), "(pos:27..29,aa:Met)", *pScope, &listener), false);

    BOOST_CHECK_EQUAL(listener.Count(), 3);

    auto it = listener.begin();
    BOOST_CHECK_EQUAL(it->GetSubCode(), static_cast<int>(CCleanupMessage::ESubcode::eParseError));
    BOOST_CHECK_EQUAL(it->GetText(), "Unable to identify code-break location in ':3..5,aa:Met'");

    ++it;
    BOOST_CHECK_EQUAL(it->GetSubCode(), static_cast<int>(CCleanupMessage::ESubcode::eBadLocation));
    BOOST_CHECK_EQUAL(it->GetText(), "code-break location exceeds 3 bases");

    ++it;
    BOOST_CHECK_EQUAL(it->GetSubCode(), static_cast<int>(CCleanupMessage::ESubcode::eBadLocation));
    BOOST_CHECK_EQUAL(it->GetText(), "code-break location lies outside of coding region");
}


