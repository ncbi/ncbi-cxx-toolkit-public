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

#include "ncbi_server_infop.h"
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_service.hpp>
#include <connect/ncbi_socket.hpp>
#include <corelib/ncbidbg.hpp>


BEGIN_NCBI_SCOPE


static string x_HostOfInfo(const SSERV_Info*     info,
                           const TNCBI_IPv6Addr* addr)
{
    const char* x_host = SERV_HostOfInfo(info);
    if (x_host) {
        _ASSERT(*x_host);
        return x_host;
    }

    char xx_host[CONN_HOST_LEN + 1];
    if (NcbiAddrToString(xx_host, sizeof(xx_host), addr)) {
        _ASSERT(*xx_host);
        return xx_host;
    }

    _TROUBLE;
    return kEmptyStr;
}


template<>
struct Deleter<SConnNetInfo>
{
    static void Delete(SConnNetInfo* net_info)
    { ConnNetInfo_Destroy(net_info); }
};


/////////////////////////////////////////////////////////////////////////////
// SERV_GetServers implementation
extern
vector<CSERV_Info> SERV_GetServers(const string&  service,
                                   TSERV_TypeOnly types)
{
    // Share core functionality with C-language CONNECT library.
    {
        class CInPlaceConnIniter : protected CConnIniter
        {
        } conn_initer;  /*NCBI_FAKE_WARNING*/
    }

    vector<CSERV_Info> servers;

    AutoPtr<SConnNetInfo> net_info(ConnNetInfo_Create(service.c_str()));
    TSERV_TypeOnly what = fSERV_All;
    if (!(types & fSERV_Firewall))
        what &= ~fSERV_Firewall;

    SERV_ITER iter = SERV_Open(service.c_str(), what, SERV_ANYHOST,
                               net_info.get());
    if (iter != 0) {
        const SSERV_Info* info;
        while ((info = SERV_GetNextInfo(iter)) != 0) {
            TNCBI_IPv6Addr addr = SERV_AddrOfInfo(info);
            if (NcbiIsEmptyIPv6(&addr)) {
                string msg
                    = "SERV_GetServers('" + service
                    + "'): Service not operational";
                NCBI_THROW(CException, eUnknown, msg);
            }

            if (types == fSERV_Any  ||  (types & info->type)) {
                servers.push_back(CSERV_Info
                                  (x_HostOfInfo(info, &addr), info->port,
                                   info->rate, info->type));
            } else {
                _TRACE("SERV_GetServers('" << service << "'): Skipping "
                       << ((NcbiIsIPv4(&addr) ? "" : "[")
                           + x_HostOfInfo(info, &addr) +
                           (NcbiIsIPv4(&addr) ? "" : "]"))
                       << (info->port
                           ? ':' + NStr::NumericToString(info->port)
                           : kEmptyStr)
                       << " due to incompatible type "
                       << SERV_TypeStr(info->type)
                       << hex << setiosflags(IOS_BASE::uppercase)
                       << "(0x" << info->type << ") vs. mask=0x" << types);
            }
        }
        SERV_Close(iter);
    }

    return servers;
}


END_NCBI_SCOPE
