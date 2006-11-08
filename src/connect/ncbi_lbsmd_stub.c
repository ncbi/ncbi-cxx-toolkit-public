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


/*ARGSUSED*/
const SSERV_VTable *SERV_LBSMD_Open(SERV_ITER    iter,
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


/*ARGSUSED*/ /*DEPRECATED*/
extern ESwitch LBSMD_KeepHeapAttached(ESwitch sw/*ignored*/)
{
    /* ignore any new settings, always return "Off" here */
    return eOff;
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
int LBSM_HINFO_CpuCount(LBSM_HINFO hinfo)
{
    return -1;
}


/*ARGSUSED*/
int LBSM_HINFO_TaskCount(LBSM_HINFO hinfo)
{
    return -1;
}


/*ARGSUSED*/
int/*bool*/ LBSM_HINFO_LoadAverage(LBSM_HINFO hinfo, double lavg[2])
{
    return 0/*failure*/;
}


/*ARGSUSED*/
int/*bool*/ LBSM_HINFO_Status(LBSM_HINFO hinfo, double status[2])
{
    return 0/*failure*/;
}


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.12  2006/11/08 17:15:30  lavr
 * +LBSMD_FastHeapAccess()
 *
 * Revision 6.11  2006/10/23 21:17:34  lavr
 * -LBSM_KeepHeapAttached (has been deprecated a long ago)
 *
 * Revision 6.10  2006/03/06 20:28:21  lavr
 * Comments;  use proper LBSM_HINFO in all getters
 *
 * Revision 6.9  2006/03/06 14:42:04  lavr
 * SERV_LBSMD_Open() -- use new proto
 *
 * Revision 6.8  2006/03/05 17:44:12  lavr
 * Private API changes; cached HEAP copy; BLAST counters dropped
 *
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
