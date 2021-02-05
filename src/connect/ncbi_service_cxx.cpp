/* $Id$
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
 * Author:  David McElhany
 *
 * File Description:
 *   C++-only API for named network services.
 *
 */

#include <ncbi_pch.hpp>

#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_service.hpp>
#include <connect/ncbi_socket.h>
#include <connect/ncbi_socket.hpp>
#include <corelib/ncbidiag.hpp>


BEGIN_NCBI_SCOPE


template<>
struct Deleter<SConnNetInfo>
{
    static void Delete(SConnNetInfo* net_info)
    { ConnNetInfo_Destroy(net_info); }
};


/////////////////////////////////////////////////////////////////////////////
// SERV_GetServers implementation
extern
vector<CSERV_Info> SERV_GetServers(const string& service,
                                   TSERV_Type    types)
{
    // Share core functionality with C-language CONNECT library.
    {
        class CInPlaceConnIniter : protected CConnIniter
        {
        } conn_initer;  /*NCBI_FAKE_WARNING*/
    }

    vector<CSERV_Info>  servers;

    AutoPtr<SConnNetInfo> net_info(ConnNetInfo_Create(service.c_str()));

    SERV_ITER iter = SERV_Open(service.c_str(), fSERV_All, SERV_ANYHOST,
                               net_info.get());
    if (iter != 0) {
        const SSERV_Info * info;
        while ((info = SERV_GetNextInfo(iter)) != 0) {
            unsigned int    host = info->host;
            unsigned int    port = info->port;
            double          rate = info->rate;
            ESERV_Type      type = info->type;

            if (host == 0) {
                string msg("GetHostsForService: Service '");
                msg += service + "' is not operational.";
                NCBI_THROW(CException, eUnknown, msg);
            }

            string hostname(CSocketAPI::gethostbyaddr(host));
            if (types == fSERV_Any  ||  (types & info->type)) {
                servers.push_back(CSERV_Info(hostname, port, rate, type));
            } else {
                ERR_POST(Info << "Skipping " << hostname
                    << " due to incompatible type " << info->type
                    << " (mask=0x" << hex << types << ").");
            }
        }
        SERV_Close(iter);
    }

    return servers;
}


END_NCBI_SCOPE
