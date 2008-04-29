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
 *   NCBI host info constructor and getters
 *
 */

#include "ncbi_lbsmd.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
/* Not defined on MacOS.9 :-( */
#  define M_PI 3.14159265358979323846
#endif


typedef struct SHostInfoTag {
    const char* env;
    const char* arg;
    const char* val;
    double      pad;    /* for proper 'hinfo' alignment; also as a magic */
} SHOST_Info;


HOST_INFO HINFO_Create(const void* hinfo, size_t hinfo_size, const char* env,
                       const char* arg, const char* val)
{
    SHOST_Info* host_info;
    size_t      size;
    size_t      e_s;
    size_t      a_s;
    size_t      v_s;
    char*       s;

    if (!hinfo)
        return 0;
    e_s = env && *env ? strlen(env) + 1 : 0;
    a_s = arg && *arg ? strlen(arg) + 1 : 0;
    v_s = a_s &&  val ? strlen(val) + 1 : 0;
    size = sizeof(*host_info) + hinfo_size;
    if (!(host_info = (SHOST_Info*) calloc(1, size + e_s + a_s + v_s)))
        return 0;
    memcpy((char*) host_info + sizeof(*host_info), hinfo, hinfo_size);
    s = (char*) host_info + size;
    if (e_s) {
        host_info->env = (const char*) memcpy(s, env, e_s);
        s += e_s;
    }
    if (a_s) {
        host_info->arg = (const char*) memcpy(s, arg, a_s);
        s += a_s;
    }
    if (v_s) {
        host_info->val = (const char*) memcpy(s, val, v_s);
        s += v_s;
    }
    host_info->pad = M_PI;
    return host_info;
}


int HINFO_CpuCount(const HOST_INFO host_info)
{
    if (!host_info || host_info->pad != M_PI)
        return -1;
    return LBSM_HINFO_CpuCount((const char*) host_info + sizeof(*host_info));
}


int HINFO_CpuUnits(const HOST_INFO host_info)
{
    if (!host_info || host_info->pad != M_PI)
        return -1;
    return LBSM_HINFO_CpuUnits((const char*) host_info + sizeof(*host_info));
}


double HINFO_CpuClock(const HOST_INFO host_info)
{
    if (!host_info || host_info->pad != M_PI)
        return 0.0;
    return LBSM_HINFO_CpuClock((const char*) host_info + sizeof(*host_info));
}

int HINFO_TaskCount(const HOST_INFO host_info)
{
    if (!host_info || host_info->pad != M_PI)
        return -1;
    return LBSM_HINFO_TaskCount((const char*) host_info + sizeof(*host_info));
}
 

int HINFO_Memusage(const HOST_INFO host_info, double memusage[5])
{
    if (!host_info || host_info->pad != M_PI)
        return -1;
    return LBSM_HINFO_Memusage((const char*) host_info + sizeof(*host_info),
                               memusage);
}


int/*bool*/ HINFO_LoadAverage(const HOST_INFO host_info, double lavg[2])
{
    if (!host_info || host_info->pad != M_PI)
        return 0;
    return LBSM_HINFO_LoadAverage((const char*) host_info + sizeof(*host_info),
                                  lavg);
}


int/*bool*/ HINFO_Status(const HOST_INFO host_info, double status[2])
{
    if (!host_info || host_info->pad != M_PI)
        return 0;
    return LBSM_HINFO_Status((const char*) host_info + sizeof(*host_info),
                             status);
}


const char* HINFO_Environment(const HOST_INFO host_info)
{
    if (!host_info || host_info->pad != M_PI)
        return 0;
    return host_info->env;
}


const char* HINFO_AffinityArgument(const HOST_INFO host_info)
{
    if (!host_info || host_info->pad != M_PI)
        return 0;
    return host_info->arg;
}


const char* HINFO_AffinityArgvalue(const HOST_INFO host_info)
{
    if (!host_info || host_info->pad != M_PI)
        return 0;
    return host_info->val;
}
