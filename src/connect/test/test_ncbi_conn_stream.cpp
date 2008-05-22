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
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* This header must go last */
#include "test_assert.h"

#ifdef NCBI_OS_UNIX
#  define DEV_NULL "/dev/null"
#else
#  define DEV_NULL "NUL"
#endif // NCBI_OS_UNIX


static const size_t kBufferSize = 512*1024;


BEGIN_NCBI_SCOPE


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


int main(int argc, const char* argv[])
{
    USING_NCBI_SCOPE;
    CNcbiRegistry* reg;
    TFCDC_Flags flag = 0;
    SConnNetInfo* net_info;
    size_t i, j, k, l, m, n, size;
    const char* env = getenv("CONN_DEBUG_PRINTOUT");

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);
    if (argc <= 1)
        g_NCBI_ConnectRandomSeed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    else
        g_NCBI_ConnectRandomSeed = atoi(argv[1]);
    CORE_LOGF(eLOG_Note, ("Random SEED = %u", g_NCBI_ConnectRandomSeed));
    srand(g_NCBI_ConnectRandomSeed);

    SetDiagTrace(eDT_Enable);
    SetDiagPostLevel(eDiag_Info);
    SetDiagPostFlag(eDPF_All);
    reg = s_CreateRegistry();
    CONNECT_Init(reg);

    // short explanatory message
    LOG_POST(Info << "Test 0: Checking error log setup");
    ERR_POST(Info << "Test log message using C++ Toolkit posting");
    CORE_LOG(eLOG_Note, "Another test message using C Toolkit posting");
    LOG_POST(Info << "Test 0 completed\n");

    LOG_POST("Test 1 of 6: Memory stream");
    // Testing memory stream out-of-sequence interleaving operations
    m = (rand() & 0x00FF) + 1;
    size = 0;
    for (n = 0;  n < m;  n++) {
        CConn_MemoryStream* ms = 0;
        string data, back;
        size_t sz = 0;
#if 0
        LOG_POST("  Micro-test " << (int) n << " of " << (int) m << " start");
#endif
        k = (rand() & 0x00FF) + 1;
        for (i = 0;  i < k;  i++) {
            l = (rand() & 0x00FF) + 1;
            string bit;
            bit.resize(l);
            for (j = 0;  j < l;  j++) {
                bit[j] = "0123456789"[rand() % 10];
            }
#if 0
            LOG_POST("    Data bit at " << (unsigned long) sz <<
                     ", " << (unsigned long) l << " byte(s) long: " << bit);
#endif
            sz += l;
            data += bit;
            if (ms)
                assert(*ms << bit);
            else if (i == 0) {
                switch (n % 4) {
                case 0:
#if 0
                    LOG_POST("  CConn_MemoryStream()");
#endif
                    ms = new CConn_MemoryStream;
                    assert(*ms << bit);
                    break;
                case 1:
                {{
                    BUF buf = 0;
                    assert(BUF_Write(&buf, bit.data(), l));
#if 0
                    LOG_POST("  CConn_MemoryStream(BUF)");
#endif
                    ms = new CConn_MemoryStream(buf, eTakeOwnership);
                    break;
                }}
                default:
                    break;
                }
            }
        }
        switch (n % 4) {
        case 2:
#if 0
            LOG_POST("  CConn_MemoryStream(" <<
                     (unsigned long) data.size() << ')');
#endif
            ms = new CConn_MemoryStream(data.data(),data.size(), eNoOwnership);
            break;
        case 3:
        {{
            BUF buf = 0;
            assert(BUF_Append(&buf, data.data(), data.size()));
#if 0
            LOG_POST("  CConn_MemoryStream(BUF, " <<
                     (unsigned long) data.size() << ')');
#endif
            ms = new CConn_MemoryStream(buf, eTakeOwnership);
            break;
        }}
        default:
            break;
        }
        assert(ms);
        if (!(rand() & 1)) {
            assert(*ms << endl);
            *ms >> back;
            assert(ms->good());
            SetDiagTrace(eDT_Disable);
            *ms >> ws;
            SetDiagTrace(eDT_Enable);
            ms->clear();
        } else
            ms->ToString(&back);
#if 0
        LOG_POST("  Data size=" << (unsigned long) data.size());
        LOG_POST("  Back size=" << (unsigned long) back.size());
        for (i = 0;  i < data.size()  &&  i < back.size(); i++) {
            if (data[i] != back[i]) {
                LOG_POST("  Data differ at pos " << (unsigned long) i <<
                         ": '" << data[i] << "' vs '" << back[i] << '\'');
                break;
            }
        }
#endif
        assert(back == data);
        size += sz;
        delete ms;
#if 0
        LOG_POST("  Micro-test " << (int) n << " of " << (int) m << " finish");
#endif
    }
    LOG_POST("Test 1 completed in " <<
             (int) m    << " iteration(s) with " <<
             (int) size << " byte(s) transferred\n");

    LOG_POST("Test 2 of 6:  FTP download");
    if (!(net_info = ConnNetInfo_Create(0)))
        ERR_POST(Fatal << "Cannot create net info");
    if (env) {
        if (    strcasecmp(env, "1")    == 0  ||
                strcasecmp(env, "TRUE") == 0  ||
                strcasecmp(env, "SOME") == 0)
            flag |= eFCDC_LogControl;
        else if (strcasecmp(env, "DATA") == 0)
            flag |= eFCDC_LogData;
        else if (strcasecmp(env, "ALL")  == 0)
            flag |= eFCDC_LogAll;
    }
    CConn_FTPDownloadStream ftp("ftp.ncbi.nlm.nih.gov",
                                "Misc/test_ncbi_conn_stream.FTP.data",
                                "ftp"/*default*/, "none"/*default*/,
                                "/toolbox/ncbi_tools++/DATA",
                                0/*port = default*/, flag,
                                0/*offset*/, net_info->timeout);
    ConnNetInfo_Destroy(net_info);
    for (size = 0; ftp.good(); size += ftp.gcount()) {
        char buf[512];
        ftp.read(buf, sizeof(buf));
    }
    ftp.Close();
    if (size) {
        LOG_POST("Test 2 completed: " << (unsigned long) size <<
                 " bytes downloaded via FTP\n");
    } else
        ERR_POST(Fatal << "Test 2 failed");

