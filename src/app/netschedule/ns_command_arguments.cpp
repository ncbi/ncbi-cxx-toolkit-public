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
#include <corelib/request_ctx.hpp>


#include "ns_command_arguments.hpp"
#include "ns_db.hpp"


USING_NCBI_SCOPE;


void SNSCommandArguments::x_Reset()
{
    job_id              = 0;
    job_return_code     = 0;
    port                = 0;
    timeout             = 0;
    job_mask            = 0;
    start_after_job_id  = 0;
    count               = 0;
    client_data_version = -1;

    cmd.erase();
    auth_token.erase();
    input.erase();
    output.erase();
    affinity_token.erase();
    job_key.erase();
    queue_from_job_key.erase();
    err_msg.erase();
    comment.erase();
    description.erase();
    ip.erase();
    option.erase();
    progress_msg.erase();
    qname.erase();
    qclass.erase();
    sid.erase();
    job_statuses_string.erase();
    aff_to_add.erase();
    aff_to_del.erase();
    start_after.erase();
    group.erase();
    alert.erase();
    service.erase();
    user.erase();
    client_data.erase();
    ncbi_phid.erase();

    any_affinity = false;
    wnode_affinity = false;
    reader_affinity = false;
    exclusive_new_aff = false;
    mode = false;
    drain = false;
    effective = false;
    pullback = false;
    blacklist = true;
    no_retries = false;
    prioritized_aff = false;

    affinity_may_change = false;
    group_may_change = false;

    job_statuses.clear();

    return;
}


