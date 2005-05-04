#ifndef CONNECT___NCBI_DISPD__H
#define CONNECT___NCBI_DISPD__H

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
 *   with the use of NCBI network dispatcher (DISPD).
 *
 */

#include "ncbi_servicep.h"


#ifdef __cplusplus
extern "C" {
#endif


const SSERV_VTable* SERV_DISPD_Open(SERV_ITER           iter,
                                    const SConnNetInfo* net_info,
                                    SSERV_Info**        info,
                                    HOST_INFO*          host_info);


#ifdef __cplusplus
}  /* extern "C" */
#endif


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.12  2005/05/04 16:18:04  lavr
 * -<connect/ncbi_connutil.h>
 *
 * Revision 6.11  2002/10/28 20:12:56  lavr
 * Module renamed and host info API included
 *
 * Revision 6.10  2002/09/19 18:08:56  lavr
 * Header file guard macro changed; log moved to end
 *
 * Revision 6.9  2002/04/13 06:40:16  lavr
 * Few tweaks to reduce the number of syscalls made
 *
 * Revision 6.8  2002/02/05 22:04:13  lavr
 * Included header files rearranged
 *
 * Revision 6.7  2001/04/24 21:32:06  lavr
 * SERV_DISPD_STALE_RATIO_OK and SERV_DISPD_LOCAL_SVC_BONUS moved to .c file
 *
 * Revision 6.6  2001/03/06 23:57:06  lavr
 * SERV_DISPD_LOCAL_SVC_BONUS #define'd for services running locally
 *
 * Revision 6.5  2000/12/29 18:18:22  lavr
 * RATIO added to update pool if it exhausted due to expiration times of
 * untaken services.
 *
 * Revision 6.4  2000/10/20 17:24:08  lavr
 * 'SConnNetInfo' now passed as 'const' to 'SERV_DISPD_Open'
 *
 * Revision 6.3  2000/10/05 22:36:21  lavr
 * Additional parameters in call to DISPD mapper
 *
 * Revision 6.2  2000/05/22 16:53:13  lavr
 * Rename service_info -> server_info everywhere (including
 * file names) as the latter name is more relevant
 *
 * Revision 6.1  2000/05/12 18:39:49  lavr
 * First working revision
 *
 * ==========================================================================
 */

#endif /* CONNECT___NCBI_DISPD__H */
