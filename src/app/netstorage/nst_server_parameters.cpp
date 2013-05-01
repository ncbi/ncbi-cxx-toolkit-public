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
#include "nst_server_parameters.hpp"


USING_NCBI_SCOPE;

#define GetIntNoErr(name, dflt) \
    reg.GetInt(sname, name, dflt, 0, IRegistry::eReturn)
#define GetBoolNoErr(name, dflt) \
    reg.GetBool(sname, name, dflt, 0, IRegistry::eReturn)


static unsigned int     default_max_connections = 100;
static unsigned int     default_max_threads = 25;
static unsigned int     default_init_threads = 10;
static unsigned short   default_port = 9100;
static unsigned int     default_network_timeout = 10;
static bool             default_log = true;


void SNSTServerParameters::Read(const IRegistry& reg, const string& sname)
{
    max_connections = GetIntNoErr("max_connections", default_max_connections);
    max_threads     = GetIntNoErr("max_threads", default_max_threads);

    init_threads = GetIntNoErr("init_threads", default_init_threads);
    if (init_threads > max_threads) {
        LOG_POST(Message << Warning <<
                 "INI file sets init_threads > max_threads. "
                 "Assume init_threads = max_threads(" << max_threads <<
                 ") instead of given " << init_threads);
        init_threads = max_threads;
    }

    port = (unsigned short) GetIntNoErr("port", default_port);

    network_timeout = GetIntNoErr("network_timeout", default_network_timeout);
    if (network_timeout == 0) {
        LOG_POST(Message << Warning <<
            "INI file sets 0 sec. network timeout. Assume " <<
            default_network_timeout << " seconds.");
        network_timeout = default_network_timeout;
    }

    log = GetBoolNoErr("log", default_log);
}

