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
 * File Description: NetStorage config file utilities
 *
 */

#include <ncbi_pch.hpp>

#include "nst_config.hpp"
#include <connect/services/netstorage.hpp>



BEGIN_NCBI_SCOPE

const string    g_LogPrefix = "Validating config file: ";


static bool NSTValidateServerSection(const IRegistry &  reg);
static bool NSTValidateDatabaseSection(const IRegistry &  reg);
static bool NSTValidateBool(const IRegistry &  reg,
                            const string &  section, const string &  entry);
static bool NSTValidateInt(const IRegistry &  reg,
                           const string &  section, const string &  entry);
static bool NSTValidateDouble(const IRegistry &  reg,
                              const string &  section, const string &  entry);
static bool NSTValidateString(const IRegistry &  reg,
                              const string &  section, const string &  entry);
static string NSTRegValName(const string &  section, const string &  entry);
static string NSTOutOfLimitMessage(const string &  section,
                                   const string &  entry,
                                   unsigned int  low_limit,
                                   unsigned int  high_limit);


bool NSTValidateConfigFile(const IRegistry &  reg)
{
    bool    server_well_formed = NSTValidateServerSection(reg);
    bool    database_well_formed = NSTValidateDatabaseSection(reg);
    return server_well_formed && database_well_formed;
}


// Returns true if the config file is well formed
bool NSTValidateServerSection(const IRegistry &  reg)
{
    const string    section = "server";
    bool            well_formed = true;

    // port is a unique value in this section. NS must not start
    // if there is a problem with port.
    bool    port_ok = NSTValidateInt(reg, section, "port");
    if (port_ok) {
        unsigned int    port_val = reg.GetInt(section, "port", 0);
        if (port_val < port_low_limit || port_val > port_high_limit)
            NCBI_THROW(CNetStorageException, eInvalidConfig,
                       "Invalid " + NSTRegValName(section, "port") +
                       " value. Allowed range: " +
                       NStr::NumericToString(port_low_limit) +
                       " to " +
                       NStr::NumericToString(port_high_limit));
    }

    bool    max_conn_ok = NSTValidateInt(reg, section, "max_connections");
    well_formed = well_formed && max_conn_ok;
    if (max_conn_ok) {
        unsigned int    val = reg.GetInt(section, "max_connections",
                                         default_max_connections);
        if (val < max_connections_low_limit ||
            val > max_connections_high_limit) {
            well_formed = false;
            LOG_POST(Warning <<
                     NSTOutOfLimitMessage(section, "max_connections",
                                          max_connections_low_limit,
                                          max_connections_high_limit));
        }
    }

    bool            max_threads_ok = NSTValidateInt(reg, section,
                                                    "max_threads");
    unsigned int    max_threads_val = default_max_threads;
    well_formed = well_formed && max_threads_ok;
    if (max_threads_ok) {
        max_threads_val = reg.GetInt(section, "max_threads",
                                     default_max_threads);
        if (max_threads_val < max_threads_low_limit ||
            max_threads_val > max_threads_high_limit) {
            well_formed = false;
            max_threads_ok = false;
            LOG_POST(Warning <<
                     NSTOutOfLimitMessage(section, "max_threads",
                                          max_threads_low_limit,
                                          max_threads_high_limit));
        }
    }

    bool            init_threads_ok = NSTValidateInt(reg, section,
                                                     "init_threads");
    unsigned int    init_threads_val = default_init_threads;
    well_formed = well_formed && init_threads_ok;
    if (init_threads_ok) {
        init_threads_val = reg.GetInt(section, "init_threads",
                                      default_init_threads);
        if (init_threads_val < init_threads_low_limit ||
            init_threads_val > init_threads_high_limit) {
            well_formed = false;
            init_threads_ok = false;
            LOG_POST(Warning <<
                     NSTOutOfLimitMessage(section, "init_threads",
                                          init_threads_low_limit,
                                          init_threads_high_limit));
        }
    }

    if (max_threads_ok && init_threads_ok) {
        if (init_threads_val > max_threads_val) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " values "
                             << NSTRegValName(section, "max_threads") << " and "
                             << NSTRegValName(section, "init_threads")
                             << " break the mandatory condition "
                                "init_threads <= max_threads");
        }
    }


    bool    network_timeout_ok = NSTValidateInt(reg, section,
                                                "network_timeout");
    well_formed = well_formed && network_timeout_ok;
    if (network_timeout_ok) {
        unsigned int    network_timeout_val =
                                    reg.GetInt(section, "network_timeout",
                                               default_network_timeout);
        if (network_timeout_val <= 0) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NSTRegValName(section, "network_timeout")
                             << " must be > 0");
        }
    }

    well_formed = well_formed &&
                  NSTValidateBool(reg, section, "log");
    well_formed = well_formed &&
                  NSTValidateString(reg, section, "admin_client_name");

    return well_formed;
}


