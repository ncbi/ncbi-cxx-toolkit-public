#ifndef CONNECT___NCBI_SERVICE_MISC__H
#define CONNECT___NCBI_SERVICE_MISC__H

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
 * Author:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   Top-level API to resolve NCBI service name to the server meta-address:
 *   miscellanea.
 *
 */

#include <connect/ncbi_types.h>


/** @addtogroup ServiceSupport
 *
 * @{
 */


#ifdef __cplusplus
extern "C" {
#endif


/* Set message hook procedure for messages originating from NCBI network
 * dispatcher (if used).  Any hook will be called not more than once.
 * If no hook is installed, warning message will be generated in the
 * standard log file, not more than once.
 */

typedef void (*FDISP_MessageHook)(const char* message);

extern NCBI_XCONNECT_EXPORT void DISP_SetMessageHook(FDISP_MessageHook);


/* Set default behavior of keeping attached/detaching LBSM heap
 * (if has been attached) upon service iterator closure.
 * By default, on SERV_Close() the heap gets detached, but this may not be
 * desirable in a long-run applications that use service iterators intensively,
 * and would like to avoid rapid successtions of attaching/detaching.
 * The function returns a setting that has been previously in effect.
 * OnOff == eDefault has no effect but returns the current setting.
 */

extern NCBI_XCONNECT_EXPORT ESwitch LBSM_KeepHeapAttached(ESwitch OnOff);


#ifdef __cplusplus
}  /* extern "C" */
#endif


/* @} */


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2005/05/04 16:13:14  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#endif /* CONNECT___NCBI_SERVICE_MISC__H */
