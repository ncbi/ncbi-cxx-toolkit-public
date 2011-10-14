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
 * Authors:  Anatoliy Kuznetsov, Victor Joukov
 *
 * File Description: Queue parameters
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/netschedule_api.hpp>

#include "ns_queue_parameters.hpp"
#include "ns_types.hpp"
#include "ns_db.hpp"


BEGIN_NCBI_SCOPE


SQueueParameters::SQueueParameters() :
    timeout(3600),
    notif_timeout(7),
    run_timeout(3600),
    failed_retries(0),
    empty_lifetime(0),
    max_input_size(kNetScheduleMaxDBDataSize),
    max_output_size(kNetScheduleMaxDBDataSize),
    deny_access_violations(false),
    log_access_violations(true),
    run_timeout_precision(3600)
{}



#define GetIntNoErr(name, dflt) reg.GetInt(sname, name, dflt, 0, IRegistry::eReturn)

void SQueueParameters::Read(const IRegistry& reg, const string& sname)
{
    // When modifying this, modify all places marked with PARAMETERS

    // Read parameters
    timeout = GetIntNoErr("timeout", 3600);

    notif_timeout         = GetIntNoErr("notif_timeout", 7);
    run_timeout           = GetIntNoErr("run_timeout", timeout);
    run_timeout_precision = GetIntNoErr("run_timeout_precision", run_timeout);
    program_name          = reg.GetString(sname, "program", kEmptyStr);

    failed_retries = GetIntNoErr("failed_retries", 0);
    blacklist_time = GetIntNoErr("blacklist_time", 0);
    empty_lifetime = GetIntNoErr("empty_lifetime", -1);


    // Max input size
    string  s = reg.GetString(sname, "max_input_size", kEmptyStr);
    max_input_size = kNetScheduleMaxDBDataSize;
    try {
        max_input_size = (unsigned) NStr::StringToUInt8_DataSize(s);
    }
    catch (CStringException&) {}
    max_input_size = min(kNetScheduleMaxOverflowSize, max_input_size);


    // Max output size
    s = reg.GetString(sname, "max_output_size", kEmptyStr);
    max_output_size = kNetScheduleMaxDBDataSize;
    try {
        max_output_size = (unsigned) NStr::StringToUInt8_DataSize(s);
    }
    catch (CStringException&) {}
    max_output_size = min(kNetScheduleMaxOverflowSize, max_output_size);


    deny_access_violations = reg.GetBool(sname, "deny_access_violations", false,
                                         0, IRegistry::eReturn);
    log_access_violations  = reg.GetBool(sname, "log_access_violations", true,
                                         0, IRegistry::eReturn);

    subm_hosts = reg.GetString(sname,  "subm_host",  kEmptyStr);
    wnode_hosts = reg.GetString(sname, "wnode_host", kEmptyStr);
}


END_NCBI_SCOPE

