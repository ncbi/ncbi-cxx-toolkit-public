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
#include "nst_exception.hpp"


BEGIN_NCBI_SCOPE

const string    g_LogPrefix = "Validating config file: ";


static void NSTValidateServerSection(const IRegistry &  reg,
                                     vector<string> &  warnings,
                                     bool  throw_port_exception);
static void NSTValidateDatabaseSection(const IRegistry &  reg,
                                       vector<string> &  warnings);
static void NSTValidateMetadataSection(const IRegistry &  reg,
                                       vector<string> &  warnings);
static void NSTValidateServices(const IRegistry &  reg,
                                vector<string> &  warnings);
static void NSTValidateProlongValue(const IRegistry &  reg,
                                    const string &  section,
                                    const string &  entry,
                                    vector<string> &  warnings);
static void NSTValidateTTLValue(const IRegistry &  reg,
                                const string &  section,
                                const string &  entry,
                                vector<string> &  warnings);
static bool NSTValidateBool(const IRegistry &  reg,
                            const string &  section, const string &  entry,
                            vector<string> &  warnings);
static bool NSTValidateInt(const IRegistry &  reg,
                           const string &  section, const string &  entry,
                           vector<string> &  warnings);
//static bool NSTValidateDouble(const IRegistry &  reg,
//                              const string &  section, const string &  entry,
//                              vector<string> &  warnings);
static bool NSTValidateString(const IRegistry &  reg,
                              const string &  section, const string &  entry,
                              vector<string> &  warnings);
static string NSTRegValName(const string &  section, const string &  entry);
static string NSTOutOfLimitMessage(const string &  section,
                                   const string &  entry,
                                   unsigned int  low_limit,
                                   unsigned int  high_limit);


void NSTValidateConfigFile(const IRegistry &  reg,
                           vector<string> &  warnings,
                           bool  throw_port_exception)
{
    NSTValidateServerSection(reg, warnings, throw_port_exception);
    NSTValidateDatabaseSection(reg, warnings);
    NSTValidateMetadataSection(reg, warnings);
    NSTValidateServices(reg, warnings);
}


// Returns true if the config file is well formed
void NSTValidateServerSection(const IRegistry &  reg,
                              vector<string> &  warnings,
                              bool  throw_port_exception)
{
    const string    section = "server";

    // port is a unique value in this section. NST must not start
    // if there is a problem with port.
    bool    ok = NSTValidateInt(reg, section, "port", warnings);
    if (ok) {
        int     port_val = reg.GetInt(section, "port", 0);
        if (port_val < port_low_limit || port_val > port_high_limit) {
            string  msg = "Invalid " + NSTRegValName(section, "port") +
                          " value. Allowed range: " +
                          NStr::NumericToString(port_low_limit) +
                          " to " + NStr::NumericToString(port_high_limit);
            if (throw_port_exception)
                NCBI_THROW(CNetStorageServerException, eInvalidConfig, msg);
            warnings.push_back(msg);
        }
    } else {
        if (throw_port_exception)
            NCBI_THROW(CNetStorageServerException, eInvalidConfig,
                       "Invalid " + NSTRegValName(section, "port") +
                       " parameter.");
    }

    ok = NSTValidateInt(reg, section, "max_connections", warnings);
    if (ok) {
        int     val = reg.GetInt(section, "max_connections",
                                 default_max_connections);
        if (val < int(max_connections_low_limit) ||
            val > int(max_connections_high_limit))
            warnings.push_back(
                     NSTOutOfLimitMessage(section, "max_connections",
                                          max_connections_low_limit,
                                          max_connections_high_limit));
    }

    ok = NSTValidateInt(reg, section, "max_threads", warnings);
    unsigned int    max_threads_val = default_max_threads;
    if (ok) {
        int     val = reg.GetInt(section, "max_threads", default_max_threads);
        if (val < int(max_threads_low_limit) ||
            val > int(max_threads_high_limit))
            warnings.push_back(
                     NSTOutOfLimitMessage(section, "max_threads",
                                          max_threads_low_limit,
                                          max_threads_high_limit));
        else
            max_threads_val = val;
    }

    ok = NSTValidateInt(reg, section, "init_threads", warnings);
    unsigned int    init_threads_val = default_init_threads;
    if (ok) {
        int     val = reg.GetInt(section, "init_threads", default_init_threads);
        if (val < int(init_threads_low_limit) ||
            val > int(init_threads_high_limit))
            warnings.push_back(
                     NSTOutOfLimitMessage(section, "init_threads",
                                          init_threads_low_limit,
                                          init_threads_high_limit));
        else
            init_threads_val = val;
    }

    if (init_threads_val > max_threads_val)
        warnings.push_back(g_LogPrefix + " values " +
                           NSTRegValName(section, "max_threads") + " and " +
                           NSTRegValName(section, "init_threads") +
                           " break the mandatory condition "
                            "init_threads <= max_threads");


    ok = NSTValidateInt(reg, section, "network_timeout", warnings);
    if (ok) {
        int     val = reg.GetInt(section, "network_timeout",
                                 default_network_timeout);
        if (val <= 0)
            warnings.push_back(g_LogPrefix + " value " +
                               NSTRegValName(section, "network_timeout") +
                               " must be > 0");
    }

    NSTValidateBool(reg, section, "log", warnings);
    NSTValidateBool(reg, section, "log_timing", warnings);
    NSTValidateBool(reg, section, "log_timing_nst_api", warnings);
    NSTValidateBool(reg, section, "log_timing_client_socket", warnings);
    NSTValidateString(reg, section, "admin_client_name", warnings);
}