void SNSCommandArguments::AssignValues(TNSProtoParams &           params,
                                       const string &             command,
                                       bool                       need_to_generate,
                                       CSocket &                  peer_socket,
                                       CCompoundIDPool::TInstance id_pool)
{
    x_Reset();
    cmd = command;

    NON_CONST_ITERATE(TNSProtoParams, it, params) {
        const CTempString &     key = it->first;
        const CTempString &     val = it->second;
        size_t                  size_to_check = 0;


        if (key.empty())
            continue;

        switch (key[0]) {
        case 'a':
            if (key == "aff") {
                affinity_token = NStr::ParseEscapes(val);
                x_CheckAffinityList(affinity_token);
            }
            else if (key == "auth_token")
                auth_token = val;
            else if (key == "add") {
                aff_to_add = NStr::ParseEscapes(val);
                x_CheckAffinityList(aff_to_add);
            }
            else if (key == "any_aff")
                any_affinity = x_GetBooleanValue(val, key);
            else if (key == "alert")
                alert = NStr::ParseEscapes(val);
            else if (key == "affinity_may_change")
                affinity_may_change = x_GetBooleanValue(val, key);
            break;
        case 'b':
            if (key == "blacklist")
                blacklist = x_GetBooleanValue(val, key);
            break;
        case 'c':
            if (key == "comment")
                comment = NStr::ParseEscapes(val);
            else if (key == "count")
                count = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
            break;
        case 'd':
            if (key == "del") {
                aff_to_del = NStr::ParseEscapes(val);
                x_CheckAffinityList(aff_to_del);
            }
            else if (key == "drain")
                drain = x_GetBooleanValue(val, key);
            else if (key == "description") {
                description = NStr::ParseEscapes(val);
                size_to_check = description.size();
            }
            else if (key == "data")
                client_data = NStr::ParseEscapes(val);
            break;
        case 'e':
            if (key == "exclusive_new_aff")
                exclusive_new_aff = x_GetBooleanValue(val, key);
            else if (key == "err_msg")
                err_msg = x_NormalizeErrorMessage(NStr::ParseEscapes(val));
            else if (key == "effective")
                effective = x_GetBooleanValue(val, key);
            break;
        case 'g':
            if (key == "group") {
                group = NStr::ParseEscapes(val);
                x_CheckGroupList(group);
            }
            else if (key == "group_may_change")
                group_may_change = x_GetBooleanValue(val, key);
            break;
        case 'i':
            if (key == "input") {
                input = NStr::ParseEscapes(val);
                if (input.size() > kNetScheduleMaxOverflowSize)
                    NCBI_THROW(CNetScheduleException, eDataTooLong,
                               "input exceeds the max allowed length. "
                               "Received: " +
                               NStr::NumericToString(input.size()) +
                               " Allowed: " +
                               NStr::NumericToString(
                                                kNetScheduleMaxOverflowSize));
            }
            else if (key == "ip") {
                ip = NStr::ParseEscapes(val);
                if (ip.empty()) {
                    ip = peer_socket.GetPeerAddress(eSAF_IP);
                    it->second = ip;
                }
            }
            break;
        case 'j':
            if (key == "job_key") {
                job_key = val;
                if (!val.empty()) {
                    CNetScheduleKey     parsed_key(val, id_pool);
                    job_id = parsed_key.id;
                    queue_from_job_key = parsed_key.queue;
                }
                if (job_id == 0)
                    NCBI_THROW(CNetScheduleException,
                               eInvalidParameter, "Invalid job key");
            }
            else if (key == "job_return_code")
                job_return_code = NStr::StringToInt(val,
                                                    NStr::fConvErr_NoThrow);
            break;
        case 'm':
            if (key == "msk")
                job_mask = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
            else if (key == "mode")
                mode = x_GetBooleanValue(val, key);
            break;
        case 'n':
            if (key == "ncbi_phid") {
                ncbi_phid = NStr::ParseEscapes(val);
                if (ncbi_phid.empty()) {
                    if (need_to_generate) {
                        ncbi_phid = CDiagContext::GetRequestContext().SetHitID();
                        // Dictionary is updated for the logging
                        it->second = ncbi_phid;
                    }
                }
                else
                    CDiagContext::GetRequestContext().SetHitID(ncbi_phid);
            }
            else if (key == "no_retries")
                no_retries = x_GetBooleanValue(val, key);
            break;
        case 'o':
            if (key == "output") {
                output = NStr::ParseEscapes(val);
                if (output.size() > kNetScheduleMaxOverflowSize)
                    NCBI_THROW(CNetScheduleException, eDataTooLong,
                               "output exceeds the max allowed length. "
                               "Received: " +
                               NStr::NumericToString(output.size()) +
                               " Allowed: " +
                               NStr::NumericToString(
                                                kNetScheduleMaxOverflowSize));
            }
            else if (key == "option")
                option = NStr::ParseEscapes(val);
            break;
        case 'p':
            if (key == "port") {
                port = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
                if (port > 65535)
                    NCBI_THROW(CNetScheduleException,
                               eInvalidParameter, "Invalid port number");
            }
            else if (key == "progress_msg") {
                progress_msg = NStr::ParseEscapes(val);
                size_to_check = progress_msg.size();
            }
            else if (key == "pullback")
                pullback = x_GetBooleanValue(val, key);
            else if (key == "prioritized_aff")
                prioritized_aff = x_GetBooleanValue(val, key);
            break;
        case 'q':
            if (key == "qname") {
                qname = NStr::ParseEscapes(val);
                x_CheckQueueName(qname, key);
            }
            else if (key == "qclass") {
                qclass = NStr::ParseEscapes(val);
                x_CheckQueueName(qclass, key);
            }
            break;
        case 'r':
            if (key == "reader_aff")
                reader_affinity = x_GetBooleanValue(val, key);
            break;
        case 's':
            if (key == "status") {
                job_statuses_string = val;

                if (!job_statuses_string.empty()) {
                    list<string>    statuses;
                    NStr::Split(job_statuses_string, ",", statuses,
                                NStr::fSplit_NoMergeDelims);

                    for (list<string>::const_iterator k = statuses.begin();
                         k != statuses.end(); ++k )
                        job_statuses.push_back(
                                        CNetScheduleAPI::StringToStatus(*k));
                }
            }
            else if (key == "sid") {
                sid = NStr::ParseEscapes(val);
                if (sid.empty()) {
                    if (need_to_generate) {
                        sid = CDiagContext::GetRequestContext().SetSessionID();
                        it->second = sid;
                    }
                }
                else
                    CDiagContext::GetRequestContext().SetSessionID(sid);
            }
            else if (key == "start_after") {
                start_after = val;
                if (!val.empty())
                    start_after_job_id = CNetScheduleKey(val, id_pool).id;
                if (start_after_job_id == 0)
                    NCBI_THROW(CNetScheduleException,
                               eInvalidParameter,
                               "Invalid job ID in 'start_after' option key");
            }
            else if (key == "service")
                service = NStr::ParseEscapes(val);
            break;
        case 't':
            if (key == "timeout")
                timeout = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
            break;
        case 'u':
            if (key == "user")
                user = NStr::ParseEscapes(val);
            break;
        case 'v':
            if (key == "version")
                client_data_version = NStr::StringToInt(val);
            break;
        case 'w':
            if (key == "wnode_aff")
                wnode_affinity = x_GetBooleanValue(val, key);
            break;
        default:
            break;
        }


        if (size_to_check > kNetScheduleMaxDBDataSize - 1)
            NCBI_THROW(CNetScheduleException, eDataTooLong,
                       "Argument '" + string(key) + "' size (" +
                       NStr::NumericToString(size_to_check) +
                       " bytes) exceeds the DB max limit ( " +
                       NStr::NumericToString(kNetScheduleMaxDBDataSize - 1) +
                       " bytes)");
    }

    return;
}


