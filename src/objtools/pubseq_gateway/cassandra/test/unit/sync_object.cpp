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
* Author:  Dmitrii Saprykin, NCBI
*
* File Description:
*   Unit test suite for CSatInfoSchemaProvider
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <gtest/gtest.h>
#include <objtools/pubseq_gateway/impl/cassandra/SyncObj.hpp>


BEGIN_IDBLOB_SCOPE

TEST(SSignalHandlerTest, SigIntDelivery) {
    SSignalHandler::s_WatchCtrlCPressed(true);
    ASSERT_FALSE(SSignalHandler::s_CtrlCPressed());
    {
        struct sigaction sa{};
        ASSERT_EQ(sigaction(SIGINT, nullptr, &sa), 0);
        EXPECT_NE(sa.sa_handler, SIG_DFL);
    }
    pid_t pid = fork();
    ASSERT_GE(pid, 0);
    if (pid == 0) {
        raise(SIGINT);
        struct sigaction sa{};
        bool default_handler_verified{false};
        // Current signal handler should be reset to default
        if (sigaction(SIGINT, nullptr, &sa) == 0) {
            default_handler_verified = sa.sa_handler == SIG_DFL;
        }
        if (SSignalHandler::s_CtrlCPressed() && default_handler_verified) {
            _exit(0);
        }
        else {
            _exit(1);
        }
    }
    else {
        int status{-1};
        waitpid(pid, &status, 0);
        ASSERT_TRUE(WIFEXITED(status));
        EXPECT_EQ(WEXITSTATUS(status), 0);
    }
    // Lets verify signal handler reset
    SSignalHandler::s_WatchCtrlCPressed(false);
    ASSERT_FALSE(SSignalHandler::s_CtrlCPressed());
    {
        struct sigaction sa{};
        ASSERT_EQ(sigaction(SIGINT, nullptr, &sa), 0);
        EXPECT_EQ(sa.sa_handler, SIG_DFL);
    }
}

END_IDBLOB_SCOPE
