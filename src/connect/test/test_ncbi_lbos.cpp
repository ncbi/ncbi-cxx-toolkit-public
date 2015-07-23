/* $Id$
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
 * Author:  Dmitriy Elisov
 *
 * File Description:
 *   LBOS mapper tests
 *
 */

#include <ncbi_pch.hpp>
#include "test_ncbi_lbos_common.hpp"

#ifdef LBOS_TEST_MT
#undef     NCBITEST_CHECK_MESSAGE
#define    NCBITEST_CHECK_MESSAGE( P, M )  NCBI_ALWAYS_ASSERT(P,M)
#else  /* if LBOS_TEST_MT not defined */
    /* This header must be included before all Boost.Test headers if there
       are any                                                               */
    #include    <corelib/test_boost.hpp>
#endif /* #ifdef LBOS_TEST_MT */

USING_NCBI_SCOPE;


NCBITEST_INIT_TREE()
{
    /* Template to skip tests (and for easy navigation with "go to definition"
     * function of your IDE). Commenting all lines is usually
     * necessary. If something is uncommented for unknown reason, comment
     * it now! */

      /* Compose LBOS address */
//   NCBITEST_DISABLE(g_LBOS_ComposeLBOSAddress__LBOSExists__ShouldReturnLbos);
//   NCBITEST_DISABLE(g_LBOS_ComposeLBOSAddress__RoleFail__ShouldReturnNULL);
//   NCBITEST_DISABLE(g_LBOS_ComposeLBOSAddress__DomainFail__ShouldReturnNULL);
//   /* Reset iterator */
//   NCBITEST_DISABLE(SERV_Reset__NoConditions__IterContainsZeroCandidates);
//   NCBITEST_DISABLE(SERV_Reset__MultipleReset__ShouldNotCrash);
//   NCBITEST_DISABLE(SERV_Reset__Multiple_AfterGetNextInfo__ShouldNotCrash);
//   /* Close iterator */
//   NCBITEST_DISABLE(SERV_CloseIter__AfterOpen__ShouldWork);
//   NCBITEST_DISABLE(SERV_CloseIter__AfterReset__ShouldWork);
//   NCBITEST_DISABLE(SERV_CloseIter__AfterGetNextInfo__ShouldWork);
//   NCBITEST_DISABLE(SERV_CloseIter__FullCycle__ShouldWork);
//   /* Resolve via LBOS */
//   NCBITEST_DISABLE(s_LBOS_ResolveIPPort__ServiceExists__ReturnHostIP);
//   NCBITEST_DISABLE(s_LBOS_ResolveIPPort__ServiceDoesNotExist__ReturnNULL);
//   NCBITEST_DISABLE(s_LBOS_ResolveIPPort__NoLBOS__ReturnNULL);
//   NCBITEST_DISABLE(s_LBOS_ResolveIPPort__FakeMassiveInput__ShouldProcess);
//   NCBITEST_DISABLE(s_LBOS_ResolveIPPort__FakeErrorInput__ShouldNotCrash);
//   /* Get LBOS address */
//   NCBITEST_DISABLE(g_LBOS_GetLBOSAddresses__SpecificMethod__FirstInResult);
//   //NCBITEST_DISABLE(
//           g_LBOS_GetLBOSAddresses__CustomHostNotProvided__SkipCustomHost);
//   CBITEST_DISABLE(g_LBOS_GetLBOSAddresses__NoConditions__AddressDefOrder);
//   /* Get candidates */
//   NCBITEST_DISABLE(s_LBOS_FillCandidates__LBOSNoResponse__SkipLBOS);
//   NCBITEST_DISABLE(s_LBOS_FillCandidates__LBOSResponse__Finish);
//   //NCBITEST_DISABLE(s_LBOS_FillCandidates__NetInfoProvided__UseNetInfo);
//   /* GetNextInfo */
//   CBITEST_DISABLE(SERV_GetNextInfoEx__EmptyCands__RunGetCandidates);
//   NCBITEST_DISABLE(SERV_GetNextInfoEx__ErrorUpdating__ReturnNull);
//   NCBITEST_DISABLE(SERV_GetNextInfoEx__HaveCands__ReturnNext);
//   NCBITEST_DISABLE(SERV_GetNextInfoEx__LastCandReturned__ReturnNull);
//   NCBITEST_DISABLE(SERV_GetNextInfoEx__DataIsNull__ReconstructData);
//   NCBITEST_DISABLE(SERV_GetNextInfoEx__IterIsNull__ReconstructData);
//   NCBITEST_DISABLE(SERV_GetNextInfoEx__WrongMapper__ReturnNull);
//   /* Open */
//   NCBITEST_DISABLE(SERV_LBOS_Open__IterIsNull__ReturnNull);
//   CBITEST_DISABLE(SERV_LBOS_Open__NetInfoNull__ReturnNull);
//   NCBITEST_DISABLE(SERV_LBOS_Open__ServerExists__ReturnLbosOperations);
//   NCBITEST_DISABLE(SERV_LBOS_Open__InfoPointerProvided__WriteNull);
//   NCBITEST_DISABLE(SERV_LBOS_Open__NoSuchService__ReturnNull);
//   /*General LBOS*/
//   NCBITEST_DISABLE(SERV_OpenP__ServerExists__ShouldReturnLbosOperations);
//   NCBITEST_DISABLE(TestLbos_OpenP__ServerDoesNotExist__ShouldReturnNull);
//   NCBITEST_DISABLE(TestLbos_FindMethod__LbosExist__ShouldWork);
//   /* Announcement / deannouncement */
//   //NCBITEST_DISABLE(AnnouncementDeannouncement__Announce__FindMyself);
//   /* Stability */
//   NCBITEST_DISABLE(Stability__GetNext_Reset__ShouldNotCrash);
//   NCBITEST_DISABLE(Stability__FullCycle__ShouldNotCrash);
//   /* Performance */
//    NCBITEST_DISABLE(Performance__FullCycle__ShouldNotCrash);
//   /*Multi-threading*/
//   NCBITEST_DISABLE(MultiThreading_test1);
}


NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
            "LBOS mapper Unit Test");
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    CONNECT_Init(dynamic_cast<ncbi::IRWRegistry*>(&config));
}


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Compose_LBOS_address )//////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Should try only one current zone
 * 2. If could not read files, give up this industry and return NULL
 * 3. If unexpected content, return NULL
 * 4. If nothing found, return NULL
 */

/* Composing LBOS address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should try only one current zone and return it  */
BOOST_AUTO_TEST_CASE(g_LBOS_ComposeLBOSAddress__LBOSExists__ShouldReturnLbos)
{
    Compose_LBOS_address::LBOSExists__ShouldReturnLbos();
}

/* Composing LBOS address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on ZONE  */
BOOST_AUTO_TEST_CASE(g_LBOS_ComposeLBOSAddress__RoleFail__ShouldReturnNULL)
{
    Compose_LBOS_address::RoleFail__ShouldReturnNULL();
}

/* Composing LBOS address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on DOMAIN  */
BOOST_AUTO_TEST_CASE(g_LBOS_ComposeLBOSAddress__DomainFail__ShouldReturnNULL)
{
    Compose_LBOS_address::DomainFail__ShouldReturnNULL();
}

BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Reset_iterator)/////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Should make capacity of elements in data->cand equal zero
 * 2. Should be able to reset iter N times consequently without crash
 * 3. Should be able to "reset iter, then getnextinfo" N times consequently
 *    without crash
 */

BOOST_AUTO_TEST_CASE(SERV_Reset__NoConditions__IterContainsZeroCandidates)
{
    Reset_iterator::NoConditions__IterContainsZeroCandidates();
}


BOOST_AUTO_TEST_CASE(SERV_Reset__MultipleReset__ShouldNotCrash)
{
    Reset_iterator::MultipleReset__ShouldNotCrash();
}


BOOST_AUTO_TEST_CASE(SERV_Reset__Multiple_AfterGetNextInfo__ShouldNotCrash)
{
    Reset_iterator::Multiple_AfterGetNextInfo__ShouldNotCrash();
}

BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Close_iterator)/////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Should work immediately after Open
 * 2. Should work immediately after Reset
 * 3. Should work immediately after Open, GetNextInfo
 * 4. Should work immediately after Open, GetNextInfo, Reset
 */
BOOST_AUTO_TEST_CASE(SERV_CloseIter__AfterOpen__ShouldWork)
{
    Close_iterator::AfterOpen__ShouldWork();
}

BOOST_AUTO_TEST_CASE(SERV_CloseIter__AfterReset__ShouldWork)
{
    Close_iterator::AfterReset__ShouldWork();
}

