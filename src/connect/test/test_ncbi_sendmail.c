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
 *   Simple test for CORE_Sendmail
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.5  2001/03/07 20:49:29  lavr
 * Forgotten #include <string.h> added
 *
 * Revision 6.4  2001/03/06 04:32:31  lavr
 * Custom header test added
 *
 * Revision 6.3  2001/03/02 20:01:53  lavr
 * "../ncbi_priv.h" explained
 *
 * Revision 6.2  2001/02/28 17:48:07  lavr
 * Huge body test added
 *
 * Revision 6.1  2001/02/28 00:53:45  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include "../ncbi_priv.h"               /* CORE logging facilities */
#include <connect/ncbi_sendmail.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#define TEST_HUGE_BODY_SIZE     (1024*100)


int main(void)
{
    const char* body[] = {
        "This is a simple test",
        "This is a test with\n.",
        0,
        ".",
        "\n.\n",
        ".\n",
        "",
        "a\nb\nc\nd\n.",
        ".\na"
    };
    const char* subject[] = {
        0,
        "CORE_SendMail Test",
        "",
    };
    const char* to[] = {
        "lavr",
        "lavr@pavo"
    };
    size_t i, j, k, n, m;
    const char* mx_host;
    SSendMailInfo info;
    const char* retval;
    STimeout mx_tmo;
    char* huge_body;
    short mx_port;
    FILE *fp;

    CORE_SetLOGFILE(stderr, 0/*false*/);
    srand(time(0));

    CORE_LOG(eLOG_Note, "Phase 1 of 2: Testing CORE_SendMail");

    n = (sizeof(to)/sizeof(to[0]))*
        (sizeof(subject)/sizeof(subject[0]))*
        (sizeof(body)/sizeof(body[0]));
    m = 0;
    for (i = 0; i < sizeof(to)/sizeof(to[0]); i++)
        for (j = 0; j < sizeof(subject)/sizeof(subject[0]); j++)
            for (k = 0; k < sizeof(body)/sizeof(body[0]); k++) {
                CORE_LOGF(eLOG_Note, ("Test %u of %u",
                                      (unsigned)(++m), (unsigned)n));
                retval = CORE_SendMail(to[i], subject[j], body[k]);
                if (retval != 0)
                    CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
            }

    CORE_LOG(eLOG_Note, "Phase 2 of 2: Testing CORE_SendMailEx");

    SendMailInfo_Init(&info);
    mx_port = info.mx_port;
    mx_host = info.mx_host;
    mx_tmo  = info.mx_timeout;

    CORE_LOG(eLOG_Note, "Testing bad port");
    info.mx_port = 10;
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx Test", "Test", &info);
    if (!retval)
        CORE_LOG(eLOG_Fatal, "Test failed");
    CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));

    CORE_LOG(eLOG_Note, "Testing bad protocol");
    info.mx_port = 21;
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx Test", "Test", &info);
    if (!retval)
        CORE_LOG(eLOG_Fatal, "Test failed");
    CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));

    CORE_LOG(eLOG_Note, "Testing timeout");
    info.mx_timeout.sec = 5;
    info.mx_port = 80;
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx Test", "Test", &info);
    if (!retval)
        CORE_LOG(eLOG_Error, "Test failed");
    else
        CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));

    info.mx_port = mx_port;
    info.mx_timeout = mx_tmo;

    CORE_LOG(eLOG_Note, "Testing bad host");
    info.mx_host = "abrakadabra";
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx Test", "Test", &info);
    if (!retval)
        CORE_LOG(eLOG_Fatal, "Test failed");
    CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));

    info.mx_host = mx_host;

    CORE_LOG(eLOG_Note, "Testing cc");
    info.cc = "vakatov";
    retval = CORE_SendMailEx("", "CORE_SendMailEx Test", "Test", &info);
    if (retval)
        CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
    CORE_LOG(eLOG_Note, "Test passed");

    CORE_LOG(eLOG_Note, "Testing bcc");
    info.cc = 0;
    info.bcc = "vakatov";
    retval = CORE_SendMailEx(0, "CORE_SendMailEx Test", "Test", &info);
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
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx Test", huge_body, &info);
    if (retval)
        CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
    if (!(fp = fopen("test_ncbi_sendmail.out", "w")) ||
        fwrite(huge_body, TEST_HUGE_BODY_SIZE, 1, fp) != 1) {
        CORE_LOG(eLOG_Error, "Test failed: Cannot store huge body to file");
    } else {
        fclose(fp);
        CORE_LOG(eLOG_Note, "Success: Check test_ncbi_sendmail.out");
    }       
    free(huge_body);

    CORE_LOG(eLOG_Note, "Testing custom headers");
    info.header = "Organization: NCBI/NLM/NIH\nReference: abcdefghijk";
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx Test",
                             "Custom header test", &info);
    if (retval)
        CORE_LOGF(eLOG_Fatal, ("Test failed: %s", retval));
    CORE_LOG(eLOG_Note, "Test passed");

    CORE_LOG(eLOG_Note, "Testing no recipients");
    retval = CORE_SendMailEx(0, "CORE_SendMailEx Test", "Test", &info);
    if (!retval)
        CORE_LOG(eLOG_Fatal, "Test failed");
    CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));

    CORE_LOG(eLOG_Note, "Testing bad from");
    strcpy(info.from, "blahblah@blahblah");
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx Test", "Test", &info);
    if (!retval)
        CORE_LOG(eLOG_Error, "Test failed");
    else
        CORE_LOGF(eLOG_Error, ("Test passed: %s", retval));
    
    CORE_LOG(eLOG_Note, "Testing bad magic");
    info.magic_number = 0;
    retval = CORE_SendMailEx("lavr", "CORE_SendMailEx Test", "Test", &info);
    if (!retval)
        CORE_LOG(eLOG_Fatal, "Test failed");
    CORE_LOGF(eLOG_Note, ("Test passed: %s", retval));
    
    CORE_LOG(eLOG_Note, "Test completed");
    return 0;
}