bool NSTValidateDatabaseSection(const IRegistry &  reg)
{
    const string    section = "database";
    bool            well_formed = true;

    bool    server_name_ok = NSTValidateString(reg, section, "server_name");
    well_formed = well_formed && server_name_ok;
    if (server_name_ok) {
        string      value = reg.GetString(section, "server_name", "");
        if (value.empty()) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NSTRegValName(section, "server_name")
                             << " must not be empty");
        }
    }

    bool    user_name_ok = NSTValidateString(reg, section, "user_name");
    well_formed = well_formed && user_name_ok;
    if (user_name_ok) {
        string      value = reg.GetString(section, "user_name", "");
        if (value.empty()) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NSTRegValName(section, "user_name")
                             << " must not be empty");
        }
    }

    bool    password_ok = NSTValidateString(reg, section, "password");
    well_formed = well_formed && password_ok;
    if (password_ok) {
        string      value = reg.GetString(section, "password", "");
        if (value.empty()) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NSTRegValName(section, "password")
                             << " must not be empty");
        }
    }
    
    bool    database_ok = NSTValidateString(reg, section, "database");
    well_formed = well_formed && database_ok;
    if (database_ok) {
        string      value = reg.GetString(section, "database", "");
        if (value.empty()) {
            well_formed = false;
            LOG_POST(Warning << g_LogPrefix << " value "
                             << NSTRegValName(section, "database")
                             << " must not be empty");
        }
    }

    return well_formed;
}



// Forms the ini file value name for logging
string NSTRegValName(const string &  section, const string &  entry)
{
    return "[" + section + "]/" + entry;
}


// Forms an out of limits message
string NSTOutOfLimitMessage(const string &  section,
                            const string &  entry,
                            unsigned int  low_limit,
                            unsigned int  high_limit)
{
    return g_LogPrefix + NSTRegValName(section, entry) +
           " is out of limits (" + NStr::NumericToString(low_limit) + "..." +
           NStr::NumericToString(high_limit) + ")";

}


// Checks that a boolean value is fine
bool NSTValidateBool(const IRegistry &  reg,
                     const string &  section, const string &  entry)
{
    try {
        reg.GetBool(section, entry, false);
    }
    catch (...) {
        LOG_POST(Warning << g_LogPrefix << "unexpected value of ["
                         << NSTRegValName(section, entry)
                         << ". Expected boolean value.");
        return false;
    }
    return true;
}


// Checks that an integer value is fine
bool NSTValidateInt(const IRegistry &  reg,
                    const string &  section, const string &  entry)
{
    try {
        reg.GetInt(section, entry, 0);
    }
    catch (...) {
        LOG_POST(Warning << g_LogPrefix << "unexpected value of ["
                         << NSTRegValName(section, entry)
                         << ". Expected integer value.");
        return false;
    }
    return true;
}


// Checks that a double value is fine
bool NSTValidateDouble(const IRegistry &  reg,
                       const string &  section, const string &  entry)
{
    try {
        reg.GetDouble(section, entry, 0.0);
    }
    catch (...) {
        LOG_POST(Warning << g_LogPrefix << "unexpected value of "
                         << NSTRegValName(section, entry)
                         << ". Expected floating point value.");
        return false;
    }
    return true;
}


// Checks that a double value is fine
bool NSTValidateString(const IRegistry &  reg,
                       const string &  section, const string &  entry)
{
    try {
        reg.GetString(section, entry, "");
    }
    catch (...) {
        LOG_POST(Warning << g_LogPrefix << "unexpected value of "
                         << NSTRegValName(section, entry)
                         << ". Expected string value.");
        return false;
    }
    return true;
}

END_NCBI_SCOPE

