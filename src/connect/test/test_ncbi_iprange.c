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
 *   Test suite for "ncbi_iprange.[ch]"
 *
 */

#include <ncbiconf.h>
#include "../ncbi_priv.h"
#include <connect/ncbi_iprange.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* This header must go last */
#include <common/test_assert.h>


int main(int argc, const char* argv[])
{
    int n, err = 0;

    CORE_SetLOGFormatFlags(fLOG_Short | fLOG_OmitNoteLevel);
    CORE_SetLOGFILE(stderr, 0/*no auto-close*/);

    for (n = 1;  n < argc;  ++n) {
        SIPRange range;
        if (NcbiParseIPRange(&range, argv[n])) {
            char buf[150];
            const char* back = NcbiDumpIPRange(&range, buf, sizeof(buf));
            CORE_LOGF(eLOG_Note,
                      ("%d: `%s' = %s", n, argv[n], back));
        } else {
            CORE_LOGF(eLOG_Error,
                      ("%d: Cannot parse `%s'", n, argv[n]));
            ++err;
        }
    }

    CORE_SetLOG(0);
    return err;
}
