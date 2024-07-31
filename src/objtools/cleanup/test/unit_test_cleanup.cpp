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
*   Sample unit tests file for cleanup.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistd.hpp>

#include <corelib/test_boost.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/general/Object_id.hpp>

#include "../newcleanupp.hpp"


USING_NCBI_SCOPE;
USING_SCOPE(objects);


static CRef<CSeq_loc> s_CreateNewMix() 
{
    auto pLoc = Ref(new CSeq_loc());
    pLoc->SetMix();
    return pLoc;
}


BOOST_AUTO_TEST_CASE(Test_RW_2301)
{
    auto pNullLoc = Ref(new CSeq_loc());
    pNullLoc->SetNull();

    CNewCleanup_imp cleanup_imp(Ref(new CCleanupChange()));
    {
        // mix(null, null) -> null
        auto pLoc = Ref(new CSeq_loc());
        auto& mix = pLoc->SetMix();
        mix.AddSeqLoc(*pNullLoc);
        mix.AddSeqLoc(*pNullLoc);
        BOOST_CHECK_EQUAL(mix.Get().size(), 2);
        cleanup_imp.SeqLocBC(*pLoc);
        // Check that a mix consisting of 2 Nulls is converted to a Null loc
        BOOST_CHECK(pLoc->IsNull());
    }

    auto pId = Ref(new CSeq_id());
    pId->SetLocal().SetStr("dummyId");
    {
        // mix(mix(null, interval, null)) -> interval
        // Add an interval
        auto pLoc = s_CreateNewMix();
        auto pInnerMix = s_CreateNewMix();
        auto& innerMix = pInnerMix->SetMix();
        innerMix.AddSeqLoc(*pNullLoc);
        innerMix.AddInterval(*pId, 25, 1006);
        innerMix.AddSeqLoc(*pNullLoc);

        BOOST_CHECK_EQUAL(innerMix.Get().size(), 3);
        pLoc->SetMix().Set().push_back(pInnerMix);

        BOOST_CHECK_EQUAL(pLoc->GetMix().Get().size(), 1);
        cleanup_imp.SeqLocBC(*pLoc);
        BOOST_CHECK(pLoc->IsInt());
    }

    {
        // mix(interval, mix(null, interval), interval) -> mix(interval, null, interval, null, interval)
        auto pLoc = s_CreateNewMix();
        auto& mix = pLoc->SetMix();

        auto pInnerMix = s_CreateNewMix();
        auto& innerMix = pInnerMix->SetMix();
        pLoc->SetMix().AddInterval(*pId, 2, 20);
        innerMix.AddSeqLoc(*pNullLoc);
        innerMix.AddInterval(*pId, 22, 30);
        mix.Set().push_back(pInnerMix);
        mix.AddInterval(*pId, 32, 40);

        cleanup_imp.SeqLocBC(*pLoc);

        BOOST_CHECK(pLoc->IsMix());
        BOOST_CHECK_EQUAL(pLoc->GetMix().Get().size(), 5);
        auto it = pLoc->GetMix().Get().begin();
        BOOST_CHECK((*it)->IsInt());
        ++it;
        BOOST_CHECK((*it)->IsNull()); // The '*' is critical. Check for a null location, not an empty CRef.
        ++it;
        BOOST_CHECK((*it)->IsInt());
        ++it;
        BOOST_CHECK((*it)->IsNull()); // The '*' is critical. Check for a null location, not an empty CRef.
        ++it;
        BOOST_CHECK((*it)->IsInt());
    }
}
