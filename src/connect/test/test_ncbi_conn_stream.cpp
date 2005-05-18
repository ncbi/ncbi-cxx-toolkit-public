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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Standard test for the CONN-based streams
 *
 */

#include <ncbi_pch.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* This header must go last */
#include "test_assert.h"


BEGIN_NCBI_SCOPE


static const size_t kBufferSize = 512*1024;


static CNcbiRegistry* s_CreateRegistry(void)
{
    CNcbiRegistry* reg = new CNcbiRegistry;

    // Compose a test registry
    reg->Set("ID1", "CONN_" REG_CONN_HOST, DEF_CONN_HOST);
    reg->Set("ID1", "CONN_" REG_CONN_PATH, DEF_CONN_PATH);
    reg->Set("ID1", "CONN_" REG_CONN_ARGS, DEF_CONN_ARGS);
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_HOST,     "www.ncbi.nlm.nih.gov");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_PATH,      "/Service/bounce.cgi");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_ARGS,           "arg1+arg2+arg3");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_REQ_METHOD,     "POST");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_TIMEOUT,        "5.0");
    reg->Set(DEF_CONN_REG_SECTION, REG_CONN_DEBUG_PRINTOUT, "TRUE");
    return reg;
}


END_NCBI_SCOPE


int main(void)
{
    USING_NCBI_SCOPE;
    size_t i, j, k, l, size;

    g_NCBI_ConnectRandomSeed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDENT;
    srand(g_NCBI_ConnectRandomSeed);

    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_All);
    CONNECT_Init(s_CreateRegistry());

    LOG_POST(Info << "Checking error log setup"); // short explanatory mesg
    ERR_POST(Info << "Test log message using C++ Toolkit posting");
    CORE_LOG(eLOG_Note, "Another test message using C Toolkit posting");

    CConn_FTPDownloadStream ftp("ftp.ncbi.nlm.nih.gov",
                                "/toolbox/ncbi_tools++/"
                                "CURRENT/RELEASE_NOTES.html");
    for (size = 0; ftp; size += ftp.gcount()) {
        char buf[512];
        ftp.read(buf, sizeof(buf));
    }
    ftp.Close();
    LOG_POST("Test 0 of 3: " << (unsigned long) size <<
             " bytes downloaded via FTP");

#if 1
    {{
        // Test for timeouts and memory leaks in unused stream
        STimeout tmo = {8, 0};
        CConn_IOStream* s  =
            new CConn_ServiceStream("ID1", fSERV_Any, 0, 0, &tmo);
        delete s;
    }}
