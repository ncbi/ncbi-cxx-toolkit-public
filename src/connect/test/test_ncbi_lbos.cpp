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
 *   lbos mapper tests
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
 * necessary. If something is uncommented for unknown reason, please 
 * comment it now!         
 * It is aligned to the left edge because function names are very long and it 
 * allows to save some lines                                                 */
///////////////////////////////////////////////////////////////////////////////
//                      Compose lbos address                                 //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(g_LBOS_ComposeLBOSAddress__LBOSExists__ShouldReturnLbos);
//NCBITEST_DISABLE(g_LBOS_ComposeLBOSAddress__RoleFail__ShouldReturnNULL);
//NCBITEST_DISABLE(g_LBOS_ComposeLBOSAddress__DomainFail__ShouldReturnNULL);
///////////////////////////////////////////////////////////////////////////////
//                          Reset iterator                                   //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(SERV_Reset__NoConditions__IterContainsZeroCandidates);
//NCBITEST_DISABLE(SERV_Reset__MultipleReset__ShouldNotCrash);
//NCBITEST_DISABLE(SERV_Reset__Multiple_AfterGetNextInfo__ShouldNotCrash);
///////////////////////////////////////////////////////////////////////////////
//                          Close iterator                                   //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(SERV_CloseIter__AfterOpen__ShouldWork);
//NCBITEST_DISABLE(SERV_CloseIter__AfterReset__ShouldWork);
//NCBITEST_DISABLE(SERV_CloseIter__AfterGetNextInfo__ShouldWork);
//NCBITEST_DISABLE(SERV_CloseIter__FullCycle__ShouldWork);
///////////////////////////////////////////////////////////////////////////////
//                                DTab                                       //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(DTab::DTabRegistryAndHttp__RegistryGoesFirst);
///////////////////////////////////////////////////////////////////////////////
//                        Resolve via lbos                                   //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(s_LBOS_ResolveIPPort__ServiceExists__ReturnHostIP);
//NCBITEST_DISABLE(s_LBOS_ResolveIPPort__ServiceDoesNotExist__ReturnNULL);
//NCBITEST_DISABLE(s_LBOS_ResolveIPPort__NoLBOS__ReturnNULL);
//NCBITEST_DISABLE(s_LBOS_ResolveIPPort__FakeMassiveInput__ShouldProcess);
//NCBITEST_DISABLE(s_LBOS_ResolveIPPort__FakeErrorInput__ShouldNotCrash);
///////////////////////////////////////////////////////////////////////////////
//                         Get lbos address                                  //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(g_LBOS_GetLBOSAddresses__SpecificMethod__FirstInResult);
//NCBITEST_DISABLE(
            g_LBOS_GetLBOSAddresses__CustomHostNotProvided__SkipCustomHost);
//NCBITEST_DISABLE(g_LBOS_GetLBOSAddresses__NoConditions__AddressDefOrder);
///////////////////////////////////////////////////////////////////////////////
//                           Get candidates                                  //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(s_LBOS_FillCandidates__LBOSNoResponse__SkipLBOS);
//NCBITEST_DISABLE(s_LBOS_FillCandidates__LBOSResponse__Finish);
//NCBITEST_DISABLE(s_LBOS_FillCandidates__NetInfoProvided__UseNetInfo);
///////////////////////////////////////////////////////////////////////////////
//                          GetNextInfo                                      //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(SERV_GetNextInfoEx__EmptyCands__RunGetCandidates);
//NCBITEST_DISABLE(SERV_GetNextInfoEx__ErrorUpdating__ReturnNull);
//NCBITEST_DISABLE(SERV_GetNextInfoEx__HaveCands__ReturnNext);
//NCBITEST_DISABLE(SERV_GetNextInfoEx__LastCandReturned__ReturnNull);
//NCBITEST_DISABLE(SERV_GetNextInfoEx__DataIsNull__ReconstructData);
//NCBITEST_DISABLE(SERV_GetNextInfoEx__IterIsNull__ReturnNull);
//NCBITEST_DISABLE(SERV_GetNextInfoEx__WrongMapper__ReturnNull);
///////////////////////////////////////////////////////////////////////////////
//                               Open                                        //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(SERV_LBOS_Open__IterIsNull__ReturnNull);
//NCBITEST_DISABLE(SERV_LBOS_Open__NetInfoNull__ConstructNetInfo);
//NCBITEST_DISABLE(SERV_LBOS_Open__ServerExists__ReturnLbosOperations);
//NCBITEST_DISABLE(SERV_LBOS_Open__InfoPointerProvided__WriteNull);
//NCBITEST_DISABLE(SERV_LBOS_Open__NoSuchService__ReturnNull);
///////////////////////////////////////////////////////////////////////////////
//                           General lbos                                    //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(SERV_OpenP__ServerExists__ShouldReturnLbosOperations);
//NCBITEST_DISABLE(TestLbos_OpenP__ServerDoesNotExist__ShouldReturnNull);
//NCBITEST_DISABLE(TestLbos_FindMethod__LbosExist__ShouldWork);
///////////////////////////////////////////////////////////////////////////////
//                           Initialization                                  //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(Initialization__MultithreadInitialization__ShouldNotCrash);
//NCBITEST_DISABLE(Initialization__InitializationFail__TurnOff);
//NCBITEST_DISABLE(Initialization__InitializationSuccess__StayOn);
//NCBITEST_DISABLE(Initialization__OpenNotInitialized__ShouldInitialize);
//NCBITEST_DISABLE(Initialization__OpenWhenTurnedOff__ReturnNull);
//NCBITEST_DISABLE(
            Initialization__s_LBOS_Initialize__s_LBOS_InstancesListNotNULL);
