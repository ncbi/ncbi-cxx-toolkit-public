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
    void        (*Close)(SERV_ITER iter);
} SSERV_VTable;


/* Iterator structure
 */
struct SSERV_IterTag {
    char*        service;        /* requested service name */
    TSERV_Type   type;           /* requested server type(s) */
    unsigned int preferred_host; /* preferred host to select */
    SSERV_Info** skip;           /* servers to skip */
    size_t       n_skip;         /* number of servers in the array */
    size_t       n_max_skip;     /* number of allocated slots in the array */

    const SSERV_VTable* op;      /* table of virtual functions */

    void*        data;           /* private data field */
};


#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* NCBI_SERVICEP__H */
