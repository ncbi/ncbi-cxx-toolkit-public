#ifndef NCBI_SERVICE__H
#define NCBI_SERVICE__H

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
 *   Top-level API to resolve NCBI service name to the service meta-address.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.1  2000/05/05 20:24:00  lavr
 * Initial revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_service_info.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Flags to specify the algorithm to choose the most preferred service
 * from the set of available services
 */
typedef enum {
    fSERV_Default = 0x0,
    fSERV_Blast   = 0x1
} ESERV_Flags;
typedef int TSERV_Flags;  /* binary OR of "ESERV_Flags" */


/* Iterator through the services
 */
struct SSERV_IterTag;
typedef struct SSERV_IterTag* SERV_ITER;


/* Create iterator for the iterative service lookup
 */
SERV_ITER SERV_OpenSimple
(const char* service           /* service name */
 );

SERV_ITER SERV_Open
(const char*  service,         /* service name */
 TSERV_Flags  flags,           /* flags for the service selection algorithm */
 TSERV_Type   type,            /* mask of type of service requested */
 unsigned int preferred_host   /* preferred host to use service on */
 );

SERV_ITER SERV_OpenEx
(const char*       service,        /* service name */
 TSERV_Flags       flags,          /* selection algorithm flags */
 TSERV_Type        type,           /* mask of type of service requested */
 unsigned int      preferred_host, /* preferred host to use service on */
 const SSERV_Info* skip[],         /* services NOT to select */
 size_t            n_skip          /* number of services in preceding array */
 );


/* Get the next service meta-address
 * Note that the application program should NOT destroy
 * returned service info: it will be freed automatically upon
 * iterator destruction.
 */
const SSERV_Info* SERV_GetNextInfo
(SERV_ITER iter                 /* handle obtained via 'SERV_Open*' call */
 );


/* Deallocate iterator
 */
void SERV_Close
(SERV_ITER iter                 /* handle obtained via 'SERV_Open*' call */
 );


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_SERVICE__H */