//NCBITEST_DISABLE(
        Initialization__s_LBOS_FillCandidates__s_LBOS_InstancesListNotNULL);
//NCBITEST_DISABLE(Initialization__PrimaryLBOSInactive__SwapAddresses);
///////////////////////////////////////////////////////////////////////////////
//                           Announcement                                    //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(Announcement__AllOK__ReturnSuccess);
//NCBITEST_DISABLE(Announcement__AllOK__LBOSAnswerProvided);
//NCBITEST_DISABLE(Announcement__AllOK__AnnouncedServerSaved);
//NCBITEST_DISABLE(Announcement__NoLBOS__ReturnNoLBOSAndNotFind);
//NCBITEST_DISABLE(Announcement__NoLBOS__LBOSAnswerNull);
//NCBITEST_DISABLE(Announcement__LBOSError__ReturnServerError);
//NCBITEST_DISABLE(Announcement__LBOSError__LBOSAnswerProvided);
//NCBITEST_DISABLE(
             Announcement__AlreadyAnnouncedInTheSameZone__ReplaceInStorage);
//NCBITEST_DISABLE(Announcement__AnotherRegion__NoAnnounce);
//NCBITEST_DISABLE(Announcement__IncorrectURL__ReturnInvalidArgs);
//NCBITEST_DISABLE(Announcement__IncorrectPort__ReturnInvalidArgs);
//NCBITEST_DISABLE(Announcement__IncorrectVersion__ReturnInvalidArgs);
//NCBITEST_DISABLE(Announcement__IncorrectServiceName__ReturnInvalidArgs);
//NCBITEST_DISABLE(Announcement__RealLife__VisibleAfterAnnounce);
//NCBITEST_DISABLE(
               Announcement__ResolveLocalIPError__Return_DNS_RESOLVE_ERROR);
//NCBITEST_DISABLE(Announcement__IP0000__ReplaceWithLocalIP);
//NCBITEST_DISABLE(Announcement__ResolveLocalIPError__Return_DNS_RESOLVE_ERROR);
//NCBITEST_DISABLE(Announcement__LBOSOff__ReturnELBOS_Off);
//NCBITEST_DISABLE(Announcement__LBOSAnnounceCorruptOutput__ReturnServerError);
///////////////////////////////////////////////////////////////////////////////
//                     Announcement from Registry                            //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(AnnouncementRegistry__ParamsGood__ReturnSuccess);
//NCBITEST_DISABLE(
               AnnouncementRegistry__CustomSectionNoVars__ReturnInvalidArgs);
//NCBITEST_DISABLE(
AnnouncementRegistry__CustomSectionEmptyOrNullAndSectionIsOk__ReturnSuccess);
//NCBITEST_DISABLE(
                AnnouncementRegistry__ServiceEmptyOrNull__ReturnInvalidArgs);
