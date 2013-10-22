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
* Author:  Sergey Satskiy
*
* File Description:
*   Unit test for CRandom
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>


#include <corelib/ncbiapp.hpp>
#include <util/random_gen.hpp>

// This header must be included before all Boost.Test headers if there are any
#include <corelib/test_boost.hpp>


USING_NCBI_SCOPE;


BOOST_AUTO_TEST_CASE(TestLFGCreate1)
{
    try {
        CRandom     rnd;
        BOOST_CHECK(rnd.GetRandMethod() == CRandom::eGetRand_LFG);
    }
    catch (...) {
        BOOST_FAIL("Error creating CRandom()");
    }
}


BOOST_AUTO_TEST_CASE(TestSysCreate1)
{
    try {
        CRandom     rnd(CRandom::eGetRand_Sys);
        BOOST_CHECK(rnd.GetRandMethod() == CRandom::eGetRand_Sys);
    }
    catch (...) {
        LOG_POST("Could not instantiate SYS random generator");
    }
}


BOOST_AUTO_TEST_CASE(TestLFGSeed)
{
    try {
        CRandom     rnd;
        BOOST_CHECK(rnd.GetRandMethod() == CRandom::eGetRand_LFG);

        rnd.SetSeed(16);
        BOOST_CHECK(rnd.GetSeed() == 16);

        for (size_t  k = 0; k < 1024; ++k) {
            rnd.Randomize();
            if (rnd.GetSeed() != 16)
                return;
        }
        BOOST_FAIL("Cannot randomize LFG generator");
    }
    catch (const CException &  excpt) {
        BOOST_FAIL("Error in LFG seed test: " + string(excpt.what()));
    }
    catch (...) {
        BOOST_FAIL("Unknown error in LFG seed test");
    }
}


BOOST_AUTO_TEST_CASE(TestSysRandomize)
{
    try {
        CRandom     rnd(CRandom::eGetRand_Sys);
        BOOST_CHECK(rnd.GetRandMethod() == CRandom::eGetRand_Sys);

        try {
            rnd.Randomize();
        }
        catch (...) {
            BOOST_FAIL("SYS generator must not throw on Randomize()");
        }
    }
    catch (...) {
        return;
    }
}


BOOST_AUTO_TEST_CASE(TestSysSetSeed)
{
    try {
        CRandom     rnd(CRandom::eGetRand_Sys);
        BOOST_CHECK(rnd.GetRandMethod() == CRandom::eGetRand_Sys);

        try {
            rnd.SetSeed(32);
            BOOST_FAIL("SYS generator must throw on SetSeed(...)");
        }
        catch (...) {
            // Exception is expected
            return;
        }
    }
    catch (...) {
        return;
    }
}


BOOST_AUTO_TEST_CASE(TestSysGetSeed)
{
    try {
        CRandom     rnd(CRandom::eGetRand_Sys);
        BOOST_CHECK(rnd.GetRandMethod() == CRandom::eGetRand_Sys);

        try {
            rnd.GetSeed();
            BOOST_FAIL("SYS generator must throw on GetSeed(...)");
        }
        catch (...) {
            // Exception is expected
            return;
        }
    }
    catch (...) {
        return;
    }
}


BOOST_AUTO_TEST_CASE(TestSysReset)
{
    try {
        CRandom     rnd(CRandom::eGetRand_Sys);
        BOOST_CHECK(rnd.GetRandMethod() == CRandom::eGetRand_Sys);

        try {
            rnd.Reset();
            BOOST_FAIL("SYS generator must throw on Reset()");
        }
        catch (...) {
            // Exception is expected
            return;
        }
    }
    catch (...) {
        return;
    }
}


BOOST_AUTO_TEST_CASE(TestLFGMax)
{
    try {
        CRandom     rnd;
        BOOST_CHECK(rnd.GetRandMethod() == CRandom::eGetRand_LFG);

        rnd.Randomize();
        CRandom::TValue limit = CRandom::GetMax();
        for (size_t  k = 0; k < 10000; ++k) {
            BOOST_CHECK(rnd.GetRand() <= limit);
        }
    }
    catch (const CException &  excpt) {
        BOOST_FAIL("Error in LFG max test: " + string(excpt.what()));
    }
    catch (...) {
        BOOST_FAIL("Unknown error in LFG max test");
    }
}


BOOST_AUTO_TEST_CASE(TestSysMax)
{
    try {
        CRandom     rnd(CRandom::eGetRand_Sys);
        BOOST_CHECK(rnd.GetRandMethod() == CRandom::eGetRand_Sys);

        CRandom::TValue limit = CRandom::GetMax();
        for (size_t  k = 0; k < 10000; ++k) {
            BOOST_CHECK(rnd.GetRand() <= limit);
        }
    }
    catch (...) {
        return;
    }
}


BOOST_AUTO_TEST_CASE(TestLFGRange)
{
    try {
        CRandom     rnd;
        BOOST_CHECK(rnd.GetRandMethod() == CRandom::eGetRand_LFG);

        rnd.Randomize();
        const CRandom::TValue low_limit = 17;
        const CRandom::TValue up_limit = 517;
        for (size_t  k = 0; k < 10000; ++k) {
            CRandom::TValue     value = rnd.GetRand(low_limit, up_limit);
            BOOST_CHECK(value >= low_limit);
            BOOST_CHECK(value <= up_limit);
        }
    }
    catch (const CException &  excpt) {
        BOOST_FAIL("Error in LFG range test: " + string(excpt.what()));
    }
    catch (...) {
        BOOST_FAIL("Unknown error in LFG range test");
    }
}


BOOST_AUTO_TEST_CASE(TestSysRange)
{
    try {
        CRandom     rnd(CRandom::eGetRand_Sys);
        BOOST_CHECK(rnd.GetRandMethod() == CRandom::eGetRand_Sys);

        const CRandom::TValue low_limit = 17;
        const CRandom::TValue up_limit = 517;
        for (size_t  k = 0; k < 10000; ++k) {
            CRandom::TValue     value = rnd.GetRand(low_limit, up_limit);
            BOOST_CHECK(value >= low_limit);
            BOOST_CHECK(value <= up_limit);
        }
    }
    catch (...) {
        return;
    }
}


BOOST_AUTO_TEST_CASE(TestSpeedRate)
{
    try {
        CRandom     sys_rnd(CRandom::eGetRand_Sys);
        BOOST_CHECK(sys_rnd.GetRandMethod() == CRandom::eGetRand_Sys);

        CRandom     lfg_rnd;
        BOOST_CHECK(lfg_rnd.GetRandMethod() == CRandom::eGetRand_LFG);
        lfg_rnd.Randomize();

        CStopWatch  lfg_watch;
        lfg_watch.Start();
        for (size_t  k = 0; k < 10000; ++k) {
            lfg_rnd.GetRand();
        }
        double      lfg_elapsed = lfg_watch.Elapsed();

        CStopWatch  sys_watch;
        sys_watch.Start();
        for (size_t  k = 0; k < 10000; ++k) {
            sys_rnd.GetRand();
        }
        double      sys_elapsed = sys_watch.Elapsed();

        if (lfg_elapsed > 0)
            LOG_POST("10000 random generation; LFG took " <<
                     lfg_elapsed << " seconds; "
                     "SYS took " <<
                     sys_elapsed << " seconds; "
                     "Rate (sys/lfg): " << sys_elapsed/lfg_elapsed);
    }
    catch (...) {
        return;
    }
}

