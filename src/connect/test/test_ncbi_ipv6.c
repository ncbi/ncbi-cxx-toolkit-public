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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "test_assert.h"  /* This header must go last */


int main(int argc, const char* argv[])
{
    TNCBI_IPv6Addr addr, a, b, temp;
    unsigned int n, m;
    const char *str;
    char buf[150];

    g_NCBI_ConnectRandomSeed
        = (unsigned int) time(0) ^ NCBI_CONNECT_SRAND_ADDEND;
    srand(g_NCBI_ConnectRandomSeed);
    if (rand() % 13) {
        for (n = 0;  n < sizeof(addr.octet);  ++n)
            addr.octet[n] = rand() & 0xFF;
    } else
        memset(&addr, 0, sizeof(addr));
    if (rand() % 11)
        m = rand() % (sizeof(addr.octet) * 8 + 1);
    else
        m = (rand() & 1) * 0xFF;
    if (!NcbiAddrToString(buf, sizeof(buf), &addr))
        assert(*buf == '\0');
    printf("Address  = %s/%u\n", buf, m);
    a = addr;
    NcbiIPv6Subnet(&a,                          m);
    if (!NcbiAddrToString(buf, sizeof(buf), &a))
        assert(*buf == '\0');
    printf("Subnet   = %s\n", buf);
    b = addr;
    NcbiIPv6Suffix(&b, sizeof(addr.octet) * 8 - m);
    if (!NcbiAddrToString(buf, sizeof(buf), &b))
        assert(*buf == '\0');
    printf("Suffix   = %s\n", buf);
    for (n = 0;  n < sizeof(addr.octet);  ++n) {
        /* XOR here (instead of OR) to test that there's no overlap */
        temp.octet[n] = a.octet[n] ^ b.octet[n];
    }
    if (!NcbiAddrToString(buf, sizeof(buf), &temp))
        assert(*buf == '\0');
    printf("Combined = %s\n", buf);
    if (m > sizeof(addr.octet) * 8) {
        /* Because "first" and "last" bit are the same in this case */
        assert(memcmp(&a, &addr, sizeof(addr)) == 0);
        assert(memcmp(&b, &addr, sizeof(addr)) == 0);
        assert(NcbiIsEmptyIPv6(&temp));
    } else
        assert(memcmp(&temp, &addr, sizeof(addr)) == 0);

    if (!(str = NcbiStringToAddr(&addr, argv[1], 0))) {
        printf("\"%s\" is not a valid IPv6 address\n", argv[1]);
        return 1;
    }
    if (!NcbiAddrToString(buf, sizeof(buf), &addr)) {
        printf("Cannot print IPv6 address\n");
        return 1;
    }
    printf("\"%.*s\" = %s\n", (int)(str - argv[1]), argv[1], buf);
    if (*str)
        printf("Unparsed part: \"%s\"\n", str);
    if (NcbiAddrToDNS(buf, sizeof(buf), &addr)) {
        printf("Domain = %s\n", buf);
        str = NcbiStringToAddr(&temp, buf, 0);
        assert(str  &&  !*str);
        assert(memcmp(&temp, &addr, sizeof(addr)) == 0);
    }
    return 0;
}