//NCBITEST_DISABLE(
                AnnouncementRegistry__VersionEmptyOrNull__ReturnInvalidArgs);
//NCBITEST_DISABLE(AnnouncementRegistry__PortEmptyOrNull__ReturnInvalidArgs);
//NCBITEST_DISABLE(AnnouncementRegistry__PortOutOfRange__ReturnInvalidArgs);
//NCBITEST_DISABLE(
//               AnnouncementRegistry__PortContainsLetters__ReturnInvalidArgs);
//NCBITEST_DISABLE(
//           AnnouncementRegistry__HealthchecktEmptyOrNull__ReturnInvalidArgs);
//NCBITEST_DISABLE(
//   AnnouncementRegistry__HealthcheckDoesNotStartWithHttp__ReturnInvalidArgs);
///////////////////////////////////////////////////////////////////////////////
//                            Deannouncement                                 //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(Deannouncement__Deannounced__Return1);
//NCBITEST_DISABLE(Deannouncement__Deannounced__AnnouncedServerRemoved);
//NCBITEST_DISABLE(Deannouncement__NoLBOS__Return0);
//NCBITEST_DISABLE(Deannouncement__LBOSExistsDeannounceError__Return0);
//NCBITEST_DISABLE(Deannouncement__RealLife__InvisibleAfterDeannounce);
//NCBITEST_DISABLE(Deannouncement__AnotherDomain__DoNothing);
/* This test does not work yet; Vladimir has to fix lbos so it announces 
 * servers with dead health checks.                                          */
//NCBITEST_DISABLE(Deannouncement__NoHostProvided__LocalAddress);
///////////////////////////////////////////////////////////////////////////////
//                          Deannounce all                                   //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(DeannouceAll__AllDeannounced__NoSavedLeft);
///////////////////////////////////////////////////////////////////////////////
//                             Stability                                     //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(Stability__GetNext_Reset__ShouldNotCrash);
//NCBITEST_DISABLE(Stability__FullCycle__ShouldNotCrash);
///////////////////////////////////////////////////////////////////////////////
//                            Performance                                    //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(Performance__FullCycle__ShouldNotCrash);
///////////////////////////////////////////////////////////////////////////////
//                          Multi-threading                                  //
///////////////////////////////////////////////////////////////////////////////
//NCBITEST_DISABLE(MultiThreading_test1);
}

NCBITEST_AUTO_INIT()
{
    CConnNetInfo net_info;
    boost::unit_test::framework::master_test_suite().p_name->assign(
                                                    "lbos mapper Unit Test");
    CNcbiRegistry& config = CNcbiApplication::Instance()->GetConfig();
    CONNECT_Init(dynamic_cast<ncbi::IRWRegistry*>(&config));
    size_t start = 0, end = 0;
    char *lbos_ouput_orig = g_LBOS_UnitTesting_GetLBOSFuncs()->
            UrlReadAll(*net_info, "http://lbos.dev.be-md.ncbi.nlm.nih.gov:8080"
            "/lbos/text/service", NULL);
    string lbos_output = string(lbos_ouput_orig);
    free(lbos_ouput_orig);
    LBOS_Deannounce("/lbostest", /* for initialization */
                    "1.0.0",
                    "lbos.dev.be-md.ncbi.nlm.nih.gov",
                    5000);        
    while (start != string::npos) {
        string to_find = "/lbostest\t";
        start = lbos_output.find(to_find, start);
        if (start == string::npos)
            break;
        start = lbos_output.find("\t", start); //skip service name
        start = lbos_output.find("\t", start); //skip service name
        start = lbos_output.find(":", start); //skip ip
        start += 1; //skip ":"
        end = lbos_output.find("\t", start);
        unsigned short port = 
                       NStr::StringToInt(lbos_output.substr(start, end-start));
        
        LBOS_Deannounce("/lbostest",
                        "1.0.0",
                        "lbos.dev.be-md.ncbi.nlm.nih.gov",
                        port);        
    }
}

