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
 * File Description: job request parameters
 *
 */

#include <ncbi_pch.hpp>
#include <connect/services/netschedule_api.hpp>


#include "ns_command_arguments.hpp"

USING_NCBI_SCOPE;


void SNSCommandArguments::x_Reset()
{
    job_id             = 0;
    job_return_code    = 0;
    port               = 0;
    timeout            = 0;
    job_mask           = 0;
    start_after_job_id = 0;
    count              = 0;
    job_status         = CNetScheduleAPI::eJobNotFound;

    cmd.erase();
    auth_token.erase();
    input.erase();
    output.erase();
    affinity_token.erase();
    job_key.erase();
    err_msg.erase();
    comment.erase();
    ip.erase();
    option.erase();
    progress_msg.erase();
    qname.erase();
    qclass.erase();
    sid.erase();
    job_status_string.erase();
    aff_to_add.erase();
    aff_to_del.erase();
    start_after.erase();

    any_affinity = false;
    wnode_affinity = false;

    return;
}


void SNSCommandArguments::AssignValues(const TNSProtoParams &  params,
                                       const string &          command)
{
    x_Reset();
    cmd = command;

    ITERATE(TNSProtoParams, it, params) {
        const CTempString &     key = it->first;
        const CTempString &     val = it->second;

        if (key.empty())
            continue;

        if (val.size() > kNetScheduleMaxDBDataSize - 1  &&
            key != "input"   &&
            key != "output")
        {
            NCBI_THROW(CNetScheduleException, eDataTooLong,
                       "User input exceeds the limit.");
        }

        switch (key[0]) {
        case 'a':
            if (key == "aff")
                affinity_token = val;
            else if (key == "auth_token")
                auth_token = val;
            else if (key == "add")
                aff_to_add = val;
            else if (key == "any_aff") {
                int tmp = NStr::StringToInt(val);
                if (tmp != 0 && tmp != 1)
                    NCBI_THROW(CNetScheduleException, eInvalidParameter,
                               "any_aff accepted values are 0 and 1.");
                any_affinity = (tmp == 1);
            }
            break;
        case 'c':
            if (key == "comment")
                comment = val;
            else if (key == "count")
                count = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
            break;
        case 'd':
            if (key == "del")
                aff_to_del = val;
            break;
        case 'e':
            if (key == "err_msg")
                err_msg = val;
            break;
        case 'i':
            if (key == "input")
                input = val;
            else if (key == "ip")
                ip = val;
            break;
        case 'j':
            if (key == "job_key") {
                job_key = val;
                if (!val.empty())
                    job_id = CNetScheduleKey(val).id;
                if (job_id == 0)
                    NCBI_THROW(CNetScheduleException,
                               eInvalidParameter, "Invalid job ID");
            }
            else if (key == "job_return_code")
                job_return_code = NStr::StringToInt(val, NStr::fConvErr_NoThrow);
            break;
        case 'm':
            if (key == "msk")
                job_mask = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
            break;
        case 'o':
            if (key == "output")
                output = val;
            else if (key == "option")
                option = val;
            break;
        case 'p':
            if (key == "port") {
                port = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
                if (port > 65535)
                    NCBI_THROW(CNetScheduleException,
                               eInvalidParameter, "Invalid port number");
            }
            else if (key == "progress_msg")
                progress_msg = val;
            break;
        case 'q':
            if (key == "qname")
                qname = val;
            else if (key == "qclass")
                qclass = val;
            break;
        case 's':
            if (key == "status") {
                job_status = CNetScheduleAPI::StringToStatus(val);
                job_status_string = val;
            }
            else if (key == "sid")
                sid = val;
            else if (key == "start_after") {
                start_after = val;
                if (!val.empty())
                    start_after_job_id = CNetScheduleKey(val).id;
                if (start_after_job_id == 0)
                    NCBI_THROW(CNetScheduleException,
                               eInvalidParameter,
                               "Invalid job ID in 'start_after' option key");
            }
            break;
        case 't':
            if (key == "timeout")
                timeout = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
            break;
        case 'w':
            if (key == "wnode_aff") {
                int tmp = NStr::StringToInt(val);
                if (tmp != 0 && tmp != 1)
                    NCBI_THROW(CNetScheduleException, eInvalidParameter,
                               "wnode_aff accepted values are 0 and 1.");
                wnode_affinity = (tmp == 1);
            }
        default:
            break;
        }
    }

    return;
}

