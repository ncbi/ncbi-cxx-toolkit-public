#ifndef NETSTORAGE_CONSTANTS__HPP
#define NETSTORAGE_CONSTANTS__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description: NetStorage constants used in the server code
 *
 */

BEGIN_NCBI_SCOPE


// Configuration file default values
const unsigned short    port_low_limit = 1;
const unsigned short    port_high_limit = 65535;

const unsigned int      default_network_timeout = 10;

const unsigned int      default_max_connections = 500;
const unsigned int      max_connections_low_limit = 1;
const unsigned int      max_connections_high_limit = 1000;

const unsigned int      default_init_threads = 10;
const unsigned int      init_threads_low_limit = 1;
const unsigned int      init_threads_high_limit = 1000;

const unsigned int      default_max_threads = 50;
const unsigned int      max_threads_low_limit = 1;
const unsigned int      max_threads_high_limit = 1000;

const bool              default_log = true;
const bool              default_log_timing_nst_api = false;
const bool              default_log_timing_client_socket = false;

const double            default_execute_sp_timeout = 20.0;


// Limitations
const size_t            max_attr_value = 900;


// Client name namespaces; see CXX-8023
const string            kMyNcbiIdNamespace = "my_ncbi_id";
const string            kDefaultNamespace = "";

END_NCBI_SCOPE

#endif /* NETSTORAGE_CONSTANTS__HPP */

