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
 * Author:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Implementation of net cache client with load balancer interface.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_conn_exception.hpp>
#include <connect/netcache_client.hpp>
#include <connect/ncbi_service.h>


BEGIN_NCBI_SCOPE

void NetCache_ConfigureWithLB(CNetCacheClient* nc_client, 
                              const string&    service_name)
{
    SERV_ITER srv_it = SERV_OpenSimple(service_name.c_str());

    if (srv_it == 0) {
        goto err;
    }

    {{
    const SSERV_Info* sinfo;
    STimeout& to = nc_client->SetCommunicationTimeout();

    while ((sinfo = SERV_GetNextInfoEx(srv_it, 0)) != 0) {

        try {
            auto_ptr<CSocket> sock(
                new CSocket(sinfo->host, sinfo->port, &to));
            EIO_Status st = sock->GetStatus(eIO_Open);
            if (st != eIO_Success) {
                continue;
            }

            nc_client->SetSocket(sock.release(), 
                                 eTakeOwnership);
            SERV_Close(srv_it);
            return;
            
        } catch(exception&){
            // try another server
        }

    } // while

    SERV_Close(srv_it);
    }}
err:
    // cannot connect
    NCBI_THROW(CNetCacheException, eCommunicationError, 
               "Cannot connect to service using load balancer.");
}



END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2004/12/20 13:48:56  kuznets
 * Fixed compilation problem (GCC)
 *
 * Revision 1.1  2004/12/17 15:26:48  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