void NSTValidateDatabaseSection(const IRegistry &  reg,
                                vector<string> &  warnings)
{
    const string    section = "database";

    bool    ok = NSTValidateString(reg, section, "service", warnings);
    if (ok) {
        string      value = reg.GetString(section, "service", "");
        if (value.empty())
            warnings.push_back(g_LogPrefix + " value " +
                               NSTRegValName(section, "service") +
                               " must not be empty");
    }

    ok = NSTValidateString(reg, section, "user_name", warnings);
    if (ok) {
        string      value = reg.GetString(section, "user_name", "");
        if (value.empty())
            warnings.push_back(g_LogPrefix + " value " +
                               NSTRegValName(section, "user_name") +
                               " must not be empty");
    }

    ok = NSTValidateString(reg, section, "password", warnings);
    if (ok) {
        string      value = reg.GetString(section, "password", "");
        if (value.empty())
            warnings.push_back(g_LogPrefix + " value " +
                               NSTRegValName(section, "password") +
                               " must not be empty");
    }

    ok = NSTValidateString(reg, section, "database", warnings);
    if (ok) {
        string      value = reg.GetString(section, "database", "");
        if (value.empty())
            warnings.push_back(g_LogPrefix + " value " +
                               NSTRegValName(section, "database") +
                               " must not be empty");
    }
}


void NSTValidateMetadataSection(const IRegistry &  reg,
                                vector<string> &  warnings)
{
    const string    section = "metadata_conf";

    NSTValidateTTLValue(reg, section, "ttl", warnings);
    NSTValidateProlongValue(reg, section, "prolong_on_read", warnings);
    NSTValidateProlongValue(reg, section, "prolong_on_write", warnings);
}


// Case insensitive unary predicate for a list
struct SCaseInsensitivePredicate
{
    SCaseInsensitivePredicate(const string &  pattern) : p(pattern)
    {}
    bool operator()(const string & m)
    { return NStr::CompareNocase(m, p) == 0; }

    string p;
};


