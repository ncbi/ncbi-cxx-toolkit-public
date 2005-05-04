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
 *   Dummy LBSMD mapper for non-UNIX and non-inhouse platforms.
 *
 */

#include "ncbi_lbsmd.h"
#include <connect/ncbi_service_misc.h>


const SSERV_VTable* SERV_LBSMD_Open(SERV_ITER iter,
                                    SSERV_Info** info, HOST_INFO* hinfo)
{
    return 0;
}


extern char* LBSMD_GetConfig(void)
{
    return 0;
}


/*ARGSUSED*/
extern ESwitch LBSM_KeepHeapAttached(ESwitch sw/*ignored*/)
{
    /* ignore any new settings, always return Off */
    return eOff;
}


int LBSM_HINFO_CpuCount(const void* load_ptr)
{
    return -1;
}


int LBSM_HINFO_TaskCount(const void* load_ptr)
{
    return -1;
}


int/*bool*/ LBSM_HINFO_LoadAverage(const void* load_ptr, double lavg[2])
{
    return 0/*failure*/;
}


int/*bool*/ LBSM_HINFO_Status(const void* load_ptr, double status[2])
{
    return 0/*failure*/;
}


int/*bool*/ LBSM_HINFO_BLASTParams(const void* load_ptr, unsigned int blast[8])
{
    return 0/*failure*/;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.7  2005/05/04 16:16:08  lavr
 * +<connect/ncbi_service_misc.h>, +LBSMD_GetConfig(), +LBSM_KeepHeapAttached()
 *
 * Revision 6.6  2003/08/11 19:08:56  lavr
 * Mention "non-inhouse" in file description
 *
 * Revision 6.5  2002/10/28 20:12:57  lavr
 * Module renamed and host info API included
 *
 * Revision 6.4  2002/10/11 19:52:57  lavr
 * +SERV_LBSMD_GetConfig()
 *
 * Revision 6.3  2002/04/13 06:40:44  lavr
 * Few tweaks to reduce the number of syscalls made
 *
 * Revision 6.2  2001/09/10 21:25:35  lavr
 * Unimportant code style compliance change
 *
 * Revision 6.1  2000/10/06 18:06:03  lavr
 * Initial revision
 *
 * ==========================================================================
 */