// A single affinity is a particular case of a list of affinities
// So it is harmless to use this function to check both:
// - commands which accept a single affinity
// - commands which accept a list of affinities
void SNSCommandArguments::x_CheckAffinityList(const string &  val)
{
    list<string>    affs;
    NStr::Split(val, "\t,", affs, NStr::fSplit_NoMergeDelims);
    for (list<string>::const_iterator
            k = affs.begin(); k != affs.end(); ++k)
        if (k->size() > kNetScheduleMaxDBDataSize - 1)
            NCBI_THROW(
                CNetScheduleException, eDataTooLong,
                "Affinity token '" + *k + "' length (" +
                NStr::NumericToString(k->size()) +
                " bytes) exceeds the limit ( " +
                NStr::NumericToString(kNetScheduleMaxDBDataSize - 1) +
                " bytes)");
}


// A single group is a particular case of a list of groups
// So it is harmless to use this function to check both:
// - commands which accept a single group
// - commands which accept a list of groups
void SNSCommandArguments::x_CheckGroupList(const string &  val)
{
    list<string>    groups;
    NStr::Split(val, "\t,", groups, NStr::fSplit_NoMergeDelims);
    for (list<string>::const_iterator
            k = groups.begin(); k != groups.end(); ++k)
        if (k->size() > kNetScheduleMaxDBDataSize - 1)
            NCBI_THROW(
                CNetScheduleException, eDataTooLong,
                "Group token '" + *k + "' length (" +
                NStr::NumericToString(k->size()) +
                " bytes) exceeds the limit ( " +
                NStr::NumericToString(kNetScheduleMaxDBDataSize - 1) +
                " bytes)");
}


void SNSCommandArguments::x_CheckQueueName(const string &  val,
                                           const string &  key)
{
    if (val.size() > kMaxQueueNameSize - 1) {
        string      q = "queue";
        if (key == "qclass")
            q += " class";
        NCBI_THROW(CNetScheduleException, eDataTooLong,
                   "The  '" + key + "' " + q + " name length (" +
                   NStr::NumericToString(val.size()) +
                   " bytes) exceeds the limit ( " +
                   NStr::NumericToString(kMaxQueueNameSize - 1) +
                   " bytes)");
    }
}


bool SNSCommandArguments::x_GetBooleanValue(const string &  val,
                                            const string &  key)
{
    int tmp = 0;
    try {
        tmp = NStr::StringToInt(val);
    } catch (...) {
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   key +
                   " parameter must be an integer value (0 or 1 are allowed)");
    }

    if (tmp != 0 && tmp != 1)
        NCBI_THROW(CNetScheduleException, eInvalidParameter,
                   key + " parameter accepted values are 0 and 1");
    return tmp == 1;
}


string SNSCommandArguments::x_NormalizeErrorMessage(const string &  val)
{
    if (val.size() > kNetScheduleMaxDBErrSize - 1) {
        // Truncate the message, see CXX-2617
        ERR_POST(Warning << "The err_msg parameter length ("
                         << val.size() << "bytes) exceeds the limit "
                         << kNetScheduleMaxDBErrSize - 1 << " bytes) and is "
                            "truncated");
        const string        suffix = " TRUNCATED";
        return val.substr(0, kNetScheduleMaxDBErrSize - suffix.size() - 2) +
               suffix;
    }
    return val;
}

