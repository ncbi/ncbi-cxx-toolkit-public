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


USING_NCBI_SCOPE;

static map<string, EDumpFields>    ks_DumpFields = {
    { "id", eId },
    { "key", eKey },
    { "status", eStatus },
    { "last_touch", eLastTouch },
    { "erase_time", eEraseTime },
    { "run_expiration", eRunExpiration },
    { "read_expiration", eReadExpiration },
    { "subm_notif_port", eSubmitNotifPort },
    { "subm_notif_expiration", eSubmitNotifExpiration },
    { "listener_notif", eListenerNotif },
    { "listener_notif_expiration", eListenerNotifExpiration },
    { "events", eEvents },
    { "run_counter", eRunCounter },
    { "read_counter", eReadCounter },
    { "affinity", eAffinity },
    { "group", eGroup },
    { "mask", eMask },
    { "input", eInput },
    { "output", eOutput },
    { "progress_msg", eProgressMsg },
    { "remote_client_sid", eRemoteClientSID },
    { "remote_client_ip", eRemoteClientIP },
    { "ncbi_phid", eNcbiPHID },
    { "need_subm_progress_msg_notif", eNeedSubmitProgressMsgNotif },
    { "need_lsnr_progress_msg_notif", eNeedListenerProgressMsgNotif },
    { "need_stolen_notif", eNeedStolenNotif },
    { "gc_erase_time", eGCEraseTime },
    { "scope", eScope }
};



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
    scope.erase();

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

    need_progress_msg = false;
    need_stolen = false;

    order_first = true;
    dump_fields = eAll;

    job_statuses.clear();
}


