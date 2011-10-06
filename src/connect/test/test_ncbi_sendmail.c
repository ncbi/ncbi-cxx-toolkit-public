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
/* This header must go last */
#include "test_assert.h"


#define TEST_HUGE_BODY_SIZE     (1024*100)


int main(int argc, const char* argv[])
{
    const char custom_body[] =
        "Subject: Custom sized body\n"
        "\n"
        "Custom sized body\n"
        "0123456789\n"; /* these 11 chars to ignore */
    const char* body[] = {
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
    const char* subject[] = {
        0,
        "CORE_SendMail Test",
        "",
    };
    const char* to[] = {
        "lavr",
        "lavr@pavo",
        " \"Anton Lavrentiev\"   <lavr@pavo>  , lavr, <lavr>   ",
    };
    size_t i, j, k, n, m;
    const char* mx_host;
    SSendMailInfo info;
    const char* retval;
    STimeout mx_tmo;
    char* huge_body;
    short mx_port;
    char val[32];
    FILE* fp;

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

    if (argc > 1) {
        CORE_LOG(eLOG_Note, "Special test requested");
        if ((fp = fopen(argv[1], "rb")) != 0          &&
            fseek(fp, 0, SEEK_END) == 0               &&
            (m = ftell(fp)) != (size_t)(-1)           &&
            fseek(fp, 0, SEEK_SET) == 0               &&
            (huge_body = (char*) malloc(m + 1)) != 0  &&
            fread(huge_body, m, 1, fp) == 1) {
            huge_body[m] = '\0';
            CORE_LOGF(eLOG_Note, ("Sending file (%lu bytes)",
                                  (unsigned long) m));
            retval = CORE_SendMail("lavr", "File", huge_body);
            if (retval) {
                CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
            } else {
                CORE_LOG(eLOG_Note, "Test passed");
            }
        } else
            CORE_LOG_ERRNO(eLOG_Error, errno, "Test failed");
        return 0;
    }

#if 1
    CORE_LOG(eLOG_Note, "Phase 1 of 2: Testing CORE_SendMail");

    n = (sizeof(to)/sizeof(to[0]))*
        (sizeof(subject)/sizeof(subject[0]))*
        (sizeof(body)/sizeof(body[0]));
    m = 0;
    for (i = 0; i < sizeof(to)/sizeof(to[0]); i++) {
        for (j = 0; j < sizeof(subject)/sizeof(subject[0]); j++)
            for (k = 0; k < sizeof(body)/sizeof(body[0]); k++) {
                CORE_LOGF(eLOG_Note, ("Test %u of %u",
                                      (unsigned)(++m), (unsigned)n));
                retval = CORE_SendMail(to[i], subject[j], body[k]);
                if (retval != 0)
                    CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
            }
    }
#else
    CORE_LOG(eLOG_Note, "Phase 1 of 2: Skipping CORE_SendMail tests");
#endif

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
    for (i = 0; i < TEST_HUGE_BODY_SIZE - 1; i++)
        huge_body[i] = "0123456789\nABCDEFGHIJKLMNOPQRSTUVWXYZ ."[rand() % 39];
    huge_body[i] = 0;
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
    info.magic_number = 0;
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx", "Bad Magic", &info);
    if (!retval)
        CORE_LOG(eLOG_Fatal, "Test failed");
    CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));

    CORE_LOG(eLOG_Note, "TEST completed successfully");
    CORE_SetLOG(0);
    return 0;
}
