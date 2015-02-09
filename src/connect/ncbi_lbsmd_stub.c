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
 *   Dummy LBSMD mapper for non-UNIX and non-inhouse platforms.
 *
 */

#include "ncbi_lbsmd.h"


/*ARGSUSED*/
const SSERV_VTable* SERV_LBSMD_Open(SERV_ITER    iter,
                                    SSERV_Info** info,
                                    HOST_INFO*   host_info,
                                    int/*bool*/  dispd_to_follow)
{
    return 0;
}


extern const char* LBSMD_GetConfig(void)
{
    return 0;
}


/*ARGSUSED*/
extern ESwitch LBSMD_FastHeapAccess(ESwitch sw/*ignored*/)
{
    /* ignore any new settings, always return "not implemented" */
    return eDefault;
}


/*ARGSUSED*/
extern HEAP LBSMD_GetHeapCopy(TNCBI_Time time/*ignored*/)
{
    return 0;
}


/*ARGSUSED*/
extern const char* LBSMD_GetHostParameter(unsigned int host,
                                          const char*  name)
{
    return 0;
}


/*ARGSUSED*/
unsigned int LBSMD_GetLocalHostAddress(const void* v)
{
    return SOCK_GetLocalHostAddress(eDefault);
}


/*ARGSUSED*/
int LBSM_HINFO_CpuCount(const HOST_INFO hinfo)
{
    return -1;
}


/*ARGSUSED*/
int LBSM_HINFO_CpuUnits(const HOST_INFO hinfo)
{
    return -1;
}


/*ARGSUSED*/
double LBSM_HINFO_CpuClock(const HOST_INFO hinfo)
{
    return 0.0;
}


/*ARGSUSED*/
int LBSM_HINFO_TaskCount(const HOST_INFO hinfo)
{
    return -1;
}


/*ARGSUSED*/
int LBSM_HINFO_Memusage(const HOST_INFO hinfo, double memusage[5])
{
    return 0/*failure*/;
}


/*ARGSUSED*/
int LBSM_HINFO_MachineParams(const HOST_INFO hinfo, SHINFO_Params* p)
{
    return 0/*failure*/;
}


/*ARGSUSED*/
int/*bool*/ LBSM_HINFO_LoadAverage(const HOST_INFO hinfo, double lavg[2])
{
    return 0/*failure*/;
}


/*ARGSUSED*/
int/*bool*/ LBSM_HINFO_Status(const HOST_INFO hinfo, double status[2])
{
    return 0/*failure*/;
}
