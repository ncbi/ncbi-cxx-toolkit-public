#ifndef NETSTORAGE_SERVER_PARAMS__HPP
#define NETSTORAGE_SERVER_PARAMS__HPP

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

#include <corelib/ncbireg.hpp>
#include <connect/server.hpp>

#include <string>


BEGIN_NCBI_SCOPE


//
// NetStorage server parameters
//
struct SNetStorageServerParameters : SServer_Parameters
{
    void Read(const IRegistry& reg, const string& sname,
              string &  decrypt_warning);

    unsigned short  port;
    unsigned int    network_timeout;
    bool            log;
    bool            log_timing_nst_api;
    bool            log_timing_client_socket;
    string          admin_client_names;
    string          data_path;
};

END_NCBI_SCOPE

#endif