BOOST_AUTO_TEST_CASE(SERV_CloseIter__FullCycle__ShouldWork)
{
    Close_iterator::FullCycle__ShouldWork();
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


/* Composing lbos address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should try only one current zone and return it  */
BOOST_AUTO_TEST_CASE(g_LBOS_ComposeLBOSAddress__LBOSExists__ShouldReturnLbos)
{
    Compose_LBOS_address::LBOSExists__ShouldReturnLbos();
}

/* Composing lbos address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on ZONE  */
BOOST_AUTO_TEST_CASE(g_LBOS_ComposeLBOSAddress__RoleFail__ShouldReturnNULL)
{
    Compose_LBOS_address::RoleFail__ShouldReturnNULL();
}

/* Composing lbos address from /etc/ncbi/role + /etc/ncbi/domain:
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
BOOST_AUTO_TEST_SUITE( Close_iterator )////////////////////////////////////////
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
BOOST_AUTO_TEST_SUITE( Dtab )//////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Mix of registry DTab and HTTP Dtab: registry goes first */
BOOST_AUTO_TEST_CASE(DTab__DTabRegistryAndHttp__RegistryGoesFirst) 
{
    DTab::DTabRegistryAndHttp__RegistryGoesFirst();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Resolve_via_LBOS )//////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Should return string with IP:port if OK
 * 2. Should return NULL if lbos answered "not found"
 * 3. Should return NULL if lbos is not reachable
 * 4. Should be able to support up to M IP:port combinations (not checking for
 *    repeats) with storage overhead not more than same as size needed (that
 *    is, all space consumed is twice as size needed, used and unused space
 *    together)
 * 5. Should be able to correctly process incorrect lbos output. If from 5
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

/* Composing lbos address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should try only one current zone and return it  */
BOOST_AUTO_TEST_CASE(g_LBOS_GetLBOSAddresses__SpecificMethod__FirstInResult)
{
    Get_LBOS_address::SpecificMethod__FirstInResult();
}

/* Composing lbos address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on ZONE  */
BOOST_AUTO_TEST_CASE(
        g_LBOS_GetLBOSAddresses__CustomHostNotProvided__SkipCustomHost)
{
    Get_LBOS_address::CustomHostNotProvided__SkipCustomHost();
}

/* Composing lbos address from /etc/ncbi/role + /etc/ncbi/domain:
 * Should return NULL if fail on DOMAIN  */
BOOST_AUTO_TEST_CASE(g_LBOS_GetLBOSAddresses__NoConditions__AddressDefOrder)
{
    Get_LBOS_address::NoConditions__AddressDefOrder();
}

BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Get_candidates )////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Iterate through received lbos's addresses, if there is not response from
 *    current lbos
 * 2. If one lbos works, do not try another lbos
 * 3. If net_info was provided for Serv_OpenP, the same net_info should be
 *    available while getting candidates via lbos to provide DTABs*/

/* We need this variables and function to count how many times algorithm will
 * try to resolve IP, and to know if it uses the headers we provided.
 */

BOOST_AUTO_TEST_CASE(s_LBOS_FillCandidates__LBOSNoResponse__SkipLBOS) {
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
BOOST_AUTO_TEST_SUITE( Get_Next_Info )/////////////////////////////////////////
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
 * what lbos sends to mapper. The best way is to emulate lbos
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

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__IterIsNull__ReturnNull)
{
    GetNextInfo::IterIsNull__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_GetNextInfoEx__WrongMapper__ReturnNull)
{
    GetNextInfo::WrongMapper__ReturnNull();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Open )//////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* Open:
 * 1. If iter is NULL, return NULL
 * 2. If net_info is NULL, construct own net_info
 * 3. If read from lbos successful, return s_op
 * 4. If read from lbos successful and host info pointer != NULL, write NULL
 *    to host info
 * 5. If read from lbos unsuccessful or no such service, return 0
 */
BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__IterIsNull__ReturnNull)
{
    Open::IterIsNull__ReturnNull();
}