static string   s_AffOption = "aff";
static string   s_AuthTokenOption = "auth_token";
static string   s_AddOption = "add";
static string   s_AnyAffOption = "any_aff";
static string   s_AlertOption = "alert";
static string   s_AffinityMayChangeOption = "affinity_may_change";
static string   s_BlacklistOption = "blacklist";
static string   s_CommentOption = "comment";
static string   s_CountOption = "count";
static string   s_DelOption = "del";
static string   s_DrainOption = "drain";
static string   s_DescriptionOption = "description";
static string   s_DataOption = "data";
static string   s_ExclusiveNewAffOption = "exclusive_new_aff";
static string   s_ErrMsgOption = "err_msg";
static string   s_EffectiveOption = "effective";
static string   s_FieldsOption = "fields";
static string   s_GroupOption = "group";
static string   s_GroupMayChangeOption = "group_may_change";
static string   s_InputOption = "input";
static string   s_IpOption = "ip";
static string   s_JobKeyOption = "job_key";
static string   s_JobReturnCodeOption = "job_return_code";
static string   s_MskOption = "msk";
static string   s_ModeOption = "mode";
static string   s_NcbiPhidOption = "ncbi_phid";
static string   s_NoRetriesOption = "no_retries";
static string   s_NeedStolenOption = "need_stolen";
static string   s_NeedProgressMsgOption = "need_progress_msg";
static string   s_OutputOption = "output";
static string   s_OptionOption = "option";
static string   s_OrderOption = "order";
static string   s_PortOption = "port";
static string   s_ProgressMsgOption = "progress_msg";
static string   s_PullbackOption = "pullback";
static string   s_PrioritizedAffOption = "prioritized_aff";
static string   s_QNameOption = "qname";
static string   s_QClassOption = "qclass";
static string   s_ReaderAffOption = "reader_aff";
static string   s_StatusOption = "status";
static string   s_SidOption = "sid";
static string   s_StartAfterOption = "start_after";
static string   s_ServiceOption = "service";
static string   s_ScopeOption = "scope";
static string   s_TimeoutOption = "timeout";
static string   s_UserOption = "user";
static string   s_VersionOption = "version";
static string   s_WnodeAffOption = "wnode_aff";



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
            if (key == s_AffOption) {
                affinity_token = NStr::ParseEscapes(val);
                x_CheckAffinityList(affinity_token);
            }
            else if (key == s_AuthTokenOption)
                auth_token = val;
            else if (key == s_AddOption) {
                aff_to_add = NStr::ParseEscapes(val);
                x_CheckAffinityList(aff_to_add);
            }
            else if (key == s_AnyAffOption)
                any_affinity = x_GetBooleanValue(val, key);
            else if (key == s_AlertOption)
                alert = NStr::ParseEscapes(val);
            else if (key == s_AffinityMayChangeOption)
                affinity_may_change = x_GetBooleanValue(val, key);
            break;
        case 'b':
            if (key == s_BlacklistOption)
                blacklist = x_GetBooleanValue(val, key);
            break;
        case 'c':
            if (key == s_CommentOption)
                comment = NStr::ParseEscapes(val);
            else if (key == s_CountOption)
                count = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
            break;
        case 'd':
            if (key == s_DelOption) {
                aff_to_del = NStr::ParseEscapes(val);
                x_CheckAffinityList(aff_to_del);
            }
            else if (key == s_DrainOption)
                drain = x_GetBooleanValue(val, key);
            else if (key == s_DescriptionOption) {
                description = NStr::ParseEscapes(val);
                size_to_check = description.size();
            }
            else if (key == s_DataOption)
                client_data = NStr::ParseEscapes(val);
            break;
        case 'e':
            if (key == s_ExclusiveNewAffOption)
                exclusive_new_aff = x_GetBooleanValue(val, key);
            else if (key == s_ErrMsgOption)
                err_msg = x_NormalizeErrorMessage(NStr::ParseEscapes(val));
            else if (key == s_EffectiveOption)
                effective = x_GetBooleanValue(val, key);
            break;
        case 'f':
            if (key == s_FieldsOption) {
                dump_fields = x_GetDumpFields(val);
            }
            break;
        case 'g':
            if (key == s_GroupOption) {
                group = NStr::ParseEscapes(val);
                x_CheckGroupList(group);
            }
            else if (key == s_GroupMayChangeOption)
                group_may_change = x_GetBooleanValue(val, key);
            break;
        case 'i':
            if (key == s_InputOption) {
                input = NStr::ParseEscapes(val);
                if (input.size() > kNetScheduleMaxOverflowSize)
                    NCBI_THROW(CNetScheduleException, eDataTooLong,
                               "input exceeds the max allowed length. "
                               "Received: " + to_string(input.size()) +
                               " Allowed: " +
                               to_string(kNetScheduleMaxOverflowSize));
            }
            else if (key == s_IpOption) {
                ip = NStr::ParseEscapes(val);
                if (ip.empty()) {
                    ip = peer_socket.GetPeerAddress(eSAF_IP);
                    it->second = ip;
                }
            }
            break;
        case 'j':
            if (key == s_JobKeyOption) {
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
            else if (key == s_JobReturnCodeOption)
                job_return_code = NStr::StringToInt(val,
                                                    NStr::fConvErr_NoThrow);
            break;
        case 'm':
            if (key == s_MskOption)
                job_mask = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
            else if (key == s_ModeOption)
                mode = x_GetBooleanValue(val, key);
            break;
        case 'n':
            if (key == s_NcbiPhidOption) {
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
            else if (key == s_NoRetriesOption)
                no_retries = x_GetBooleanValue(val, key);
            else if (key == s_NeedStolenOption)
                need_stolen = x_GetBooleanValue(val, key);
            else if (key == s_NeedProgressMsgOption)
                need_progress_msg = x_GetBooleanValue(val, key);
            break;
        case 'o':
            if (key == s_OutputOption) {
                output = NStr::ParseEscapes(val);
                if (output.size() > kNetScheduleMaxOverflowSize)
                    NCBI_THROW(CNetScheduleException, eDataTooLong,
                               "output exceeds the max allowed length. "
                               "Received: " + to_string(output.size()) +
                               " Allowed: " +
                               to_string(kNetScheduleMaxOverflowSize));
            }
            else if (key == s_OptionOption) {
                option = NStr::ParseEscapes(val);
            } else if (key == s_OrderOption) {
                order_first = x_GetOrderFirst(val);
            }
            break;
        case 'p':
            if (key == s_PortOption) {
                port = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
                if (port > 65535)
                    NCBI_THROW(CNetScheduleException,
                               eInvalidParameter, "Invalid port number");
            }
            else if (key == s_ProgressMsgOption) {
                progress_msg = NStr::ParseEscapes(val);
                size_to_check = progress_msg.size();
            }
            else if (key == s_PullbackOption)
                pullback = x_GetBooleanValue(val, key);
            else if (key == s_PrioritizedAffOption)
                prioritized_aff = x_GetBooleanValue(val, key);
            break;
        case 'q':
            if (key == s_QNameOption) {
                qname = NStr::ParseEscapes(val);
                x_CheckQueueName(qname, key);
            }
            else if (key == s_QClassOption) {
                qclass = NStr::ParseEscapes(val);
                x_CheckQueueName(qclass, key);
            }
            break;
        case 'r':
            if (key == s_ReaderAffOption)
                reader_affinity = x_GetBooleanValue(val, key);
            break;
        case 's':
            if (key == s_StatusOption) {
                job_statuses_string = val;

                if (!job_statuses_string.empty()) {
                    list<string>    statuses;
                    NStr::Split(job_statuses_string, ",", statuses);

                    for (list<string>::const_iterator k = statuses.begin();
                         k != statuses.end(); ++k )
                        job_statuses.push_back(
                                        CNetScheduleAPI::StringToStatus(*k));
                }
            }
            else if (key == s_SidOption) {
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
            else if (key == s_StartAfterOption) {
                start_after = val;
                if (!val.empty())
                    start_after_job_id = CNetScheduleKey(val, id_pool).id;
                if (start_after_job_id == 0)
                    NCBI_THROW(CNetScheduleException,
                               eInvalidParameter,
                               "Invalid job ID in 'start_after' option key");
            }
            else if (key == s_ServiceOption)
                service = NStr::ParseEscapes(val);
            else if (key == s_ScopeOption)
                scope = NStr::ParseEscapes(val);
            break;
        case 't':
            if (key == s_TimeoutOption)
                timeout = NStr::StringToUInt(val, NStr::fConvErr_NoThrow);
            break;
        case 'u':
            if (key == s_UserOption)
                user = NStr::ParseEscapes(val);
            break;
        case 'v':
            if (key == s_VersionOption)
                client_data_version = NStr::StringToInt(val);
            break;
        case 'w':
            if (key == s_WnodeAffOption)
                wnode_affinity = x_GetBooleanValue(val, key);
            break;
        default:
            break;
        }


        if (size_to_check > kNetScheduleMaxDBDataSize - 1)
            NCBI_THROW(CNetScheduleException, eDataTooLong,
                       "Argument '" + string(key) + "' size (" +
                       to_string(size_to_check) +
                       " bytes) exceeds the DB max limit ( " +
                       to_string(kNetScheduleMaxDBDataSize - 1) + " bytes)");
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
    NStr::Split(val, "\t,", affs);
    for (const auto &  aff : affs)
        if (aff.size() > kNetScheduleMaxDBDataSize - 1)
            NCBI_THROW(
                CNetScheduleException, eDataTooLong,
                "Affinity token '" + aff + "' length (" +
                to_string(aff.size()) + " bytes) exceeds the limit ( " +
                to_string(kNetScheduleMaxDBDataSize - 1) + " bytes)");
}


// A single group is a particular case of a list of groups
// So it is harmless to use this function to check both:
// - commands which accept a single group
// - commands which accept a list of groups
void SNSCommandArguments::x_CheckGroupList(const string &  val)
{
    list<string>    groups;
    NStr::Split(val, "\t,", groups);
    for (const auto &  group : groups)
        if (group.size() > kNetScheduleMaxDBDataSize - 1)
            NCBI_THROW(
                CNetScheduleException, eDataTooLong,
                "Group token '" + group + "' length (" +
                to_string(group.size()) + " bytes) exceeds the limit ( " +
                to_string(kNetScheduleMaxDBDataSize - 1) + " bytes)");
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
                   to_string(val.size()) + " bytes) exceeds the limit ( " +
                   to_string(kMaxQueueNameSize - 1) + " bytes)");
    }
}


bool SNSCommandArguments::x_GetBooleanValue(const string &  val,
                                            const string &  key)
{
    static string   zero = "0";
    static string   one = "1";

    if (val == zero)
        return false;
    if (val == one)
        return true;

    NCBI_THROW(CNetScheduleException, eInvalidParameter,
               key + " parameter accepted values are 0 and 1");
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


bool SNSCommandArguments::x_GetOrderFirst(const string &  val)
{
    static string   first = "first";
    static string   last = "last";

    if (val == first)
        return true;
    if (val == last)
        return false;

    NCBI_THROW(CNetScheduleException, eInvalidParameter,
               "The order parameter valid values are 'first' and 'last'");
}


TDumpFields SNSCommandArguments::x_GetDumpFields(const string &  val)
{
    TDumpFields     fields = 0;

    if (val.empty())
        return eAll;

    list<string>    field_names;
    NStr::Split(val, ",", field_names);
    for (const auto &   field_name : field_names) {
        auto    it = ks_DumpFields.find(field_name);

        if (it == ks_DumpFields.end()) {
            ERR_POST(Warning << "Dump field name " << field_name
                             << " is not supported. Ignore and continue.");
        } else {
            fields |= it->second;
        }
    }
    return fields;
}