#if 1
    {{
        // Test for timeouts and memory leaks in unused stream
        STimeout tmo = {8, 0};
        CConn_IOStream* s  =
            new CConn_ServiceStream("ID1", fSERV_Any, 0, 0, &tmo);
        delete s;
    }}
#endif

    LOG_POST("Test 3 of 6: Big buffer bounce");
    CConn_HttpStream ios(0, "User-Header: My header\r\n", 0, 0, 0, 0,
                         fHCC_UrlEncodeArgs | fHCC_AutoReconnect |
                         fHCC_Flushable);

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

    if (!ios.flush()) {
        ERR_POST("Error flushing data");
        return 1;
    }

    ios.read(buf2, kBufferSize + 1);
    streamsize buflen = ios.gcount();

    if (!ios.good() && !ios.eof()) {
        ERR_POST("Error receiving data");
        return 2;
    }

    LOG_POST(buflen << " bytes obtained" << (ios.eof() ? " (EOF)" : ""));
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
        LOG_POST("Test 3 passed\n");

    // Clear EOF condition
    ios.clear();

    LOG_POST("Test 4 of 6: Random bounce");

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

    LOG_POST(buflen << " bytes obtained in " << l << " iteration(s)" <<
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
        LOG_POST("Test 4 passed\n");

    // Clear EOF condition
    ios.clear();

    LOG_POST("Test 5 of 6: Truly binary bounce");

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

    LOG_POST(buflen << " bytes obtained" << (ios.eof() ? " (EOF)" : ""));

    for (i = 0; i < kBufferSize; i++) {
        if (buf2[i] != buf1[i])
            break;
    }
    if (i < kBufferSize)
        ERR_POST("Not entirely bounced, mismatch position: " << i + 1);
    else if ((size_t) buflen > kBufferSize)
        ERR_POST("Sent: " << kBufferSize << ", bounced: " << buflen);
    else
        LOG_POST("Test 5 passed\n");

    delete[] buf1;
    delete[] buf2;

    LOG_POST("Test 6 of 6: NcbiStreamCopy()");

    ofstream ofs(DEV_NULL);
    assert(ofs);

    CConn_HttpStream ifs("http://www.ncbi.nlm.nih.gov", 0,
                        "My-Header: Header\r\n", fHCC_Flushable);
    ifs << "Sample input -- should be ignored";

    if (!ifs.good()  ||  !ifs.flush()  ||  !NcbiStreamCopy(ofs, ifs))
        ERR_POST(Fatal << "Test 6 failed");
    else
        LOG_POST("Test 6 passed\n");

    CORE_SetREG(0);
    delete reg;

    CORE_SetLOG(0);
    CORE_SetLOCK(0);

    return 0/*okay*/;
}