BOOST_AUTO_TEST_CASE(SERV_LBOS_Open__NetInfoNull__ConstructNetInfo)
{
    Open::NetInfoNull__ConstructNetInfo();
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
 * We use such name of service that we MUST get lbos's answer
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
BOOST_AUTO_TEST_SUITE( Initialization )////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Multithread simultaneous SERV_LBOS_Open() when lbos is not yet
*    initialized should not crash
* 2. At initialization if no lbos found, mapper must turn OFF
* 3. At initialization if lbos found, mapper should be ON
* 4. If lbos has not yet been initialized, it should be initialized
*    at SERV_LBOS_Open()
* 5. If lbos turned OFF, it MUST return NULL on SERV_LBOS_Open()
* 6. s_LBOS_InstancesList MUST not be NULL at beginning of s_LBOS_
*    Initialize()
* 7. s_LBOS_InstancesList MUST not be NULL at beginning of
*    s_LBOS_FillCandidates()
* 8. s_LBOS_FillCandidates() should switch first and good lbos
*    addresses, if first is not responding
*/

/** Multithread simultaneous SERV_LBOS_Open() when lbos is not yet
 * initialized should not crash                                              */
BOOST_AUTO_TEST_CASE(Initialization__MultithreadInitialization__ShouldNotCrash)
{
     Initialization::MultithreadInitialization__ShouldNotCrash();
}
/** At initialization if no lbos found, mapper must turn OFF                 */
BOOST_AUTO_TEST_CASE(Initialization__InitializationFail__TurnOff)
{
     Initialization::InitializationFail__TurnOff();
}
/** At initialization if lbos found, mapper should be ON                     */
BOOST_AUTO_TEST_CASE(Initialization__InitializationSuccess__StayOn)
{
     Initialization::InitializationSuccess__StayOn();
}
/** If lbos has not yet been initialized, it should be initialized at
 * SERV_LBOS_Open()                                                          */
BOOST_AUTO_TEST_CASE(Initialization__OpenNotInitialized__ShouldInitialize)
{
     Initialization::OpenNotInitialized__ShouldInitialize();
}
/** If lbos turned OFF, it MUST return NULL on SERV_LBOS_Open()              */
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
/** s_LBOS_FillCandidates() should switch first and good lbos addresses, if
   first is not responding                                                   */
BOOST_AUTO_TEST_CASE(Initialization__PrimaryLBOSInactive__SwapAddresses)
{
     Initialization::PrimaryLBOSInactive__SwapAddresses();
}
BOOST_AUTO_TEST_SUITE_END()



///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( AnnounceTest )//////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*  1. Successfully announced: return SUCCESS
 *  2. Successfully announced: SLBOS_AnnounceHandle deannounce_handle 
 *     contains info needed to later deannounce announced server
 *  3. Successfully announced: char* lbos_answer contains answer of lbos
 *  4. Successfully announced: information about announcement is saved to 
 *     hidden lbos mapper's storage
 *  5. Could not find lbos: return NO_LBOS
 *  6. Could not find lbos: char* lbos_answer is set to NULL
 *  7. Could not find lbos: SLBOS_AnnounceHandle deannounce_handle is set to
 *     NULL
 *  8. lbos returned error: return LBOS_ERROR
 *  9. lbos returned error: char* lbos_answer contains answer of lbos
 * 10. lbos returned error: SLBOS_AnnounceHandle deannounce_handle is set to 
 *     NULL
 * 11. Server announced again (service name, IP and port coincide) and 
 *     announcement in the same zone, replace old info about announced 
 *     server in internal storage with new one.
 * 12. Server announced again and trying to announce in another 
 *     zone - return MULTIZONE_ANNOUNCE_PROHIBITED
 * 13. Was passed incorrect healthcheck URL (NULL or empty not starting with 
 *     "http(s)://"): do not announce and return INVALID_ARGS
 * 14. Was passed incorrect port (zero): do not announce and return 
 *     INVALID_ARGS
 * 15. Was passed incorrect version(NULL or empty): do not announce and 
 *     return INVALID_ARGS
 * 16. Was passed incorrect service nameNULL or empty): do not announce and 
 *     return INVALID_ARGS
 * 17. Real-life test: after announcement server should be visible to 
 *     resolve
 * 18. If was passed "0.0.0.0" as IP, should replace it with local IP or 
 *     hostname
 * 19. Was passed "0.0.0.0" as IP and could not manage to resolve local host 
 *     IP: do not announce and return DNS_RESOLVE_ERROR
 * 20. lbos is OFF - return eLBOS_Off                                        
 * 21. Announced successfully, but LBOS return corrupted answer - 
 *     return SERVER_ERROR                                                   */

/*  1. Successfully announced : return SUCCESS                               */
BOOST_AUTO_TEST_CASE(Announcement__AllOK__ReturnSuccess)
{
    Announcement::AllOK__ReturnSuccess();
}

