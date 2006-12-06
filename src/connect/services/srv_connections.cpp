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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <connect/ncbi_socket.hpp>
#include <connect/services/srv_connections.hpp>

#include <connect/ncbi_service.h>

BEGIN_NCBI_SCOPE

void NCBI_XCONNECT_EXPORT DiscoverLBServices(const string& service_name, 
                                             vector<pair<string,unsigned int> >& services)
{
    SConnNetInfo* net_info = ConnNetInfo_Create(service_name.c_str());
    TSERV_Type stype = fSERV_Any | fSERV_Promiscuous;
    SERV_ITER srv_it = SERV_Open(service_name.c_str(), stype, 0, net_info);
    ConnNetInfo_Destroy(net_info);

    if (srv_it != 0) {
        const SSERV_Info* sinfo;
        while ((sinfo = SERV_GetNextInfoEx(srv_it, 0)) != 0) {
            
            // string host = CSocketAPI::gethostbyaddr(sinfo->host);
            string host = CSocketAPI::ntoa(sinfo->host);
            // string::size_type pos = host.find_first_of(".");
            // if (pos != string::npos) {
            //    host.erase(pos, host.size());
            // }
            services.push_back(make_pair(host,sinfo->port));
        } // while
        SERV_Close(srv_it);
    }
}

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 6.1  2006/12/06 15:00:00  didenko
 * Added service connections template classes
 *
 * ===========================================================================
 */
 