BOOST_AUTO_TEST_CASE(SERV_CloseIter__AfterGetNextInfo__ShouldWork)
{
    Close_iterator::AfterGetNextInfo__ShouldWork();
}

/* In this test we check three different situations: when getnextinfo was
 * called once, was called to see half of all found servers, and was called to
 * iterate through all found servers. */
BOOST_AUTO_TEST_CASE(SERV_CloseIter__FullCycle__ShouldWork)
{
    Close_iterator::FullCycle__ShouldWork();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Resolve_via_LBOS )//////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Should return string with IP:port if OK
 * 2. Should return NULL if LBOS answered "not found"
 * 3. Should return NULL if LBOS is not reachable
 * 4. Should be able to support up to M IP:port combinations (not checking for
 *    repeats) with storage overhead not more than same as size needed (that
 *    is, all space consumed is twice as size needed, used and unused space
 *    together)
 * 5. Should be able to correctly process incorrect LBOS output. If from 5
 *    servers one is not valid, 4 other servers should be still returned
 *    by mapper
 */

BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__ServiceExists__ReturnHostIP)
{
    Resolve_via_LBOS::ServiceExists__ReturnHostIP();
}


BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__ServiceDoesNotExist__ReturnNULL)
{
    Resolve_via_LBOS::ServiceDoesNotExist__ReturnNULL();
}

BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__NoLBOS__ReturnNULL)
{
    Resolve_via_LBOS::NoLBOS__ReturnNULL();
}

BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__FakeMassiveInput__ShouldProcess)
{
    Resolve_via_LBOS::FakeMassiveInput__ShouldProcess();
}


BOOST_AUTO_TEST_CASE(s_LBOS_ResolveIPPort__FakeErrorInput__ShouldNotCrash)
{
    Resolve_via_LBOS::FakeErrorInput__ShouldNotCrash();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Compose_LBOS_address )//////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Should try only one current zone
 * 2. If could not read files, give up this industry and return NULL
 * 3. If unexpected content, return NULL
 * 4. If nothing found, return NULL
 */

/* Composing LBOS address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should try only one current zone and return it  */
BOOST_AUTO_TEST_CASE(g_LBOS_GetLBOSAddresses__SpecificMethod__FirstInResult)
{
    Get_LBOS_address::SpecificMethod__FirstInResult();
}

/* Composing LBOS address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on ZONE  */
BOOST_AUTO_TEST_CASE(
        g_LBOS_GetLBOSAddresses__CustomHostNotProvided__SkipCustomHost)
{
    Get_LBOS_address::CustomHostNotProvided__SkipCustomHost();
}

/* Composing LBOS address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on DOMAIN  */
BOOST_AUTO_TEST_CASE(g_LBOS_GetLBOSAddresses__NoConditions__AddressDefOrder)
{
    Get_LBOS_address::NoConditions__AddressDefOrder();
}

BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Get_candidates )////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Iterate through received LBOS's addresses, if there is not response from
 *    current LBOS
 * 2. If one LBOS works, do not try another LBOS
 * 3. If net_info was provided for Serv_OpenP, the same net_info should be
 *    available while getting candidates via lbos to provide DTABs*/

/* We need this variables and function to count how many times algorithm will
 * try to resolve IP, and to know if it uses the headers we provided.
 */

BOOST_AUTO_TEST_CASE(s_LBOS_FillCandidates__LBOSNoResponse__SkipLBOS){
    Get_candidates::LBOSNoResponse__SkipLBOS();
}

BOOST_AUTO_TEST_CASE(s_LBOS_FillCandidates__LBOSResponse__Finish) {
    Get_candidates::LBOSResponds__Finish();
}

