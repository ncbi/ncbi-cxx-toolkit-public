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
 *   Top-level API to resolve NCBI service name to the server meta-address.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.22  2001/06/19 19:12:01  lavr
 * Type change: size_t -> TNCBI_Size; time_t -> TNCBI_Time
 *
 * Revision 6.21  2001/05/24 21:28:12  lavr
 * Timeout for skip servers increased to 1 year period
 *
 * Revision 6.20  2001/05/17 15:02:51  lavr
 * Typos corrected
 *
 * Revision 6.19  2001/04/24 21:37:26  lavr
 * New code for: SERV_MapperName() and SERV_Penalize().
 *
 * Revision 6.18  2001/03/21 21:23:30  lavr
 * Explicit type conversion size_t -> unsigned in printf
 *
 * Revision 6.17  2001/03/20 22:03:32  lavr
 * BUGFIX in SERV_Print (miscalculation of buflen for accepted server types)
 *
 * Revision 6.16  2001/03/06 23:55:25  lavr
 * SOCK_gethostaddr -> SOCK_gethostbyname
 *
 * Revision 6.15  2001/03/05 23:10:29  lavr
 * SERV_WriteInfo takes only one argument now
 *
 * Revision 6.14  2001/03/02 20:09:51  lavr
 * Support added for SERV_LOCALHOST as preferred_host.
 *
 * Revision 6.13  2001/03/01 00:31:23  lavr
 * SERV_OpenSimple now builds SConnNetInfo to use both LBSMD and DISPD.CGI
 *
 * Revision 6.12  2001/02/09 17:33:06  lavr
 * Modified: fSERV_StatelessOnly overrides info->stateless
 *
 * Revision 6.11  2001/01/25 17:05:32  lavr
 * Bugfix in SERV_OpenEx: op was not inited to 0
 *
 * Revision 6.10  2001/01/08 23:47:29  lavr
 * (unsigned char) conversion in isspace
 *
 * Revision 6.9  2001/01/08 22:38:34  lavr
 * Numerous patches to code after debugging
 *
 * Revision 6.8  2000/12/29 18:01:27  lavr
 * SERV_Print introduced; pool of skipped services now follows
 * expiration times of services and is updated on that basis.
 *
 * Revision 6.7  2000/12/06 22:20:30  lavr
 * Skip info list is not maintained forever; instead the entries get
 * deleted in accordance with their expiration period
 *
 * Revision 6.6  2000/10/20 17:19:04  lavr
 * SConnNetInfo made 'const' as a parameter to 'SERV_Open*'
 * 'SERV_Update' added as a private interface
 *
 * Revision 6.5  2000/10/05 22:36:21  lavr
 * Additional parameters in call to DISPD mapper
 *
 * Revision 6.4  2000/05/31 23:12:23  lavr
 * First try to assemble things together to get working service mapper
 *
 * Revision 6.3  2000/05/22 16:53:11  lavr
 * Rename service_info -> server_info everywhere (including
 * file names) as the latter name is more relevant
 *
 * Revision 6.2  2000/05/12 21:42:59  lavr
 * Cleaned up for the C++ compilation, etc.
 *
 * Revision 6.1  2000/05/12 18:50:20  lavr
 * First working revision
 *
 * ==========================================================================
 */

#include "ncbi_servicep.h"
#include "ncbi_servicep_lbsmd.h"
#include "ncbi_servicep_dispd.h"
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


SERV_ITER SERV_OpenSimple(const char* service)
{
    SConnNetInfo* info = ConnNetInfo_Create(service);
    SERV_ITER iter = SERV_Open(service, fSERV_Any, 0, info);
    ConnNetInfo_Destroy(info);
    return iter;
}


SERV_ITER SERV_Open(const char* service, TSERV_Type type,
                    unsigned int preferred_host, const SConnNetInfo* info)
{
    return SERV_OpenEx(service, type, preferred_host, info, 0, 0);
}