/*  3. Successfully announced : char* lbos_answer contains answer of lbos    */
BOOST_AUTO_TEST_CASE(Announcement__AllOK__LBOSAnswerProvided)
{
    Announcement::AllOK__LBOSAnswerProvided();
}

/*  4. Successfully announced: information about announcement is saved to
 *     hidden lbos mapper's storage                                          */
BOOST_AUTO_TEST_CASE(Announcement__AllOK__AnnouncedServerSaved)
{
    Announcement::AllOK__AnnouncedServerSaved();
}
/*  5. Could not find lbos: return NO_LBOS                                   */
BOOST_AUTO_TEST_CASE(Announcement__NoLBOS__ReturnNoLBOSAndNotFind)
{
    Announcement::NoLBOS__ReturnNoLBOSAndNotFind();
}
/*  6. Could not find lbos : char* lbos_answer is set to NULL                */
BOOST_AUTO_TEST_CASE(Announcement__NoLBOS__LBOSAnswerNull)
{
    Announcement::NoLBOS__LBOSAnswerNull();
}
/*  8. lbos returned error: return eLBOS_ServerError                         */
BOOST_AUTO_TEST_CASE(Announcement__LBOSError__ReturnServerError)
{
    Announcement::LBOSError__ReturnServerError();
}
/*  9. lbos returned error : char* lbos_answer contains answer of lbos       */
BOOST_AUTO_TEST_CASE(Announcement__LBOSError__LBOSAnswerProvided)
{
    Announcement::LBOSError__LBOSAnswerProvided();
}
/* 11. Server announced again(service name, IP and port coincide) and
 *     announcement in the same zone, replace old info about announced
 *     server in internal storage with new one.                              */
BOOST_AUTO_TEST_CASE(
                 Announcement__AlreadyAnnouncedInTheSameZone__ReplaceInStorage)
{
    Announcement::AlreadyAnnouncedInTheSameZone__ReplaceInStorage();
}
/* 12. Trying to announce in another domain - do nothing                     */
BOOST_AUTO_TEST_CASE(
        Announcement__AnotherRegion__NoAnnounce)
{
    Announcement::AnotherRegion__NoAnnounce();
}
/* 13. Was passed incorrect healthcheck URL(NULL or empty not starting with
 *     "http(s)://") : do not announce and return INVALID_ARGS               */
BOOST_AUTO_TEST_CASE(Announcement__IncorrectURL__ReturnInvalidArgs)
{
    Announcement::IncorrectURL__ReturnInvalidArgs();
}
/* 14. Was passed incorrect port(zero) : do not announce and return
 *     INVALID_ARGS                                                          */
BOOST_AUTO_TEST_CASE(Announcement__IncorrectPort__ReturnInvalidArgs)
{
    Announcement::IncorrectPort__ReturnInvalidArgs();
}
/* 15. Was passed incorrect version(NULL or empty) : do not announce and
 *     return INVALID_ARGS                                                   */
BOOST_AUTO_TEST_CASE(Announcement__IncorrectVersion__ReturnInvalidArgs)
{
    Announcement::IncorrectVersion__ReturnInvalidArgs();
}
/* 16. Was passed incorrect service name (NULL or empty): do not announce and
 *     return INVALID_ARGS                                                   */
BOOST_AUTO_TEST_CASE(Announcement__IncorrectServiceName__ReturnInvalidArgs)
{
    Announcement::IncorrectServiceName__ReturnInvalidArgs();
}
/* 17. Real - life test : after announcement server should be visible to
 *     resolve                                                               */
BOOST_AUTO_TEST_CASE(Announcement__RealLife__VisibleAfterAnnounce)
{
    Announcement::RealLife__VisibleAfterAnnounce();
}
/* 18. If was passed "0.0.0.0" as IP, should replace it with local IP or
 *     hostname                                                              */
BOOST_AUTO_TEST_CASE(Announcement__IP0000__ReplaceWithLocalIP)
{
    Announcement::IP0000__ReplaceWithLocalIP();
}
/* 19. Was passed "0.0.0.0" as IP and could not manage to resolve local host
 *     IP : do not announce and return DNS_RESOLVE_ERROR                     */
