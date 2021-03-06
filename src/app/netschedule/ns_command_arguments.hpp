#ifndef NETSCHEDULE_JS_COMMAND_ARGUMENTS__HPP
#define NETSCHEDULE_JS_COMMAND_ARGUMENTS__HPP

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

#include <connect/services/netservice_protocol_parser.hpp>
#include <connect/services/compound_id.hpp>
#include <string>

#include "ns_types.hpp"


BEGIN_NCBI_SCOPE


enum EDumpFields {
    eId = (1 << 0),
    eKey = (1 << 1),
    eStatus = (1 << 2),
    eLastTouch = (1 << 3),
    eEraseTime = (1 << 4),
    eRunExpiration = (1 << 5),
    eReadExpiration = (1 << 6),
    eSubmitNotifPort = (1 << 7),
    eSubmitNotifExpiration = (1 << 8),
    eListenerNotif = (1 << 9),
    eListenerNotifExpiration = (1 << 10),
    eEvents = (1 << 11),
    eRunCounter = (1 << 12),
    eReadCounter = (1 << 13),
    eAffinity = (1 << 14),
    eGroup = (1 << 15),
    eMask = (1 << 16),
    eInput = (1 << 17),
    eOutput = (1 << 18),
    eProgressMsg = (1 << 19),
    eRemoteClientSID = (1 << 20),
    eRemoteClientIP = (1 << 21),
    eNcbiPHID = (1 << 22),
    eNeedSubmitProgressMsgNotif = (1 << 23),
    eNeedListenerProgressMsgNotif = (1 << 24),
    eNeedStolenNotif = (1 << 25),
    eGCEraseTime = (1 << 26),
    eScope = (1 << 27),

    eAll = eId | eKey | eStatus | eLastTouch | eEraseTime |
           eRunExpiration | eReadExpiration | eSubmitNotifPort |
           eSubmitNotifExpiration | eListenerNotif |
           eListenerNotifExpiration | eEvents |
           eRunCounter | eReadCounter | eAffinity | eGroup |
           eMask | eInput | eOutput | eProgressMsg | eRemoteClientSID |
           eRemoteClientIP | eNcbiPHID | eNeedSubmitProgressMsgNotif |
           eNeedListenerProgressMsgNotif | eNeedStolenNotif |
           eGCEraseTime | eScope
};
typedef long TDumpFields;


struct SNSCommandArguments
{
    unsigned int    job_id;
    int             job_return_code;
    unsigned int    port;
    unsigned int    timeout;
    unsigned int    job_mask;
    unsigned int    start_after_job_id;
    unsigned int    count;
    int             client_data_version;

    string          cmd;
    string          auth_token;
    string          input;
    string          output;
    string          affinity_token;
    string          job_key;
    string          queue_from_job_key;
    string          err_msg;
    string          comment;
    string          description;
    string          ip;
    string          option;
    string          progress_msg;
    string          qname;
    string          qclass;
    string          sid;
    string          job_statuses_string;
    string          aff_to_add;
    string          aff_to_del;
    string          start_after;
    string          group;
    string          alert;
    string          service;
    string          user;
    string          client_data;
    string          ncbi_phid;
    string          scope;

    bool            any_affinity;
    bool            wnode_affinity;
    bool            reader_affinity;
    bool            exclusive_new_aff;
    bool            mode;
    bool            drain;
    bool            effective;
    bool            pullback;
    bool            blacklist;  // RETURN2 only: add or not to blacklist
    bool            no_retries; // FPUT2 only
    bool            prioritized_aff;    // GET2/READ2

    // READ/READ2 commands
    bool            affinity_may_change;
    bool            group_may_change;

    bool            need_progress_msg;
    bool            need_stolen;

    // DUMP
    bool            order_first;
    TDumpFields     dump_fields;

    vector<TJobStatus>  job_statuses;

    void AssignValues(TNSProtoParams &           params,
                      const string &             command,
                      bool                       need_to_generate,
                      CSocket &                  peer_socket,
                      CCompoundIDPool::TInstance id_pool);

    private:
        void x_Reset();
        void x_CheckAffinityList(const string &  val);
        void x_CheckGroupList(const string &  val);
        void x_CheckQueueName(const string &  val, const string &  key);
        bool x_GetBooleanValue(const string &  val, const string &  key);
        string x_NormalizeErrorMessage(const string &  val);
        bool x_GetOrderFirst(const string &  val);
        TDumpFields x_GetDumpFields(const string &  val);
};

END_NCBI_SCOPE

#endif

