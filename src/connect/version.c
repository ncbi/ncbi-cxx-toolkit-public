/* $Id$
 * ==========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *            National Center for Biotechnology Information (NCBI)
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government do not place any restriction on its use or reproduction.
 *  We would, however, appreciate having the NCBI and the author cited in
 *  any work or product based on this material
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 * ==========================================================================
 *
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Versioning
 *
 */

#include "ncbi_config.h"
#include "version.h"
#include <string.h>


/*ARGSUSED*/
const char* g_VersionStr(const char* rev)
{
#ifndef NCBI_PACKAGE
    static const char* s_Version = 0;
    if (!s_Version) {
        if (rev  &&  *rev) {
            static char buf[80];
            const char* s = rev + (*rev == '$' ? strcspn(rev, " \t") : 0);
            size_t len = strspn(s += strspn(s, " \t"), "0123456789");
            const char* t = NETDAEMONS_VERSION;
            if (len  &&  len + strlen(t += strcspn(t, "/[")) < sizeof(buf)) {
                memcpy(buf,       s, len);
                strcpy(buf + len, t);
                s_Version = buf;
                return s_Version;
            }
        }
        s_Version = NETDAEMONS_VERSION;
    }
    return s_Version;
#else
    return NETDAEMONS_VERSION;
#endif /*!NCBI_PACKAGE*/
}