BOOST_AUTO_TEST_CASE(
                   Announcement__ResolveLocalIPError__Return_DNS_RESOLVE_ERROR)
{
    Announcement::ResolveLocalIPError__Return_DNS_RESOLVE_ERROR();
}
/* 20. lbos is OFF - return eLBOS_Off                                        */
BOOST_AUTO_TEST_CASE(Announcement__LBOSOff__ReturnELBOS_Off)
{
    Announcement::LBOSOff__ReturnELBOS_Off();
}
/*21. Announced successfully, but LBOS return corrupted answer -
      return SERVER_ERROR                                                    */
BOOST_AUTO_TEST_CASE(
                    Announcement__LBOSAnnounceCorruptOutput__ReturnServerError)
{
    Announcement::LBOSAnnounceCorruptOutput__ReturnServerError();
}
BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( AnnounceRegistryTest )//////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*  1.  All parameters good (Custom section has all parameters correct in 
        config) - return eLBOS_Success
    2.  Custom section has nothing in config - return eLBOS_InvalidArgs
    3.  Section empty or NULL (should use default section and return 
        eLBOS_Success)
    4.  Service is empty or NULL - return eLBOS_InvalidArgs
    5.  Version is empty or NULL - return eLBOS_InvalidArgs
    6.  port is empty or NULL - return eLBOS_InvalidArgs
    7.  port is out of range - return eLBOS_InvalidArgs
    8.  port contains letters - return eLBOS_InvalidArgs
    9.  healthcheck is empty or NULL - return eLBOS_InvalidArgs
    10. healthcheck does not start with http:// or https:// - return 
        eLBOS_InvalidArgs                                                    */

/*  1.  All parameters good (Custom section has all parameters correct in 
        config) - return eLBOS_Success                                       */
BOOST_AUTO_TEST_CASE(AnnouncementRegistry__ParamsGood__ReturnSuccess) 
{
    return AnnouncementRegistry::ParamsGood__ReturnSuccess();
}
/*  2.  Custom section has nothing in config - return eLBOS_InvalidArgs      */
BOOST_AUTO_TEST_CASE(
                  AnnouncementRegistry__CustomSectionNoVars__ReturnInvalidArgs)
{
    return AnnouncementRegistry::CustomSectionNoVars__ReturnInvalidArgs();
}
/*  3.  Section empty or NULL (should use default section and return 
        eLBOS_Success)                                                       */
BOOST_AUTO_TEST_CASE(
   AnnouncementRegistry__CustomSectionEmptyOrNullAndSectionIsOk__ReturnSuccess)
{
    return AnnouncementRegistry::
                       CustomSectionEmptyOrNullAndSectionIsOk__ReturnSuccess();
}
/*  4.  Service is empty or NULL - return eLBOS_InvalidArgs                  */
BOOST_AUTO_TEST_CASE(
                   AnnouncementRegistry__ServiceEmptyOrNull__ReturnInvalidArgs)
{
    return AnnouncementRegistry::ServiceEmptyOrNull__ReturnInvalidArgs();
}
/*  5.  Version is empty or NULL - return eLBOS_InvalidArgs                  */
BOOST_AUTO_TEST_CASE(
                   AnnouncementRegistry__VersionEmptyOrNull__ReturnInvalidArgs)
{
    return AnnouncementRegistry::VersionEmptyOrNull__ReturnInvalidArgs();
}
/*  6.  port is empty or NULL - return eLBOS_InvalidArgs                     */
BOOST_AUTO_TEST_CASE(AnnouncementRegistry__PortEmptyOrNull__ReturnInvalidArgs)
{
    return AnnouncementRegistry::PortEmptyOrNull__ReturnInvalidArgs();
}
/*  7.  port is out of range - return eLBOS_InvalidArgs                      */
BOOST_AUTO_TEST_CASE(AnnouncementRegistry__PortOutOfRange__ReturnInvalidArgs)
{
    return AnnouncementRegistry::PortOutOfRange__ReturnInvalidArgs();
}
/*  8.  port contains letters - return eLBOS_InvalidArgs                     */
BOOST_AUTO_TEST_CASE(
                  AnnouncementRegistry__PortContainsLetters__ReturnInvalidArgs)
{
    return AnnouncementRegistry::PortContainsLetters__ReturnInvalidArgs();
}
/*  9.  healthcheck is empty or NULL - return eLBOS_InvalidArgs              */
BOOST_AUTO_TEST_CASE(
              AnnouncementRegistry__HealthchecktEmptyOrNull__ReturnInvalidArgs)
{
    return AnnouncementRegistry::HealthchecktEmptyOrNull__ReturnInvalidArgs();
}
/*  10. healthcheck does not start with http:// or https:// - return         
        eLBOS_InvalidArgs                                                    */  
