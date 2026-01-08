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
 *   Test suite for ncbi_ipv6.[ch]
 *
 */

#include "../ncbi_priv.h"
#include <connect/ncbi_ipv6.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "test_assert.h"  /* This header must go last */


int main(int argc, const char* argv[])
{
    TNCBI_IPv6Addr addr, a, b, temp;
    unsigned int n, m;
    const char* str;
    char buf[150];
    int c;

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Short   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    CORE_SetLOGFILE_Ex(stderr, eLOG_Trace, eLOG_Fatal, 0/*no auto-close*/);

    assert(sizeof(addr) == sizeof(addr.octet));
    if (argc != 2) {
        CORE_LOG(eLOG_Note, "(internal self-test begins)");
        g_NCBI_ConnectRandomSeed
            = (unsigned int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
        srand(g_NCBI_ConnectRandomSeed);
        memset(&addr, 0, sizeof(addr));
        if (rand() % 13) {
            m = rand() & 1 ? 0 : sizeof(addr.octet) - 4;  /* if IPv4 */
            for (n = m;  n < sizeof(addr.octet);  ++n)
                addr.octet[n] = rand() & 0xFF;
            if (m  &&  (rand() & 1))  /* if mapped IPv4 */
                memset(&addr.octet[10], '\xFF', 2 * sizeof(addr.octet[10]));
            assert(!m  ||  NcbiIsIPv4Ex(&addr, 1/*compat okay*/));
        } else {
            m = 0;
            if (rand() & 1)
                memset(&addr, '\xFF', sizeof(addr));
        }
        if (rand() % 11)
            m = rand() % ((sizeof(addr.octet) * 8) + 1);
        else
            m = (rand() & 1) * 0xFF;
        assert(NcbiAddrToString(buf, sizeof(buf), &addr)  ||  *buf == '\0');
        CORE_LOGF(eLOG_Note, ("Address   = %s/%u", buf, m));
        if (rand() & 1) {
            char* p;
            for (p = buf;  *p;  ++p) {
                if (isalpha((unsigned char)(*p))) {
                    assert('a' <= *p  &&  *p <= 'f');
                    if (rand() & 1)
                        *p = toupper((unsigned char)(*p));
                } else
                    assert(*p == ':'  ||  *p == '.'  ||  isdigit((unsigned char) (*p)));
            }
            CORE_LOGF(eLOG_Note, ("Reparsing = %s", buf));
            assert(NcbiStringToAddr(&a, buf, 0));
            assert(memcmp(&addr, &a, sizeof(addr)) == 0);
        } else
            a = addr;
        NcbiIPv6Subnet(&a,                          m);
        assert(NcbiAddrToString(buf, sizeof(buf), &a)  ||  *buf == '\0');
        CORE_LOGF(eLOG_Note, ("Subnet    = %s", buf));
        b = addr;
        NcbiIPv6Suffix(&b, sizeof(addr.octet) * 8 - m);
        assert(NcbiAddrToString(buf, sizeof(buf), &b)  ||  *buf == '\0');
        CORE_LOGF(eLOG_Note, ("Suffix    = %s", buf));
        for (n = 0;  n < sizeof(addr.octet);  ++n) {
            /* XOR here (instead of OR) to test that there's no overlap */
            temp.octet[n] = a.octet[n] ^ b.octet[n];
        }
        assert(NcbiAddrToString(buf, sizeof(buf), &temp)  ||  *buf == '\0');
        CORE_LOGF(eLOG_Note, ("Combined  = %s", buf));
        if (m > sizeof(addr.octet) * 8) {
            /* Because "first" and "last" bit are the same in this case */
            assert(memcmp(&a, &addr, sizeof(addr)) == 0);
            assert(memcmp(&b, &addr, sizeof(addr)) == 0);
            assert(NcbiIsEmptyIPv6(&temp));
        } else {
            if (!m)
                assert(NcbiIsEmptyIPv6(&a));
            else if (m == sizeof(addr.octet) * 8)
                assert(NcbiIsEmptyIPv6(&b));
            assert(memcmp(&temp, &addr, sizeof(addr)) == 0);
        }
        CORE_LOG(eLOG_Note, "(internal self-test ends)\n");
    }

    n = 0;
    for (c = 1;  c < argc;  ++c) {
        int d, q;
        if (!(str = NcbiStringToAddr(&addr, argv[c], 0))) {
            CORE_LOGF(eLOG_Error,
                      ("\"%s\" is not a valid IPv6 address", argv[c]));
            ++n;
            continue;
        }
        if (!NcbiAddrToString(buf, sizeof(buf), &addr)) {
            CORE_LOGF(eLOG_Error,
                      ("Cannot print IPv6 address for \"%s\"", argv[c]));
            ++n;
            continue;
        }
        CORE_LOGF(eLOG_Note,
                  ("\"%.*s\" = %s", (int)(str - argv[c]), argv[c], buf));
        if (*str)
            CORE_LOGF(eLOG_Note, ("Unparsed part: \"%s\"\n", str));
        if (sscanf(str, "/%u%n", &d, &q) >= 1  &&  !str[q]  &&  d >= 0) {
            int/*bool*/ ipv4 = NcbiIsIPv4(&addr);
            unsigned int width = (unsigned int)
                (ipv4 ? sizeof(unsigned int) : sizeof(addr.octet)) * 8;
            m = (unsigned int) d;
            a = addr;
            (ipv4 ? NcbiIPv4Subnet : NcbiIPv6Subnet)(&a, m);
            assert(NcbiAddrToString(buf, sizeof(buf), &a)  ||  *buf == '\0');
            CORE_LOGF(eLOG_Note,
                      ("Subnet    = %s/%u", buf,
                       m >= width ? (unsigned int) width : m));
            b = addr;
            (ipv4 ? NcbiIPv4Suffix : NcbiIPv6Suffix)(&b, (unsigned int)(width - m));
            assert(NcbiAddrToString(buf, sizeof(buf), &b)  ||  *buf == '\0');
            CORE_LOGF(eLOG_Note,
                      ("Suffix    = %s", buf));
        }
        if (NcbiAddrToDNS(buf, sizeof(buf), &addr)) {
            CORE_LOGF(eLOG_Note,
                      ("Domain    = %s", buf));
            str = NcbiStringToAddr(&temp, buf, 0);
            assert(str  &&  !*str);
            assert(memcmp(&temp, &addr, sizeof(addr)) == 0);
        } else
            assert(*buf == '\0');
    }
    if (argc != 2  &&  n == 0)
        CORE_LOG(eLOG_Note, "TEST completed successfully");

    CORE_SetLOG(0);
    return n ? EXIT_FAILURE/*failure*/ : EXIT_SUCCESS/*success*/;
}
