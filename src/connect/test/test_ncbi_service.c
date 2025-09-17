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
 *   Service API test for profiling
 *
 */

#include <connect/ncbi_tls.h>
#include "../ncbi_ansi_ext.h"
#include "../ncbi_once.h"
#include "../ncbi_lbsmd.h"
#include <stdlib.h>

#include "test_assert.h"  /* This header must go last */


static unsigned int s_Resolve(const char* name, TSERV_Type types)
{
    static void* /*bool*/ s_Once = 0/*false*/;
    SConnNetInfo* net_info = 0;
    unsigned int  count;
    SERV_ITER     iter;

    if (CORE_Once(&s_Once))
        SOCK_SetupSSL(NcbiSetupTls);
    net_info = ConnNetInfo_Create(name);
    if (name  &&  (!*name  ||  strpbrk(name, "?*[")))
        types |= fSERV_Promiscuous;

    iter = SERV_OpenP(name, types,
                      0, 0, 0.0, net_info, NULL, 0, 0, NULL, NULL);
    name = SERV_MapperName(iter);
    CORE_LOGF(eLOG_Note, ("Mapper name: %s", name ? name : "<NONE>"));
    for (count = 0;  iter;  ++count) {
        SSERV_InfoCPtr info;
        char*          istr;
        if (!(info = SERV_GetNextInfo(iter)))
            break;
        if (!(istr = SERV_WriteInfo(info)))
            continue;
        CORE_LOGF(eLOG_Note, ("`%s' %s", SERV_CurrentName(iter), istr));
        free(istr);
    }
    SERV_Close(iter);

    ConnNetInfo_Destroy(net_info);
    return count;
}


static int s_SelfTest(void)
{
    /* Figure out if wildcarding would work (only in-house) */
    const char* env = getenv("FEATURES");
    const char* ptr = env ? strchr(env, "in-house-resources") : 0;
    if (ptr  &&  ptr > env  &&  *ptr == '-')
        ptr = 0;
#define WWW "www.ncbi.nlm.nih.gov"
    assert(!s_Resolve(0, fSERV_Any));
    assert( s_Resolve("bounce", fSERV_Any));
    assert( s_Resolve("bounce*", fSERV_Any)  ||  !ptr);
    assert( s_Resolve("http://"WWW"/Service", fSERV_Any));
    assert(!s_Resolve("//"WWW"/", fSERV_Standalone));
    assert( s_Resolve(WWW":5555", fSERV_Any));
    assert( s_Resolve(WWW":5555", fSERV_Standalone));
    assert( s_Resolve("//"WWW":80/", fSERV_Standalone));
    assert(!s_Resolve("//"WWW":80/Service", fSERV_Standalone));
    assert( s_Resolve("//"WWW"/", fSERV_ReverseDns));
    assert( s_Resolve("//"WWW":80/Service", fSERV_ReverseDns));
    assert( s_Resolve("http://"WWW"/Service", fSERV_ReverseDns));
#if !defined(NCBI_OS_MSWIN)  &&  !defined(NCBI_OS_CYGWIN)
    SOCK_SetIPv6API(eOn);
    assert( s_Resolve("http://jiradev01/Service", fSERV_Any));
#endif /*!NCBI_OS_MSWIN && !NCBI_OS_CYGWIN*/
    return 0;
}


int main(int argc, char* argv[])
{
    const char* service = argc > 1 ? argv[1] : 0;
    TSERV_Type  types = fSERV_Any;
    int         n;

    CORE_SetLOGFormatFlags(fLOG_None          | fLOG_Short   |
                           fLOG_OmitNoteLevel | fLOG_DateTime);
    LBSMD_FastHeapAccess(eOn);

    if (!service) {
        CORE_SetLOGFILE_Ex(stderr, eLOG_Note, eLOG_Fatal, 0/*no auto-close*/);
        CORE_LOG(eLOG_Note, "Running a self-test, please ignore error(s) logged");
        if (!(n = s_SelfTest()))
            CORE_LOG(eLOG_Note, "TEST completed successfully");
        CORE_SetLOG(0);
        return n;
    }

    CORE_SetLOGFILE_Ex(stderr, eLOG_Trace, eLOG_Fatal, 0/*no auto-close*/);
    if (argc > 2  &&  strcasecmp(argv[2], "-r") == 0) {
        types |= fSERV_ReverseDns;
        n = 3;
    } else
        n = 2;
    if (n >= argc)
        types |= fSERV_All & ~fSERV_Firewall;
    else do {
        ESERV_Type  type;
        const char* end;
        if (strcasecmp(argv[n], "ANY") == 0)
            continue;
        if (!(end = SERV_ReadType(argv[n], &type))
            ||  *end  ||  type == fSERV_Firewall) {
            CORE_LOGF(eLOG_Warning,
                        ("Bad server type `%s', %s", argv[n],
                         types & fSERV_All ? "skipping" : "assuming ANY"));
        } else {
            assert(type);
            types |= type;
        }
    } while (++n < argc);

    n = s_Resolve(service, types);

    CORE_LOGF(eLOG_Note, ("Server(s) found: %d", n));
    CORE_SetLOG(0);
    return !n;
}