BOOST_AUTO_TEST_CASE(
      AnnouncementRegistry__HealthcheckDoesNotStartWithHttp__ReturnInvalidArgs)
{
    return AnnouncementRegistry::
                          HealthcheckDoesNotStartWithHttp__ReturnInvalidArgs();
}      
BOOST_AUTO_TEST_SUITE_END()

///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Deannounce )////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/* 1. Successfully deannounced : return 1
 * 2. Successfully deannounced : if announcement was saved in local storage, 
 *    remove it
 * 3. Could not connect to provided lbos : fail and return 0
 * 4. Successfully connected to lbos, but deannounce returned error : return 0
 * 5. Real - life test : after deannouncement server should be invisible 
 *    to resolve                                                            
 * 6. Another domain - do not deannounce 
 * 7. Deannounce without IP specified - deannounce from local host 
 * 8. lbos is OFF - return eLBOS_Off                                         */
 /* 1. Successfully deannounced: return 1                                    */
BOOST_AUTO_TEST_CASE(Deannouncement__Deannounced__Return1)
{
    /* Here we specifiy port (specify that we do not intend to use specific 
     * port), because this function is used inside one another test 
     * (Deannouncement__RealLife__InvisibleAfterDeannounce)    */
    Deannouncement::Deannounced__Return1(0);
}
/* 2. Successfully deannounced : if announcement was saved in local storage, 
 *    remove it                                                              */
BOOST_AUTO_TEST_CASE(Deannouncement__Deannounced__AnnouncedServerRemoved)
{
    Deannouncement::Deannounced__AnnouncedServerRemoved();
}
/* 3. Could not connect to provided lbos : fail and return 0                 */
BOOST_AUTO_TEST_CASE(Deannouncement__NoLBOS__Return0)
{
    Deannouncement::NoLBOS__Return0();
}
/* 4. Successfully connected to lbos, but deannounce returned error: 
 *    return 0                                                               */
BOOST_AUTO_TEST_CASE(Deannouncement__LBOSExistsDeannounceError__Return0)
{
    Deannouncement::LBOSExistsDeannounceError__Return0();
}
/* 5. Real - life test : after deannouncement server should be invisible 
 *    to resolve                                                             */
BOOST_AUTO_TEST_CASE(Deannouncement__RealLife__InvisibleAfterDeannounce)
{
    Deannouncement::RealLife__InvisibleAfterDeannounce();
}
/*6. If trying to deannounce in another domain - do not deannounce           */
BOOST_AUTO_TEST_CASE(Deannouncement__AnotherDomain__DoNothing)
{
    Deannouncement::AnotherDomain__DoNothing();
}
/* 7. Deannounce without IP specified - deannounce from local host           */
BOOST_AUTO_TEST_CASE(Deannouncement__NoHostProvided__LocalAddress)
{
    Deannouncement::NoHostProvided__LocalAddress();
}
/* 8. lbos is OFF - return eLBOS_Off                                         */
BOOST_AUTO_TEST_CASE(Deannouncement__LBOSOff__ReturnELBOS_Off)
{
    Deannouncement::LBOSOff__ReturnELBOS_Off();
}

BOOST_AUTO_TEST_SUITE_END()


///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( DeannounceAll )/////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_CASE(DeannounceAll__AllDeannounced__NoSavedLeft)
{
    DeannouncementAll::AllDeannounced__NoSavedLeft();
}
BOOST_AUTO_TEST_SUITE_END()



///////////////////////////////////////////////////////////////////////////////
BOOST_AUTO_TEST_SUITE( Stability )/////////////////////////////////////////////
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
BOOST_AUTO_TEST_SUITE( Performance )///////////////////////////////////////////
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
BOOST_AUTO_TEST_SUITE( MultiThreading )////////////////////////////////////////
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
