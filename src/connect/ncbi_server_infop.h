#ifndef CONNECT___NCBI_SERVER_INFOP__H
#define CONNECT___NCBI_SERVER_INFOP__H

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
 *   NCBI server meta-address info (private part)
 *
 */

#include "ncbi_host_infop.h"
#include <connect/ncbi_server_info.h>


#ifdef __cplusplus
extern "C" {
#endif


int/*bool*/ SERV_SetLocalServerDefault(int/*bool*/ onoff);


#ifdef __cplusplus
}  /* extern "C" */
#endif


/*
 * --------------------------------------------------------------------------
 * $Log$
 * Revision 6.4  2002/10/28 20:15:21  lavr
 * +<connect/ncbi_server_info.h>
 *
 * Revision 6.3  2002/09/19 18:08:38  lavr
 * Header file guard macro changed; log moved to end
 *
 * Revision 6.2  2001/11/25 22:12:06  lavr
 * Replaced g_SERV_LocalServerDefault -> SERV_SetLocalServerDefault()
 *
 * Revision 6.1  2001/11/16 20:25:53  lavr
 * +g_SERV_LocalServerDefault as a private global parameter
 *
 * ==========================================================================
 */

#endif /* CONNECT___NCBI_SERVER_INFOP__H */