#endif

    LOG_POST("Test 1 of 3: Big buffer bounce");
    CConn_HttpStream ios(0, "User-Header: My header\r\n",
                         fHCC_UrlEncodeArgs | fHCC_AutoReconnect);

    char *buf1 = new char[kBufferSize + 1];
    char *buf2 = new char[kBufferSize + 2];

    for (j = 0; j < kBufferSize/1024; j++) {
        for (i = 0; i < 1024 - 1; i++)
            buf1[j*1024 + i] = "0123456789"[rand() % 10];
        buf1[j*1024 + 1024 - 1] = '\n';
    }
    buf1[kBufferSize] = '\0';

    if (!(ios << buf1)) {
        ERR_POST("Error sending data");
        return 1;
    }
    assert(ios.tellp() == (CT_POS_TYPE)((CT_OFF_TYPE)(kBufferSize)));

    ios.read(buf2, kBufferSize + 1);
    streamsize buflen = ios.gcount();

    if (!ios.good() && !ios.eof()) {
        ERR_POST("Error receiving data");
        return 2;
    }

    LOG_POST(Info << buflen << " bytes obtained" <<
             (ios.eof() ? " (EOF)" : ""));
    buf2[buflen] = '\0';

    for (i = 0; i < kBufferSize; i++) {
        if (!buf2[i])
            break;
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST("Not entirely bounced, mismatch position: " << i + 1);
    else if ((size_t) buflen > kBufferSize)
        ERR_POST("Sent: " << kBufferSize << ", bounced: " << buflen);
    else
        LOG_POST(Info << "Test 1 passed");

    // Clear EOF condition
    ios.clear();

    LOG_POST("Test 2 of 3: Random bounce");

    if (!(ios << buf1)) {
        ERR_POST("Error sending data");
        return 1;
    }
    assert(ios.tellp() == (CT_POS_TYPE)((CT_OFF_TYPE)(2*kBufferSize)));

    j = 0;
    buflen = 0;
    for (i = 0, l = 0; i < kBufferSize; i += j, l++) {
        k = rand()%15 + 1;

        if (i + k > kBufferSize + 1)
            k = kBufferSize + 1 - i;
        ios.read(&buf2[i], k);
        j = ios.gcount();
        if (!ios.good() && !ios.eof()) {
            ERR_POST("Error receiving data");
            return 2;
        }
        if (j != k)
            LOG_POST("Bytes requested: " << k << ", received: " << j);
        buflen += j;
        l++;
        if (!j && ios.eof())
            break;
    }

    LOG_POST(Info << buflen << " bytes obtained in " << l << " iteration(s)" <<
             (ios.eof() ? " (EOF)" : ""));
    buf2[buflen] = '\0';

    for (i = 0; i < kBufferSize; i++) {
        if (!buf2[i])
            break;
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST("Not entirely bounced, mismatch position: " << i + 1);
    else if ((size_t) buflen > kBufferSize)
        ERR_POST("Sent: " << kBufferSize << ", bounced: " << buflen);
    else
        LOG_POST(Info << "Test 2 passed");

    // Clear EOF condition
    ios.clear();

    LOG_POST("Test 3 of 3: Truly binary bounce");

    for (i = 0; i < kBufferSize; i++)
        buf1[i] = (char)(255/*rand()%256*/);

    ios.write(buf1, kBufferSize);

    if (!ios.good()) {
        ERR_POST("Error sending data");
        return 1;
    }
    assert(ios.tellp() == (CT_POS_TYPE)((CT_OFF_TYPE)(3*kBufferSize)));

    ios.read(buf2, kBufferSize + 1);
    buflen = ios.gcount();

    if (!ios.good() && !ios.eof()) {
        ERR_POST("Error receiving data");
        return 2;
    }

    LOG_POST(Info << buflen << " bytes obtained" <<
             (ios.eof() ? " (EOF)" : ""));

    for (i = 0; i < kBufferSize; i++) {
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST("Not entirely bounced, mismatch position: " << i + 1);
    else if ((size_t) buflen > kBufferSize)
        ERR_POST("Sent: " << kBufferSize << ", bounced: " << buflen);
    else
        LOG_POST(Info << "Test 3 passed");

    CORE_SetREG(0);
    // Do not delete lock and log here 'cause destructors may need them

    delete[] buf1;
    delete[] buf2;

    return 0/*okay*/;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.37  2005/05/18 15:54:15  lavr
 * Add FTP stream test (#0)
 *
 * Revision 6.36  2005/05/02 16:12:08  lavr
 * Use global random seed
 *
 * Revision 6.35  2004/11/22 20:25:11  lavr
 * "yar" replaced with "www"
 *
 * Revision 6.34  2004/10/26 17:56:42  lavr
 * 'Bounce' does not allow more than 1M, so send less (512k) of test data
 *
 * Revision 6.33  2004/05/17 20:58:22  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 6.32  2004/04/01 14:14:02  lavr
 * Spell "occurred", "occurrence", and "occurring"
 *
 * Revision 6.31  2004/01/27 17:06:45  ucko
 * Reorder headers to ensure hitting the conflicting LOG_DATA definitions
 * in the right order on AIX.
 *
 * Revision 6.30  2004/01/14 20:25:36  lavr
 * Added test of isstream::tellp()
 *
 * Revision 6.29  2003/11/04 03:28:23  lavr
 * Use explicit streamsize->size_t casts in comparisons
 *
 * Revision 6.28  2003/11/04 03:10:24  lavr
 * Remove special s_Read(); use standard istream::read() instead
 *
 * Revision 6.27  2003/05/20 21:22:24  lavr
 * Do not clear() if stream is bad() in s_Read()
 *
 * Revision 6.26  2003/05/14 03:58:43  lavr
 * Match changes in respective APIs of the tests
 *
 * Revision 6.25  2003/04/15 14:06:09  lavr
 * Changed ray.nlm.nih.gov -> ray.ncbi.nlm.nih.gov
 *
 * Revision 6.24  2003/03/25 22:16:38  lavr
 * Show that timeouts are set from CONN stream ctors
 *
 * Revision 6.23  2002/11/22 15:09:40  lavr
 * Replace all occurrences of "ray" with "yar"
 *
 * Revision 6.22  2002/06/10 19:55:11  lavr
 * Take advantage of CONNECT_Init() call
 *
 * Revision 6.21  2002/04/22 20:31:01  lavr
 * s_Read() made independent of the compiler: should work with any now
 *
 * Revision 6.20  2002/03/31 02:37:19  lavr
 * Additional registry entries for ID1 added
 *
 * Revision 6.19  2002/03/30 03:37:21  lavr
 * Added test for memory leak in unused connector
 *
 * Revision 6.18  2002/03/24 16:25:25  lavr
 * Changed "ray" -> "ray.nlm.nih.gov"
 *
 * Revision 6.17  2002/03/22 19:46:37  lavr
 * Test_assert.h made last among the include files
 *
 * Revision 6.16  2002/02/05 21:45:55  lavr
 * Included header files rearranged
 *
 * Revision 6.15  2002/01/28 20:28:28  lavr
 * Changed io_bounce.cgi -> bounce.cgi
 *
 * Revision 6.14  2002/01/16 21:23:15  vakatov
 * Utilize header "test_assert.h" to switch on ASSERTs in the Release mode too
 *
 * Revision 6.13  2001/12/30 19:45:50  lavr
 * 'static' added to every file-scope identifiers
 *
 * Revision 6.12  2001/07/24 18:00:30  lavr
 * New function introduced s_Read (instead of direct use of istream::read)
 * This function is compiler-dependent, and specially-featured for GCC
 *
 * Revision 6.11  2001/04/23 18:03:06  vakatov
 * Artificial cast to get rid of warning in the 64-bit compilation mode
 *
 * Revision 6.10  2001/03/27 23:39:16  lavr
 * Explicit cast to (char) added in buffer filling
 *
 * Revision 6.9  2001/03/24 00:50:06  lavr
 * Log typo correction
 *
 * Revision 6.8  2001/03/24 00:35:21  lavr
 * Two more tests added: randomly sized read test, and binary (-1) read test
 *
 * Revision 6.7  2001/03/22 19:19:17  lavr
 * Buffer size extended; random number generator seeded with current time
 *
 * Revision 6.6  2001/03/02 20:03:17  lavr
 * "../ncbi_priv.h" explained
 *
 * Revision 6.5  2001/01/25 17:12:01  lavr
 * Added: buffers and LOG freed upon program exit
 *
 * Revision 6.4  2001/01/23 23:21:03  lavr
 * Added proper logging
 *
 * Revision 6.3  2001/01/13 00:01:26  lavr
 * Changes in REG_cxx2c() prototype -> appropriate changes in the test
 * Explicit registry at the end
 *
 * Revision 6.2  2001/01/12 05:49:31  vakatov
 * Get rid of unused "argc", "argv" in main()
 *
 * Revision 6.1  2001/01/11 23:09:36  lavr
 * Initial revision
 *
 * ==========================================================================
 */
