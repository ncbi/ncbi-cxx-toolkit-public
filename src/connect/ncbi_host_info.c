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


typedef struct SHostInfoTag {
    const char*  env;
    double       pad;           /* for proper alignment; also as a magic */
    char         load[1];
} SHOST_Info;


HOST_INFO HINFO_Create(const void* load, size_t size, const char* env)
{
    SHOST_Info* host_info;

    if (!load)
        return 0;
    if (env && !*env)
        env = 0;
    size += sizeof(*host_info);
    if (!(host_info = (SHOST_Info*) malloc(size + (env ? strlen(env) : 0))))
        return 0;
    if (env)
        strcpy((char*) host_info + sizeof(*host_info) - 1 + size, env);
    else
        host_info->env = 0;
    host_info->pad = M_PI;
    memcpy(host_info->load, load, size);
    return host_info;
}


int HINFO_CpuCount(HOST_INFO host_info)
{
    if (!host_info || host_info->pad != M_PI)
        return -1;
    return LBSM_HINFO_CpuCount(host_info->load);
}


int HINFO_TaskCount(HOST_INFO host_info)
{
    if (!host_info || host_info->pad != M_PI)
        return -1;
    return LBSM_HINFO_TaskCount(host_info);
}


int/*bool*/ HINFO_LoadAverage(HOST_INFO host_info, double lavg[2])
{
    if (!host_info || host_info->pad != M_PI)
        return 0;
    return LBSM_HINFO_LoadAverage(host_info->load, lavg);
}


int/*bool*/ HINFO_Status(HOST_INFO host_info, double status[2])
{
    if (!host_info || host_info->pad != M_PI)
        return 0;
    return LBSM_HINFO_Status(host_info->load, status);
}


int/*bool*/ HINFO_BLASTParams(HOST_INFO host_info, unsigned int blast[8])
{
    if (!host_info || host_info->pad != M_PI)
        return 0;
    return LBSM_HINFO_BLASTParams(host_info->load, blast);
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
 * Revision 6.1  2002/10/28 20:13:45  lavr
 * Initial revision
 *
 * ==========================================================================
 */