BOOST_AUTO_TEST_CASE(s_LBOS_FillCandidates__NetInfoProvided__UseNetInfo) {
    Get_candidates::NetInfoProvided__UseNetInfo();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE(GetNextInfo)/////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * GetNextInfo:
 * 1. If no candidates found yet, or reset was just made, get candidates
 *    and return first
 * 2. If no candidates found yet, or reset was just made, and unrecoverable
 *    error while getting candidates, return 0
 * 3. If candidates already found, return next
 * 4. If last candidate was already returned, return 0
 * 5. If data is NULL for some reason, construct new data
 * 6. If iter is NULL, return NULL
 * 7. If SERV_MapperName(iter) returns name of another mapper, return NULL
 */

/* To be sure that mapper returns objects correctly, we need to be sure of
 * what LBOS sends to mapper. The best way is to emulate LBOS
 */

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__EmptyCands__RunGetCandidates)
{
    GetNextInfo::EmptyCands__RunGetCandidates();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__ErrorUpdating__ReturnNull)
{
    GetNextInfo::ErrorUpdating__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__HaveCands__ReturnNext)
{
    GetNextInfo::HaveCands__ReturnNext();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__LastCandReturned__ReturnNull)
{
    GetNextInfo::LastCandReturned__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__DataIsNull__ReconstructData)
{
    GetNextInfo::DataIsNull__ReconstructData();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__IterIsNull__ReconstructData)
{
    GetNextInfo::IterIsNull__ReconstructData();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__WrongMapper__ReturnNull)
{
    GetNextInfo::WrongMapper__ReturnNull();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE(Open)////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* Open:
 * 1. If iter is NULL, return NULL
 * 2. If net_info is NULL, construct own net_info
 * 3. If read from LBOS successful, return s_op
 * 4. If read from LBOS successful and host info pointer != NULL, write NULL
 *    to host info
 * 5. If read from LBOS unsuccessful or no such service, return 0
 */
BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__IterIsNull__ReturnNull)
{
    Open::IterIsNull__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__NetInfoNull__ReturnNull)
{
    Open::NetInfoNull__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__ServerExists__ReturnLbosOperations)
{
    Open::ServerExists__ReturnLbosOperations();
}

BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__InfoPointerProvided__WriteNull)
{
    Open::InfoPointerProvided__WriteNull();
}

BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__NoSuchService__ReturnNull)
{
    Open::NoSuchService__ReturnNull();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( GeneralLBOS )///////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * We use such name of service that we MUST get LBOS's answer
 * We check that it finds service and returns non-NULL list of
 * operations
 */
BOOST_AUTO_TEST_CASE(SERV_OpenP__ServerExists__ShouldReturnLbosOperations)
{
    GeneralLBOS::ServerExists__ShouldReturnLbosOperations();
}

BOOST_AUTO_TEST_CASE(TestLbos_OpenP__ServerDoesNotExist__ShouldReturnNull)
{
    GeneralLBOS::ServerDoesNotExist__ShouldReturnNull();
}

BOOST_AUTO_TEST_CASE(TestLbos_FindMethod__LbosExist__ShouldWork)
{
    GeneralLBOS::LbosExist__ShouldWork();
}

BOOST_AUTO_TEST_SUITE_END()



///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE(Initialization)//////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Multithread simultaneous SERV_LBOS_Open() when LBOS is not yet
*    initialized should not crash
* 2. At initialization if no LBOS found, mapper must turn OFF
* 3. At initialization if LBOS found, mapper should be ON
* 4. If LBOS has not yet been initialized, it should be initialized
*    at SERV_LBOS_Open()
* 5. If LBOS turned OFF, it MUST return NULL on SERV_LBOS_Open()
* 6. s_LBOS_InstancesList MUST not be NULL at beginning of s_LBOS_
*    Initialize()
* 7. s_LBOS_InstancesList MUST not be NULL at beginning of
*    s_LBOS_FillCandidates()
* 8. s_LBOS_FillCandidates() should switch first and good LBOS
*    addresses, if first is not responding
*/

/** Multithread simultaneous SERV_LBOS_Open() when LBOS is not yet
 * initialized should not crash                                              */
BOOST_AUTO_TEST_CASE(Initialization__MultithreadInitialization__ShouldNotCrash)
{
     Initialization::MultithreadInitialization__ShouldNotCrash();
}
/** At initialization if no LBOS found, mapper must turn OFF                 */
BOOST_AUTO_TEST_CASE(Initialization__InitializationFail__TurnOff)
{
     Initialization::InitializationFail__TurnOff();
}
/** At initialization if LBOS found, mapper should be ON                     */
BOOST_AUTO_TEST_CASE(Initialization__InitializationSuccess__StayOn)
{
     Initialization::InitializationSuccess__StayOn();
}
/** If LBOS has not yet been initialized, it should be initialized at
 * SERV_LBOS_Open()                                                          */
BOOST_AUTO_TEST_CASE(Initialization__OpenNotInitialized__ShouldInitialize)
{
     Initialization::OpenNotInitialized__ShouldInitialize();
}
/** If LBOS turned OFF, it MUST return NULL on SERV_LBOS_Open()              */
BOOST_AUTO_TEST_CASE(Initialization__OpenWhenTurnedOff__ReturnNull)
{
     Initialization::OpenWhenTurnedOff__ReturnNull();
}
/** s_LBOS_InstancesList MUST not be NULL at beginning of s_LBOS_Initialize()*/
BOOST_AUTO_TEST_CASE(
                Initialization__s_LBOS_Initialize__s_LBOS_InstancesListNotNULL)
{
     Initialization::s_LBOS_Initialize__s_LBOS_InstancesListNotNULL();
}
/** s_LBOS_InstancesList MUST not be NULL at beginning of
 * s_LBOS_FillCandidates()                                                   */
BOOST_AUTO_TEST_CASE(
            Initialization__s_LBOS_FillCandidates__s_LBOS_InstancesListNotNULL)
{
     Initialization::s_LBOS_FillCandidates__s_LBOS_InstancesListNotNULL();
}
/** s_LBOS_FillCandidates() should switch first and good LBOS addresses, if
   first is not responding                                                   */
BOOST_AUTO_TEST_CASE(Initialization__PrimaryLBOSInactive__SwapAddresses)
{
     Initialization::PrimaryLBOSInactive__SwapAddresses();
}
BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
//BOOST_AUTO_TEST_SUITE( Announcement )////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//BOOST_AUTO_TEST_CASE(Announcement__AllOK__AnnounceAndFind)
//{
//    Announce::AllOK__AnnounceAndFind();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__SelfAnnounceOK__RememberLBOS)
//{
//    Announce::SelfAnnounceOK__RememberLBOS();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__NoLBOS__ReturnErrorAndNotFind)
//{
//    Announce::NoLBOS__ReturnErrorAndNotFind();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__AlreadyExists__ReturnErrorAndFind)
//{
//    Announce::AlreadyExists__ReturnErrorAndFind();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__IPResolveError__ReturnErrorAndNotFind)
//{
//    Announce::IPResolveError__ReturnErrorAndNotFind();
//}
//
//BOOST_AUTO_TEST_CASE(
//                Announcement__IncorrectHealthcheckURL__ReturnErrorAndNotFind)
//{
//    Announce::IncorrectHealthcheckURL__ReturnErrorAndNotFind();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__IncorrectPort__ReturnErrorAndNotFind)
//{
//    Announce::IncorrectPort__ReturnErrorAndNotFind();
//}
//
//BOOST_AUTO_TEST_CASE(Announcement__IncorrectVersion__ReturnErrorAndNotFind)
//{
//    Announce::IncorrectVersion__ReturnErrorAndNotFind();
//}
//
//BOOST_AUTO_TEST_CASE(
//                   Announcement__IncorrectServiceName__ReturnErrorAndNotFind)
//{
//    Announce::IncorrectServiceName__ReturnErrorAndNotFind();
//}
//
//BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE(Stability)///////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 * 1. Just reset
 * 2. Full cycle: open mapper, get all servers, close iterator, repeat.
 */
/*  A simple stability test. We open iterator, and then, not closing it,
 * we get all servers and reset iterator and start getting all servers again
 */
BOOST_AUTO_TEST_CASE(Stability__GetNext_Reset__ShouldNotCrash)
{
    Stability::GetNext_Reset__ShouldNotCrash();
}

BOOST_AUTO_TEST_CASE(Stability__FullCycle__ShouldNotCrash)
{
    Stability::FullCycle__ShouldNotCrash();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE(Performance)/////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 *  Perform full cycle: open mapper, get all servers, close iterator, repeat.
 * Test lasts 60 seconds and measures min, max and avg performance.
 */
BOOST_AUTO_TEST_CASE(Performance__FullCycle__ShouldNotCrash)
{
    Performance::FullCycle__ShouldNotCrash();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE(MultiThreading)//////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
 *  Run all tests at once in concurrent threads and expect all tests to run
 * with the same results, except those in which mocks are not thread safe
 */
BOOST_AUTO_TEST_CASE(MultiThreading_test1)
{
    MultiThreading::TryMultiThread();
}

BOOST_AUTO_TEST_SUITE_END()
