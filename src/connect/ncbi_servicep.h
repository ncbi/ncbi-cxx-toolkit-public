#ifndef NCBI_SERVICEP__H
#define NCBI_SERVICEP__H

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
 *   Private API to define server iterator structure.
 *
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.9  2001/04/26 14:18:45  lavr
 * SERV_MapperName moved to the private header
 *
 * Revision 6.8  2001/04/24 21:33:58  lavr
 * Added members of mapper V-table: penalize(method) and name(data).
 * Service iterator has got new field 'last' to keep the latest given info.
 *
 * Revision 6.7  2001/03/06 23:57:49  lavr
 * Minor beautifications
 *
 * Revision 6.6  2000/12/29 18:12:51  lavr
 * SERV_Print added to private interface
 *
 * Revision 6.5  2000/12/06 22:21:27  lavr
 * SERV_Print added to private interface
 *
 * Revision 6.4  2000/10/20 17:22:55  lavr
 * VTable changed to have 'Update' method
 * 'SERV_Update' added to private interface
 *
 * Revision 6.3  2000/10/05 21:37:51  lavr
 * Mapper-specific private data field added
 *
 * Revision 6.2  2000/05/22 16:53:12  lavr
 * Rename service_info -> server_info everywhere (including
 * file names) as the latter name is more relevant
 *
 * Revision 6.1  2000/05/12 18:38:16  lavr
 * First working revision
 *
 * ==========================================================================
 */

#include <connect/ncbi_service.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Table of iterator "virtual functions"
 */
typedef struct {
    SSERV_Info* (*GetNextInfo)(SERV_ITER iter);
    int/*bool*/ (*Update)(SERV_ITER iter, const char* text);
    int/*bool*/ (*Penalize)(SERV_ITER iter, double penalty);
    void        (*Close)(SERV_ITER iter);
    const char* name;
} SSERV_VTable;


/* Iterator structure
 */
struct SSERV_IterTag {
    const char*  service;        /* requested service name                 */
    TSERV_Type   type;           /* requested server type(s)               */
    unsigned int preferred_host; /* preferred host to select, network b.o. */
    SSERV_Info** skip;           /* servers to skip                        */
    size_t       n_skip;         /* number of servers in the array         */
    size_t       n_max_skip;     /* number of allocated slots in the array */
    SSERV_Info*  last;           /* last server info taken out             */

    const SSERV_VTable* op;      /* table of virtual functions             */

    void*        data;           /* private data field                     */
};


/* Private interface: update mapper information from the given text
 * (<CR><LF> separated lines, usually kept from HTTP header).
 */
int/*bool*/ SERV_Update(SERV_ITER iter, const char* text);


/* Private interface: print and return the HTTP-compliant header portion
 * (<CR><LF> separated lines, including the last line) out of the information
 * contained in the iterator; to be used in mapping requests to DISPD.
 * Return value must be 'free'd.
 */
char* SERV_Print(SERV_ITER iter);


/* Get name of underlying service mapper.
 */
const char* SERV_MapperName(SERV_ITER iter);


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_SERVICEP__H */
