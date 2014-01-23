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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Simple test for CORE_Sendmail
 *
 */

#include <connect/ncbi_connutil.h>
#include <connect/ncbi_sendmail.h>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include "test_assert.h"  /* This header must go last */

#define TEST_HUGE_BODY_SIZE     (1024*100)


int main(int argc, const char* argv[])
{
    static const char custom_body[] =
        "Subject: Custom sized body\n"
        "\n"
        "Custom sized body\n"
        "0123456789\n"; /* these 11 chars to ignore */
    const char* mx_host;
    SSendMailInfo info;
    const char* retval;
    STimeout mx_tmo;
    char* huge_body;
    short mx_port;
    char val[32];
    FILE* fp;
    size_t n;

    g_NCBI_ConnectRandomSeed = (int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    srand(g_NCBI_ConnectRandomSeed);

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Level   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE(stderr, 0/*false*/);

    ConnNetInfo_GetValue(0, REG_CONN_DEBUG_PRINTOUT, val, sizeof(val),
                         DEF_CONN_DEBUG_PRINTOUT);
    if (ConnNetInfo_Boolean(val)
        ||  (*val  &&  (strcasecmp(val, "all")  == 0  ||
                        strcasecmp(val, "some") == 0  ||
                        strcasecmp(val, "data") == 0))) {
        SOCK_SetDataLoggingAPI(eOn);
    }

    SendMailInfo_InitEx(&info, 0, eCORE_UsernameReal);
    CORE_LOGF(eLOG_Note, ("REAL: <%s>", info.from));
    SendMailInfo_InitEx(&info, 0, eCORE_UsernameLogin);
    CORE_LOGF(eLOG_Note, ("LOGIN: <%s>", info.from));
    SendMailInfo_InitEx(&info, 0, eCORE_UsernameCurrent);
    CORE_LOGF(eLOG_Note, ("CURRENT: <%s>", info.from));

#if 1
    strcpy(val, "@");
    SendMailInfo_InitEx(&info, val, eCORE_UsernameCurrent);
    CORE_LOGF(eLOG_Note, ("@ - <%s>", info.from));

    strcpy(info.from, "user0");
    SendMailInfo_InitEx(&info, info.from, eCORE_UsernameCurrent);
    CORE_LOGF(eLOG_Note, ("user0 - <%s>", info.from));

    strcpy(info.from, "user1@");
    SendMailInfo_InitEx(&info, info.from, eCORE_UsernameCurrent);
    CORE_LOGF(eLOG_Note, ("user1@ - <%s>", info.from));

    strcpy(val, "@host2.net");
    SendMailInfo_InitEx(&info, val, eCORE_UsernameLogin);
    CORE_LOGF(eLOG_Note, ("@host2.net - <%s>", info.from));

    strcpy(val, "user3@host3.net");
    SendMailInfo_InitEx(&info, val, eCORE_UsernameReal);
    CORE_LOGF(eLOG_Note, ("user3@host3.net - <%s>", info.from));

    strcpy(info.from, "user4@host4.net");
    SendMailInfo_InitEx(&info, info.from, eCORE_UsernameReal);
    CORE_LOGF(eLOG_Note, ("user4@host4.net - <%s>", info.from));

    SendMailInfo_InitEx(&info, 0, eCORE_UsernameReal);
    CORE_LOGF(eLOG_Note, ("NULL - <%s>", info.from));

    if ((huge_body = (char*) malloc(TEST_HUGE_BODY_SIZE)) != 0) {

        strcpy(huge_body, "user5@");
        for (n = 0;  n < TEST_HUGE_BODY_SIZE - 6;  n++)
            huge_body[n + 6] = "abcdefghijklmnopqrstuvwxyz."[rand() % 27];
        huge_body[TEST_HUGE_BODY_SIZE - 1] = '\0';
        SendMailInfo_InitEx(&info, huge_body, eCORE_UsernameCurrent);
        CORE_LOGF(eLOG_Note, ("HUGE user5@host - <%s>", info.from));

        SendMailInfo_InitEx(&info, huge_body + 5, eCORE_UsernameLogin);
        CORE_LOGF(eLOG_Note, ("HUGE @host - <%s>", info.from));

        huge_body[4] = '6';
        huge_body[5] = '_';
        for (n = 6;  n < sizeof(info.from) + 1;  n++) {
            if (huge_body[n] == '.')
                huge_body[n]  = '_';
        }
        huge_body[sizeof(info.from) + 1] = '@';
        SendMailInfo_InitEx(&info, huge_body, eCORE_UsernameReal);
        CORE_LOGF(eLOG_Note, ("HUGE user6 - <%s>", info.from));

        huge_body[4] = '7';
        huge_body[sizeof(info.from) - 10]  = '@';
        SendMailInfo_InitEx(&info, huge_body, eCORE_UsernameReal);
        CORE_LOGF(eLOG_Note, ("LONG user7 - <%s>", info.from));

        memcpy(huge_body + sizeof(info.from) - 10, "user8", 5);
        huge_body[sizeof(info.from) << 1] = '\0';
        SendMailInfo_InitEx(&info, huge_body + sizeof(info.from) - 10,
                            eCORE_UsernameReal);
        CORE_LOGF(eLOG_Note, ("LONG user8 - <%s>", info.from));

        SendMailInfo_InitEx(&info, huge_body + sizeof(info.from) + 1,
                            eCORE_UsernameReal);
        CORE_LOGF(eLOG_Note, ("LONG @host - <%s>", info.from));

        free(huge_body);
    }
#endif

    if (argc > 1) {
        CORE_LOG(eLOG_Note, "Special test requested");
        SendMailInfo_InitEx(&info, "@", eCORE_UsernameCurrent);
        strcpy(info.from, "Friend <>");
        info.header = "Sender: \"Your Sender\" <lavr@ncbi.nlm.nih.gov>\n"
                      "Reply-To: Coremake <coremake@ncbi.nlm.nih.gov>";
        if ((fp = fopen(argv[1], "rb")) != 0          &&
            fseek(fp, 0, SEEK_END) == 0               &&
            (n = ftell(fp)) != (size_t)(-1)           &&
            fseek(fp, 0, SEEK_SET) == 0               &&
            (huge_body = (char*) malloc(n + 1)) != 0  &&
            fread(huge_body, n, 1, fp) == 1) {
            huge_body[n] = '\0';
            CORE_LOGF(eLOG_Note, ("Sending file (%lu bytes)",
                                  (unsigned long) n));
            retval = CORE_SendMailEx("lavr", "File", huge_body, &info);
            if (retval) {
                CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
            } else {
                CORE_LOG(eLOG_Note, "Test passed");
            }
        } else
            CORE_LOG_ERRNO(eLOG_Error, errno, "Test failed");
        return 0;
    }

#if 0
    {
        static const char* body[] = {
            "This is a simple test",
            "This is a test with\n.",
            0,
            ".",
            "\n.\n",
            ".\n",
            "",
            "a\nb\nc\nd\n.",
            "a\r\n\rb\r\nc\r\nd\r\n.",
            ".\na"
        };
        static const char* subject[] = {
            0,
            "CORE_SendMail Test",
            "",
        };
        static const char* to[] = {
            "lavr",
            "lavr@pavo",
            " \"Anton Lavrentiev\"   <lavr@pavo>  , lavr, <lavr>   ",
        };
        size_t i, j, k, m = 0;

        CORE_LOG(eLOG_Note, "Phase 1 of 2: Testing CORE_SendMail");

        n = (sizeof(to)/sizeof(to[0]))*
            (sizeof(subject)/sizeof(subject[0]))*
            (sizeof(body)/sizeof(body[0]));
        for (i = 0;  i < sizeof(to) / sizeof(to[0]);  ++i) {
            for (j = 0;  j < sizeof(subject) / sizeof(subject[0]);  ++j)
                for (k = 0;  k < sizeof(body) / sizeof(body[0]);  ++k) {
                    CORE_LOGF(eLOG_Note, ("  Test %u of %u",
                                          (unsigned int) ++m,
                                          (unsigned int) n));
                    retval = CORE_SendMail(to[i], subject[j], body[k]);
                    if (retval != 0)
                        CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
                }
        }
    }
#else
    CORE_LOG(eLOG_Note, "Phase 1 of 2: Skipping CORE_SendMail tests");
#endif

#if 1
    CORE_LOG(eLOG_Note, "Phase 2 of 2: Testing CORE_SendMailEx");

    SendMailInfo_Init(&info);
    mx_host = info.mx_host;
    mx_port = info.mx_port;
    mx_tmo  = info.mx_timeout;

    info.mx_host = "localhost";

    CORE_LOG(eLOG_Note, "Testing bad port");
    info.mx_port = 10/*BAD*/;
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx", "Bad port", &info);
    if (!retval)
        CORE_LOG(eLOG_Fatal, "Test failed");
    CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));

    CORE_LOG(eLOG_Note, "Testing bad protocol");
    info.mx_port = CONN_PORT_FTP;
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx", "Protocol", &info);
    if (!retval)
        CORE_LOG(eLOG_Fatal, "Test failed");
    CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));

    CORE_LOG(eLOG_Note, "Testing timeout");
    info.mx_host = "www.ncbi.nlm.nih.gov";
    info.mx_timeout.sec = 5;
    info.mx_port = CONN_PORT_HTTP;
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx", "Timeout", &info);
    if (!retval)
        CORE_LOG(eLOG_Error, "Test failed");
    else
        CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));

    info.mx_port = mx_port;
    info.mx_timeout = mx_tmo;

    CORE_LOG(eLOG_Note, "Testing bad host");
    info.mx_host = "abrakadabra";
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx", "Bad host", &info);
    if (!retval)
        CORE_LOG(eLOG_Fatal, "Test failed");
    CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));

    info.mx_host = "localhost";

    CORE_LOG(eLOG_Note, "Testing cc");
    info.cc = "vakatov";
    retval = CORE_SendMailEx("", "CORE_SendMailEx", "CC", &info);
    if (retval)
        CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
    CORE_LOG(eLOG_Note, "Test passed");

    CORE_LOG(eLOG_Note, "Testing bcc");
    info.cc = 0;
    info.bcc = "vakatov";
    retval = CORE_SendMailEx(0, "CORE_SendMailEx", "Bcc", &info);
    if (retval)
        CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
    CORE_LOG(eLOG_Note, "Test passed");

    CORE_LOG(eLOG_Note, "Testing huge body");
    info.cc = 0;
    info.bcc = 0;
    if (!(huge_body = (char*) malloc(TEST_HUGE_BODY_SIZE)))
        CORE_LOG(eLOG_Fatal, "Test failed: Cannot allocate memory");
    for (n = 0;  n < TEST_HUGE_BODY_SIZE - 1;  ++n)
        huge_body[n] = "0123456789\nABCDEFGHIJKLMNOPQRSTUVWXYZ ."[rand() % 39];
    huge_body[n] = 0;
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx", huge_body, &info);
    if (retval)
        CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
    if (!(fp = fopen("test_ncbi_sendmail.out", "w")) ||
        fwrite(huge_body, TEST_HUGE_BODY_SIZE - 1, 1, fp) != 1) {
        CORE_LOG(eLOG_Error, "Test failed: Cannot store huge body to file");
    } else {
        fclose(fp);
        CORE_LOG(eLOG_Note, "Success: Check test_ncbi_sendmail.out");
    }
    free(huge_body);

    CORE_LOG(eLOG_Note, "Testing custom headers");
    info.header = "Organization: NCBI/NLM/NIH\nReference: abcdefghijk";
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx", "Custom header",&info);
    if (retval)
        CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
    CORE_LOG(eLOG_Note, "Test passed");

    CORE_LOG(eLOG_Note, "Testing no recipients");
    retval = CORE_SendMailEx(0, "CORE_SendMailEx", "No recipients", &info);
    if (!retval)
        CORE_LOG(eLOG_Fatal, "Test failed");
    CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));

    CORE_LOG(eLOG_Note, "Testing AS-IS message");
    info.mx_options = fSendMail_NoMxHeader;
    retval = CORE_SendMailEx("lavr",
                             "BAD SUBJECT SHOULD NOT APPEAR BUT IGNORED",
                             "From: yourself\n"
                             "To: yourself\n"
                             "Subject: AS-IS message\n"
                             "\n"
                             "AS-IS",
                             &info);
    if (retval)
        CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
    CORE_LOG(eLOG_Note, "Test passed");

    CORE_LOG(eLOG_Note, "Testing AS-IS custom sized message");
    info.body_size = strlen(custom_body) - 11/*to ignore*/;
    retval = CORE_SendMailEx("<lavr@pavo>",
                             "BAD SUBJECT SHOULD NOT APPEAR BUT IGNORED",
                             custom_body,
                             &info);
    if (retval)
        CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
    CORE_LOG(eLOG_Note, "Test passed");

    info.body_size = 0;
    info.mx_options = 0;
    info.mx_host = mx_host;

    CORE_LOG(eLOG_Note, "Testing bad from");
    strcpy(info.from, "blahblah@blahblah");
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx", "Bad from",&info);
    if (!retval)
        CORE_LOG(eLOG_Error, "Test failed");
    else
        CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));

    SendMailInfo_Init(&info);
    CORE_LOG(eLOG_Note, "Testing drop no FQDN option");
    info.mx_options |= fSendMail_StripNonFQDNHost;
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx", "No FQDN", &info);
    if (retval)
        CORE_LOGF(eLOG_Error, ("Test failed: %s", retval));
    else
        CORE_LOG(eLOG_Note, "Test passed");

    CORE_LOG(eLOG_Note, "Testing bad magic");
    info.magic_cookie = 0;
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx", "Bad Magic", &info);
    if (!retval)
        CORE_LOG(eLOG_Fatal, "Test failed");
    CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));
#else
    CORE_LOG(eLOG_Note, "Phase 2 of 2: Skipping CORE_SendMailEx tests");
#endif

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0;
}