// Checks the service_ZZZ sections
void NSTValidateServices(const IRegistry &  reg, vector<string> &  warnings)
{
    list<string>    sections;
    reg.EnumerateSections(&sections);

    const string    prefix = "service_";
    for (list<string>::const_iterator  k = sections.begin();
            k != sections.end(); ++k) {
        if (!NStr::StartsWith(*k, prefix, NStr::eNocase))
            continue;

        string      service(k->c_str() + prefix.size());
        if (service.empty()) {
            warnings.push_back("Section [" + *k + "] does not have "
                               "a service name.");
            continue;
        }

        if (!reg.HasEntry(*k, "metadata"))
            continue;

        if (!NSTValidateBool(reg, *k, "metadata", warnings))
            continue;   // Warning has been added

        if (!reg.GetBool(*k, "metadata", false))
            continue;

        NSTValidateTTLValue(reg, *k, "ttl", warnings);
        NSTValidateProlongValue(reg, *k, "prolong_on_read", warnings);
        NSTValidateProlongValue(reg, *k, "prolong_on_write", warnings);
    }
}


void NSTValidateProlongValue(const IRegistry &  reg,
                             const string &  section,
                             const string &  entry,
                             vector<string> &  warnings)
{
    bool            ok = NSTValidateString(reg, section, entry, warnings);
    if (ok) {
        string      value = reg.GetString(section, entry, "");
        if (!value.empty()) {
            try {
                ReadTimeSpan(value, false);  // no infinity
            } catch (const exception &  ex) {
                warnings.push_back("Exception while validating " +
                        NSTRegValName(section, entry) +
                        " value: " + ex.what());
            } catch (...) {
                warnings.push_back("Unknown exception while validating " +
                        NSTRegValName(section, entry) +
                        " value: " + value + ". It must be "
                        " in a recognizable format.");
            }
        }
    }
}


void NSTValidateTTLValue(const IRegistry &  reg,
                         const string &  section,
                         const string &  entry,
                         vector<string> &  warnings)
{
    bool            ok = NSTValidateString(reg, section, entry, warnings);
    if (ok) {
        string      value = reg.GetString(section, entry, "");
        try {
            ReadTimeSpan(value, true);  // Allow infinity
        } catch (const exception &  ex) {
            warnings.push_back("Exception while validating " +
                               NSTRegValName(section, entry) +
                               " value: " + ex.what());
        } catch (...) {
            warnings.push_back("Unknown exception while validating " +
                               NSTRegValName(section, entry) +
                               " value: " + value + ". It must be "
                               "'infinity' or a recognizable format.");
        }
    }
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
                     const string &  section, const string &  entry,
                     vector<string> &  warnings)
{
    try {
        reg.GetBool(section, entry, false);
    }
    catch (...) {
        warnings.push_back(g_LogPrefix + "unexpected value of " +
                           NSTRegValName(section, entry) +
                           ". Expected boolean value.");
        return false;
    }
    return true;
}


// Checks that an integer value is fine
bool NSTValidateInt(const IRegistry &  reg,
                    const string &  section, const string &  entry,
                    vector<string> &  warnings)
{
    try {
        reg.GetInt(section, entry, 0);
    }
    catch (...) {
        warnings.push_back(g_LogPrefix + "unexpected value of " +
                           NSTRegValName(section, entry) +
                           ". Expected integer value.");
        return false;
    }
    return true;
}


/*
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
*/


// Checks that a double value is fine
bool NSTValidateString(const IRegistry &  reg,
                       const string &  section, const string &  entry,
                       vector<string> &  warnings)
{
    try {
        reg.GetString(section, entry, "");
    }
    catch (...) {
        warnings.push_back(g_LogPrefix + "unexpected value of " +
                           NSTRegValName(section, entry) +
                           ". Expected string value.");
        return false;
    }
    return true;
}


TNSTDBValue<CTimeSpan>  ReadTimeSpan(const string &  reg_value,
                                     bool  allow_infinity)
{
    TNSTDBValue<CTimeSpan>      result;

    if (allow_infinity) {
        if (NStr::EqualNocase(reg_value, "infinity") || reg_value.empty()) {
            result.m_IsNull = true;
            return result;
        }
    }

    result.m_IsNull = false;
    result.m_Value.AssignFromSmartString(reg_value);
    return result;
}


END_NCBI_SCOPE