static int/*bool*/ s_AddSkipInfo(SERV_ITER iter, SSERV_Info* info)
{
    if (iter->n_skip == iter->n_max_skip) {
        SSERV_Info** temp;
        size_t n = iter->n_max_skip + 10;

        if (iter->skip)
            temp = (SSERV_Info**) realloc(iter->skip, sizeof(*temp) * n);
        else
            temp = (SSERV_Info**) malloc(sizeof(*temp) * n);
        if (!temp)
            return 0;

        iter->skip = temp;
        iter->n_max_skip = n;
    }

    iter->skip[iter->n_skip++] = info;
    return 1;
}


SERV_ITER SERV_OpenEx(const char* service, TSERV_Type type,
                      unsigned int preferred_host, const SConnNetInfo* info,
                      const SSERV_Info* const skip[], size_t n_skip)
{
    const SSERV_VTable* op;
    SERV_ITER iter;
    TNCBI_Time t;
    size_t i;

    if (!service || !*service ||
        !(iter = (SERV_ITER) malloc(sizeof(*iter) + strlen(service)+1)))
        return 0;

    iter->service = (char*) iter + sizeof(*iter);
    strcpy((char*) iter->service, service);
    iter->type = type;
    iter->preferred_host =
        preferred_host == SERV_LOCALHOST
        ? SOCK_gethostbyname(0) : preferred_host;
    iter->n_skip = iter->n_max_skip = 0;
    iter->skip = 0;
    iter->last = 0;
    iter->op = 0;
    iter->data = 0;

    t = (TNCBI_Time) time(0);
    for (i = 0; i < n_skip; i++) {
        size_t infolen = SERV_SizeOfInfo(skip[i]);
        SSERV_Info* info = (SSERV_Info*) malloc(infolen);

        if (!info) {
            SERV_Close(iter);
            return 0;
        }
        memcpy(info, skip[i], infolen);
        info->time = t + 3600/*hour*/*24/*day*/*365/*year - long enough :-) */;
        if (!s_AddSkipInfo(iter, info)) {
            free(info);
            SERV_Close(iter);
        }
    }
    assert(n_skip == iter->n_skip);

    if (!info) {
        if (!(op = SERV_LBSMD_Open(iter))) {
            /* LBSMD failed in non-DISPD mapping */
            SERV_Close(iter);
            return 0;
        }
    } else {
        if (info->stateless)
            iter->type |= fSERV_StatelessOnly;
        if ((info->lb_disable || !(op = SERV_LBSMD_Open(iter))) &&
            !(op = SERV_DISPD_Open(iter, info))) {
            SERV_Close(iter);
            return 0;
        }
    }
    
    assert(op != 0);
    iter->op = op;
    return iter;
}


static void s_SkipSkip(SERV_ITER iter)
{
    TNCBI_Time t = (TNCBI_Time) time(0);
    size_t i;

    i = 0;
    while (i < iter->n_skip) {
        SSERV_Info *info = iter->skip[i];
        if (info->time < t) {
            if (i < --iter->n_skip)
                memmove(iter->skip + i, iter->skip + i + 1,
                        sizeof(*iter->skip)*(iter->n_skip - i));
            free(info);
        } else
            i++;
    }
}


const SSERV_Info* SERV_GetNextInfo(SERV_ITER iter)
{
    SSERV_Info* info = 0;

    if (!iter)
        return 0;
    /* First, remove all outdated entries from our skip list */
    s_SkipSkip(iter);
    /* Next, obtain a fresh entry from the actual mapper */
    if (iter->op && iter->op->GetNextInfo &&
        (info = (*iter->op->GetNextInfo)(iter)) != 0 &&
        !s_AddSkipInfo(iter, info)) {
        free(info);
        info = 0;
    }
    iter->last = iter->skip[iter->n_skip - 1];
    return info;
}


