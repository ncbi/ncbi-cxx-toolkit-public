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
 *   NCBI host info contstuctor and getters
 *
 */

#include "ncbi_lbsmd.h"
#include "ncbi_host_infop.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
/* Not defined on MacOS.9 :-( */
#  define M_PI 3.14159265358979323846
#endif


typedef struct SHostInfoTag {
    const char* env;
    double      pad;    /* for proper 'hinfo' alignment; also as a magic */
    char        hinfo[1];
} SHOST_Info;


HOST_INFO HINFO_Create(const void* hinfo, size_t hinfo_size, const char* env)
{
    SHOST_Info* host_info;

    if (!hinfo)
        return 0;
    if (env && !*env)
        env = 0;
    hinfo_size += sizeof(*host_info);
    if (!(host_info = (SHOST_Info*)malloc(hinfo_size + (env ? strlen(env):0))))
        return 0;
    if (env)
        strcpy((char*) host_info + sizeof(*host_info) - 1 + hinfo_size, env);
    else
        host_info->env = 0;
    host_info->pad = M_PI;
    memcpy(host_info->hinfo, hinfo, hinfo_size);
    return host_info;
}


int HINFO_CpuCount(HOST_INFO host_info)
{
    if (!host_info || host_info->pad != M_PI)
        return -1;
    return LBSM_HINFO_CpuCount(host_info->hinfo);
}


int HINFO_TaskCount(HOST_INFO host_info)
{
    if (!host_info || host_info->pad != M_PI)
        return -1;
    return LBSM_HINFO_TaskCount(host_info->hinfo);
}


int/*bool*/ HINFO_LoadAverage(HOST_INFO host_info, double lavg[2])
{
    if (!host_info || host_info->pad != M_PI)
        return 0;
    return LBSM_HINFO_LoadAverage(host_info->hinfo, lavg);
}


int/*bool*/ HINFO_Status(HOST_INFO host_info, double status[2])
{
    if (!host_info || host_info->pad != M_PI)
        return 0;
    return LBSM_HINFO_Status(host_info->hinfo, status);
}


int/*bool*/ HINFO_BLASTParams(HOST_INFO host_info, unsigned int blast[8])
{
    if (!host_info || host_info->pad != M_PI)
        return 0;
    return LBSM_HINFO_BLASTParams(host_info->hinfo, blast);
}


const char* HINFO_Environment(HOST_INFO host_info)
{
    if (!host_info || host_info->pad != M_PI)
        return 0;
    return host_info->env;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.3  2002/10/28 21:55:38  lavr
 * LBSM_HINFO introduced for readability to replace plain "const void*"
 *
 * Revision 6.2  2002/10/28 20:49:04  lavr
 * Conditionally define M_PI if it is not already defined by <math.h>
 *
 * Revision 6.1  2002/10/28 20:13:45  lavr
 * Initial revision
 *
 * ==========================================================================
 */
