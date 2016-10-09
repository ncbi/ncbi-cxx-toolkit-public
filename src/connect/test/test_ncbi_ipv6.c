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

#include <connect/ncbi_ipv6.h>
#include <stdio.h>

#include "test_assert.h"  /* This header must go last */


int main(int argc, const char* argv[])
{
    TNCBI_IPv6Addr addr;
    const char *str;
    char buf[80];

    if (!(str = NcbiStringToIPv6(&addr, argv[1], 0))) {
        printf("\"%s\" is not a valid IPv6 address\n", argv[1]);
        return 1;
    }
    if (!NcbiIPv6ToString(buf, sizeof(buf), &addr)) {
        printf("Cannot print IPv6 address\n");
        return 1;
    }
    printf("\"%.*s\" = %s\n", (int)(str - argv[1]), argv[1], buf);
    if (*str)
        printf("Unparsed part: \"%s\"\n", str);
    return 0;
}
