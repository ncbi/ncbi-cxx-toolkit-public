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
 * Authors:  Denis Vakatov
 *
 * File Description: [server] section of the configuration
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/resource_info.hpp>

#include "nst_server_parameters.hpp"
#include "nst_exception.hpp"
#include "nst_config.hpp"


USING_NCBI_SCOPE;

#define GetIntNoErr(name, dflt) \
    reg.GetInt(sname, name, dflt, 0, IRegistry::eReturn)
#define GetBoolNoErr(name, dflt) \
    reg.GetBool(sname, name, dflt, 0, IRegistry::eReturn)



void SNetStorageServerParameters::Read(const IRegistry &    reg,
                                       const string &       sname,
                                       string &             decrypt_warning)
{
    max_connections = GetIntNoErr("max_connections", default_max_connections);
    max_threads     = GetIntNoErr("max_threads", default_max_threads);

    init_threads = GetIntNoErr("init_threads", default_init_threads);
    if (init_threads > max_threads) {
        ERR_POST(Warning <<
                 "INI file sets init_threads > max_threads. "
                 "Assume init_threads = max_threads(" << max_threads <<
                 ") instead of given " << init_threads);
        init_threads = max_threads;
    }

    port = (unsigned short) GetIntNoErr("port", 0);
    if (port <= 0 || port >= 65535)
        NCBI_THROW(CNetStorageServerException, eInvalidArgument,
                   "Server listening port number is not specified "
                   "or invalid ([server]/port)");

    network_timeout = GetIntNoErr("network_timeout", default_network_timeout);
    if (network_timeout == 0) {
        ERR_POST(Warning <<
            "INI file sets 0 sec. network timeout. Assume " <<
            default_network_timeout << " seconds.");
        network_timeout = default_network_timeout;
    }

    log = GetBoolNoErr("log", default_log);
    log_timing = GetBoolNoErr("log_timing", default_log_timing);
    log_timing_nst_api = GetBoolNoErr("log_timing_nst_api",
                                      default_log_timing_nst_api);
    log_timing_client_socket = GetBoolNoErr("log_timing_client_socket",
                                            default_log_timing_client_socket);


    // Deal with potentially encrypted admin client names
    try {
        admin_client_names = reg.GetEncryptedString(
                                        sname, "admin_client_name",
                                        IRegistry::fPlaintextAllowed);
    } catch (const CRegistryException &  ex) {
        decrypt_warning = "[server]/admin_client_name "
                          "decrypting error detected. " + string(ex.what());
    } catch (...) {
        decrypt_warning = "[server]/admin_client_name "
                          "unknown decrypting error";
    }
}

