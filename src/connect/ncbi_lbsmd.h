#ifndef CONNECT___NCBI_LBSMD__H
#define CONNECT___NCBI_LBSMD__H

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
 *   Low-level API to resolve NCBI service name to the server meta-address
 *   with the use of NCBI Load-Balancing Service Mapper (LBSMD).
 *
 */

#include "ncbi_servicep.h"
#include <connect/ncbi_heapmgr.h>


#ifdef __cplusplus
extern "C" {
#endif


const SSERV_VTable* SERV_LBSMD_Open(SERV_ITER    iter,
                                    SSERV_Info** info,
                                    HOST_INFO*   host_info,
                                    int/*bool*/  dispd_to_follow);


/* Get configuration file name. Returned '\0'-terminated string
 * is to be free()'d by a caller when no longer needed.
 * Return NULL if no configuration file name is available.
 * LBSMD_FastHeapAccess() was set to "eOff" and there is a cached copy
 * of LBSM heap kept in-core, it will be released by this call.
 */
extern NCBI_XCONNECT_EXPORT const char* LBSMD_GetConfig(void);


/* Get (perhaps cached) copy of LBSM heap, which is guaranteed to be
 * current for given time "time".  If "time" passed as 0, current time
 * of the day is assumed.  Return NULL if the copy operation cannot
 * be performed (due to various reasons, including the original
 * LBSM shmem to be obsolete).
 * Returned heap (if non-NULL) has a serial number reflecting which
 * shmem segment has been used to get the snapshot.  The serial number
 * is negated for newer heap structure, which has dedicated version
 * entry format.  Older heap structure uses SLBSM_OldEntry instead,
 * and has TTLs for entries instead of expiration times.  The returned
 * copy must be passed to (MT-locked by the caller) HEAP_Destroy() when
 * no longer needed.
 * The copy can be cached in-core, the only way to release it is to
 * call LBSMD_GetConfig() provided that LBSM_FastHeapAccess() has
 * been set to "eOff" (which is the default setting).
 */
extern NCBI_XCONNECT_EXPORT HEAP LBSMD_GetHeapCopy(TNCBI_Time time);


/* Host info getters */
typedef const void* LBSM_HINFO;

int LBSM_HINFO_CpuCount(LBSM_HINFO hinfo);


int LBSM_HINFO_TaskCount(LBSM_HINFO hinfo);


int/*bool*/ LBSM_HINFO_LoadAverage(LBSM_HINFO hinfo, double lavg[2]);


int/*bool*/ LBSM_HINFO_Status(LBSM_HINFO hinfo, double status[2]);


#ifdef __cplusplus
}  /* extern "C" */
#endif


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.14  2006/11/08 17:19:07  lavr
 * Change sign of used serial nos for new vs. old heap formats
 *
 * Revision 6.13  2006/03/06 20:40:14  lavr
 * Added NCBI_XCONNECT_EXPORT attribute to LBSMD_GetHeapCopy()
 *
 * Revision 6.12  2006/03/06 20:27:49  lavr
 * Comments
 *
 * Revision 6.11  2006/03/05 17:43:52  lavr
 * Private API changes; cached HEAP copy; BLAST counters dropped
 *
 * Revision 6.10  2005/05/04 16:17:32  lavr
 * +<connect/ncbi_service_misc.h>, +LBSMD_GetConfig(), +LBSM_KeepHeapAttached()
 * LBSM_UnLBSMD() added in potential heap detaching places
 *
 * Revision 6.9  2002/10/28 21:55:38  lavr
 * LBSM_HINFO introduced for readability to replace plain "const void*"
 *
 * Revision 6.8  2002/10/28 20:12:57  lavr
 * Module renamed and host info API included
 *
 * Revision 6.7  2002/10/11 19:52:45  lavr
 * +SERV_LBSMD_GetConfig()
 *
 * Revision 6.6  2002/09/19 18:09:02  lavr
 * Header file guard macro changed; log moved to end
 *
 * Revision 6.5  2002/04/13 06:40:28  lavr
 * Few tweaks to reduce the number of syscalls made
 *
 * Revision 6.4  2001/04/24 21:31:22  lavr
 * SERV_LBSMD_LOCAL_SVC_BONUS moved to .c file
 *
 * Revision 6.3  2000/12/29 18:19:12  lavr
 * BONUS added for services running locally.
 *
 * Revision 6.2  2000/05/22 16:53:13  lavr
 * Rename service_info -> server_info everywhere (including
 * file names) as the latter name is more relevant
 *
 * Revision 6.1  2000/05/12 18:39:18  lavr
 * First working revision
 *
 * ==========================================================================
 */

#endif /* CONNECT___NCBI_LBSMD__H */