const char* SERV_MapperName(SERV_ITER iter)
{
    return iter && iter->op ? iter->op->name : 0;
}


int/*bool*/ SERV_Penalize(SERV_ITER iter, double penalty)
{
    if (!iter || !iter->op || !iter->op->Penalize || !iter->last)
        return 0;
    return (*iter->op->Penalize)(iter, penalty);
}


void SERV_Close(SERV_ITER iter)
{
    size_t i;
    if ( !iter )
        return;

    if (iter->op && iter->op->Close)
        (*iter->op->Close)(iter);
    for (i = 0; i < iter->n_skip; i++)
        free(iter->skip[i]);
    if (iter->skip)
        free(iter->skip);
    free(iter);
}


int/*bool*/ SERV_Update(SERV_ITER iter, const char* text)
{
    if (!iter || !iter->op)
        return 0/*not a valid call, failed*/;
    if (!text || !iter->op->Update)
        return 1/*no update provision, success*/;
    return (*iter->op->Update)(iter, text);
}


char* SERV_Print(SERV_ITER iter)
{
    static const char accepted_types[] = "Accepted-Server-Types:";
    TSERV_Type type = (TSERV_Type) (iter->type & ~fSERV_StatelessOnly);
    char buffer[128], *str;
    size_t buflen, i;
    TSERV_Type t;
    BUF buf = 0;
    
    /* Form accepted server types (as customized header) */
    strcpy(buffer, accepted_types);
    buflen = sizeof(accepted_types) - 1;
    for (t = 1; t; t <<= 1) {
        if (type & t) {
            const char *name = SERV_TypeStr((ESERV_Type) t);
            size_t namelen = strlen(name);

            if (namelen) {
                if (buflen + 1 + namelen < sizeof(buffer)) {
                    buffer[buflen++] = ' ';
                    strcpy(&buffer[buflen], name);
                    buflen += namelen;
                } else
                    return 0;
            } else
                break;
        }
    }
    assert(buflen < sizeof(buffer));
    if (buffer[buflen - 1] != ':') {
        if (sizeof(buffer) < buflen + 2)
            return 0;
        strcpy(&buffer[buflen], "\r\n");
        assert(strlen(buffer) == buflen + 2);
        if (!BUF_Write(&buf, buffer, buflen + 2)) {
            BUF_Destroy(buf);
            return 0;
        }
    }
    /* Drop any outdated skip entries */
    s_SkipSkip(iter);
    /* Put all the rest into rejection list */
    for (i = 0; i < iter->n_skip; i++) {
        char *s1, *s2;
        if (!(str = SERV_WriteInfo(iter->skip[i])))
            break;
        /* Remove ugly " T=[0-9]*" (no harm to be there, however) */
        if ((s1 = strstr(str, " T=")) != 0) {
            s2 = s1 + 1;
            while (*s2 && !isspace((unsigned char)(*s2)))
                s2++;
            memmove(s1, s2, strlen(s2) + 1);
        }
        buflen = sprintf(buffer, "Skip-Info-%u: ", (unsigned) i + 1); 
        assert(buflen < sizeof(buffer)-1);
        if (!BUF_Write(&buf, buffer, buflen) ||
            !BUF_Write(&buf, str, strlen(str)) ||
            !BUF_Write(&buf, "\r\n", 2)) {
            free(str);
            break;
        }
        free(str);
    }
    if (i >= iter->n_skip) {
        /* Ok then, we have filled the entire header, <CR><LF> terminated */
        if ((buflen = BUF_Size(buf)) != 0) {
            if ((str = (char*) malloc(buflen + 1)) != 0) {
                if (BUF_Read(buf, str, buflen) != buflen) {
                    free(str);
                    str = 0;
                } else
                    str[buflen] = '\0';
            }
        } else
            str = 0;
    } else
        str = 0;
    BUF_Destroy(buf);
    return str;
}
